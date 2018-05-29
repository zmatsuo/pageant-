/**
 *	ssh-agent_ms.cpp
 *
 *	Copyright (c) 2018 zmatsuo
 *
 *	This software is released under the MIT License.
 *	http://opensource.org/licenses/mit-license.php
 */

#include <windows.h>
#include <stdint.h>
#include <thread>

#include "pageant.h"
#include "puttymem.h"
#define ENABLE_DEBUG_PRINT
#include "debug.h"
#include "winmisc.h"

#include "ssh-agent_ms.h"

static HANDLE hPipe;
static uint8_t szData[8*1024];
static std::thread *pipe_th;
static HANDLE hEventControl;
static HANDLE hEventConnect;
static const DWORD wait_timeout = 500;

static bool init(const wchar_t *sock_path)
{
    hPipe = CreateNamedPipeW(sock_path,
							 PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
							 PIPE_TYPE_BYTE |
							 PIPE_READMODE_BYTE |
							 PIPE_WAIT,
							 PIPE_UNLIMITED_INSTANCES,
							 sizeof(szData), sizeof(szData),
							 1000, NULL);
    if (hPipe == INVALID_HANDLE_VALUE) {
		dbgprintf("CreateNamedPipe() %S\n", _FormatMessage(GetLastError()).c_str());
		return false;
    }
	return true;
}

static inline int msglen(const void *p)
{
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
		dbgprintf("WaitForMultipleObjects() enter\n");
		DWORD dw = WaitForMultipleObjects(2, hEventArray, FALSE, INFINITE);
		dbgprintf("WaitForMultipleObjects() leave %d(%s)\n",
				  dw,
				  dw == WAIT_OBJECT_0 ? "WAIT_OBJECT_0" :
				  dw == WAIT_OBJECT_0+1 ? "WAIT_OBJECT_0+1" : "??" );
		if (dw == WAIT_OBJECT_0) {
			// event control -> exit
			return 0;
		}
		// event connect -> continue
			
		while(1) {
			
			DWORD read_size;
			DWORD read_data_size = 0;
			while(1)
			{
				ZeroMemory(&ov, sizeof(OVERLAPPED));
				ov.hEvent = hEventConnect;

				dbgprintf("read_data_size %d\n", read_data_size);
				ReadFile(
					hPipe,
					&szData[read_data_size], sizeof(szData) - read_data_size,
					&read_size, &ov);
				if(GetLastError() == ERROR_IO_PENDING) {
					hEventArray[0] = hEventControl;
					hEventArray[1] = hEventConnect;
					dbgprintf("WaitForMultipleObjects() enter\n");
					dw = WaitForMultipleObjects(2, hEventArray, FALSE, wait_timeout);
					dbgprintf("WaitForMultipleObjects() leave %d(%s)\n",
							  dw,
							  dw == WAIT_OBJECT_0 ? "WAIT_OBJECT_0" :
							  dw == WAIT_OBJECT_0+1 ? "WAIT_OBJECT_0+1" : 
							  dw == WAIT_TIMEOUT ? "WAIT_TIMEOUT" : "??");
					if (dw == WAIT_OBJECT_0) {
						// event control -> exit
						return 0;
					}
					if (dw == WAIT_TIMEOUT) {
						goto cleanup;
					}
					GetOverlappedResult(hPipe, &ov, &read_size, TRUE);
				}
				
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

			std::vector<uint8_t> reply;
			pageant_handle_msg(szData, read_data_size, reply);
			smemclr(szData, read_data_size);

			DWORD dwResult;
			BOOL r = WriteFile(hPipe, &reply[0], 4, &dwResult, NULL);
			r = FlushFileBuffers(hPipe);
			r = WriteFile(hPipe, &reply[4], reply.size()-4, &dwResult, NULL);
			r = FlushFileBuffers(hPipe);

			smemclr(&reply[0], reply.size());
		}
	cleanup:
		BOOL r = DisconnectNamedPipe(hPipe);
		(void)r;
	}
	
    return 0;
}


bool pipe_th_start(const wchar_t *sock_path)
{
	dbgprintf("pipe_th_start path=%S\n", sock_path);
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
