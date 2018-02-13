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

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_DEBUG)
#define DEBUG
#endif

// wrapper
#if 0 || defined(DEBUG)

#define debug(...) 			debug_printf(__VA_ARGS__)
#define dmemdump(buf,len)	debug_memdump(buf, len, 0);
#define dmemdumpl(buf,len)	debug_memdump(buf, len, 1);
#else

#define debug(...)
#define dmemdump(buf,len)
#define dmemdumpl(buf,len)
#endif

// wrapper
#if defined(ENABLE_DEBUG_PRINT)
#define dbgprintf(...) 			debug_fl_printf(__FILE__, __LINE__, __VA_ARGS__)
//#define dbgprintf(...) 			debug_printf(__VA_ARGS__)
#define dbgvprintf(fmt, ap) 	debug_vprintf(fmt, ap)
#else
#define dbgprintf(...)
#define dbgvprintf(fmt, ap)
#endif


// 存在する関数
void debug_printf(const char *fmt, ...);
void debug_vprintf(const char *fmt, va_list ap);
void debug_memdump(const void *buf, int len, int L);
void dputs(const char *buf);
void debug_fl_printf(const char *fn, int line, const char *fmt, ...);

void debug_console_show(int show);


#ifdef __cplusplus
}
#endif

// Local Variables:
// mode: c++
// coding: utf-8
// tab-width: 4
// End:

