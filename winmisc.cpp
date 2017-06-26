﻿/*
 * winmisc.c: miscellaneous Windows-specific things
 */
#undef UNICODE
#include <algorithm>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <string>
#include <sstream>
#include <io.h>
#include <crtdbg.h>

#include "puttymem.h"
#include "misc.h"
#include "winmisc.h"
#include "setting.h"

#define DEBUG

void platform_get_x11_auth(char *display, int *proto,
                           unsigned char *data, int *datalen)
{
    /* We don't support this at all under Windows. */
	(void)display;
	(void)proto;
	(void)data;
	(void)datalen;
}

const char platform_x11_best_transport[] = "localhost";

char *platform_get_x_display(void) {
    /* We may as well check for DISPLAY in case it's useful. */
    return dupstr(getenv("DISPLAY"));
}

#if 0
Filename filename_from_str(const char *str)
{
    Filename ret;
    strncpy(ret.path, str, sizeof(ret.path));
    ret.path[sizeof(ret.path)-1] = '\0';
    return ret;
}
#endif

#if 0
const char *filename_to_str(const Filename *fn)
{
    return fn->path;
}
#endif

#if 0
int filename_equal(Filename f1, Filename f2)
{
    return !strcmp(f1.path, f2.path);
}

int filename_is_null(Filename fn)
{
    return !*fn.path;
}
#endif

char *get_username(void)
{
    DWORD namelen;
    char *user;

    namelen = 0;
    if (GetUserName(NULL, &namelen) == FALSE) {
	/*
	 * Apparently this doesn't work at least on Windows XP SP2.
	 * Thus assume a maximum of 256. It will fail again if it
	 * doesn't fit.
	 */
	namelen = 256;
    }

    user = snewn(namelen, char);
    GetUserName(user, &namelen);

    return user;
}

std::wstring _GetModuleFileName(HMODULE hModule)
{
	std::vector<wchar_t> buf(MAX_PATH);

	for(;;) {
		DWORD r = ::GetModuleFileNameW(hModule, &buf[0], (DWORD)buf.size());
		if (r < buf.size() - 1) {
			break;
		} else if (r == 0) {
			// ?
			break;
		} else { 
			buf.resize(buf.size()*2);
		}
    }
	return &buf[0];
}

// TODO: 削除する
std::string _GetModuleFileNameA(HMODULE hModule)
{
	size_t buf_size = MAX_PATH;
	std::vector<char> buf(buf_size);

	for(;;) {
		DWORD r = ::GetModuleFileNameA(hModule, &buf[0], (DWORD)buf_size);
		if (r < buf_size - 1) {
			break;
		} else if (r == 0) {
			// ?
			break;
		} else { 
			buf_size *= 2;
			buf.resize(buf_size);
		}
    }
	return &buf[0];
}

std::wstring _GetCurrentDirectory()
{
    std::wstring dir;
    size_t buf_size = ::GetCurrentDirectoryW(0, NULL);  // getting path length
	std::vector<wchar_t> buf(buf_size);
	::GetCurrentDirectoryW((DWORD)buf_size, &buf[0]);
	return &buf[0];
}

// TODO: 削除する
std::string _GetCurrentDirectoryA()
{
    std::string dir;
    size_t buf_size = GetCurrentDirectoryA(0, NULL);  // getting path length
	std::vector<char> buf(buf_size);
	GetCurrentDirectory((DWORD)buf_size, &buf[0]);
	return &buf[0];
}

static std::wstring _SearchPath(const wchar_t *filename)
{
	const wchar_t *ext = NULL;
	DWORD r = ::SearchPathW(NULL, filename, ext, 0, NULL, NULL);
	if (r == 0) {
		return L"";	// not found
	}
	DWORD buf_size = r;
	std::vector<wchar_t> buf(buf_size);
	::SearchPathW(NULL, filename, ext, (DWORD)buf_size, &buf[0], NULL);
	return &buf[0];
}

std::wstring _SHGetFolderPath(int nFolder)
{
	wchar_t path[MAX_PATH+1];
	HRESULT r = SHGetFolderPathW(NULL, nFolder, NULL, 0, path);
	if (r != S_OK) {
		return L"";
	} else {
		return path;
	}
}

// TODO: wstinrgにする
std::string _SHGetFolderPathA(int nFolder)
{
	char path[MAX_PATH+1];
	HRESULT r = SHGetFolderPath(NULL, nFolder, NULL, 0, path);
	if (r != S_OK) {
		return "";
	} else {
		return path;
	}
}

std::wstring _SHGetKnownFolderPath(REFKNOWNFOLDERID rfid)
{
	PWSTR pszPath = NULL;
	HRESULT r = SHGetKnownFolderPath(rfid, KF_FLAG_CREATE, NULL, &pszPath);
	if (r != S_OK) {
		return L"";
	}
	std::wstring path = pszPath;
	CoTaskMemFree(pszPath);
	return path;
}

/**
 * get full path from filename
 * - when search_path=true, search from $PATH
 *
 *	@return		std::string path
 *	@param		filename
 *	@param		search from $path		true/false
 */
std::wstring get_full_path(const wchar_t *filename, bool search_path)
{
	std::wstring f;

    f = _GetModuleFileName(NULL);
	size_t pos = f.rfind(L'\\');
	if (pos != std::string::npos) {
		f = f.substr(0, pos+1);
	}
	f = f + filename;
	if (_waccess(f.c_str(), 0) == 0) {
		return f;
    }

    f = _GetCurrentDirectory();
	f = f + L"\\" + filename;
	if (_waccess(f.c_str(), 0) == 0) {
		return f;
    }

	if (search_path) {
		f = _SearchPath(filename);
		if (!f.empty()) {
			return f;
		}
	}
	
	return L"";
}

std::wstring _ExpandEnvironmentStrings(const wchar_t *str)
{
    DWORD len = ::ExpandEnvironmentStringsW(str, NULL, 0);
	std::vector<wchar_t> s(len+1);
    ExpandEnvironmentStringsW(str, &s[0], len+1);
    return &s[0];
}

std::string _ExpandEnvironmentStrings(const char *str)
{
    DWORD len = ::ExpandEnvironmentStringsA(str, NULL, 0);
	std::vector<char> s(len+1);
    ExpandEnvironmentStringsA(str, &s[0], len+1);
    return &s[0];
}

/**
 *	@retvalue	true	read ok
 *	@retvalue	false	entry does not exist
 */
bool reg_read(
	HKEY hKey,
	const wchar_t *subkey, const wchar_t *valuename, DWORD &dwType,
	std::vector<uint8_t> &data)
{
	HKEY hRegKey;
	LSTATUS r;
	r = RegOpenKeyExW(hKey, subkey,  0, KEY_ALL_ACCESS, &hRegKey);
	if (r != ERROR_SUCCESS) {
		data.clear();
		return false;
	}
	bool ret_value = true;
	DWORD size;
    r = RegQueryValueExW(hRegKey, valuename, 0, &dwType,
						 NULL, &size);
	if (r != ERROR_SUCCESS) {
		// no entry
		data.clear();
		ret_value = false;
	}
	if (ret_value == true) {
		data.resize(size);
		if (size > 0) {
			r = RegQueryValueExW(hRegKey, valuename, 0, &dwType,
								 &data[0], &size);
			if (r != ERROR_SUCCESS) {
				data.clear();
				ret_value = false;
			}
		}
	}
	RegCloseKey(hRegKey);
	return ret_value;
}

bool reg_read_cur_user(const wchar_t *subkey, const wchar_t *valuename,
					   std::wstring &str)
{
	std::vector<uint8_t> data;
	DWORD dwType;
	bool result = reg_read(
		HKEY_CURRENT_USER, subkey, valuename, dwType, data);
	if (result == false || dwType != REG_SZ) {
		str.clear();
		return false;
	}
	str = std::wstring((wchar_t *)&data[0]);
	return true;
}

bool reg_read_cur_user(const wchar_t *subkey, const wchar_t *valuename,
					   DWORD &dword)
{
	std::vector<uint8_t> data;
	DWORD dwType;
	bool result = reg_read(
		HKEY_CURRENT_USER, subkey, valuename, dwType, data);
	if (result == false || dwType != REG_DWORD) {
		dword = 0;
		return false;
	}
	dword = *(DWORD *)&data[0];
	return true;
}

bool reg_read_cur_user(const wchar_t *subkey, const wchar_t *valuename,
					   int &_int)
{
	DWORD dword;
	bool r = reg_read_cur_user(subkey, valuename, dword);
	if (r == false) {
		_int = 0;
		return false;
	}
	_int = (int)dword;
	return true;
}

