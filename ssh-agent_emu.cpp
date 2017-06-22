
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
#include <stdint.h>
#include <thread>
#include <vector>

#include "debug.h"
#include "libunixdomain\sock_server.h"
#include "ssh-agent_emu.h"
#include "pageant.h"
#include "puttymem.h"

// TODO:hへ
extern "C" void smemclr(void *b, size_t len);


#undef DEBUG
#undef debug

#if 0
#define debug(...)
#else
#define debug(...)  	debug_printf(__VA_ARGS__)
#endif

static sock_server_t *pSS;
static bool exit_flag;

#define RECV_BUF_SIZE		512
#define RECV_TIMEOUT		10*1000	// ms
#define CONNECT_TIMEOUT		10*1000	// ms

typedef struct {
	std::vector<uint8_t> recv_buf;
	size_t buf_size;
	size_t recv_size;
	size_t left_size;
	uint8_t *send_buf;
	size_t send_size;
	bool flash;
} echo_server_data_t;

static inline int msglen(const void *p)
{
	return 4 + ntohl(*(const uint32_t *)p);
}

static int ssh_agent_server(sock_server_notify_param_st *notify)
{
	echo_server_data_t *me = (echo_server_data_t *)notify->data;
		
	switch (notify->type) {
	case SS_NOTIFY_CONNECT:
	{
		debug("connect\n");
		me = new echo_server_data_t;
		notify->u.connect.data = me;
		const size_t init_buf_size = RECV_BUF_SIZE;
		me->recv_buf.resize(init_buf_size);
		me->buf_size = init_buf_size;
		me->recv_size = 0;
		me->left_size = init_buf_size;
		me->flash = false;
		notify->recv.ptr = &me->recv_buf[me->recv_size];
		notify->recv.size = me->left_size;
		debug("recvreq %zd\n", notify->recv.size);
		notify->send.ptr = NULL;
		notify->send.size = 0;
		notify->timeout = CONNECT_TIMEOUT;
		break;
	}
	case SS_NOTIFY_CLOSE:
	case SS_NOTIFY_DISCONNECT:
		debug("close from client\n");
		delete me;
		return 1;
		break;
	case SS_NOTIFY_RECV:
	{
		me->left_size -= notify->u.recv.size;
		me->recv_size += notify->u.recv.size;
		debug("recv len=%zd buf=%zd/%zd\n",
			   notify->u.recv.size, me->recv_size, me->buf_size);
		if (me->left_size == 0) {
			me->left_size = RECV_BUF_SIZE;
			me->buf_size += RECV_BUF_SIZE;
			me->recv_buf.resize(me->buf_size);
		}
		notify->recv.ptr = &me->recv_buf[me->recv_size];
		notify->recv.size = me->left_size;
		notify->timeout = RECV_TIMEOUT;
		if (me->recv_size >= 5) {
			int len = msglen(&me->recv_buf[0]);
			if (me->recv_size >= len) {
				int replaylen;
				void *send = pageant_handle_msg_2(		// pageant.c
					&me->recv_buf[0], &replaylen);
				me->send_buf = (uint8_t *)send;
				me->send_size = replaylen;
				notify->send.ptr = (uint8_t *)send;
				notify->send.size = replaylen;
				debug("send %zd\n", notify->send.size);
				notify->recv.size = 0;
				notify->timeout = CONNECT_TIMEOUT;
			}
		}
		break;
	}
	case SS_NOTIFY_SEND:
	{
		debug("send finish\n");

		smemclr(me->send_buf,me->send_size);
		sfree(me->send_buf);
		me->send_buf = NULL;
		me->send_size = 0;
		notify->send.ptr = NULL;
		notify->send.size = 0;

		me->recv_size = 0;
		me->left_size = me->buf_size;
		notify->recv.ptr = &me->recv_buf[0];
		notify->recv.size = me->buf_size;
		debug("recvreq %zd\n", notify->recv.size);
		break;
	}
	case SS_NOTIFY_TIMEOUT:
	{
		if (me->recv_size != 0) {
			debug("timeout, flash recv data\n");
			me->recv_size = 0;
			me->left_size = me->buf_size;
			notify->timeout = CONNECT_TIMEOUT;
		} else {
			debug("timeout, close\n");
			debug(" bufsize %zd\n", me->buf_size);
			delete me;
			return 1;
		}
		break;
	}
	default:
	{
		break;
	}
	}
	return 0;	// continue
}

static void ssh_agent_server_init(const wchar_t *sock_path)
{
	debug("ssh agent server start (%ls)\n", sock_path);
	sock_server_init_t init = { sizeof(sock_server_init_t) };
	init.accept_count = 10;
	init.socket_type = SOCK_SERVER_TYPE_UNIXDOMAIN;
	init.Wsocket_path = sock_path;
	init.serv_func = ssh_agent_server;
	pSS = sock_server_init(&init);
	if (pSS == NULL) {
		debug("init error\n");
		return;
	}
	int r = sock_server_open(pSS);
	if (r != 0) {
		debug("open error %d\n", r);
		return;
	}
	exit_flag = false;
}

static void echo_server_thread()
{
	debug("server start\n");
	sock_server_main(pSS);
	debug("server finish\n");
}

static std::thread *th;

bool ssh_agent_server_start(const wchar_t *sock_path)
{
	if (th != NULL)
		return false;
	ssh_agent_server_init(sock_path);
	th = new std::thread(echo_server_thread);
	return true;
}

void ssh_agent_server_stop()
{
	if (th != NULL) {
		sock_server_reqest_exit(pSS);
		th->join();
		delete th;
		th = NULL;
		sock_server_close(pSS);
		pSS = NULL;
	}
	debug("ssh agent server finish\n");
}



// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:

