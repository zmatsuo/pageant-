
#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

wchar_t *to_wcs(const char *src, int codePage, int *len);
char *to_string(const wchar_t *src, int codePage, int *len);

#if defined(__cplusplus)
}
#endif

#if defined(__cplusplus)
void to_wstring(const char *src, std::wstring &dest, int codePage = -1);
void to_utf8(const wchar_t *src, std::string &utf8);
void to_utf8(const char *src, std::string &utf8, int codePage = -1);
#endif