bool reg_write(HKEY hKey, const wchar_t *subkey, const wchar_t *valuename,
			   DWORD dwType, const void *data_ptr, size_t data_size)
{
	HKEY hRegKey;
	LONG r = RegOpenKeyExW(hKey, subkey,  0, KEY_ALL_ACCESS, &hRegKey);
	if (r != ERROR_SUCCESS) {
		return false;
	}
	bool result = true;
	r = RegSetValueExW(hRegKey, valuename, 0, dwType,
                       (LPBYTE)data_ptr, (DWORD)data_size);
	if (r != ERROR_SUCCESS) {
		result = false;
	}
	RegCloseKey(hRegKey);
	return result;
}

bool reg_write_cur_user(const wchar_t *subkey, const wchar_t *valuename,
						const std::string &str)
{
	return reg_write(HKEY_CURRENT_USER, subkey, valuename,
					 REG_SZ, (void *)str.c_str(), (str.size()+1));
}

bool reg_write_cur_user(const wchar_t *subkey, const wchar_t *valuename,
						const std::wstring &str)
{
	return reg_write(HKEY_CURRENT_USER, subkey, valuename,
					 REG_SZ, (void *)str.c_str(), (str.size()+1) * sizeof(wchar_t));
}

bool reg_delete(HKEY hKey, const wchar_t *subkey, const wchar_t *valuename)
{
	HKEY hRegKey;
	LSTATUS r;
	r = RegOpenKeyExW(hKey, subkey,  0, KEY_ALL_ACCESS, &hRegKey);
	if (r != ERROR_SUCCESS) {
		return false;
	}
	bool ret_value = true;
    r = RegDeleteValueW(hRegKey, valuename);
	if (r != ERROR_SUCCESS) {
		ret_value = false;
	}
	RegCloseKey(hRegKey);
	return ret_value;
}

bool reg_delete_cur_user(const wchar_t *subkey, const wchar_t *valuename)
{
	return reg_delete(HKEY_CURRENT_USER, subkey, valuename);
}

void exec(const wchar_t *file, const wchar_t *param)
{
	SHELLEXECUTEINFOW si = { sizeof(SHELLEXECUTEINFOW) };
	si.lpVerb = L"open";
	si.lpFile = file;
	si.lpParameters = param;
	
	si.nShow = SW_SHOW;
	BOOL r = ShellExecuteExW(&si);
	if(r == FALSE) {
		MessageBoxA(NULL, "Unable to execute", "Error",
				   MB_OK | MB_ICONERROR);
	}
}

void exec_regedit(const wchar_t *open_path)
{
	// 開くレジストリの場所をセットしておく
	reg_write_cur_user(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Applets\\Regedit",
					   L"LastKey", open_path);

	// 実行
	std::wstring path = _SHGetKnownFolderPath(FOLDERID_Windows);
	path += L"\\regedit.exe";
	exec(path.c_str());
}


#ifdef DEBUG
static int debug_console_enabled;	// 0/1/2=未決/disable/enable
static bool debug_console_opend;
static HWND debug_console_hwnd;
static bool debug_console_show_flag = true;
//#define PROG L"ssh_agent"

std::string sprintf(const char *fmt, ...)
{
	char *dupstr;
	va_list ap;
	va_start(ap, fmt);
	dupstr = dupvprintf(fmt, ap);
	va_end(ap);
	std::string ret = dupstr;
	sfree(dupstr);
	return ret;
}

#if 0
static int allocconsole2()
{
	STARTUPINFO si = { 0 };
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput =  GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = { 0 };

	char test[] = "\0";
	CreateProcessA(0, test, 0, 0, FALSE, 0, 0, 0, &si, &pi);

	return 1;
}
#endif

#if 0
static int allocconsole3()
{
	STARTUPINFO startupInfo;
	PROCESS_INFORMATION processInfo;
	::ZeroMemory(&startupInfo,sizeof(startupInfo));
	startupInfo.cb = sizeof(startupInfo);
	startupInfo.dwFlags = STARTF_USESHOWWINDOW;
	startupInfo.wShowWindow = SW_HIDE;
//		startupInfo.wShowWindow = SW_SHOW;
	char test[] = "cmd.exe";
	if (!CreateProcessA( NULL, test, NULL, NULL, FALSE, 
						CREATE_NEW_CONSOLE, NULL, NULL, &startupInfo, &processInfo))
	{
		int error = 0;;
	}
	DWORD dwResult = ::WaitForInputIdle(processInfo.hProcess, 10000);
	if (dwResult == -1 || dwResult == WAIT_TIMEOUT) {
		DWORD error = GetLastError();
		int e = 0;;
	}
	if (AttachConsole(processInfo.dwProcessId) == 0)
	{
		DWORD error = GetLastError();
		int e = 0;;
	}
	return 1;
}
#endif

