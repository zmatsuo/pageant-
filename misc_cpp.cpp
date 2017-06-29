
#include <windows.h>
#include <vector>

#include "misc_cpp.h"

std::string wc_to_mb(const std::wstring &wstr)
{
    const UINT cp = CP_ACP;
    const int len = ::WideCharToMultiByte(cp, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    std::vector<char> buf(len+1);
    ::WideCharToMultiByte(cp, 0, wstr.c_str(),-1, &buf[0], len+1, NULL,NULL);
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
