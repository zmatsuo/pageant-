
#include <Windows.h>
#include <vector>


#include "misc_cpp.h"

std::string wc_to_mb(const std::wstring &wstr)
{
    const UINT cp = CP_ACP;
    int len = ::WideCharToMultiByte(cp, 0, wstr.c_str(),-1, NULL,0, NULL,NULL);
    std::vector<char> buf(len+1);
    ::WideCharToMultiByte(cp, 0, wstr.c_str(),-1, &buf[0], len+1, NULL,NULL);
    return &buf[0];
}
