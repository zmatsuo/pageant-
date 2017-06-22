
#if !defined(_SOCK_SOCKET_H_)
#define _SOCK_SOCKET_H_

#include <stdint.h>
#include <stdlib.h>		// for size_t
#if defined(_MSC_VER)
#include <winsock2.h>	// for SOCKET
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sock_server_st sock_server_t;

typedef enum {
	SOCK_SERVER_TYPE_TCP,
	SOCK_SERVER_TYPE_UNIXDOMAIN,
	SOCK_SERVER_TYPE_MAX,
} sock_server_type_t;

typedef enum {
	SS_NOTIFY_CONNECT,
	SS_NOTIFY_DISCONNECT,		// disconnect from client
	SS_NOTIFY_CLOSE,			// cleanup
	SS_NOTIFY_RECV,
	SS_NOTIFY_SEND,
	SS_NOTIFY_TIMEOUT
} sock_server_notify_t;

typedef struct sock_server_notify_param_st {
	sock_server_notify_t type;
#if defined(_MSC_VER) || defined(__MINGW32__)
	SOCKET sock;
#else
	int sock;
#endif
	union {
		struct {
			void *data;				// @retval	data
			const struct sockaddr *addr;
		} connect;
		struct {
			uint8_t *ptr;
			size_t size;
		} recv;
	} u;
	struct {
		uint8_t *ptr;
		size_t size;
	} recv;
	struct {
		const uint8_t *ptr;
		size_t size;
	} send;
	uint64_t timeout;
	void *data;
} sock_server_notify_param_t;

typedef struct sock_server_init_st {
	int size;					// sizeof(sock_server_init_t)
	int accept_count;			// client(server) max
	int (*serv_func)(sock_server_notify_param_t *notify);
								// 
	sock_server_type_t socket_type;
	uint16_t port_no;
	const char *socket_path;	// utf8
#if defined(_MSC_VER) || defined(__MINGW32__)
//	const char *Asocket_path;	// acp
	const wchar_t *Wsocket_path;// wchar
#endif
	uint16_t winsock_version;	// optional (windows only)
} sock_server_init_t;

sock_server_t *sock_server_init(const sock_server_init_t *init_info);
int sock_server_open(sock_server_t *pSS);
int sock_server_main(sock_server_t *pSS);
void sock_server_close(sock_server_t *pSS);
void sock_server_reqest_exit(sock_server_t *pSS);

#ifdef __cplusplus
}
#endif

#endif	// _SOCK_SOCKET_H_

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
