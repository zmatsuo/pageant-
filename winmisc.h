
#ifndef _WINMISC_H_
#define	_WINMISC_H_

#include <windows.h>
#include <Shlobj.h>	// for CSIDL_ constant

#include <string>

std::string _GetModuleFileNameA(HMODULE hModule);
std::wstring _GetModuleFileName(HMODULE hModule);
std::string _GetCurrentDirectoryA();
std::wstring _GetCurrentDirectory();
std::wstring _SHGetFolderPath(int nFolder);
std::string _SHGetFolderPathA(int nFolder);
std::wstring _SHGetKnownFolderPath(REFKNOWNFOLDERID rfid);
std::string _ExpandEnvironmentStrings(const char *str);
std::wstring _ExpandEnvironmentStrings(const wchar_t *str);

bool reg_read_cur_user(const wchar_t *subkey, const wchar_t *valuename,
					   std::wstring &str);
bool reg_write_cur_user(const wchar_t *subkey, const wchar_t *valuename,
						const std::string &str);
bool reg_write_cur_user(const wchar_t *subkey, const wchar_t *valuename,
						const std::wstring &str);
bool reg_read_cur_user(const wchar_t *subkey, const wchar_t *valuename,
					   DWORD &dword);
bool reg_read_cur_user(const wchar_t *subkey, const wchar_t *valuename,
					   int &_int);
bool reg_delete(HKEY hKey, const wchar_t *subkey, const wchar_t *valuename);
bool reg_delete_cur_user(const wchar_t *subkey, const wchar_t *valuename);
void exec(const wchar_t *file, const wchar_t *param = NULL);
void exec_regedit(const wchar_t *open_path);

// misc stuff
HWND get_hwnd();
std::wstring get_full_path(const wchar_t *filename, bool search_path = false);

#endif	// _WINMISC_H_

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:

