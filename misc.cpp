/**
   misc.c
   from putty
   Platform-independent routines shared between all PuTTY programs.

   Copyright (c) 2017 zmatsuo

   This software is released under the MIT License.
   http://opensource.org/licenses/mit-license.php
*/
#undef UNICODE
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>
#include <ctype.h>
#include <assert.h>
#include <windows.h>
#include <crtdbg.h>

#include "filename.h"
#include "winutils.h"
#include "version.h"
#include "puttymem.h"
#include "misc.h"
#include "codeconvert.h"


/* ----------------------------------------------------------------------
 * String handling routines.
 */

char *dupstr(const char *s)
{
    char *p = NULL;
    if (s) {
        size_t len = strlen(s);
        p = snewn(len + 1, char);
        strcpy(p, s);
    }
    return p;
}

/* Allocate the concatenation of N strings. Terminate arg list with NULL. */
char *dupcat(const char *s1, ...)
{
    size_t len;
    char *p, *q, *sn;
    va_list ap;

    len = strlen(s1);
    va_start(ap, s1);
    while (1) {
	sn = va_arg(ap, char *);
	if (!sn)
	    break;
	len += strlen(sn);
    }
    va_end(ap);

    p = snewn(len + 1, char);
    strcpy(p, s1);
    q = p + strlen(p);

    va_start(ap, s1);
    while (1) {
	sn = va_arg(ap, char *);
	if (!sn)
	    break;
	strcpy(q, sn);
	q += strlen(q);
    }
    va_end(ap);

    return p;
}

void burnstr(char *string)             /* sfree(str), only clear it first */
{
    if (string) {
        smemclr(string, strlen(string));
        sfree(string);
    }
}

int toint(unsigned u)
{
    /*
     * Convert an unsigned to an int, without running into the
     * undefined behaviour which happens by the strict C standard if
     * the value overflows. You'd hope that sensible compilers would
     * do the sensible thing in response to a cast, but actually I
     * don't trust modern compilers not to do silly things like
     * assuming that _obviously_ you wouldn't have caused an overflow
     * and so they can elide an 'if (i < 0)' test immediately after
     * the cast.
     *
     * Sensible compilers ought of course to optimise this entire
     * function into 'just return the input value'!
     */
    if (u <= (unsigned)INT_MAX)
        return (int)u;
    else if (u >= (unsigned)INT_MIN)   /* wrap in cast _to_ unsigned is OK */
        return INT_MIN + (int)(u - (unsigned)INT_MIN);
    else
        return INT_MIN; /* fallback; should never occur on binary machines */
}

/*
 * Do an sprintf(), but into a custom-allocated buffer.
 * 
 * Currently I'm doing this via vsnprintf. This has worked so far,
 * but it's not good, because vsnprintf is not available on all
 * platforms. There's an ifdef to use `_vsnprintf', which seems
 * to be the local name for it on Windows. Other platforms may
 * lack it completely, in which case it'll be time to rewrite
 * this function in a totally different way.
 * 
 * The only `properly' portable solution I can think of is to
 * implement my own format string scanner, which figures out an
 * upper bound for the length of each formatting directive,
 * allocates the buffer as it goes along, and calls sprintf() to
 * actually process each directive. If I ever need to actually do
 * this, some caveats:
 * 
 *  - It's very hard to find a reliable upper bound for
 *    floating-point values. %f, in particular, when supplied with
 *    a number near to the upper or lower limit of representable
 *    numbers, could easily take several hundred characters. It's
 *    probably feasible to predict this statically using the
 *    constants in <float.h>, or even to predict it dynamically by
 *    looking at the exponent of the specific float provided, but
 *    it won't be fun.
 * 
 *  - Don't forget to _check_, after calling sprintf, that it's
 *    used at most the amount of space we had available.
 * 
 *  - Fault any formatting directive we don't fully understand. The
 *    aim here is to _guarantee_ that we never overflow the buffer,
 *    because this is a security-critical function. If we see a
 *    directive we don't know about, we should panic and die rather
 *    than run any risk.
 */
static char *dupvprintf_inner(char *buf, int oldlen, int *oldsize,
                              const char *fmt, va_list ap)
{
    int len, size, newsize;

    assert(*oldsize >= oldlen);
    size = *oldsize - oldlen;
    if (size == 0) {
        size = 512;
        newsize = oldlen + size;
        buf = sresize(buf, newsize, char);
    } else {
        newsize = *oldsize;
    }

    while (1) {
#if defined _WINDOWS && !defined __WINE__ && _MSC_VER < 1900 /* 1900 == VS2015 has real snprintf */
#define vsnprintf _vsnprintf
#endif
#ifdef va_copy
	/* Use the `va_copy' macro mandated by C99, if present.
	 * XXX some environments may have this as __va_copy() */
	va_list aq;
	va_copy(aq, ap);
	len = vsnprintf(buf + oldlen, size, fmt, aq);
	va_end(aq);
#else
	/* Ugh. No va_copy macro, so do something nasty.
	 * Technically, you can't reuse a va_list like this: it is left
	 * unspecified whether advancing a va_list pointer modifies its
	 * value or something it points to, so on some platforms calling
	 * vsnprintf twice on the same va_list might fail hideously
	 * (indeed, it has been observed to).
	 * XXX the autoconf manual suggests that using memcpy() will give
	 *     "maximum portability". */
	len = vsnprintf(buf + oldlen, size, fmt, ap);
#endif
	if (len >= 0 && len < size) {
	    /* This is the C99-specified criterion for snprintf to have
	     * been completely successful. */
            *oldsize = newsize;
	    return buf;
	} else if (len > 0) {
	    /* This is the C99 error condition: the returned length is
	     * the required buffer size not counting the NUL. */
	    size = len + 1;
	} else {
	    /* This is the pre-C99 glibc error condition: <0 means the
	     * buffer wasn't big enough, so we enlarge it a bit and hope. */
	    size += 512;
	}
        newsize = oldlen + size;
        buf = sresize(buf, newsize, char);
    }
}

char *dupvprintf(const char *fmt, va_list ap)
{
    int size = 0;
    return dupvprintf_inner(NULL, 0, &size, fmt, ap);
}
char *dupprintf(const char *fmt, ...)
{
    char *ret;
    va_list ap;
    va_start(ap, fmt);
    ret = dupvprintf(fmt, ap);
    va_end(ap);
    return ret;
}

// ローカルだけ、
#if 1
typedef struct strbuf {
    char *s;
    int len, size;
} strbuf;
strbuf *strbuf_new(void)
{
    strbuf *buf = snew(strbuf);
    buf->len = 0;
    buf->size = 512;
    buf->s = snewn(buf->size, char);
    *buf->s = '\0';
    return buf;
}
void strbuf_free(strbuf *buf)
{
    sfree(buf->s);
    sfree(buf);
}
char *strbuf_str(strbuf *buf)
{
    return buf->s;
}
char *strbuf_to_str(strbuf *buf)
{
    char *ret = buf->s;
    sfree(buf);
    return ret;
}
void strbuf_catfv(strbuf *buf, const char *fmt, va_list ap)
{
    buf->s = dupvprintf_inner(buf->s, buf->len, &buf->size, fmt, ap);
    buf->len += strlen(buf->s + buf->len);
}
void strbuf_catf(strbuf *buf, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    strbuf_catfv(buf, fmt, ap);
    va_end(ap);
}
#endif

/*
 * Read an entire line of text from a file. Return a buffer
 * malloced to be as big as necessary (caller must free).
 */
char *fgetline(FILE *fp)
{
    char *ret = snewn(512, char);
    size_t size = 512, len = 0;
    while (fgets(ret + len, (int)(size - len), fp)) {
	len += strlen(ret + len);
	if (len > 0 && ret[len-1] == '\n')
	    break;		       /* got a newline, we're done */
	size = len + 512;
	ret = sresize(ret, size, char);
    }
    if (len == 0) {		       /* first fgets returned NULL */
	sfree(ret);
	return NULL;
    }
    ret[len] = '\0';
    return ret;
}

/*
 * Perl-style 'chomp', for a line we just read with fgetline. Unlike
 * Perl chomp, however, we're deliberately forgiving of strange
 * line-ending conventions. Also we forgive NULL on input, so you can
 * just write 'line = chomp(fgetline(fp));' and not bother checking
 * for NULL until afterwards.
 */
char *chomp(char *str)
{
    if (str) {
        size_t len = strlen(str);
        while (len > 0 && (str[len-1] == '\r' || str[len-1] == '\n'))
            len--;
        str[len] = '\0';
    }
    return str;
}

/* ----------------------------------------------------------------------
 * Core base64 encoding and decoding routines.
 */

void base64_encode_atom(const unsigned char *data, int n, char *out)
{
    static const char base64_chars[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    unsigned word;

    word = data[0] << 16;
    if (n > 1)
	word |= data[1] << 8;
    if (n > 2)
	word |= data[2];
    out[0] = base64_chars[(word >> 18) & 0x3F];
    out[1] = base64_chars[(word >> 12) & 0x3F];
    if (n > 1)
	out[2] = base64_chars[(word >> 6) & 0x3F];
    else
	out[2] = '=';
    if (n > 2)
	out[3] = base64_chars[word & 0x3F];
    else
	out[3] = '=';
}

int base64_decode_atom(const char *atom, unsigned char *out)
{
    int vals[4];
    int i, v, len;
    unsigned word;
    char c;

    for (i = 0; i < 4; i++) {
	c = atom[i];
	if (c >= 'A' && c <= 'Z')
	    v = c - 'A';
	else if (c >= 'a' && c <= 'z')
	    v = c - 'a' + 26;
	else if (c >= '0' && c <= '9')
	    v = c - '0' + 52;
	else if (c == '+')
	    v = 62;
	else if (c == '/')
	    v = 63;
	else if (c == '=')
	    v = -1;
	else
	    return 0;		       /* invalid atom */
	vals[i] = v;
    }

    if (vals[0] == -1 || vals[1] == -1)
	return 0;
    if (vals[2] == -1 && vals[3] != -1)
	return 0;

    if (vals[3] != -1)
	len = 3;
    else if (vals[2] != -1)
	len = 2;
    else
	len = 1;

    word = ((vals[0] << 18) |
	    (vals[1] << 12) | ((vals[2] & 0x3F) << 6) | (vals[3] & 0x3F));
    out[0] = (word >> 16) & 0xFF;
    if (len > 1)
	out[1] = (word >> 8) & 0xFF;
    if (len > 2)
	out[2] = word & 0xFF;
    return len;
}

#if 0
/*
 * Determine whether or not a Config structure represents a session
 * which can sensibly be launched right now.
 */
int cfg_launchable(const Config *cfg)
{
    if (cfg->protocol == PROT_SERIAL)
	return cfg->serline[0] != 0;
    else
	return cfg->host[0] != 0;
}

char const *cfg_dest(const Config *cfg)
{
    if (cfg->protocol == PROT_SERIAL)
	return cfg->serline;
    else
	return cfg->host;
}
#endif

/*
 * Validate a manual host key specification (either entered in the
 * GUI, or via -hostkey). If valid, we return TRUE, and update 'key'
 * to contain a canonicalised version of the key string in 'key'
 * (which is guaranteed to take up at most as much space as the
 * original version), suitable for putting into the Conf. If not
 * valid, we return FALSE.
 */
int validate_manual_hostkey(char *key)
{
    char *p, *q, *r, *s;

    /*
     * Step through the string word by word, looking for a word that's
     * in one of the formats we like.
     */
    p = key;
    while ((p += strspn(p, " \t"))[0]) {
        q = p;
        p += strcspn(p, " \t");
        if (*p) *p++ = '\0';

        /*
         * Now q is our word.
         */

        if (strlen(q) == 16*3 - 1 &&
            q[strspn(q, "0123456789abcdefABCDEF:")] == 0) {
            /*
             * Might be a key fingerprint. Check the colons are in the
             * right places, and if so, return the same fingerprint
             * canonicalised into lowercase.
             */
            int i;
            for (i = 0; i < 16; i++)
                if (q[3*i] == ':' || q[3*i+1] == ':')
                    goto not_fingerprint; /* sorry */
            for (i = 0; i < 15; i++)
                if (q[3*i+2] != ':')
                    goto not_fingerprint; /* sorry */
            for (i = 0; i < 16*3 - 1; i++)
                key[i] = tolower(q[i]);
            key[16*3 - 1] = '\0';
            return TRUE;
        }
      not_fingerprint:;

        /*
         * Before we check for a public-key blob, trim newlines out of
         * the middle of the word, in case someone's managed to paste
         * in a public-key blob _with_ them.
         */
        for (r = s = q; *r; r++)
            if (*r != '\n' && *r != '\r')
                *s++ = *r;
        *s = '\0';

        if (strlen(q) % 4 == 0 && strlen(q) > 2*4 &&
            q[strspn(q, "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                     "abcdefghijklmnopqrstuvwxyz+/=")] == 0) {
            /*
             * Might be a base64-encoded SSH-2 public key blob. Check
             * that it starts with a sensible algorithm string. No
             * canonicalisation is necessary for this string type.
             *
             * The algorithm string must be at most 64 characters long
             * (RFC 4251 section 6).
             */
            unsigned char decoded[6];
            unsigned alglen;
            int minlen;
            int len = 0;

            len += base64_decode_atom(q, decoded+len);
            if (len < 3)
                goto not_ssh2_blob;    /* sorry */
            len += base64_decode_atom(q+4, decoded+len);
            if (len < 4)
                goto not_ssh2_blob;    /* sorry */

            alglen = GET_32BIT_MSB_FIRST(decoded);
            if (alglen > 64)
                goto not_ssh2_blob;    /* sorry */

            minlen = ((alglen + 4) + 2) / 3;
            if (strlen(q) < minlen)
                goto not_ssh2_blob;    /* sorry */

            strcpy(key, q);
            return TRUE;
        }
      not_ssh2_blob:;
    }

    return FALSE;
}

int smemeq(const void *av, const void *bv, size_t len)
{
    const unsigned char *a = (const unsigned char *)av;
    const unsigned char *b = (const unsigned char *)bv;
    unsigned val = 0;

    while (len-- > 0) {
        val |= *a++ ^ *b++;
    }
    /* Now val is 0 iff we want to return 1, and in the range
     * 0x01..0xFF iff we want to return 0. So subtracting from 0x100
     * will clear bit 8 iff we want to return 0, and leave it set iff
     * we want to return 1, so then we can just shift down. */
    return (0x100 - val) >> 8;
}

int match_ssh_id(int stringlen, const void *string, const char *id)
{
    size_t idlen = strlen(id);
    return (idlen == stringlen && !memcmp(string, id, idlen));
}

void *get_ssh_string(int *datalen, const void **data, int *stringlen)
{
    void *ret;
    unsigned int len;

    if (*datalen < 4)
        return NULL;
    len = GET_32BIT_MSB_FIRST((const unsigned char *)*data);
    if (*datalen - 4 < len)
        return NULL;
    ret = (void *)((const char *)*data + 4);
    *datalen -= len + 4;
    *data = (const char *)*data + len + 4;
    *stringlen = len;
    return ret;
}

int get_ssh_uint32(int *datalen, const void **data, unsigned *ret)
{
    if (*datalen < 4)
        return FALSE;
    *ret = GET_32BIT_MSB_FIRST((const unsigned char *)*data);
    *datalen -= 4;
    *data = (const char *)*data + 4;
    return TRUE;
}

int strstartswith(const char *s, const char *t)
{
    return !memcmp(s, t, strlen(t));
}

int strendswith(const char *s, const char *t)
{
    size_t slen = strlen(s), tlen = strlen(t);
    return slen >= tlen && !strcmp(s + (slen - tlen), t);
}

#define BUILDINFO_PLATFORM "Windows"

#define str(s)			#s
#define xstr(s)			str(s)

char *buildinfo(const char *newline)
{
    strbuf *buf = strbuf_new();

    strbuf_catf(buf, "Build platform: %d-bit %s",
                (int)(CHAR_BIT * sizeof(void *)),
                BUILDINFO_PLATFORM);

#ifdef __clang_version__
    strbuf_catf(buf, "%sCompiler: clang %s", newline, __clang_version__);
#elif defined __GNUC__ && defined __VERSION__
    strbuf_catf(buf, "%sCompiler: gcc %s", newline, __VERSION__);
#elif defined _MSC_VER
    strbuf_catf(buf, "%sCompiler: Visual Studio", newline);
#if _MSC_VER == 1913
    strbuf_catf(buf, " 2017 / MSVC++ 15.6");
    strbuf_catf(buf, " (%s)",
		(_MSC_FULL_VER == 191326128) ? "15.6" :
		"_MSC_FULL_VER=" xstr( _MSC_FULL_VER));
#elif (_MSC_VER == 1911) || (_MSC_VER == 1912)
    strbuf_catf(buf, " 2017 / MSVC++ 15.0");
    strbuf_catf(buf, " (%s)",
		(_MSC_FULL_VER == 191225835) ? "15.5.5" :
		"_MSC_FULL_VER=" xstr( _MSC_FULL_VER));
#elif _MSC_VER == 1900
    strbuf_catf(buf, " 2015 / MSVC++ 14.0");
    strbuf_catf(buf, " (%s)",
		(_MSC_FULL_VER == 190024215) ? "Update 3(KB3165756)" :
		"(" xstr( _MSC_FULL_VER) ")" );
#elif _MSC_VER == 1800
    strbuf_catf(buf, " 2013 / MSVC++ 12.0");
#elif _MSC_VER == 1700
    strbuf_catf(buf, " 2012 / MSVC++ 11.0");
#elif _MSC_VER == 1600
    strbuf_catf(buf, " 2010 / MSVC++ 10.0");
#elif  _MSC_VER == 1500
    strbuf_catf(buf, " 2008 / MSVC++ 9.0");
#elif  _MSC_VER == 1400
    strbuf_catf(buf, " 2005 / MSVC++ 8.0");
#elif  _MSC_VER == 1310
    strbuf_catf(buf, " 2003 / MSVC++ 7.1");
#else
    strbuf_catf(buf, ", unrecognised version(_MSC_VER=%d)", (int)_MSC_VER);
#endif
#endif	// _MSC_VER

#ifdef BUILDINFO_GTK
    {
        char *gtk_buildinfo = buildinfo_gtk_version();
        if (gtk_buildinfo) {
            strbuf_catf(buf, "%sCompiled against GTK version %s",
                        newline, gtk_buildinfo);
            sfree(gtk_buildinfo);
        }
    }
#endif

#ifdef NO_SECURITY
    strbuf_catf(buf, "%sBuild option: NO_SECURITY", newline);
#endif
#ifdef NO_SECUREZEROMEMORY
    strbuf_catf(buf, "%sBuild option: NO_SECUREZEROMEMORY", newline);
#endif
#ifdef NO_IPV6
    strbuf_catf(buf, "%sBuild option: NO_IPV6", newline);
#endif
#ifdef NO_GSSAPI
    strbuf_catf(buf, "%sBuild option: NO_GSSAPI", newline);
#endif
#ifdef STATIC_GSSAPI
    strbuf_catf(buf, "%sBuild option: STATIC_GSSAPI", newline);
#endif
#ifdef UNPROTECT
    strbuf_catf(buf, "%sBuild option: UNPROTECT", newline);
#endif
#ifdef FUZZING
    strbuf_catf(buf, "%sBuild option: FUZZING", newline);
#endif
#ifdef DEBUG
    strbuf_catf(buf, "%sBuild option: DEBUG", newline);
#endif

    strbuf_catf(buf, "%sSource commit: %s", newline, commitid);

    return strbuf_to_str(buf);
}

Filename *filename_from_wstr(const wchar_t *str)
{
    Filename *ret = snew(Filename);
    ret->path = _wcsdup(str);
    return ret;
}

Filename *filename_from_str(const char *str)
{
    std::wstring wstr = acp_to_wc(str);
    Filename *ret = filename_from_wstr(wstr.c_str());
    return ret;
}

#if 0
Filename *filename_copy(const Filename *fn)
{
    return filename_from_str(fn->path);
}
#endif

const char *filename_to_str(const Filename *fn)
{
    return dup_wc_to_mb(fn->path);
}

#if 0
int filename_equal(const Filename *f1, const Filename *f2)
{
    return !strcmp(f1->path, f2->path);
}

int filename_is_null(const Filename *fn)
{
    return !*fn->path;
}
#endif

#if 0
Filename filename_from_str(const char *str)
{
    Filename ret;
    strncpy(ret.path, str, sizeof(ret.path));
    ret.path[sizeof(ret.path)-1] = '\0';
    return ret;
}
#endif

#if 0
const char *filename_to_str(const Filename *fn)
{
    return fn->path;
}
#endif

#if 0
int filename_equal(Filename f1, Filename f2)
{
    return !strcmp(f1.path, f2.path);
}

int filename_is_null(Filename fn)
{
    return !*fn.path;
}
#endif

void filename_free(Filename *fn)
{
    sfree(fn->path);
    sfree(fn);
}

void logevent(void *f, const char *msg)
{
    (void)f;
    (void)msg;
}
