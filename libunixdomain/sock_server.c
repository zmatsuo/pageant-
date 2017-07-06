/*
 *	simple socket server
 *
 *
 *
 */

// configuable
//#define	FD_SETSIZE	10

//
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>
#if defined(_MSC_VER) || defined(__MINGW32__)
#include <winsock2.h>
#include <windows.h>
#include "ud_socket.h"
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/un.h>
#endif
#include <errno.h>
#if defined(_MSC_VER)
#include <io.h>			// for unlink()
#else
#include <unistd.h>
#include <string.h>		// for strdup()
#endif
#if defined(__MINGW32__)
#if __MINGW32_MAJOR_VERSION <= 3 || __MINGW32_MINOR_VERSION < 11
WINBASEAPI ULONGLONG WINAPI GetTickCount64(VOID);	// there was no prototype
#endif
#endif

#include "sock_server.h"

#if !defined(_MSC_VER) && !defined(__MINGW32__)
typedef int SOCKET;
#define closesocket(p)	close(p)
#define INVALID_SOCKET	(-1)
#define	SOCKET_ERROR	(-1)
#define	SD_BOTH			SHUT_RDWR
#define	SD_RECEIVE		SHUT_RD
#endif

#if defined(_MSC_VER)
#pragma comment(lib, "ws2_32.lib")
#endif

#if 1
#define debug(...)
#else
#define debug(...)  printf(__VA_ARGS__)
#endif


typedef struct server_list_st {
	SOCKET sock;
	_Bool available;
	_Bool shutdown_receive;
	const uint8_t *send_ptr;
	size_t send_size;
	uint8_t *recv_ptr;
	size_t recv_size;
	void *data;
	uint64_t timeout;
} server_list_t;

typedef struct sock_server_st {
	int server_list_size;
	server_list_t *server_list;
	SOCKET listen_socket;
	sock_server_type_t socket_type;
	uint16_t port_no;
#if defined(_MSC_VER) || defined(__MINGW32__)
	const wchar_t *socket_path;
#else
	const char *socket_path;
#endif
	fd_set read_set;
	fd_set write_set;
	_Bool exit_requet_flag;
	int (*serv_func)(sock_server_notify_param_t *param);
	_Bool running_flag;
#if defined(_MSC_VER) || defined(__MINGW32__)
	ud_data_t *fd_ud;
#endif
} sock_server_t;

/**
 *	utf8 -> wchar_t
 *	buffer is allocaed by malloc()
 */
#if defined(_MSC_VER) || defined(__MINGW32__)
static wchar_t *utf8_2_utf16(const char *utf8)
{
	if (utf8 == NULL ) {
		return NULL;
	}
	int size = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
	wchar_t *buf = (wchar_t *)malloc(sizeof(wchar_t)*size);
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, buf, size);
    return buf;
}
#endif

/**
 * @retval	ms unit
 */
static uint64_t get_serial_time(void)
{
#if defined(_MSC_VER) || defined(__MINGW32__)
	return (uint64_t)GetTickCount64();
#else
	struct timespec tp;
#if defined(CLOCK_BOOTTIME)
	clock_gettime(CLOCK_BOOTTIME, &tp);
#else
	clock_gettime(CLOCK_REALTIME, &tp);
#endif
	uint64_t t = tp.tv_sec * 1000 + tp.tv_nsec / 1000000;
	return t;
#endif
}

/**
 *	初期化
 *
 *	@return	sock_server_tのポインタ
 *			NULLのときエラー
 */
sock_server_t *sock_server_init(const sock_server_init_t *init_info)
{
	if (init_info == NULL) {
		return NULL;
	}
	if (init_info->size != sizeof(sock_server_init_t)) {
		return NULL;
	}
#if 0
	debug("system:%s\n",
		   /// system
#if defined(_MSC_VER)
		   "Visual Studio"
#elif defined(__CYGWIN__)
		   "cygwin"
#else
		   "?? linux??"
#endif
		);
#endif

	sock_server_t *pSS = NULL;

#if defined(_MSC_VER) || defined(__MINGW32__)
	WORD wVersionRequested = WINSOCK_VERSION;	/*MAKEWORD(2,2)*/
	if (init_info->winsock_version != 0) {
		wVersionRequested = init_info->winsock_version;
	}
	WSADATA wsaData;
	int r = WSAStartup(wVersionRequested, &wsaData);
	if (r != 0) {
		goto cleanup;
	}
#endif

	pSS = (sock_server_t *)calloc(sizeof(sock_server_t), 1);
	if (pSS == NULL) {
		return NULL;
	}
		
	if (init_info->socket_type < 0 || 
		SOCK_SERVER_TYPE_MAX <= init_info->socket_type) {
		goto cleanup;
	}
	pSS->socket_type = init_info->socket_type;

	pSS->exit_requet_flag = false;
	pSS->serv_func = init_info->serv_func;
	switch (pSS->socket_type) {
	case SOCK_SERVER_TYPE_TCP:
		pSS->port_no = init_info->port_no;
		break;
	case SOCK_SERVER_TYPE_UNIXDOMAIN:
#if defined(_MSC_VER) || defined(__MINGW32__)
		if (init_info->Wsocket_path != NULL) {
			pSS->socket_path = _wcsdup(init_info->Wsocket_path);
		} else {
			pSS->socket_path = utf8_2_utf16(init_info->socket_path);
		}
#else
		pSS->socket_path = strdup(init_info->socket_path);
#endif
		break;
    default:
        goto cleanup;
	}
	if (init_info->serv_func == NULL) {
		goto cleanup;
	}
	pSS->server_list_size = init_info->accept_count;
	if (pSS->server_list_size > FD_SETSIZE) {
		// clip, too large
		pSS->server_list_size = FD_SETSIZE;
	}
	pSS->serv_func = init_info->serv_func;

	size_t sz = sizeof(server_list_t)*pSS->server_list_size;
	pSS->server_list = (server_list_t *)malloc(sz);
	for (int i = 0; i< pSS->server_list_size; i++) {
		pSS->server_list[i].available = false;
	}
	return pSS;

cleanup:
	free(pSS);
	return NULL;
}

/**
 *	通信用のソケットを開く
 *
 *	@retval	0	エラーなし
 *
 */
int sock_server_open(sock_server_t *pSS)
{
	SOCKET sock;
	int r;

	switch (pSS->socket_type) {
	case SOCK_SERVER_TYPE_TCP:
		sock = socket(AF_INET, SOCK_STREAM, 0);
		break;
	case SOCK_SERVER_TYPE_UNIXDOMAIN:
#if defined(_MSC_VER) || defined(__MINGW32__)
	{
		ud_open_data_t ud_open_data = {
			sizeof(ud_open_data)
		};
		ud_open_data.flag = UD_FLAG_UNIX_DOMAIN_CYGWIN;
		ud_open_data.WSockPath = pSS->socket_path;
		sock = ud_socket(&ud_open_data, &pSS->fd_ud);
	}
#else
		sock = socket(AF_UNIX, SOCK_STREAM, 0);
#endif
		break;
	default:
		sock = INVALID_SOCKET;
		break;
	}
	if (sock == INVALID_SOCKET) {
		debug("socket() error\n");
		return 1;
	}

	switch(pSS->socket_type) {
	case SOCK_SERVER_TYPE_TCP:
	{
		// tcp socket
		struct sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(pSS->port_no);
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); /* localhost, 127.0.0.1*/

		r = bind(sock, (struct sockaddr *)&addr, sizeof(addr));
		break;
	}
	case SOCK_SERVER_TYPE_UNIXDOMAIN:
		// unix domain socket
#if defined(_MSC_VER) || defined(__MINGW32__)
		debug("bind unixdomain '%ls'\n", pSS->socket_path);
		r = ud_bind(sock, pSS->fd_ud);
#else
	{
		const char *socket_path = pSS->socket_path;
		debug("bind unixdomain '%s'\n", socket_path);
		if (access(socket_path, 0) == 0) {
			unlink(socket_path);
			if (access(socket_path, 0) == 0) {
				return EISCONN;
			}
		}
		struct sockaddr_un addr;
		memset(&addr, 0, sizeof(addr));
		addr.sun_family = AF_UNIX;
		strcpy(addr.sun_path, pSS->socket_path);
		r = bind(sock, (struct sockaddr *)&addr, sizeof(addr));
	}
#endif
		break;
	default:
		r = SOCKET_ERROR;
		break;
	}
	if (r == SOCKET_ERROR) {
		debug("bind() error %d\n", errno);
		return 1;
	}
	r = listen(sock, pSS->server_list_size);
	if (r == SOCKET_ERROR) {
		debug("listen() error\n");
		return 1;
	}

	pSS->listen_socket = sock;
	return 0;
}

static void close_server(
	sock_server_t *pSS,
	int server_index)
{
	server_list_t *p = &pSS->server_list[server_index];
	SOCKET sock = p->sock;
	FD_CLR(sock, &pSS->read_set);
	FD_CLR(sock, &pSS->write_set);
	shutdown(sock, SD_BOTH);
	closesocket(sock);
	p->available = false;
}

static void call_server(
	sock_server_t *pSS,
	int server_index,
	sock_server_notify_param_t *notify_param)
{
	sock_server_notify_t notify_type = notify_param->type;

	server_list_t *p = &pSS->server_list[server_index];

	notify_param->data = p->data;
	notify_param->sock = p->sock;
	notify_param->send.ptr = p->send_ptr;
	notify_param->send.size = p->send_size;
	notify_param->timeout = p->timeout;
	int r = pSS->serv_func(notify_param);
	if (r == 0) {
		// continue;
		if (notify_type == SS_NOTIFY_CONNECT) {
			p->data = notify_param->u.connect.data;
		}
		if (p->timeout != notify_param->timeout)
		{
			p->timeout = get_serial_time() + notify_param->timeout;
		}
		if (p->shutdown_receive == false &&
			(notify_param->recv.ptr != p->recv_ptr ||
			 notify_param->recv.size != p->recv_size))
		{
			p->recv_ptr = notify_param->recv.ptr;
			p->recv_size = notify_param->recv.size;
			if (p->recv_ptr == NULL || p->recv_size == 0) {
				debug("[%d] recv buf is empty?\n", server_index);
			}
		}
		if (notify_param->send.ptr != p->send_ptr ||
			notify_param->send.size != p->send_size)
		{
			p->send_ptr = notify_param->send.ptr;
			p->send_size = notify_param->send.size;
			if (p->send_ptr != NULL && p->send_size != 0) {
				// start send
				FD_SET(p->sock, &pSS->write_set);
			} else {
				// stop send
				FD_CLR(p->sock, &pSS->write_set);
			}
		}
	} else {
		// disconnect
		debug("[%d] disconnect from server\n", server_index);
		close_server(pSS, server_index);
	}
}

int sock_server_main(sock_server_t *pSS)
{
	debug("start server\n");
	pSS->running_flag = true;
	SOCKET listen_sock = pSS->listen_socket;
	FD_ZERO(&pSS->read_set);
	FD_ZERO(&pSS->write_set);
	FD_SET(listen_sock, &pSS->read_set);
	while(1) {
		SOCKET fd_max = listen_sock;
		for (int i=0; i<pSS->server_list_size; i++) {
			if (pSS->server_list[i].available != false) {
				SOCKET fd = pSS->server_list[i].sock;
				if (fd_max < fd) {
					fd_max = fd;
				}
			}
		}

		int select_ret;
		fd_set read_set_tmp;
		fd_set write_set_tmp;
		while(1) {
			struct timeval polling_interval;
			polling_interval.tv_sec = 0;			// sec
			polling_interval.tv_usec = 10*1000;		// usec
			read_set_tmp = pSS->read_set;
			write_set_tmp = pSS->write_set;
			select_ret = select((int)(fd_max+1),
								&read_set_tmp, &write_set_tmp,
								NULL, &polling_interval);
			if (select_ret != 0) {
				// not timeout
				break;
			}

			if (pSS->exit_requet_flag) {
				// exit
				break;
			}

			sock_server_notify_param_t notify_param = { SS_NOTIFY_TIMEOUT };
			uint64_t now = get_serial_time();
			bool timeout = false;
			for (int i = 0; i < pSS->server_list_size; i++) {
				server_list_t *p = &pSS->server_list[i];
				if (p->available == false || p->timeout == 0) {
					continue;
				}
				if (now > p->timeout) {
					p->timeout = 0;
					call_server(pSS, i, &notify_param);
					timeout = true;
				}
			}
			if (timeout) {
				break;
			}
		}

		if (pSS->exit_requet_flag) {
			// exit
			break;
		}

		if (select_ret > 0) {

			// listen socket
			if (FD_ISSET(listen_sock, &read_set_tmp)) {
				// accept
				struct sockaddr_in client;
				int len = sizeof(client);
				SOCKET client_sock;
				debug("accept?\n");
#if defined(_MSC_VER) || defined(__MINGW32__)
				if (pSS->socket_type == SOCK_SERVER_TYPE_UNIXDOMAIN) {
					client_sock = ud_accept(listen_sock,
											(struct sockaddr *)&client, &len,
											pSS->fd_ud);
				} else
#endif
				{
					client_sock = accept(listen_sock, (struct sockaddr *)&client, &len);
				}
				if (client_sock == INVALID_SOCKET) {
					debug("accept error\n");
				} else {
					int i = 0;
					while(i < pSS->server_list_size &&
						  pSS->server_list[i].available != false)
						i++;
					if (i == pSS->server_list_size) {
						debug("no space\n");
						shutdown(client_sock, SD_BOTH);
						closesocket(client_sock);
					} else {
						server_list_t *p = &pSS->server_list[i];
						debug("[%d] accept\n", i);
						p->sock = client_sock;
						p->available = true;
						p->send_ptr = NULL;
						p->send_size = 0;
						p->timeout = 0;
						p->data = NULL;
						p->shutdown_receive = false;
						sock_server_notify_param_t notify_param = { SS_NOTIFY_CONNECT };
						call_server(pSS, i, &notify_param);
						FD_SET(client_sock, &pSS->read_set);
					}
				}
			}
			
			for (int i = 0; i < pSS->server_list_size; i++) {
				server_list_t *p = &pSS->server_list[i];
				if (p->available == false) {
					continue;
				}

				SOCKET sock = p->sock;
				if (FD_ISSET(sock, &read_set_tmp)) {
					// receive
					uint8_t *recv_ptr = p->recv_ptr;
					size_t recv_size = p->recv_size;
					if (recv_ptr != NULL && recv_size > 0) {
						int r = recv(sock, (char *)recv_ptr, (int)recv_size, 0);
						if (r == 0) {
							// disconnect from client
							debug("[%d] recv() = 0 disconnect from client\n", i);
							FD_CLR(p->sock, &pSS->read_set);
							shutdown(p->sock, SD_RECEIVE);
							p->shutdown_receive = true;
							sock_server_notify_param_t notify_param = { SS_NOTIFY_DISCONNECT };
							call_server(pSS, i, &notify_param);
						} else if (r < 0) {
							// error
							debug("[%d] recv error\n", i);
							sock_server_notify_param_t notify_param = { SS_NOTIFY_CLOSE };
							call_server(pSS, i, &notify_param);
							close_server(pSS, i);
						} else if (p->recv_ptr == NULL || p->recv_size == 0) {
							debug("[%d] recv: buf is empty?\n", i);
						} else {
							recv_size = r;
							debug("[%d] recv: len=%zd\n", i, recv_size);
							sock_server_notify_param_t notify_param = { SS_NOTIFY_RECV };
							notify_param.u.recv.ptr = recv_ptr;
							notify_param.u.recv.size = recv_size;
							p->recv_ptr += recv_size;
							p->recv_size -= recv_size;
							call_server(pSS, i, &notify_param);
						}
					}
				}

				if (FD_ISSET(sock, &write_set_tmp)) {
					// send
					int r = send(sock, (char *)p->send_ptr, (int)p->send_size, 0);
					if (r <= 0) {
						// disconnect or error
						debug("[%d] %s send\n", i, r == 0 ? "disconnect" : "error");
						sock_server_notify_param_t notify_param = { SS_NOTIFY_CLOSE };
						call_server(pSS, i, &notify_param);
						close_server(pSS, i);
						p->available = false;
					} else {
						size_t send_size = r;
						size_t left = p->send_size - send_size;
						debug("[%d] send: len=%zd (left %zd)\n", i, send_size, left);
						p->send_size = left;
						p->send_ptr += send_size;
						if (left == 0) {
							p->send_size = 0;
							FD_CLR(sock, &pSS->write_set);
							sock_server_notify_param_t notify_param = { SS_NOTIFY_SEND };
							call_server(pSS, i, &notify_param);
						}
					}
				}
			}
		} else if (select_ret < 0) {
			debug("select return signal?\n");
			if (errno == EINTR) {
				// シグナル受信
				if (pSS->exit_requet_flag) {
					break;
				}
				//なにもしない
				continue;
			}
			debug("select error\n");
		}
	}


	// close clients
	for (int i = 0; i < pSS->server_list_size; i++) {
		if (pSS->server_list[i].available == true) {
			debug("[%d] disconnect\n", i);
			close_server(pSS, i);
		}
	}

	// close listen socket
#if defined(_MSC_VER) || defined(__MINGW32__)
	if (pSS->socket_type == SOCK_SERVER_TYPE_UNIXDOMAIN) {
		ud_closesocket(listen_sock, pSS->fd_ud);
	} else
#endif
	{
		shutdown(listen_sock, SD_BOTH);
		closesocket(listen_sock);
		if (pSS->socket_type == SOCK_SERVER_TYPE_UNIXDOMAIN) {
#if defined(_MSC_VER) || defined(__MINGW32__)
			_wunlink(pSS->socket_path);
#else
			unlink(pSS->socket_path);
#endif
		}
	}

	debug("finish\n");
	pSS->running_flag = false;
	return 0;
}

void sock_server_reqest_exit(sock_server_t *pSS)
{
	pSS->exit_requet_flag = 1;
	while (pSS->running_flag) {
#if defined(_MSC_VER) || defined(__MINGW32__)
		Sleep(10);	// 10ms
#else
		usleep(10*1000);
#endif
	};
}

void sock_server_close(sock_server_t *pSS)
{
	free(pSS->server_list);
	pSS->server_list = NULL;
	free((void *)pSS->socket_path);
	pSS->socket_path = NULL;
	free(pSS);

#if defined(_MSC_VER) || defined(__MINGW32__)
	WSACleanup();
#endif
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature-unix
// tab-width: 4
// End:
