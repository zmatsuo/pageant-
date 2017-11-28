#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>
#include <ctype.h>
#include <assert.h>
#include <windows.h>
#include <crtdbg.h>

#include "pageant+.h"
#include "debug.h"

void debug_console_puts(const char *buf);

#if 1 || defined(DEBUG)
void debug_printf(const char *fmt, ...)
{
#if defined(DEVELOP_VERSION)
    char buf[256];
    va_list ap;

    va_start(ap, fmt);
//    buf = dupvprintf(fmt, ap);
    vsprintf(buf, fmt, ap);

    FILE *fp = fopen("debug.log", "a+");
    if (fp != NULL) {
	fputs(buf, fp);	// \n is not output
	fflush(fp);
	fclose(fp);
    }

//    printf(buf);
    debug_console_puts(buf);
//    dputs(buf);
//    sfree(buf);
    va_end(ap);
#else
    (void)fmt;
#endif	// defined(DEVELOP_VERSION)
}


void debug_memdump(const void *buf, int len, int L)
{
#if defined(DEVELOP_VERSION)
    int i;
    const unsigned char *p = buf;
    char foo[17];
    if (L) {
	int delta;
	debug_printf("\t%d (0x%x) bytes:\n", len, len);
	delta = (int)(15 & (uintptr_t) p);
	p -= delta;
	len += delta;
    }
    for (; 0 < len; p += 16, len -= 16) {
	debug_printf("  ");
	if (L)
	    debug_printf("%p: ", p);
	strcpy(foo, "................");	/* sixteen dots */
	for (i = 0; i < 16 && i < len; ++i) {
	    if (&p[i] < (unsigned char *) buf) {
		debug_printf("   ");	       /* 3 spaces */
		foo[i] = ' ';
	    } else {
		debug_printf("%c%02.2x",
			&p[i] != (unsigned char *) buf
			&& i % 4 ? '.' : ' ', p[i]
		    );
		if (p[i] >= ' ' && p[i] <= '~')
		    foo[i] = (char) p[i];
	    }
	}
	foo[i] = '\0';
	debug_printf("%*s%s\n", (16 - i) * 3 + 2, "", foo);
    }
#else
    (void)buf;
    (void)len;
    (void)L;
#endif	// defined(DEVELOP_VERSION)
}

#endif				/* def DEBUG */