void debug_console_open()
{
	BOOL r = AllocConsole();
	//BOOL r = allocconsole2();
	//BOOL r = allocconsole3();
	if (r == FALSE) {
		return;
	}
	debug_console_hwnd = GetConsoleWindow();
	if (!debug_console_show_flag) {
		ShowWindow(debug_console_hwnd, SW_HIDE);
	}

	FILE *fp = freopen("con", "w", stdout);
	if (fp == NULL)
		for (;;);
	fp = freopen("con", "w", stderr);
	if (fp == NULL)
		for (;;);

	debug_console_opend = true;
}

void debug_console_init()
{
	if (debug_console_enabled == 1) {
		return;
	}

	if (debug_console_enabled == 0) {
		const char *key = "debug/console_enable";
		if (setting_isempty(key)) {
			debug_console_enabled = 1;
			return;
		} else {
			debug_console_enabled = setting_get_bool(key) == false ? 1 : 2;
			if (debug_console_enabled == 1) {
				return;
			}
		}
	}
	
    if (!debug_console_opend) {
		debug_console_open();
	}
}

void debug_console_puts(const char *buf)
{
    if (!debug_console_opend) {
		debug_console_init();
    } else {
		printf("%s", buf);
	}
}

void debug_console_show(int show)
{
    debug_console_show_flag = show ? true : false;
    if (!debug_console_opend) {
		debug_console_open();
	}
    if (debug_console_opend) {
		ShowWindow(debug_console_hwnd, show ? SW_SHOW : SW_HIDE);
    }
}

#endif

#ifdef DEBUG
void dputs(const char *buf)
{
    // chop
    char *s = dupstr(buf);
    const size_t len = strlen(s) - 1;
//    if (s[len] == '\n') {
//		s[len] = '\0';
//    }

    char *s2 = dupprintf("%s", s);

    debug_console_puts(s);

    OutputDebugStringA(s2);

    sfree(s);
    sfree(s2);
}
#endif

#ifdef MINEFIELD
/*
 * Minefield - a Windows equivalent for Electric Fence
 */

#define PAGESIZE 4096

/*
 * Design:
 * 
 * We start by reserving as much virtual address space as Windows
 * will sensibly (or not sensibly) let us have. We flag it all as
 * invalid memory.
 * 
 * Any allocation attempt is satisfied by committing one or more
 * pages, with an uncommitted page on either side. The returned
 * memory region is jammed up against the _end_ of the pages.
 * 
 * Freeing anything causes instantaneous decommitment of the pages
 * involved, so stale pointers are caught as soon as possible.
 */

static int minefield_initialised = 0;
static void *minefield_region = NULL;
static long minefield_size = 0;
static long minefield_npages = 0;
static long minefield_curpos = 0;
static unsigned short *minefield_admin = NULL;
static void *minefield_pages = NULL;

static void minefield_admin_hide(int hide)
{
    int access = hide ? PAGE_NOACCESS : PAGE_READWRITE;
    VirtualProtect(minefield_admin, minefield_npages * 2, access, NULL);
}

static void minefield_init(void)
{
    int size;
    int admin_size;
    int i;

    for (size = 0x40000000; size > 0; size = ((size >> 3) * 7) & ~0xFFF) {
	minefield_region = VirtualAlloc(NULL, size,
					MEM_RESERVE, PAGE_NOACCESS);
	if (minefield_region)
	    break;
    }
    minefield_size = size;

    /*
     * Firstly, allocate a section of that to be the admin block.
     * We'll need a two-byte field for each page.
     */
    minefield_admin = minefield_region;
    minefield_npages = minefield_size / PAGESIZE;
    admin_size = (minefield_npages * 2 + PAGESIZE - 1) & ~(PAGESIZE - 1);
    minefield_npages = (minefield_size - admin_size) / PAGESIZE;
    minefield_pages = (char *) minefield_region + admin_size;

    /*
     * Commit the admin region.
     */
    VirtualAlloc(minefield_admin, minefield_npages * 2,
		 MEM_COMMIT, PAGE_READWRITE);

    /*
     * Mark all pages as unused (0xFFFF).
     */
    for (i = 0; i < minefield_npages; i++)
	minefield_admin[i] = 0xFFFF;

    /*
     * Hide the admin region.
     */
    minefield_admin_hide(1);

    minefield_initialised = 1;
}

static void minefield_bomb(void)
{
    div(1, *(int *) minefield_pages);
}

static void *minefield_alloc(int size)
{
    int npages;
    int pos, lim, region_end, region_start;
    int start;
    int i;

    npages = (size + PAGESIZE - 1) / PAGESIZE;

    minefield_admin_hide(0);

    /*
     * Search from current position until we find a contiguous
     * bunch of npages+2 unused pages.
     */
    pos = minefield_curpos;
    lim = minefield_npages;
    while (1) {
	/* Skip over used pages. */
	while (pos < lim && minefield_admin[pos] != 0xFFFF)
	    pos++;
	/* Count unused pages. */
	start = pos;
	while (pos < lim && pos - start < npages + 2 &&
	       minefield_admin[pos] == 0xFFFF)
	    pos++;
	if (pos - start == npages + 2)
	    break;
	/* If we've reached the limit, reset the limit or stop. */
	if (pos >= lim) {
	    if (lim == minefield_npages) {
		/* go round and start again at zero */
		lim = minefield_curpos;
		pos = 0;
	    } else {
		minefield_admin_hide(1);
		return NULL;
	    }
	}
    }

    minefield_curpos = pos - 1;

    /*
     * We have npages+2 unused pages starting at start. We leave
     * the first and last of these alone and use the rest.
     */
    region_end = (start + npages + 1) * PAGESIZE;
    region_start = region_end - size;
    /* FIXME: could align here if we wanted */

    /*
     * Update the admin region.
     */
    for (i = start + 2; i < start + npages + 1; i++)
	minefield_admin[i] = 0xFFFE;   /* used but no region starts here */
    minefield_admin[start + 1] = region_start % PAGESIZE;

    minefield_admin_hide(1);

    VirtualAlloc((char *) minefield_pages + region_start, size,
		 MEM_COMMIT, PAGE_READWRITE);
    return (char *) minefield_pages + region_start;
}

static void minefield_free(void *ptr)
{
    int region_start, i, j;

    minefield_admin_hide(0);

    region_start = (char *) ptr - (char *) minefield_pages;
    i = region_start / PAGESIZE;
    if (i < 0 || i >= minefield_npages ||
	minefield_admin[i] != region_start % PAGESIZE)
	minefield_bomb();
    for (j = i; j < minefield_npages && minefield_admin[j] != 0xFFFF; j++) {
	minefield_admin[j] = 0xFFFF;
    }

    VirtualFree(ptr, j * PAGESIZE - region_start, MEM_DECOMMIT);

    minefield_admin_hide(1);
}

static int minefield_get_size(void *ptr)
{
    int region_start, i, j;

    minefield_admin_hide(0);

    region_start = (char *) ptr - (char *) minefield_pages;
    i = region_start / PAGESIZE;
    if (i < 0 || i >= minefield_npages ||
	minefield_admin[i] != region_start % PAGESIZE)
	minefield_bomb();
    for (j = i; j < minefield_npages && minefield_admin[j] != 0xFFFF; j++);

    minefield_admin_hide(1);

    return j * PAGESIZE - region_start;
}

void *minefield_c_malloc(size_t size)
{
    if (!minefield_initialised)
	minefield_init();
    return minefield_alloc(size);
}

void minefield_c_free(void *p)
{
    if (!minefield_initialised)
	minefield_init();
    minefield_free(p);
}

/*
 * realloc _always_ moves the chunk, for rapid detection of code
 * that assumes it won't.
 */
void *minefield_c_realloc(void *p, size_t size)
{
    size_t oldsize;
    void *q;
    if (!minefield_initialised)
	minefield_init();
    q = minefield_alloc(size);
    oldsize = minefield_get_size(p);
    memcpy(q, p, (oldsize < size ? oldsize : size));
    minefield_free(p);
    return q;
}

#endif				/* MINEFIELD */


// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
