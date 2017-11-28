/*
 * Pageant: the PuTTY Authentication Agent.
 */
#undef UNICODE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <algorithm>
#include <windows.h>
#include <aclapi.h>
#include <thread>

#include "pageant+.h"
#include "misc.h"
#include "pageant_msg.h"		// for AGENT_COPYDATA_ID
#include "pageant.h"
#include "winutils.h"
#include "gui_stuff.h"

#include "winpgnt.h"

#define APPNAME			APP_NAME	// in pageant+.h
#define WINDOW_CLASS_NAME	"Pageant"

#define DEBUG_IPC

static HWND ghwnd;		// todo:hWnd
static std::thread *winpgnt_th;

//static int already_running;
//static int flags = FLAG_SYNCAGENT;

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
    {
		COPYDATASTRUCT *cds;
		char *mapname;
		void *p;
		HANDLE filemap;
		HANDLE proc;
		PSID mapowner, procowner;
		PSECURITY_DESCRIPTOR psd1 = NULL, psd2 = NULL;
		int ret = 0;

		cds = (COPYDATASTRUCT *) lParam;
		if (cds->dwData != AGENT_COPYDATA_ID)
			return 0;	       /* not our message, mate */
		mapname = (char *) cds->lpData;
		if (mapname[cds->cbData - 1] != '\0')
			return 0;	       /* failure to be ASCIZ! */
#ifdef DEBUG_IPC
		debug("mapname is :%s:\n", mapname);
#endif
		filemap = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, mapname);
#ifdef DEBUG_IPC
		debug("filemap is %p\n", filemap);
#endif
		if (filemap != NULL && filemap != INVALID_HANDLE_VALUE) {
			int rc;
			if ((proc = OpenProcess(MAXIMUM_ALLOWED, FALSE,
									GetCurrentProcessId())) ==
				NULL) {
#ifdef DEBUG_IPC
				debug("couldn't get handle for process\n");
#endif
				return 0;
			}
			if (GetSecurityInfo(proc, SE_KERNEL_OBJECT,
								OWNER_SECURITY_INFORMATION,
								&procowner, NULL, NULL, NULL,
								&psd2) != ERROR_SUCCESS) {
#ifdef DEBUG_IPC
				debug("couldn't get owner info for process\n");
#endif
				CloseHandle(proc);
				return 0;      /* unable to get security info */
			}
			CloseHandle(proc);
			rc = GetSecurityInfo(filemap, SE_KERNEL_OBJECT,
								 OWNER_SECURITY_INFORMATION,
								 &mapowner, NULL, NULL, NULL,
								 &psd1);
			if (rc != ERROR_SUCCESS) {
#ifdef DEBUG_IPC
				debug(
					"couldn't get owner info for filemap: %d\n",
					rc);
#endif
				return 0;
			}
#ifdef DEBUG_IPC
			debug("got security stuff\n");
#endif
			if (!EqualSid(mapowner, procowner))
				return 0;      /* security ID mismatch! */
#ifdef DEBUG_IPC
			debug("security stuff matched\n");
#endif
			LocalFree(psd1);
			LocalFree(psd2);
			p = MapViewOfFile(filemap, FILE_MAP_WRITE, 0, 0, 0);
#ifdef DEBUG_IPC
			debug("p is %p\n", p);
#endif
			{
				int reply_len;
				uint8_t *reply = (uint8_t *)pageant_handle_msg_2(p, &reply_len);
				memcpy(p, reply, reply_len);
				smemclr(reply, reply_len);
				sfree(reply);
			}
			ret = 1;
			UnmapViewOfFile(p);
		}
		CloseHandle(filemap);
		return ret;
    }
    default:
		return DefWindowProc(hwnd, message, wParam, lParam);
		break;
    }

    return 0;
}

static bool winpgnt_init(void)
{
    HINSTANCE hInstance = (HINSTANCE)GetModuleHandle(NULL);
    WNDCLASS wndclass = {0};

    wndclass.lpfnWndProc = WndProc;
    wndclass.hInstance = hInstance;
    wndclass.lpszClassName = WINDOW_CLASS_NAME;

    RegisterClass(&wndclass);

    ghwnd = CreateWindow(
		WINDOW_CLASS_NAME,
		WINDOW_CLASS_NAME,	// title must 'Pagent'
		WS_OVERLAPPEDWINDOW | WS_VSCROLL,
		CW_USEDEFAULT, CW_USEDEFAULT,
		100, 100, NULL, NULL, hInstance, NULL);

    ShowWindow(ghwnd, SW_HIDE);

    debug("pageant window handle %p\n", ghwnd);

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

    debug("pageant window WM_CLOSE\n");
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

