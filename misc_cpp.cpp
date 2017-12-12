
#include <windows.h>
#include <vector>
#include <stdlib.h>

#include "misc_cpp.h"

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
