
#pragma once

#include <SDKDDKVer.h>
#include <windows.h>
#include <Shlobj.h>		// for CSIDL_ constant
#include <tlhelp32.h>	// for PROCESSENTRY32W

#include <string>
#include <vector>
#include <thread>

#include "pageant+.h"	// for DEVELOP_VERSION

// debug

#if defined(DEVELOP_VERSION)
void debug_console_puts(const char *buf);
void debug_console_init(int enable);
void debug_console_show(int show);
#endif

// path
std::wstring _GetModuleFileName(HMODULE hModule);
std::wstring _GetCurrentDirectory();
std::wstring _SHGetKnownFolderPath(REFKNOWNFOLDERID rfid);
bool _SHGetKnownFolderPath(REFKNOWNFOLDERID rfid, std::wstring &path);
std::wstring _ExpandEnvironmentStrings(const wchar_t *str);

// ini file
bool _WritePrivateProfileString(
	const wchar_t *section, const wchar_t *key, const wchar_t *ini,
	const wchar_t *str);
bool _GetPrivateProfileString(
	const wchar_t *section, const wchar_t *key, const wchar_t *ini,
	std::wstring &str);
bool _GetPrivateProfileSectionNames(
	const wchar_t *ini,
	std::vector<std::wstring> &strAry);

// registory
bool reg_read(HKEY hKey, const wchar_t *subkey, const wchar_t *valuename,
			  DWORD &dwType, std::vector<uint8_t> &data);
bool reg_write(HKEY hKey, const wchar_t *subkey, const wchar_t *valuename,
			   DWORD dwType, const void *data_ptr, size_t data_size);

// registory HKEY_CURRENT_USER
bool reg_read_cur_user(const wchar_t *subkey, const wchar_t *valuename,
					   std::vector<std::wstring> &str);
bool reg_read_cur_user(const wchar_t *subkey, const wchar_t *valuename,
					   std::wstring &str);
bool reg_read_cur_user(const wchar_t *subkey, const wchar_t *valuename,
					   DWORD &dword);
bool reg_read_cur_user(const wchar_t *subkey, const wchar_t *valuename,
					   int &_int);
bool reg_write_cur_user(const wchar_t *subkey, const wchar_t *valuename,
						const std::vector<std::wstring> &str);
bool reg_write_cur_user(const wchar_t *subkey, const wchar_t *valuename,
						const wchar_t *str);
bool reg_write_cur_user(const wchar_t *subkey, const wchar_t *valuename,
						DWORD dword);
bool reg_write_cur_user(const wchar_t *subkey, const wchar_t *valuename,
						int _int);
bool reg_delete(HKEY hKey, const wchar_t *subkey, const wchar_t *valuename);
bool reg_delete(HKEY hKey, const wchar_t *subkey);
bool reg_delete_tree(HKEY hKey, const wchar_t *subkey);
bool reg_delete_cur_user(const wchar_t *subkey, const wchar_t *valuename);
bool reg_delete_cur_user(const wchar_t *key);
bool reg_enum_key(
	HKEY _hKey,
	const wchar_t *subkey,
	std::vector<std::wstring> &keys);

// string covert
bool _WideCharToMultiByte(const wchar_t *wstr_ptr, size_t wstr_len, std::string &str, bool utf8);
bool _WideCharToMultiByte(const std::wstring &wstr, std::string &str, bool utf8);
bool _MultiByteToWideChar(const char *str_ptr, size_t str_len, std::wstring &wstr, bool utf8);
bool _MultiByteToWideChar(const std::string &str, std::wstring &wstr, bool utf8);

// exec
void exec(const wchar_t *file, const wchar_t *param = NULL);
void exec_regedit(const wchar_t *open_path);

// for debug
void SetThreadName(DWORD dwThreadID, const char* threadName);
void SetThreadName(const char* threadName);
void setThreadName(std::thread *thread, const char *threadName);		// todo debugへ持っていく

// misc stuff
std::wstring get_full_path(const wchar_t *filename, bool search_path = false);

std::wstring _FormatMessage(DWORD last_error);
std::wstring _GetComputerNameEx(COMPUTER_NAME_FORMAT NameType);
std::wstring _GetUserName();
bool _GetFileSize(const wchar_t *path, uint64_t &size);
bool _GetFileAttributes(const wchar_t *path, DWORD &attributes);
bool _CreateDirectory(const wchar_t *path);
bool _PathFileExists(const wchar_t *path);
std::wstring _GetTempPath();
bool _ShellExecuteExAdmin(
	const wchar_t *exe, const wchar_t *param, DWORD *exit_code);
std::vector<HWND> _EnumWindows();
std::vector<PROCESSENTRY32W> _Process();

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:

