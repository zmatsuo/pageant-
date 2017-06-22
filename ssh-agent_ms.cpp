
#undef UNICODE
#include <windows.h>
#include <tchar.h>
#include <stdint.h>
#include <string>
#include <thread>

extern "C" {
#include "pageant.h"
}
#include "puttymem.h"

#include "ssh-agent_ms.h"

static HANDLE hPipe;
static uint8_t szData[8*1024];
static std::thread *pipe_th;

static void init(const wchar_t *sock_path)
{
	std::wstring path = L"\\\\.\\pipe\\";
	path += sock_path;
	
    hPipe = CreateNamedPipeW(path.c_str(),
							 PIPE_ACCESS_DUPLEX,
							 PIPE_TYPE_BYTE, 1,
							 sizeof(szData), sizeof(szData),
							 1000, NULL);
    if (hPipe == INVALID_HANDLE_VALUE) {
	printf("パイプの作成に失敗しました。");
    }
}

static inline int msglen(const void *p)
{
    // GET_32BIT(p);
	return 4 + ntohl(*(const uint32_t *)p);
}

int pipe_main()
{
	while(1) {
		// 待つ
		ConnectNamedPipe(hPipe, NULL);

		while(1) {

			DWORD read_size;
			DWORD read_data_size = 0;
			while(1)
			{
				ReadFile(hPipe,
						 &szData[read_data_size], sizeof(szData) - read_data_size,
						 &read_size, NULL);
				if (read_size == 0) {
					goto cleanup;
				}
				read_data_size += read_size;
				if (read_data_size >= 5) {
					DWORD len = msglen(&szData[0]);
					if (read_data_size >= len) {
						break;
					}
				}
			}

			int reply_len;
			uint8_t *reply = (uint8_t *)pageant_handle_msg_2(szData, &reply_len);
			smemclr(szData, read_data_size);

			DWORD dwResult;
			BOOL r = WriteFile(hPipe, reply, 4, &dwResult, NULL);
			r = FlushFileBuffers(hPipe);
			r = WriteFile(hPipe, &reply[4], reply_len-4, &dwResult, NULL);
			r = FlushFileBuffers(hPipe);

			smemclr(reply, reply_len);
			sfree(reply);
		}
	cleanup:
		BOOL r = DisconnectNamedPipe(hPipe);
		(void)r;
	}
	
    return 0;
}


bool pipe_th_start(const wchar_t *sock_path)
{
	if (pipe_th != nullptr)
		return false;
    init(sock_path);
    pipe_th = new std::thread(pipe_main);
	return true;
}

void pipe_th_stop()
{
	if (pipe_th != nullptr) {
		CloseHandle(hPipe);		// かなり強引
		hPipe = NULL;
		pipe_th->join();
		delete pipe_th;
		pipe_th = nullptr;
	}
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
