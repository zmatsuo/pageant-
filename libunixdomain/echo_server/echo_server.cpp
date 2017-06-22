
//
#include <stdio.h>
#include <inttypes.h>
#if defined(_MSC_VER) || defined(__MINGW32__)
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif
#include <errno.h>
#include <signal.h>
#if !defined(_MSC_VER)
#include <unistd.h>
#else
#include <windows.h>
#endif
#include <vector>
#include <thread>

#include "sock_server.h"

#if defined(_MSC_VER) || defined(__MINGW32__)
#define SOCK_PATH	"C:/cygwin64/tmp/test.sock"
#else
#define SOCK_PATH	"/tmp/test.sock"
#endif

static sock_server_t *pSS;
static bool exit_flag;

static void signal_handler(int sig)
{
	printf("signal called %d\n", sig);
	exit_flag = true;
	signal(sig, signal_handler);
	sock_server_reqest_exit(pSS);
}

static void set_signal_handler()
{
	signal(SIGINT, signal_handler);		// ctrl+c
#if defined(SIGBREAK)
    signal(SIGBREAK, signal_handler);	// ctrl+break windows only?
#endif
    signal(SIGTERM, signal_handler);	// unavailable on windows
	signal(SIGABRT, signal_handler);	// available on windows ?
}

typedef struct {
	std::vector<uint8_t> recv_buf;
	size_t buf_size;
	size_t recv_size;
	size_t left_size;
	bool flash;
} echo_server_data_t;

#if defined(_MSC_VER)
static void narrow_buf(SOCKET sock)
{
	int size = 10;
	setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char*)&size, (int)sizeof(size));
	setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)&size, (int)sizeof(size));
}
#else
static void narrow_buf(int sock)
{
	int size = 10;
	setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char*)&size, (int)sizeof(size));
	setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)&size, (int)sizeof(size));
}
#endif

#define RECV_BUF_SIZE		512
#define RECV_TIMEOUT		10*1000		// ms
#define CONNECT_TIMEOUT		10*1000		// ms

static int echo_server(sock_server_notify_param_st *notify)
{
	echo_server_data_t *me = (echo_server_data_t *)notify->data;
		
	switch (notify->type) {
	case SS_NOTIFY_CONNECT:
	{
		printf("connect\n");
		me = new echo_server_data_t;
		narrow_buf(notify->sock);
		notify->u.connect.data = me;
		const size_t init_buf_size = RECV_BUF_SIZE;
		me->recv_buf.resize(init_buf_size);
		me->buf_size = init_buf_size;
		me->recv_size = 0;
		me->left_size = init_buf_size;
		me->flash = false;
		notify->recv.ptr = &me->recv_buf[me->recv_size];
		notify->recv.size = me->left_size;
		printf("recvreq %zd\n", notify->recv.size);
		notify->send.ptr = NULL;
		notify->send.size = 0;
		notify->timeout = CONNECT_TIMEOUT;
		break;
	}
	case SS_NOTIFY_CLOSE:
		printf("close from client\n");
		delete me;
		break;
	case SS_NOTIFY_RECV:
	{
		me->left_size -= notify->u.recv.size;
		me->recv_size += notify->u.recv.size;
		printf("recv len=%zd buf=%zd/%zd\n",
			   notify->u.recv.size, me->recv_size, me->buf_size);
		if (me->left_size == 0) {
			me->left_size = RECV_BUF_SIZE;
			me->buf_size += RECV_BUF_SIZE;
			me->recv_buf.resize(me->buf_size);
		}
		if (me->recv_size > me->buf_size * 90 / 100) {
			notify->send.ptr = &me->recv_buf[0];
			notify->send.size = me->recv_size;
			printf("send %zd\n", notify->send.size);
			notify->recv.size = 0;
		} else {
			notify->recv.ptr = &me->recv_buf[me->recv_size];
			notify->recv.size = me->left_size;
			printf("recvreq %zd\n", notify->recv.size);
			notify->timeout = RECV_TIMEOUT;
		}
		break;
	}
	case SS_NOTIFY_SEND:
	{
		printf("send finish\n");
		if (me->flash) {
			printf("close\n");
			printf(" bufsize %zd\n", me->buf_size);
			delete me;
			return 1;
		}
		me->recv_size = 0;
		me->left_size = me->buf_size;
		notify->recv.ptr = &me->recv_buf[0];
		notify->recv.size = me->buf_size;
		printf("recvreq %zd\n", notify->recv.size);
		notify->send.ptr = NULL;
		notify->send.size = 0;
		break;
	}
	case SS_NOTIFY_TIMEOUT:
	{
		if (me->recv_size != 0) {
			printf("timeout, flash recv data\n");
			notify->send.ptr = &me->recv_buf[0];
			notify->send.size = me->recv_size;
			printf("send %zd\n", notify->send.size);
			notify->recv.ptr = NULL;
			notify->recv.size = 0;
			notify->timeout = CONNECT_TIMEOUT;
		} else {
			printf("timeout, close\n");
			printf(" bufsize %zd\n", me->buf_size);
			delete me;
			return 1;
		}
		break;
	}
	case SS_NOTIFY_DISCONNECT:
	{
		printf("disconnect from client\n");
		if (me->recv_size != 0) {
			notify->send.ptr = &me->recv_buf[0];
			notify->send.size = me->recv_size;
			me->flash = true;
			printf("send %zd (flash)\n", notify->send.size);
		} else {
			printf("close\n");
			printf(" bufsize %zd\n", me->buf_size);
			delete me;
			return 1;
		}
		break;
	}
	default:
		break;
	}
	return 0;	// continue
}

static void echo_server_thread()
{
	printf("server start\n");
	sock_server_main(pSS);
	printf("server finish\n");
}

int main()
{
	printf("echo server\n");

#if defined(_MSC_VER) && defined(_DEBUG)
	{
		int tmpFlag = _CrtSetDbgFlag( _CRTDBG_REPORT_FLAG );
		tmpFlag |= 0
			| _CRTDBG_LEAK_CHECK_DF
			| 0;
		_CrtSetDbgFlag( tmpFlag );
	}
#endif
	set_signal_handler();

	sock_server_init_t init = { sizeof(sock_server_init_t) };
	init.accept_count = 20;
#if 1
	init.socket_type = SOCK_SERVER_TYPE_UNIXDOMAIN;
	init.socket_path = SOCK_PATH;
	printf("%s\n", init.socket_path);
#else
	init.socket_type = SOCK_SERVER_TYPE_TCP;
	init.port_no = 30000;
	printf("port %d\n", init.port_no);
#endif
	init.serv_func = echo_server;
	pSS = sock_server_init(&init);
	if (pSS == NULL) {
		printf("init error\n");
		return 0;
	}
	int r = sock_server_open(pSS);
	if (r != 0) {
		printf("open error %d\n", r);
		return 0;
	}
	exit_flag = false;
	std::thread th(echo_server_thread);
	while(exit_flag == false) {
#if defined(_MSC_VER)
		Sleep(10);
#else
		usleep(10 * 1000);
#endif
	}
	th.join();
	sock_server_close(pSS);

	return 0;
}

// Local Variables:
// mode: c-mode
// coding: utf-8-with-signature
// tab-width: 4
// End:
