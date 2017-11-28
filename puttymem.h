/*
 * PuTTY memory-handling header.
 */

#ifndef PUTTY_PUTTYMEM_H
#define PUTTY_PUTTYMEM_H

#include <windows.h>		/* for SecureZeroMemory */

#include <stddef.h>			/* for size_t */
#include <string.h>			/* for memcpy() */
#include <stdlib.h>			/* for malloc() famiy */
#include <crtdbg.h>
#define malloc(size)	_malloc_dbg(size,_NORMAL_BLOCK,__FILE__,__LINE__) 
//#define new ::new(_NORMAL_BLOCK, __FILE__, __LINE__)

#ifdef __cplusplus
extern "C" {
#endif

#if 0
// use safe malloc family
void *safemalloc(size_t, size_t);
void *saferealloc(void *, size_t, size_t);
void safefree(void *);
void safememclr(void *b, size_t len);
#endif

/* #define MALLOC_LOG  do this if you suspect putty of leaking memory */
#ifdef MALLOC_LOG
#define smalloc(z) (mlog(__FILE__,__LINE__), safemalloc(z,1))
#define snmalloc(z,s) (mlog(__FILE__,__LINE__), safemalloc(z,s))
#define srealloc(y,z) (mlog(__FILE__,__LINE__), saferealloc(y,z,1))
#define snrealloc(y,z,s) (mlog(__FILE__,__LINE__), saferealloc(y,z,s))
#define sfree(z) (mlog(__FILE__,__LINE__), safefree(z))
void mlog(char *, int);
#else
#if 0
#define smalloc(z) 		safemalloc(z,1)
#define snmalloc 		safemalloc
#define srealloc(y,z)	saferealloc(y,z,1)
#define snrealloc 		saferealloc
#define sfree 			safefree
#define	smemclr(y, z)	safememclr(y,z)
#endif
#define smalloc(size)				malloc(size)
#define snmalloc(size, count)		malloc(size*count)
#define srealloc(ptr, size, count)	realloc(ptr, size*count)
#define snrealloc(ptr, size, count)	realloc(ptr, size*count)
#define sfree(ptr)					free(ptr)
#define	smemclr(ptr, size)			SecureZeroMemory(ptr, size)
#endif

/*
 * Direct use of smalloc within the code should be avoided where
 * possible, in favour of these type-casting macros which ensure
 * you don't mistakenly allocate enough space for one sort of
 * structure and assign it to a different sort of pointer.
 */
#define snew(type) ((type *)snmalloc(1, sizeof(type)))
#define snewn(n, type) ((type *)snmalloc((n), sizeof(type)))
#define sresize(ptr, n, type) ((type *)snrealloc((ptr), (n), sizeof(type)))

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// coding: utf-8-with-signature
// tab-width: 4
// End:
