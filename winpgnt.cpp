/*
 * Pageant: the PuTTY Authentication Agent.
 */
#undef UNICODE
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <assert.h>
#include <tchar.h>
#include <malloc.h>
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <aclapi.h>
#include <thread>

#include "pageant+.h"
#include "misc.h"
#include "ssh.h"
#include "tree234.h"
#include "pageant_msg.h"
#include "pageant.h"

#include "winpgnt.h"
#include "winutils.h"

#include "db.h"

#include "gui_stuff.h"
#include "winpgnt.h"

#define APPNAME			APP_NAME	// in pageant+.h
#define WINDOW_CLASS_NAME	"Pageant"

#define DEBUG_IPC

static HWND hwnd;
static std::thread *winpgnt_th;

static int already_running;

/*
 * Print a modal (Really Bad) message box and perform a fatal exit.
 */
void modalfatalbox(const char *fmt, ...)
{
    va_list ap;
    char *buf;

    va_start(ap, fmt);
    buf = dupvprintf(fmt, ap);
    va_end(ap);
    MessageBox(hwnd, buf, "Pageant Fatal Error",
	       MB_SYSTEMMODAL | MB_ICONERROR | MB_OK);
    sfree(buf);
    exit(1);
}

#if 0
/*
 * We need this to link with the RSA code, because rsaencrypt()
 * pads its data with random bytes. Since we only use rsadecrypt()
 * and the signing functions, which are deterministic, this should
 * never be called.
 *
 * If it _is_ called, there is a _serious_ problem, because it
 * won't generate true random numbers. So we must scream, panic,
 * and exit immediately if that should happen.
 */
int random_byte(void)
{
    MessageBox(hwnd, "Internal Error", APPNAME, MB_OK | MB_ICONERROR);
    debug("internal error");
    exit(0);
    /* this line can't be reached but it placates MSVC's warnings :-) */
    return 0;
}
#endif

/*
 * Warn about the obsolescent key file format.
 */
void old_keyfile_warning(void)
{
    static const char mbtitle[] = "PuTTY Key File Warning";
    static const char message[] =
	"You are loading an SSH-2 private key which has an\n"
	"old version of the file format. This means your key\n"
	"file is not fully tamperproof. Future versions of\n"
	"PuTTY may stop supporting this private key format,\n"
	"so we recommend you convert your key to the new\n"
	"format.\n"
	"\n"
	"You can perform this conversion by loading the key\n"
	"into PuTTYgen and then saving it again.";

    MessageBox(hwnd, message, mbtitle, MB_OK);
}

//////////////////////////////////////////////////////////////////////////////

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
	    if ((rc = GetSecurityInfo(filemap, SE_KERNEL_OBJECT,
				      OWNER_SECURITY_INFORMATION,
				      &mapowner, NULL, NULL, NULL,
				      &psd1) != ERROR_SUCCESS)) {
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

// static int flags = FLAG_SYNCAGENT;

static void winpgnt_main(void)
{
    HINSTANCE hInstance = (HINSTANCE)GetModuleHandle(NULL);
    WNDCLASS wndclass = {0};

    wndclass.lpfnWndProc = WndProc;
    wndclass.hInstance = hInstance;
    wndclass.lpszClassName = WINDOW_CLASS_NAME;

    RegisterClass(&wndclass);

    hwnd = CreateWindow(
		WINDOW_CLASS_NAME,
		WINDOW_CLASS_NAME,	// title must 'Pagent'
			WS_OVERLAPPEDWINDOW | WS_VSCROLL,
			CW_USEDEFAULT, CW_USEDEFAULT,
			100, 100, NULL, NULL, hInstance, NULL);

    ShowWindow(hwnd, SW_HIDE);

    debug("pageant window handle %p\n", hwnd);
    
    {
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0) == 1) {
	    TranslateMessage(&msg);
	    DispatchMessage(&msg);
	}
    }

    debug("pageant window WM_CLOSE\n");
}

//////////////////////////////////////////////////////////////////////////////

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
	PostMessage(hwnd, WM_CLOSE, 0, 0);
	winpgnt_th->join();
	delete winpgnt_th;
	winpgnt_th = nullptr;
	hwnd = NULL;
    }
}

// Local Variables:
// coding: utf-8-with-signature
// End:

