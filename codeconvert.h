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
#endif

#if defined(__cplusplus)
extern "C" {
#endif
wchar_t *dup_mb_to_wc(const char *string);
char *dup_wc_to_mb(const wchar_t *wstring);
#if defined(__cplusplus)
}
#endif
