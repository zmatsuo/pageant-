
#include <string>
#include <stdlib.h>

#include "misc_cpp.h"

// @retval	false		error or no env
bool _getenv(const wchar_t *name, std::wstring &val)
{
	std::wstring &buf = val;
	if (buf.size() == 0) {
		buf.resize(128);
	}
    size_t size;
    errno_t r = _wgetenv_s(&size, &buf[0], buf.size(), name);
    if (r == ERANGE) {
		buf.resize(size);		// sizeは'\0'を含む
		r = _wgetenv_s(&size, &buf[0], buf.size(), name);
    }
    if (r != 0 || size == 0) {
		val.clear();
		return false;
    }
	buf.resize(size - 1);		// '\0'を削除
    return true;
}

std::wstring _getenv(const wchar_t *name)
{
    std::wstring val;
    _getenv(name, val);
    return val;
}

errno_t _splitpath(
	const wchar_t *path,
	std::wstring *drive,
	std::wstring *dir,
	std::wstring *fname,
	std::wstring *ext)
{
	wchar_t _drive[_MAX_DRIVE];
	wchar_t _dir[_MAX_DIR];
	wchar_t _fname[_MAX_FNAME];
	wchar_t _ext[_MAX_EXT];
	errno_t e = _wsplitpath_s(path, _drive, _dir, _fname, _ext);
	if (e != 0) {
		if (drive != nullptr)
			drive->clear();
		if (dir != nullptr)
			dir->clear();
		if (fname != nullptr)
			fname->clear();
		if (ext != nullptr)
			ext->clear();
		return e;
	}
	if (drive != nullptr)
		*drive = _drive;
	if (dir != nullptr)
		*dir = _dir;
	if (fname != nullptr)
		*fname = _fname;
	if (ext != nullptr)
		*ext = _ext;
	return 0;
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
