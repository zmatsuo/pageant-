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

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_DEBUG)
#define DEBUG
#endif

#ifdef DEBUG

void debug_printf(const char *fmt, ...);
void debug_memdump(const void *buf, int len, int L);

#define debug(...) 			debug_printf(__VA_ARGS__)
#define dmemdump(buf,len)	debug_memdump(buf, len, 0);
#define dmemdumpl(buf,len)	debug_memdump(buf, len, 1);
#else

#define debug_printf(...)
#define debug(...)
#define dmemdump(buf,len)
#define dmemdumpl(buf,len)
#endif

void dputs(const char *buf);

void debug_console_show(int show);

#ifdef __cplusplus
}
#endif

// Local Variables:
// mode: c++
// coding: utf-8
// tab-width: 4
// End:

