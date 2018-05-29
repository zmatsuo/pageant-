/**
   ssh-agent_emu.cpp

   Copyright (c) 2017-2018 zmatsuo

   This software is released under the MIT License.
   http://opensource.org/licenses/mit-license.php
*/

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

//#define ENABLE_DEBUG_PRINT
#include "debug.h"
#include "sock_server.h"
#include "ssh-agent_emu.h"
#include "pageant.h"
#include "puttymem.h"

#if defined(_DEBUG)
#define new ::new(_NORMAL_BLOCK, __FILE__, __LINE__)
#endif

#if 0
#define debug(...)
#else
#define debug(...)  	dbgprintf(__VA_ARGS__)
#endif

#define RECV_BUF_SIZE		512
#define RECV_TIMEOUT		10*1000	// ms
#define CONNECT_TIMEOUT		10*1000	// ms

// 1クライアント毎のデータ
typedef struct {
	std::vector<uint8_t> recv_buf;
	size_t buf_size;
	size_t recv_size;
	size_t left_size;
	uint8_t *send_buf;
	size_t send_size;
	bool flash;
	std::vector<uint8_t> reply;
} echo_server_data_t;

class SocketServer {
public:
	bool server_init(const wchar_t *sock_path);
	bool server_init_native(const wchar_t *sock_path);
	bool server_init(uint16_t port_no);
	void server_stop();
	SocketServer() {
		th = nullptr;
		pSS = nullptr;
		exit_flag = true;
	}

private:
	std::thread *th;
	sock_server_t *pSS;
	bool exit_flag;

	bool server_init(const sock_server_init_t *init);
	void server_thread_main();
};

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
				pageant_handle_msg(
					&me->recv_buf[0], me->recv_size,
					me->reply);
				smemclr(&me->recv_buf[0], me->recv_size);
				me->send_buf = nullptr;
				me->send_size = 0;
				notify->send.ptr = &me->reply[0];
				notify->send.size = me->reply.size();
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

		smemclr(&me->reply[0], me->reply.size());
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

bool SocketServer::server_init(const sock_server_init_t *init)
{
	pSS = sock_server_init(init);
	if (pSS == nullptr) {
		debug("init error\n");
		return false;
	}
	int r = sock_server_open(pSS);
	if (r != 0) {
		debug("open error %d\n", r);
		return false;
	}
	exit_flag = false;
	th = new std::thread(&SocketServer::server_thread_main, this);
	return true;
}

bool SocketServer::server_init(const wchar_t *sock_path)
{
	debug("ssh agent cygwin unix sock server start (%ls)\n", sock_path);
	sock_server_init_t init = { sizeof(sock_server_init_t) };
	init.accept_count = 10;
	init.socket_type = SOCK_SERVER_TYPE_UNIXDOMAIN;
	init.Wsocket_path = sock_path;
	init.serv_func = ssh_agent_server;
	return server_init(&init);
}

bool SocketServer::server_init_native(const wchar_t *sock_path)
{
	debug("ssh agent native unix sock server start (%ls)\n", sock_path);
	sock_server_init_t init = { sizeof(sock_server_init_t) };
	init.accept_count = 10;
	init.socket_type = SOCK_SERVER_TYPE_UNIXDOMAIN_NATIVE;
	init.Wsocket_path = sock_path;
	init.serv_func = ssh_agent_server;
	return server_init(&init);
}

bool SocketServer::server_init(uint16_t port_no)
{
	debug("ssh agent server start port %d\n", port_no);
	sock_server_init_t init = { sizeof(sock_server_init_t) };
	init.accept_count = 10;
	init.socket_type = SOCK_SERVER_TYPE_TCP;
	init.port_no = port_no;
	init.serv_func = ssh_agent_server;
	return server_init(&init);
}

void SocketServer::server_thread_main()
{
	debug("server start\n");
	sock_server_main(pSS);
	debug("server finish\n");
}

void SocketServer::server_stop()
{
	if (th != nullptr) {
		sock_server_reqest_exit(pSS);
		th->join();
		delete th;
		th = nullptr;
	}
	if (pSS != nullptr) {
		sock_server_close(pSS);
		pSS = nullptr;
	}
}

static SocketServer *unixSocketServer;

bool ssh_agent_cygwin_unixdomain_start(const wchar_t *sock_path)
{
	if (unixSocketServer != nullptr)
		return false;
	unixSocketServer = new SocketServer;
	bool r = unixSocketServer->server_init(sock_path);
	if (r == false) {
		delete unixSocketServer;
		unixSocketServer = nullptr;
		return false;
	}
	return true;
}

void ssh_agent_cygwin_unixdomain_stop()
{
	if (unixSocketServer != nullptr) {
		unixSocketServer->server_stop();
		delete unixSocketServer;
		unixSocketServer = nullptr;
	}
	debug("ssh agent server finish\n");
}

static SocketServer *nativeUnixSocketServer;

bool ssh_agent_native_unixdomain_start(const wchar_t *sock_path)
{
	debug("ssh agent native unix socket server start\n");
	if (nativeUnixSocketServer != nullptr)
		return false;
	nativeUnixSocketServer = new SocketServer;
	bool r = nativeUnixSocketServer->server_init_native(sock_path);
	if (r == false) {
		delete nativeUnixSocketServer;
		nativeUnixSocketServer = nullptr;
		return false;
	}
	return true;
}

void ssh_agent_native_unixdomain_stop()
{
	if (nativeUnixSocketServer != nullptr) {
		nativeUnixSocketServer->server_stop();
		delete nativeUnixSocketServer;
		nativeUnixSocketServer = nullptr;
	}
	debug("ssh agent native unix socket server finish\n");
}

static SocketServer *localhostTcpSocketServer;

bool ssh_agent_localhost_start(int port_no)
{
	if (localhostTcpSocketServer != nullptr)
		return false;
	localhostTcpSocketServer = new SocketServer;
	bool r = localhostTcpSocketServer->server_init(
		static_cast<uint16_t>(port_no));
	if (r == false) {
		delete localhostTcpSocketServer;
		localhostTcpSocketServer = nullptr;
		return false;
	}
	return true;
}

void ssh_agent_localhost_stop()
{
	if (localhostTcpSocketServer != nullptr) {
		localhostTcpSocketServer->server_stop();
		delete localhostTcpSocketServer;
		localhostTcpSocketServer = nullptr;
	}
	debug("ssh agent server finish\n");
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:

