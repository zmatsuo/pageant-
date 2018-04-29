/**
   codeconvert.cpp

   Copyright (c) 2017 zmatsuo

   This software is released under the MIT License.
   http://opensource.org/licenses/mit-license.php
*/

#include "codeconvert.h"

#include <string.h>
#include <crtdbg.h>
#include <string>

#include "winmisc.h"
#include "misc.h"
#include "puttymem.h"

std::string wc_to_acp(const std::wstring &wstr)
{
	std::string str;
	_WideCharToMultiByte(wstr, str, false);
	return str;
}

std::string wc_to_utf8(const std::wstring &wstr)
{
	std::string str;
	_WideCharToMultiByte(wstr, str, true);
	return str;
}

std::string wc_to_mb(const std::wstring &wstr)
{
	std::string str;
	_WideCharToMultiByte(wstr, str, true);
	return str;
}

std::wstring mb_to_wc(const std::string &str)
{
	std::wstring wstr;
	_MultiByteToWideChar(str, wstr, true);
	return wstr;
}

std::wstring mb_to_wc(const char *str)
{
	return mb_to_wc(std::string(str));
}

std::wstring acp_to_wc(const std::string &str)
{
	std::wstring wstr;
	_MultiByteToWideChar(str, wstr, false);
	return wstr;
}

std::wstring utf8_to_wc(const std::string &str)
{
	std::wstring wstr;
	_MultiByteToWideChar(str, wstr, true);
	return wstr;
}

wchar_t *dup_mb_to_wc(const char *string)
{
	std::wstring wstr;
	_MultiByteToWideChar(string, strlen(string), wstr, true);

	size_t wstr_len = wstr.length();
	wchar_t *ret = snewn(wstr_len+1 , wchar_t);
	memcpy(ret, wstr.c_str(), (wstr_len+1) * sizeof(wchar_t));
	return ret;
}

char *dup_wc_to_mb(const wchar_t *wstring)
{
	std::string str;
	_WideCharToMultiByte(wstring, wcslen(wstring), str, false);

	size_t str_len = str.length();
	char *ret = snewn(str_len+1 , char);
	memcpy(ret, str.c_str(), (str_len+1) * sizeof(char));
	return ret;
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
