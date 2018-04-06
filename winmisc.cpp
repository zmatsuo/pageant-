/*
 * winmisc.c: miscellaneous Windows-specific things
 */
#undef UNICODE
#define _CRT_SECURE_NO_WARNINGS
#include <algorithm>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <Lmcons.h>		// for UNLEN
#include <string>
#include <sstream>
#include <io.h>
#include <crtdbg.h>
#include <thread>

#include "misc.h"
#include "winmisc.h"

std::wstring _GetModuleFileName(HMODULE hModule)
{
	std::vector<wchar_t> buf(MAX_PATH);

	for(;;) {
		DWORD r = ::GetModuleFileNameW(hModule, &buf[0], (DWORD)buf.size());
		if (r == 0) {
			// 関数が失敗
			buf[0] = 0;
			break;
		} else if (r < buf.size() - 1) {
			break;
		} else { 
			buf.resize(buf.size()*2);
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

static std::wstring _SearchPath(const wchar_t *filename)
{
	std::vector<wchar_t> buf(256);
	const wchar_t *ext = NULL;
	DWORD r = ::SearchPathW(NULL, filename, ext, (DWORD)buf.size(), &buf[0], NULL);
	if (r == 0) {
		// not found
		return L"";
	}
	if (r < buf.size()) {
		return &buf[0];
	}
	buf.resize(r);
	r = ::SearchPathW(NULL, filename, ext, (DWORD)buf.size(), &buf[0], NULL);
	return &buf[0];
}

/**
 *
 *	SHGetFolderPath() is supported only for backward compatibility.
 */
bool _SHGetKnownFolderPath(REFKNOWNFOLDERID rfid, std::wstring &path)
{
	PWSTR pszPath = NULL;
	HRESULT r = ::SHGetKnownFolderPath(rfid, KF_FLAG_DEFAULT, NULL, &pszPath);
	if (r != S_OK) {
		return false;
	}
	path = pszPath;
	CoTaskMemFree(pszPath);
	return true;
}

std::wstring _SHGetKnownFolderPath(REFKNOWNFOLDERID rfid)
{
	std::wstring path;
	bool r = _SHGetKnownFolderPath(rfid, path);
	if (r == false) {
		return L"";
	}
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

    f = _GetCurrentDirectory();
	f = f + L"\\" + filename;
	if (_waccess(f.c_str(), 0) == 0) {
		return f;
    }

    f = _GetModuleFileName(NULL);
	size_t pos = f.rfind(L'\\');
	if (pos != std::string::npos) {
		f = f.substr(0, pos+1);
	}
	f = f + filename;
	if (_waccess(f.c_str(), 0) == 0) {
		return f;
    }

	if (search_path) {
		f = _SearchPath(filename);
		if (!f.empty()) {
			return f;
		}
	}

	f.clear();
	return f;
}

std::wstring _ExpandEnvironmentStrings(const wchar_t *str)
{
    DWORD len = ::ExpandEnvironmentStringsW(str, NULL, 0);
	std::wstring s(len, 0);		// len = include '\0'
    ExpandEnvironmentStringsW(str, &s[0], len);
	s.resize(len-1);	// remove '\0'
    return s;
}

/**
 *	@retvalue	true	read ok
 *	@retvalue	false	entry does not exist
 */
bool reg_read(HKEY hKey, const wchar_t *subkey, const wchar_t *valuename,
			  DWORD &dwType, std::vector<uint8_t> &data)
{
	HKEY hRegKey;
	LONG r;
	r = RegOpenKeyExW(hKey, subkey, 0, KEY_READ, &hRegKey);
	if (r != ERROR_SUCCESS) {
		// ERROR_FILE_NOT_FOUND -> not found
		data.clear();
		return false;
	}
	bool ret_value = true;
	data.resize(256);
	while (1) {
		DWORD size = (DWORD)data.size();
		r = RegQueryValueExW(hRegKey, valuename, 0, &dwType,
							 &data[0], &size);
		if (r == ERROR_SUCCESS) {
			data.resize(size);
			break;
		} else if (r == ERROR_MORE_DATA) {
			if (size > data.size()) {
				data.resize(size);
			}
		} else {	// if (r == ERROR_FILE_NOT_FOUND)
			data.clear();
			ret_value = false;
			break;
		}
	}
	RegCloseKey(hRegKey);
	return ret_value;
}

bool reg_read_cur_user(const wchar_t *subkey, const wchar_t *valuename,
					   std::vector<std::wstring> &strs)
{
	strs.clear();
	std::vector<uint8_t> data;
	DWORD dwType;
	bool result = reg_read(
		HKEY_CURRENT_USER, subkey, valuename, dwType, data);
	if (result == false || dwType != REG_MULTI_SZ) {
		strs.clear();
		return false;
	}

	size_t pos = 0;
	while(1) {
		const wchar_t *p = (wchar_t *)&data[pos];
		if (*p == L'\0')
			break;
		std::wstring s = (wchar_t *)&data[pos];
		strs.push_back(s);
		pos += (s.length() + 1) * sizeof(wchar_t);
		if (pos > data.size())
			break;
	}
	return true;
}

bool reg_read_cur_user(const wchar_t *subkey, const wchar_t *valuename,
					   std::wstring &str)
{
	std::vector<uint8_t> data;
	DWORD dwType;
	bool result = reg_read(
		HKEY_CURRENT_USER, subkey, valuename, dwType, data);
	if (result == false || (dwType != REG_SZ && dwType != REG_EXPAND_SZ)) {
		str.clear();
		return false;
	}
	str = std::wstring((wchar_t *)&data[0]);
	if (dwType == REG_EXPAND_SZ) {
		str = _ExpandEnvironmentStrings(str.c_str());
	}
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
	LONG r;
#if 0
	r = RegOpenKeyExW(hKey, subkey,  0, KEY_ALL_ACCESS, &hRegKey);
#endif
    r = RegCreateKeyExW(hKey, subkey, 0, NULL, REG_OPTION_NON_VOLATILE,
					    KEY_ALL_ACCESS, NULL, &hRegKey, NULL);
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
						const std::vector<std::wstring> &strs)
{
	if (valuename == NULL) {
		HKEY hKey = HKEY_CURRENT_USER;
		return reg_delete(hKey, subkey);
	}
	if (strs.empty()) {
		HKEY hKey = HKEY_CURRENT_USER;
		return reg_delete(hKey, subkey, valuename);
	}
	std::vector<wchar_t> buf;
	for (const auto s : strs) {
		const wchar_t *p = s.c_str();
		buf.insert(buf.end(), p, p + s.length());
		buf.push_back(L'\0');
	}
	buf.push_back(L'\0');
	size_t len = buf.size();
	const wchar_t *ptr = &buf[0];
	return reg_write(HKEY_CURRENT_USER, subkey, valuename,
					 REG_MULTI_SZ, (void *)ptr, (len) * sizeof(wchar_t));
}

bool reg_write_cur_user(const wchar_t *subkey, const wchar_t *valuename,
						const wchar_t *str)
{
	if (valuename == nullptr) {
		HKEY hKey = HKEY_CURRENT_USER;
		return reg_delete(hKey, subkey);
	}
	if (str == nullptr) {
		HKEY hKey = HKEY_CURRENT_USER;
		return reg_delete(hKey, subkey, valuename);
	}
	size_t len = wcslen(str);
	return reg_write(HKEY_CURRENT_USER, subkey, valuename,
					 REG_SZ, (void *)str, (len+1) * sizeof(wchar_t));
}

bool reg_write_cur_user(const wchar_t *subkey, const wchar_t *valuename,
						DWORD dword)
{
	return reg_write(
		HKEY_CURRENT_USER, subkey, valuename, REG_DWORD, &dword, sizeof(dword));
}

bool reg_write_cur_user(const wchar_t *subkey, const wchar_t *valuename,
						int _int)
{
	return reg_write_cur_user(subkey, valuename, (DWORD)_int);
}

bool reg_delete(HKEY hKey, const wchar_t *subkey, const wchar_t *valuename)
{
	HKEY hRegKey;
	LSTATUS r;
	r = RegOpenKeyExW(hKey, subkey, 0, KEY_ALL_ACCESS, &hRegKey);
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

bool reg_delete(HKEY hKey, const wchar_t *_subkey)
{
	const wchar_t *sep = wcsrchr(_subkey, L'\\');
	if (sep == NULL) {
		return false;
	}
	std::wstring key(_subkey, sep);
	std::wstring subkey(sep+1);

	HKEY hRegKey;
	LSTATUS r;
	r = RegOpenKeyExW(hKey, key.c_str(),  0, KEY_ALL_ACCESS, &hRegKey);
	if (r != ERROR_SUCCESS) {
		return false;
	}
	bool ret_value = true;
    r = RegDeleteKeyW(hRegKey, subkey.c_str());
	if (r != ERROR_SUCCESS) {
		ret_value = false;
	}
	RegCloseKey(hRegKey);
	return ret_value;
}

bool reg_delete_tree(HKEY hKey, const wchar_t *subkey)
{
	LONG r = RegDeleteTreeW(hKey, subkey);
	return (r == ERROR_SUCCESS) ? true : false;
}

bool reg_delete_cur_user(const wchar_t *subkey, const wchar_t *valuename)
{
	if (valuename == nullptr) {
		return reg_delete(HKEY_CURRENT_USER, subkey);
	}
	return reg_delete(HKEY_CURRENT_USER, subkey, valuename);
}

bool reg_delete_cur_user(const wchar_t *subkey)
{
	return reg_delete(HKEY_CURRENT_USER, subkey);
}

bool reg_enum_key(
	HKEY _hKey,
	const wchar_t *subkey,
	std::vector<std::wstring> &keys)
{
	keys.clear();

	HKEY hRegKey;
	LONG r;
	r = RegOpenKeyW(_hKey, subkey, &hRegKey);
	if (r != ERROR_SUCCESS) {
		return false;
	}

	int index_key = 0;
	for(;;) {
		wchar_t buf[MAX_PATH + 1];
		r = RegEnumKeyW(hRegKey, index_key, buf, _countof(buf));
		if (r != ERROR_SUCCESS) {
			break;
		}
		keys.push_back(buf);
		index_key++;
	}

	RegCloseKey(hRegKey);
	return true;
}

static std::wstring escape_str(const wchar_t *_org)
{
	const wchar_t *dead_char = L"=";
	const wchar_t *encode_str = L"%3d";
	std::wstring encode(_org);
	std::wstring::size_type pos = 0;
	while(1) {
		pos = encode.find(dead_char, pos);
		if (pos == std::wstring::npos) {
			break;
		}
		encode.replace(pos, 1, encode_str);
		pos += 3;
	}
	return encode;
}

#if 0
static std::wstring unescape_str(const wchar_t *encode)
{
	const wchar_t *dead_char = L"=";
	const wchar_t *encode_str = L"%3d";
	std::wstring decode(encode);
	std::wstring::size_type pos = 0;
	while(1) {
		pos = decode.find(encode_str, pos);
		if (pos == std::wstring::npos) {
			break;
		}
		decode.replace(pos, 1, dead_char);
		pos += 1;
	}
	return decode;
}
#endif

bool _WritePrivateProfileString(
	const wchar_t *section, const wchar_t *key, const wchar_t *ini,
	const wchar_t *str)
{
	bool create_bom_only_file = false;
	if (!_PathFileExists(ini)) {
		create_bom_only_file = true;
	} else {
		uint64_t size;
		bool r = _GetFileSize(ini, size);
		if (r == false || size == 0) {
			create_bom_only_file = true;
		}
	}
	if (create_bom_only_file) {
		// create utf16le ini file
		FILE *fp = _wfopen(ini, L"wb");
		if (fp != NULL) {
			const static uint8_t bom_utf16_le[] = {0xff, 0xfe};
			fwrite(bom_utf16_le, sizeof(bom_utf16_le), 1, fp);
			fclose(fp);
		}
	}

	BOOL r;
	if (key == nullptr) {
		r = ::WritePrivateProfileStringW(section, NULL, str, ini);
	} else {
		const std::wstring key_encode = escape_str(key);
		r = ::WritePrivateProfileStringW(section, key_encode.c_str(), str, ini);
	}
	return r == FALSE ? false : true;
}

bool _GetPrivateProfileString(
	const wchar_t *section, const wchar_t *key, const wchar_t *ini,
	std::wstring &str)
{
	const std::wstring key_encode = escape_str(key);
	std::vector<wchar_t> buf(1024);
	buf[0] = 0;
	while(1) {
		DWORD r = ::GetPrivateProfileStringW(section, key_encode.c_str(), L"", &buf[0], (DWORD)buf.size(), ini);
		if (buf[0] == L'\0') {
			// エントリがない(または key= )
			str.clear();
			return false;
		}
		if (r < (DWORD)buf.size() - 2) {
			break;
		}
		buf.resize(buf.size()*2);
	}
	str = &buf[0];
	return true;
}

bool _GetPrivateProfileSectionNames(
	const wchar_t *ini,
	std::vector<std::wstring> &strAry)
{
	std::vector<wchar_t> buf(256);
	for(;;) {
		DWORD r = ::GetPrivateProfileSectionNamesW(&buf[0], (DWORD)buf.size(), ini);
		if (r == 0) {
			// no entry
			strAry.clear();
			return false;
			break;
		} else if ( r< (DWORD)buf.size() - 2) {
			// ok
			break;
		} else {
			buf.resize(buf.size()*2);
		}
	}
	strAry.clear();
	wchar_t *p = &buf[0];
	while(*p != '\0') {
		strAry.push_back(p);
		p += wcslen(p) + 1;
	}
	return true;
}

/**
 *	@param	wstr_len	wstr長	EOS(L'\0')を含まない(wcslen()の戻り値)
 */
bool _WideCharToMultiByte(const wchar_t *wstr_ptr, size_t wstr_len, std::string &str, bool utf8)
{
    UINT codePage = utf8 ? CP_UTF8 : CP_ACP;
	DWORD flags = 0;
    int len =
		::WideCharToMultiByte(codePage, flags,
							  wstr_ptr, (DWORD)wstr_len,
							  NULL, 0,
							  NULL, NULL);
	if (len == 0) {
		str.clear();
		return false;
	}
	str.resize(len);
	char *buf =
		&str[0];		// C++11/14
		//str.data();	// C++17(c++1z)
	len = 
		::WideCharToMultiByte(codePage, flags,
							  wstr_ptr, (DWORD)wstr_len,
							  buf, len,
							  NULL,NULL);
	if (len == 0) {
		str.clear();
		return false;
	}
    return true;
}

bool _WideCharToMultiByte(const std::wstring &wstr, std::string &str, bool utf8)
{
	return _WideCharToMultiByte(wstr.c_str(), wstr.length(), str, utf8);
}

/**
 *	@param	str_len		str長	EOS('\0')を含まない(strlen()の戻り値)
 */
bool _MultiByteToWideChar(const char *str_ptr, size_t str_len, std::wstring &wstr, bool utf8)
{
    UINT codePage = utf8 ? CP_UTF8 : CP_ACP;
	DWORD flags = MB_ERR_INVALID_CHARS;
	int len =
		::MultiByteToWideChar(
			codePage, flags,
			str_ptr, (int)str_len,
			NULL, 0);
	if (len == 0) {
		wstr.clear();
		return false;
	}
	wstr.resize(len);
	wchar_t *buf =
		&wstr[0];		// C++11/14
		//wstr.data();	// C++17(c++1z)
	len =
		::MultiByteToWideChar(
			codePage, flags,
			str_ptr, (int)str_len,
			buf, len);
	if (len == 0) {
		wstr.clear();
		return false;
	}
	return true;
}

bool _MultiByteToWideChar(const std::string &str, std::wstring &wstr, bool utf8)
{
	return _MultiByteToWideChar(str.c_str(), str.length(), wstr, utf8);
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


#if defined(DEVELOP_VERSION)
static bool debug_console_enabled;
static bool debug_console_opend;
static HWND debug_console_hwnd;
static bool debug_console_show_flag = true;

static void debug_console_open()
{
	BOOL r = AllocConsole();
	if (r == FALSE) {
		return;
	}
	debug_console_hwnd = GetConsoleWindow();
	if (!debug_console_show_flag) {
		ShowWindow(debug_console_hwnd, SW_HIDE);
	}

	{
		static FILE *stdin_new;
		static FILE *stderr_new;
		static FILE *stdout_new;
		freopen_s(&stdin_new, "CON", "r", stdin);
		freopen_s(&stdout_new, "CON", "w", stdout);
		freopen_s(&stderr_new, "CON", "w", stderr);
	}
	debug_console_opend = true;
}

void debug_console_init(int enable)
{
	debug_console_enabled = enable == 0 ? false : true;
    if (!debug_console_opend) {
		debug_console_open();
	}
}

void debug_console_puts(const char *buf)
{
    if (!debug_console_opend) {
		debug_console_open();
    }
	printf("%s", buf);
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

const DWORD MS_VC_EXCEPTION=0x406D1388;

#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO
{
	DWORD dwType; // Must be 0x1000.
	LPCSTR szName; // Pointer to name (in user addr space).
	DWORD dwThreadID; // Thread ID (-1=caller thread).
	DWORD dwFlags; // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)

void SetThreadName(DWORD dwThreadID, const char* threadName)
{
	// DWORD dwThreadID = ::GetThreadId( static_cast<HANDLE>( t.native_handle() ) );

	THREADNAME_INFO info;
	info.dwType = 0x1000;
	info.szName = threadName;
	info.dwThreadID = dwThreadID;
	info.dwFlags = 0;

	__try
	{
		RaiseException( MS_VC_EXCEPTION, 0, sizeof(info)/sizeof(ULONG_PTR), (ULONG_PTR*)&info );
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
	}
}

void SetThreadName(const char* threadName)
{
    SetThreadName(GetCurrentThreadId(),threadName);
}

void setThreadName(std::thread *thread, const char *threadName)
{
    DWORD threadId = ::GetThreadId( static_cast<HANDLE>( thread->native_handle() ) );
    SetThreadName(threadId, threadName);
}

std::wstring _FormatMessage(DWORD last_error)
{
	std::wstring msg(256, 0);
	const DWORD dwflags = 0
		| FORMAT_MESSAGE_FROM_SYSTEM
		| FORMAT_MESSAGE_IGNORE_INSERTS
		| FORMAT_MESSAGE_MAX_WIDTH_MASK
		;
	while(1) {
		DWORD r = ::FormatMessageW(dwflags,
								   0,
								   last_error,
								   0,
								   &msg[0], (DWORD)msg.size(),
								   NULL);
		if (r == 0) {
			auto e = GetLastError();
			if (e == ERROR_INSUFFICIENT_BUFFER) {
				msg.resize(msg.size()*2);
				continue;
			}
			return L"error\n";
		} else if (r < msg.size()) {
			msg.resize(r);
			break;
		} else if (r == msg.size()) {
			msg.resize(msg.size()*2);
			continue;
		}
	}
	return msg;
}

std::wstring _GetUserName()
{
	std::wstring str(UNLEN, 0);
	DWORD len = static_cast<DWORD>(str.size());
    BOOL r = GetUserNameW(&str[0], &len);
	if (r == 0) {
		return L"";
	}
	str.resize(len - 1);
	return str;
}

std::wstring _GetComputerNameEx(COMPUTER_NAME_FORMAT NameType)
{
	std::wstring str(MAX_COMPUTERNAME_LENGTH, 0);
	while(1) {
		DWORD str_len = static_cast<DWORD>(str.size());
		BOOL r = GetComputerNameExW(NameType, &str[0], &str_len);
		if (r != 0) {
			str.resize(str_len);	// str_len = '\0'分は含まれていない
			return str;
		}
		auto error = GetLastError();
		if (error != ERROR_MORE_DATA) {
			return L"";
		}
		str.resize(str_len);	// str_len = '\0'分は含まれている
	}
}

bool _GetFileSize(const wchar_t *path, uint64_t &size)
{
	HANDLE hFile =
		CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
					FILE_ATTRIBUTE_NORMAL, NULL);
	if(hFile == INVALID_HANDLE_VALUE){
		size = 0;
		return false;
	}

	LARGE_INTEGER FileSize;
	BOOL r = GetFileSizeEx(hFile, &FileSize);
	size = ((uint64_t)FileSize.HighPart << 32) + FileSize.LowPart;

	CloseHandle(hFile);

	return r == 0 ? false : true;
}

bool _GetFileAttributes(const wchar_t *path, DWORD &attributes)
{
	attributes = GetFileAttributesW(path);
	return attributes == -1 ? false : true;
}

bool _CreateDirectory(const wchar_t *path)
{
	BOOL r = CreateDirectoryW(path, NULL);
	return r == 0 ? false : true;
}

// same as PathFileExists in Shlwapi.lib
bool _PathFileExists(const wchar_t *path)
{
	DWORD attributes;
	return _GetFileAttributes(path, attributes);
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
