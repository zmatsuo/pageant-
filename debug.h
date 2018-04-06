/*
 * Debugging functions.
 *
 * Output goes to debug.log
 *
 * debug(()) (note the double brackets) is like printf().
 *
 * dmemdump() and dmemdumpl() both do memory dumps.  The difference
 * is that dmemdumpl() is more suited for when the memory address is
 * important (say because you'll be recording pointer values later
 * on).  dmemdump() is more concise.
 */
#pragma once

#include <stddef.h>		// for size_t
#include <stdarg.h>		// for va_list

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*debugOutputFunc)(const char *s);

void debug_printf(const char *fmt, ...);
void debug_vprintf(const char *fmt, va_list ap);
void debug_memdump(const void *buf, size_t len, int L);
void debug_fl_printf(const char *fn, int line, const char *fmt, ...);
void debug_fl_vprintf(const char *fn, int line, const char *fmt, const va_list ap);
void debug_add_output(debugOutputFunc func);
void debug_remove_output(debugOutputFunc func);
void debug_set_thread_name(const char *name);

#ifdef __cplusplus
}
#endif


#if !defined(_DEBUG) && defined(ENABLE_DEBUG_PRINT)
#pragma message("release build but ENABLE_DEBUG_PRINT defined")
#undef ENABLE_DEBUG_PRINT
#endif

static inline void dbgflprintf_(const char *fn, int line, _Printf_format_string_ const char *fmt, ...) {
#if defined(ENABLE_DEBUG_PRINT)
    va_list ap;

    va_start(ap, fmt);
    debug_fl_vprintf(fn, line, fmt, ap);
    va_end(ap);
#else
	(void)fn; (void)line; (void)fmt;
#endif
}

#if defined(ENABLE_DEBUG_PRINT)
#define dbgprintf(fmt, ...) 	dbgflprintf_(__FILE__, __LINE__, fmt, __VA_ARGS__)
#else
#define dbgprintf(fmt, ...) 	(void)0
#endif

// wrapper
#if defined(ENABLE_DEBUG_PRINT)
#define dbgvprintf(fmt, ap) debug_vprintf(fmt, ap)
#define dmemdump(buf,len)	debug_memdump(buf, len, 0);
#define dmemdumpl(buf,len)	debug_memdump(buf, len, 1);
#else
#define dbgvprintf(fmt, ap)
#define dmemdump(buf,len)
#define dmemdumpl(buf,len)
#endif


// Local Variables:
// mode: c++
// coding: utf-8
// tab-width: 4
// End:

