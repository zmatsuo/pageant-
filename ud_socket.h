
#if !defined(_UD_SOCKET_H_)
#define _UD_SOCKET_H_

#include <stdint.h>
#include <winsock2.h>
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ud_data_st ud_data_t;

#define UD_FLAG_NATIVE					(0)
#define UD_FLAG_UNIX_DOMAIN_CYGWIN		(1)		// windows only
#define UD_FLAG_UNIX_DOMAIN_MINGW		(2)		// windows only

typedef struct ud_open_data_st {
	uint16_t size;				// sizeof(ud_open_data_t)
	uint16_t flag;
//	uint16_t portNo;			// sock_type = AF_INET
	const char *sockPath;		// (utf8) flag == UNIX_DOMAIN
	const char *ASockPath;		// (ansi) refferd when sockPath == NULL
	const wchar_t *WSockPath;	// (wchar_t) refferd when ASockPath == NULL
} ud_open_data_t;

SOCKET ud_socket(const ud_open_data_t *data, ud_data_t **fd_ud);
int ud_bind(SOCKET sock, ud_data_t *fd_ud);
SOCKET ud_accept(SOCKET sock, struct sockaddr *client, int *len, ud_data_t *fd_ud);
void ud_closesocket(SOCKET sock, ud_data_t *fd_ud);
const char *ud_version(void);

#ifdef __cplusplus
}
#endif

#endif	// _UD_SOCKET_H_

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
