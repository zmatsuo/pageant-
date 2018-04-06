
#include <string>
#include "codeconvert.h"

bool _getenv(const wchar_t *name, std::wstring &val);
std::wstring _getenv(const wchar_t *name);
errno_t _splitpath(
	const wchar_t *path,
	std::wstring *drive,
	std::wstring *dir,
	std::wstring *fname,
	std::wstring *ext);

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
