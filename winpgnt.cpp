/**
   winpgnt.cpp
   Pageant: the PuTTY Authentication Agent.

   This software is released under the MIT License.
   http://opensource.org/licenses/mit-license.php
*/
#include <stdint.h>
#include <windows.h>
#include <aclapi.h>

#include <thread>

#include "pageant+.h"
#include "pageant_msg.h"		// for AGENT_COPYDATA_ID
#include "pageant.h"
#include "puttymem.h"
//#define ENABLE_DEBUG_PRINT
#include "debug.h"
#include "winpgnt.h"

#define WINDOW_CLASS_NAME	L"Pageant"

static HWND ghwnd;
static std::thread *winpgnt_th;

static inline int msglen(const void *p)
{
	return ntohl(*(const uint32_t *)p);
}

static LRESULT wm_copydata(const COPYDATASTRUCT *cds)
{
	if (cds->dwData != AGENT_COPYDATA_ID)
		return 0;	       /* not our message, mate */

	const char *mapname = (char *) cds->lpData;
	if (mapname[cds->cbData - 1] != '\0')
		return 0;	       /* failure to be ASCIZ! */
	dbgprintf("mapname is :%s:\n", mapname);
	HANDLE filemap = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, mapname);
	dbgprintf("filemap is %p\n", filemap);
	if (filemap != NULL && filemap != INVALID_HANDLE_VALUE) {
		HANDLE proc;
		if ((proc = OpenProcess(MAXIMUM_ALLOWED, FALSE,
								GetCurrentProcessId())) ==
			NULL) {
			dbgprintf("couldn't get handle for process\n");
			return 0;
		}
		PSECURITY_DESCRIPTOR psd1 = NULL, psd2 = NULL;
		PSID mapowner, procowner;
		if (GetSecurityInfo(proc, SE_KERNEL_OBJECT,
							OWNER_SECURITY_INFORMATION,
							&procowner, NULL, NULL, NULL,
							&psd2) != ERROR_SUCCESS) {
			dbgprintf("couldn't get owner info for process\n");
			CloseHandle(proc);
			return 0;      /* unable to get security info */
		}
		CloseHandle(proc);
		int rc;
		rc = GetSecurityInfo(filemap, SE_KERNEL_OBJECT,
							 OWNER_SECURITY_INFORMATION,
							 &mapowner, NULL, NULL, NULL,
							 &psd1);
		if (rc != ERROR_SUCCESS) {
			dbgprintf(
				"couldn't get owner info for filemap: %d\n",
				rc);
			return 0;
		}
		dbgprintf("got security stuff\n");
		if (!EqualSid(mapowner, procowner))
			return 0;      /* security ID mismatch! */
		dbgprintf("security stuff matched\n");
		LocalFree(psd1);
		LocalFree(psd2);

		uint8_t *p = (uint8_t *)MapViewOfFile(filemap, FILE_MAP_WRITE, 0, 0, 0);
		dbgprintf("p is %p\n", p);

		size_t request_len = msglen(p) + 4;
		std::vector<uint8_t> reply;
		pageant_handle_msg(p, request_len, reply);
		smemclr(p, request_len);
		memcpy(p, &reply[0], reply.size());
		smemclr(&reply[0], reply.size());

		UnmapViewOfFile(p);
	}
	CloseHandle(filemap);
	return 1;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT message,
				WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_CLOSE:
		DestroyWindow(hwnd);
		break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_COPYDATA:
		return wm_copydata((COPYDATASTRUCT *)lParam);
    default:
		return DefWindowProc(hwnd, message, wParam, lParam);
		break;
    }

    return 0;
}

static bool winpgnt_init(void)
{
    HINSTANCE hInstance = (HINSTANCE)GetModuleHandle(NULL);
    WNDCLASSW wndclass = {0};

    wndclass.lpfnWndProc = WndProc;
    wndclass.hInstance = hInstance;
    wndclass.lpszClassName = WINDOW_CLASS_NAME;

    RegisterClassW(&wndclass);

    ghwnd = CreateWindowW(
		WINDOW_CLASS_NAME,
		WINDOW_CLASS_NAME,	// title must 'Pagent'
		WS_OVERLAPPEDWINDOW | WS_VSCROLL,
		CW_USEDEFAULT, CW_USEDEFAULT,
		100, 100, NULL, NULL, hInstance, NULL);

    ShowWindow(ghwnd, SW_HIDE);

    dbgprintf("pageant window handle %p\n", ghwnd);

	return true;
}

static void winpgnt_main(void)
{
	winpgnt_init();

	MSG msg;
	while (GetMessage(&msg, ghwnd, 0, 0) == 1) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

    dbgprintf("pageant window WM_CLOSE\n");
}


bool winpgnt_start()
{
    if (winpgnt_th != nullptr)
		return false;
    winpgnt_th = new std::thread(winpgnt_main);
    return true;
}

void winpgnt_stop()
{
    if (winpgnt_th != nullptr) {
		PostMessage(ghwnd, WM_CLOSE, 0, 0);
		winpgnt_th->join();
		delete winpgnt_th;
		winpgnt_th = nullptr;
		ghwnd = NULL;
    }
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:

