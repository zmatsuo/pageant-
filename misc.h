/*
 * Header for misc.c.
 */

#ifndef PUTTY_MISC_H
#define PUTTY_MISC_H

#include <stdio.h>		       /* for FILE * */
#include <stdarg.h>		       /* for va_list */
#include <time.h>                      /* for struct tm */

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

#include "filename.h"
//typedef struct Filename Filename;
//typedef struct FontSpec FontSpec;

#ifdef __cplusplus
extern "C" {
#endif

unsigned long parse_blocksize(const char *bs);
char ctrlparse(char *s, char **next);

char *dupstr(const char *s);
char *dupcat(const char *s1, ...);
char *dupprintf(const char *fmt, ...)
#ifdef __GNUC__
    __attribute__ ((format (printf, 1, 2)))
#endif
    ;
char *dupvprintf(const char *fmt, va_list ap);
void burnstr(char *string);

int toint(unsigned);

char *fgetline(FILE *fp);
char *chomp(char *str);

// TODO: 2箇所?
void base64_encode_atom(const unsigned char *data, int n, char *out);
int base64_decode_atom(const char *atom, unsigned char *out);

struct bufchain_granule;
typedef struct bufchain_tag {
    struct bufchain_granule *head, *tail;
    int buffersize;		       /* current amount of buffered data */
} bufchain;

void bufchain_init(bufchain *ch);
void bufchain_clear(bufchain *ch);
int bufchain_size(bufchain *ch);
void bufchain_add(bufchain *ch, const void *data, int len);
void bufchain_prefix(bufchain *ch, void **data, int *len);
void bufchain_consume(bufchain *ch, int len);
void bufchain_fetch(bufchain *ch, void *data, int len);

struct tm ltime(void);

/* Compare two fixed-length chunks of memory for equality, without
 * data-dependent control flow (so an attacker with a very accurate
 * stopwatch can't try to guess where the first mismatching byte was).
 * Returns 0 for mismatch or 1 for equality (unlike memcmp), hinted at
 * by the 'eq' in the name. */
int smemeq(const void *av, const void *bv, size_t len);

/* Extracts an SSH-marshalled string from the start of *data. If
 * successful (*datalen is not too small), advances data/datalen past
 * the string and returns a pointer to the string itself and its
 * length in *stringlen. Otherwise does nothing and returns NULL.
 *
 * Like strchr, this function can discard const from its parameter.
 * Treat it as if it was a family of two functions, one returning a
 * non-const string given a non-const pointer, and one taking and
 * returning const. */
void *get_ssh_string(int *datalen, const void **data, int *stringlen);
/* Extracts an SSH uint32, similarly. Returns TRUE on success, and
 * leaves the extracted value in *ret. */
int get_ssh_uint32(int *datalen, const void **data, unsigned *ret);
/* Given a not-necessarily-zero-terminated string in (length,data)
 * form, check if it equals an ordinary C zero-terminated string. */
int match_ssh_id(int stringlen, const void *string, const char *id);

char *buildinfo(const char *newline);

#include "debug.h"

#ifndef lenof
#define lenof(x) ( (sizeof((x))) / (sizeof(*(x))))
#endif

#ifndef min
#define min(x,y) ( (x) < (y) ? (x) : (y) )
#endif
#ifndef max
#define max(x,y) ( (x) > (y) ? (x) : (y) )
#endif

#define GET_32BIT_LSB_FIRST(cp) \
  (((unsigned long)(unsigned char)(cp)[0]) | \
  ((unsigned long)(unsigned char)(cp)[1] << 8) | \
  ((unsigned long)(unsigned char)(cp)[2] << 16) | \
  ((unsigned long)(unsigned char)(cp)[3] << 24))

#define PUT_32BIT_LSB_FIRST(cp, value) ( \
  (cp)[0] = (unsigned char)(value), \
  (cp)[1] = (unsigned char)((value) >> 8), \
  (cp)[2] = (unsigned char)((value) >> 16), \
  (cp)[3] = (unsigned char)((value) >> 24) )

#define GET_16BIT_LSB_FIRST(cp) \
  (((unsigned long)(unsigned char)(cp)[0]) | \
  ((unsigned long)(unsigned char)(cp)[1] << 8))

#define PUT_16BIT_LSB_FIRST(cp, value) ( \
  (cp)[0] = (unsigned char)(value), \
  (cp)[1] = (unsigned char)((value) >> 8) )

#define GET_32BIT_MSB_FIRST(cp) \
  (((unsigned long)(unsigned char)(cp)[0] << 24) | \
  ((unsigned long)(unsigned char)(cp)[1] << 16) | \
  ((unsigned long)(unsigned char)(cp)[2] << 8) | \
  ((unsigned long)(unsigned char)(cp)[3]))

#define GET_32BIT(cp) GET_32BIT_MSB_FIRST(cp)

#define PUT_32BIT_MSB_FIRST(cp, value) ( \
  (cp)[0] = (unsigned char)(((value) >> 24) & 0xff),	\
  (cp)[1] = (unsigned char)(((value) >> 16) & 0xff),	\
  (cp)[2] = (unsigned char)(((value) >>  8) & 0xff),	\
  (cp)[3] = (unsigned char)(((value)      ) & 0xff) )

#define PUT_32BIT(cp, value) PUT_32BIT_MSB_FIRST(cp, value)

#define GET_16BIT_MSB_FIRST(cp) \
  (((unsigned long)(unsigned char)(cp)[0] << 8) | \
  ((unsigned long)(unsigned char)(cp)[1]))

#define PUT_16BIT_MSB_FIRST(cp, value) ( \
  (cp)[0] = (unsigned char)((value) >> 8), \
  (cp)[1] = (unsigned char)(value) )


#ifdef __cplusplus
}
#endif

#endif	// PUTTY_MISC_H

// Local Variables:
// coding: utf-8-with-signature
// End:
