
#if !defined(_MSC_VER) && !defined(__MINGW32__)
#error not support
#endif

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <time.h>

#include <crtdbg.h>
#include <assert.h>

#include "ud_socket.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shlwapi.lib")

#if defined(_DEBUG)
#define malloc(size)	_malloc_dbg(size,_NORMAL_BLOCK,__FILE__,__LINE__)
#endif

typedef struct ud_data_st {
	uint16_t portNo_;
	SOCKET fd_;		// fd
	uint32_t guid_[4];
#if defined(_MSC_VER) || defined(__MINGW32__)
	const wchar_t *sockPath_;
#else
	char *sockPath_;
#endif
} ud_data_t;

#if 1
#define debug(...)
#else
#define debug(...)  printf(__VA_ARGS__)
#endif

/**
 * 49152～65535でポート番号を決める
 */
static uint16_t get_portno(void)
{
#if 1
	srand((unsigned int)time(NULL));
	uint16_t portno = 49152 + (rand() % (65535-49152));
	return portno;
#endif
#if 0
	srandom((unsigned int)time(NULL));
	int r = random();
	uint16_t portno = 49152 + (rand() % (65535-49152));
	return portno;
#endif
}

/**
 *	sockファイルを削除する
 */
static void deleteSockPath(
#if defined(_MSC_VER) || defined(__MINGW32__)
	const wchar_t *sock_path
#else
	const char *sock_path
#endif
	)
{
	if (sock_path == NULL)
		return;

#if defined(_MSC_VER) || defined(__MINGW32__)
	if (GetFileAttributesW(sock_path) != INVALID_FILE_ATTRIBUTES) {
		int r = SetFileAttributesW(sock_path, FILE_ATTRIBUTE_NORMAL);
		r = DeleteFileW(sock_path);
	}
#else
	if (_access(sock_path, 0) == 0) {
		unlink(sock_path);
	}
#endif
}

/**
 *	utf8 -> wchar_t
 *	buffer is allocaed by malloc()
 */
#if defined(_MSC_VER) || defined(__MINGW32__)
static wchar_t *to_utf16(UINT cp, const char *utf8)
{
	int size = MultiByteToWideChar(cp, 0, utf8, -1, NULL, 0);
	wchar_t *buf = (wchar_t *)malloc(sizeof(wchar_t)*size);
    MultiByteToWideChar(cp, 0, utf8, -1, buf, size);
    return buf;
}
#endif

SOCKET ud_socket(const ud_open_data_t *data, ud_data_t **fd_ud)
{
	ud_data_t *self = NULL;
    SOCKET sock = INVALID_SOCKET;
	if (data->size != sizeof(ud_open_data_t)) {
		goto cleanup_error;
	}
	self = (ud_data_t *)calloc(sizeof(ud_data_t), 1);
	if (self == NULL) {
		goto cleanup_error;
	}
	*fd_ud = self;
	
	// ソケット作成
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock == INVALID_SOCKET) {
        debug("socket() %d\n", WSAGetLastError());
		goto cleanup_error;
    }

	self->fd_ = sock;
	self->sockPath_	= NULL;

#if 0
	char value = 1;
	setsockopt(sock,
			   SOL_SOCKET, SO_REUSEADDR, (const char *)&value, sizeof(value));
#endif

	// 16byte GUID
	srand((unsigned int)time(NULL));
	for(int i = 0 ; i < _countof(self->guid_); i++) {
		self->guid_[i] = rand() * 0x10000 + rand();		// TODO: RAND_MAX=32767(0x7fff)
	}

#if defined(_MSC_VER) || defined(__MINGW32__)
	const wchar_t *sock_path;
	if (data->sockPath != NULL) {
		sock_path = to_utf16(CP_UTF8, data->sockPath);
	} else if (data->ASockPath != NULL) {
		sock_path = to_utf16(CP_ACP, data->ASockPath);
	} else {
		sock_path = _wcsdup(data->WSockPath);
	}
#else
	const char *sock_path = _strdup(data->sockPath);
#endif
	self->sockPath_ = sock_path;

	return sock;
	
cleanup_error:
	if (sock != INVALID_SOCKET) {
		closesocket(sock);
	}
	free(self);
	*fd_ud = NULL;
	return INVALID_SOCKET;
}

