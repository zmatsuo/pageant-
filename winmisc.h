
#ifndef _WINMISC_H_
#define	_WINMISC_H_

#include <windows.h>
#include <Shlobj.h>	// for CSIDL_ constant

#include <string>
#include <vector>

// path
std::wstring _GetModuleFileName(HMODULE hModule);
std::string _GetCurrentDirectoryA();
std::wstring _GetCurrentDirectory();
std::wstring _SHGetKnownFolderPath(REFKNOWNFOLDERID rfid);
bool _SHGetKnownFolderPath(REFKNOWNFOLDERID rfid, std::wstring &path);
std::string _ExpandEnvironmentStrings(const char *str);
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

// misc stuff
HWND get_hwnd();
std::wstring get_full_path(const wchar_t *filename, bool search_path = false);

#endif	// _WINMISC_H_

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:

