/*
 * Centralised Unicode-related helper functions, separate from misc.c
 * so that they can be omitted from tools that aren't including
 * Unicode handling.
 */

#include "puttymem.h"
#include "misc.h"

wchar_t *dup_mb_to_wc_c(int codepage, int flags, const char *string, int len)
{
    int mult;
    for (mult = 1 ;; mult++) {
        wchar_t *ret = snewn(mult*len + 2, wchar_t);
        int outlen;
        outlen = mb_to_wc(codepage, flags, string, len, ret, mult*len + 1);
        if (outlen < mult*len+1) {
            ret[outlen] = L'\0';
            return ret;
        }
        sfree(ret);
    }
}

wchar_t *dup_mb_to_wc(int codepage, int flags, const char *string)
{
    return dup_mb_to_wc_c(codepage, flags, string, (int)strlen(string));
}

#ifdef PUTTY_CAC
char *dup_wc_to_mb(int codepage, int flags, const wchar_t *wcstr, int wclen,
		   const char *defchr, int *defused,
		   struct unicode_data *ucsdata)
{
    int size = wc_to_mb(codepage, flags, wcstr, wclen,
			NULL, 0, defchr, defused, ucsdata);
    
    char *ret = smalloc(size+1);

    wc_to_mb(codepage, flags, wcstr, wclen,
	     ret, size, defchr, defused, ucsdata);

    return ret;
}
#endif