int ud_bind(SOCKET sock, ud_data_t *fd_ud)
{
	uint16_t port_num;
	int retry = 10;

	int r;
	int i = 0;
	do {
		port_num = get_portno();		

		SOCKADDR_IN addr;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port_num);
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); /* localhost, 127.0.0.1*/ 

		debug("port %d\n", port_num);

		r = bind(sock, (LPSOCKADDR)&addr, sizeof(addr) );
		if (r != SOCKET_ERROR) {
			break;
		}
	} while (i++ < retry);
	if (i == retry) {
		debug("bind() %d\n", WSAGetLastError());
		return SOCKET_ERROR;
	}
	fd_ud->portNo_ = port_num;

	// TODO: 接続してみて使っているかチェックする
	const wchar_t *sock_path = fd_ud->sockPath_;
	deleteSockPath(sock_path);

	FILE *fp =
#if defined(_MSC_VER) || defined(__MINGW32__)
		_wfopen
#else
		fopen
#endif
		(sock_path, L"w");
	if (fp == NULL) {
		debug("'%ls' create err\n", sock_path);
		return SOCKET_ERROR;
	}

	char buf[128];
	r = sprintf(buf, "!<socket >%d s %08x-%08x-%08x-%08x",
				port_num,
				fd_ud->guid_[0], fd_ud->guid_[1],
				fd_ud->guid_[2], fd_ud->guid_[3]);
	debug("socket '%s'\n", buf);
	fwrite(buf, r+1, 1, fp);	// +1 = write '\0'
	fclose(fp);
	r = SetFileAttributesW(sock_path, FILE_ATTRIBUTE_SYSTEM);
	if (r == 0) {
		debug("err\n");
		return SOCKET_ERROR;
	}

	return 0;
}

SOCKET ud_accept(SOCKET sock, struct sockaddr *client, int *len, ud_data_t *fd_ud)
{
	SOCKET client_sock = accept(sock, (struct sockaddr *)client, len);

	// receive guid
	char recv_buf[16];
	int nBytesRecv;
	nBytesRecv = recv(client_sock, recv_buf, sizeof(recv_buf), 0);
	if(nBytesRecv == SOCKET_ERROR) {
		debug("recv() %d\n", WSAGetLastError());
		closesocket(client_sock);
		return INVALID_SOCKET;
	}

	debug("guid receive %dbytes\n", nBytesRecv);
	for(int i=0;i<nBytesRecv;i++) {
		debug("%02x ", (unsigned char)recv_buf[i]);
	}
	debug("\n");
	if (nBytesRecv != 16) {
		closesocket(client_sock);
		return INVALID_SOCKET;
	}
	uint32_t *p = (uint32_t *)&recv_buf[0];
	if (p[0] != fd_ud->guid_[0] ||
		p[1] != fd_ud->guid_[1] ||
		p[2] != fd_ud->guid_[2] ||
		p[3] != fd_ud->guid_[3] )
	{
		debug("guid not match\n");
		closesocket(client_sock);
		return INVALID_SOCKET;
	}

	// send back guid
	int nBytesSend;
	nBytesSend = send(client_sock, recv_buf, 16, 0);
	if (nBytesSend != 16) {
		debug("send back guid err?\n");
		closesocket(client_sock);
		return INVALID_SOCKET;
	}

	// receive pid uid gid (3 x 4(uint16_t) bytes)
	nBytesRecv = recv(client_sock, recv_buf, sizeof(recv_buf), 0);
	if (nBytesRecv != 12) {
		debug("recv() %d\n", WSAGetLastError());
		closesocket(client_sock);
		return INVALID_SOCKET;
	}
	debug("pid uid gid %dbytes\n", nBytesRecv);
	for(int i=0;i<nBytesRecv;i++) {
		debug("%02x ", (unsigned char)recv_buf[i]);
	}
	debug("\n");

	// send back pid uid gid
	nBytesSend = send(client_sock, recv_buf, 12, 0);
	if (nBytesSend != 12) {
		debug("send err?\n");
		closesocket(client_sock);
		return INVALID_SOCKET;
	}

	debug("unix domain socket処理完了\n");
	return client_sock;
}

void ud_closesocket(SOCKET sock, ud_data_t *fd_ud)
{
	deleteSockPath(fd_ud->sockPath_);
	free((void *)(fd_ud->sockPath_));
	fd_ud->sockPath_ = NULL;
	shutdown(sock, 2);
    closesocket(sock);
	free(fd_ud);
}

const char *ud_version(void)
{
	return "1.0beta2";
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature-unix
// tab-width: 4
// End:
