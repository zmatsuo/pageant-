/**
   codeconvert.h

   Copyright (c) 2017 zmatsuo

   This software is released under the MIT License.
   http://opensource.org/licenses/mit-license.php
*/

#pragma once

#if defined(__cplusplus)
std::string wc_to_utf8(const std::wstring &wstr);
std::wstring utf8_to_wc(const std::string &str);
std::wstring acp_to_wc(const std::string &str);
std::string wc_to_mb(const std::wstring &wstr);
std::wstring mb_to_wc(const std::string &str);
std::string wc_to_mb(const wchar_t *wstr);
std::wstring mb_to_wc(const char *str);
#endif

#if defined(__cplusplus)
extern "C" {
#endif
wchar_t *dup_mb_to_wc(const char *string);
char *dup_wc_to_mb(const wchar_t *wstring);
#if defined(__cplusplus)
}
#endif

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
