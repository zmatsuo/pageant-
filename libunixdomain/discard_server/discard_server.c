
//#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <stdio.h>
#include <stdint.h>
#include <Ws2tcpip.h>
#include "ud_socket.h"

#if defined(_MSC_VER)
#define SOCK_PATH	"C:/cygwin64/tmp/test.sock"
#else
#define SOCK_PATH	"/tmp/test.sock"
#endif

#pragma comment(lib, "ws2_32.lib")

int server()
{
	const char *sock_path = SOCK_PATH;
	ud_open_data_t ud_open_data = {
		.size = sizeof(ud_open_data),
		.flag = UD_FLAG_UNIX_DOMAIN_CYGWIN,
		.sockPath = sock_path
	};
	ud_data_t *fd_ud;
	SOCKET fd = ud_socket(&ud_open_data, &fd_ud);

	int r = ud_bind(fd, fd_ud);
	if (r == SOCKET_ERROR) {
		printf("ud_bind() error\n");
		return 0;
	}

	int backlog = 3;
	r = listen(fd, backlog);
	if (r == SOCKET_ERROR) {
		printf("error!\n");
		return 0;
	}

	printf("クライアントの接続待ち・・・\n");
	printf("sock %s\n", sock_path);

    SOCKADDR_IN clientAddr;
    int nClientAddrLen = sizeof(clientAddr);
	SOCKET clientSoc = ud_accept(fd, (LPSOCKADDR)&clientAddr, &nClientAddrLen, fd_ud);
	if(clientSoc == INVALID_SOCKET){
		printf("accept error %d\n", WSAGetLastError());
		return -1;
	}

    IN_ADDR clientIn;
	memcpy(&clientIn, &clientAddr.sin_addr.s_addr, 4);

	char ip[32];
	inet_ntop(AF_INET, (struct in_addr *)&clientAddr.sin_addr, ip, sizeof(ip));
	printf("client %s : %d\n", ip, ntohs(clientAddr.sin_port));

	for(;;) {
		// 受信
		char recv_buf[65536];
		int nBytesRecv = recv(clientSoc, recv_buf, sizeof(recv_buf), 0);
		if(nBytesRecv == SOCKET_ERROR){
			printf("クライアントからの受信失敗です\n");
			printf("エラー%dが発生しました\n", WSAGetLastError());
			break;
		}
		if (nBytesRecv == 0) {
			printf("受信サイズ=0");
			break;
		}

		printf("受信 %dbytes\n", nBytesRecv);
		for(int i=0;i<nBytesRecv;i++){
			printf("%02x ", (uint8_t)recv_buf[i]);
		}
		printf("\n");
	}

#if 0
    // クライアントにメッセージを送信
    char message[100];
    sprintf(message, "Hello, Client!");
    if( send(clientSoc, message, sizeof(message), 0) == SOCKET_ERROR){
        printf("サーバへの送信失敗です\n");
        printf("エラー%dが発生しました\n", WSAGetLastError());
        shutdown(soc, 2);                        // 送受信を無効にする
        shutdown(clientSoc, 2);
        closesocket(soc);                        // ソケットの破棄
        closesocket(clientSoc);
        return -1;
    }
#endif

    shutdown(clientSoc, 2);
    closesocket(clientSoc);

//	ud_close(fd_ud);
	ud_closesocket(fd, fd_ud);

	return 0;
}


int main()
{
    WSADATA wsaData;
    int r = WSAStartup(MAKEWORD(2, 0), &wsaData);
    if(r != 0){
        printf("初期化失敗\n");
        return 0;
    }

	server();

	WSACleanup();

	return 0;
}

// Local Variables:
// mode: c++-mode
// coding: utf-8-with-signature
// tab-width: 4
// End:
