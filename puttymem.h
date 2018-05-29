/**
   puttymem.h
   PuTTY memory-handling header.

   Copyright (c) 2017 zmatsuo

   This software is released under the MIT License.
   http://opensource.org/licenses/mit-license.php
*/
#pragma once

#include <windows.h>		/* for SecureZeroMemory */

#include <stddef.h>			/* for size_t */
#include <string.h>			/* for memcpy() */
#include <stdlib.h>			/* for malloc() family */
#include <crtdbg.h>

#if defined(_DEBUG)
#define malloc(size)		_malloc_dbg(size,_NORMAL_BLOCK,__FILE__,__LINE__)
#define calloc(num, size)   _calloc_dbg(num, size, _NORMAL_BLOCK, __FILE__, __LINE__)
//#define free(ptr)			_free_dbg(ptr, _NORMAL_BLOCK);
#if defined(__cplusplus)
#define new ::new(_NORMAL_BLOCK, __FILE__, __LINE__)
#endif
#endif

#if defined(__cplusplus)
extern "C" {
#endif

//#define MALLOC_LOG

// safe malloc family
void *safemalloc(size_t, size_t);
void *saferealloc(void *, size_t, size_t);
void safefree(void *);
void safememclr(void *b, size_t len);

// use crt malloc family
#if 1
#define _smalloc(size)					malloc(size)
#define _snmalloc(size, count)			malloc(size*count)
#define _srealloc(ptr, size, count)		realloc(ptr, size*count)
#define _snrealloc(ptr, size, count)	realloc(ptr, size*count)
//#define _sfree(ptr)						free(ptr)
static inline void _sfree(void *ptr)		{free(ptr);}
#define	_smemclr(ptr, size)				SecureZeroMemory(ptr, size)
#endif

// use safe malloc family
#if 0
#define _smalloc(z)			safemalloc(z,1)
#define _snmalloc(z,s)		safemalloc(z,s)
#define _srealloc(y,z)		saferealloc(y,z,1)
#define _snrealloc(y,z,s)	saferealloc(y,z,s)
#define _sfree(z)			safefree(z)
#define	_smemclr(y, z)		safememclr(y,z)
#endif

void mlog(const char *, int);

#if defined(MALLOC_LOG)
#define smalloc(z)			(mlog(__FILE__,__LINE__), _smalloc(z))
#define snmalloc(z,s)		(mlog(__FILE__,__LINE__), _snmalloc(z,s))
#define srealloc(y,z)		(mlog(__FILE__,__LINE__), _srealloc(y,z,1))
#define snrealloc(y,z,s)	(mlog(__FILE__,__LINE__), _snrealloc(y,z,s))
#define sfree(z)			(mlog(__FILE__,__LINE__), _sfree(z))
#define	smemclr(y, z)		(mlog(__FILE__,__LINE__), _smemclr(y,z))
#else
#define smalloc(z)			_smalloc(z)
#define snmalloc(z,s)		_snmalloc(z,s)
#define srealloc(y,z)		_srealloc(y,z,1)
#define snrealloc(y,z,s)	_snrealloc(y,z,s)
//#define sfree(z)			_sfree(z)
static inline void sfree(void *ptr)		{_sfree(ptr);}
#define	smemclr(y, z)		_smemclr(y,z)
#endif

/*
 * Direct use of smalloc within the code should be avoided where
 * possible, in favour of these type-casting macros which ensure
 * you don't mistakenly allocate enough space for one sort of
 * structure and assign it to a different sort of pointer.
 */
#define snew(type)				((type *)snmalloc(1, sizeof(type)))
#define snewn(n, type)			((type *)snmalloc((n), sizeof(type)))
#define sresize(ptr, n, type)	((type *)snrealloc((ptr), (n), sizeof(type)))

#ifdef __cplusplus
}
#endif

// Local Variables:
// coding: utf-8-with-signature
// tab-width: 4
// End:
