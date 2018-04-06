
#undef UNICODE
#include <windows.h>
#include <tchar.h>
#include <stdint.h>
#include <string>
#include <thread>

#include "pageant.h"
#include "puttymem.h"
#include "debug.h"

#include "ssh-agent_ms.h"

static HANDLE hPipe;
static uint8_t szData[8*1024];
static std::thread *pipe_th;
static HANDLE hEventControl;
static HANDLE hEventConnect;

static bool init(const wchar_t *sock_path)
{
	std::wstring path = L"\\\\.\\pipe\\";
	path += sock_path;
	
    hPipe = CreateNamedPipeW(path.c_str(),
							 PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
							 PIPE_TYPE_BYTE, 1,
							 sizeof(szData), sizeof(szData),
							 1000, NULL);
    if (hPipe == INVALID_HANDLE_VALUE) {
		dbgprintf("パイプの作成に失敗しました。");
		return false;
    }
	return true;
}

static inline int msglen(const void *p)
{
    // GET_32BIT(p);
	return 4 + ntohl(*(const uint32_t *)p);
}

static int pipe_main()
{
	while(1) {
		// 待つ
		OVERLAPPED ov;
		ZeroMemory(&ov, sizeof(OVERLAPPED));
		ov.hEvent = hEventConnect;
		ConnectNamedPipe(hPipe, &ov);

		HANDLE hEventArray[2];
		hEventArray[0] = hEventControl;
		hEventArray[1] = hEventConnect;

		DWORD  dwEventNo = WaitForMultipleObjects(2, hEventArray, FALSE, INFINITE);
		dwEventNo -= WAIT_OBJECT_0;
		if (dwEventNo == 0) {
			// event control -> exit
			return 0;
		} else if (dwEventNo == 1) {
			// event connect -> continue
			;
		}		
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
    if (init(sock_path) == false) {
		return false;
	}
	hEventControl = CreateEvent(NULL, TRUE, FALSE, NULL);
	hEventConnect = CreateEvent(NULL, FALSE, FALSE, NULL);
    pipe_th = new std::thread(pipe_main);
	return true;
}

void pipe_th_stop()
{
	if (pipe_th == nullptr) {
		return;
	}
	SetEvent(hEventControl);
	pipe_th->join();
	delete pipe_th;
	pipe_th = nullptr;
	CloseHandle(hPipe);
	hPipe = NULL;
	CloseHandle(hEventConnect);
	CloseHandle(hEventControl);
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
