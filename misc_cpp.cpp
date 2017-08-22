
#include <windows.h>
#include <vector>
#include <stdlib.h>

#include "misc_cpp.h"

std::string wc_to_mb(const std::wstring &wstr)
{
    const UINT cp = CP_ACP;
    const int len = ::WideCharToMultiByte(cp, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    std::vector<char> buf(len+1);
    ::WideCharToMultiByte(cp, 0, wstr.c_str(), -1, &buf[0], len+1, NULL,NULL);
    return &buf[0];
}

std::wstring mb_to_wc(const std::string &str)
{
    const UINT cp = CP_UTF8;
    const int len = ::MultiByteToWideChar(cp, 0, str.c_str(), -1, NULL, 0);
    std::vector<wchar_t> buf(len+1);
    ::MultiByteToWideChar(cp, 0, str.c_str(), -1, &buf[0], len+1);
    return &buf[0];
}

bool _getenv(const wchar_t *name, std::wstring &val)
{
    std::vector<wchar_t> buf(256);
    size_t size;
    errno_t r = _wgetenv_s(&size, &buf[0], buf.size(), name);
    if (r == ERANGE) {
		buf.resize(size+1);
		r = _wgetenv_s(&size, &buf[0], buf.size(), name);
    }
    if (r != 0) {
		val.clear();
		return false;
    }
    val = &buf[0];
    return true;
}

std::wstring _getenv(const wchar_t *name)
{
    std::wstring val;
    bool r = _getenv(name, val);
    return val;
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
