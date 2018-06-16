/*
 * winutils.c: miscellaneous Windows utilities for GUI apps
 */
#include <stdio.h>
#include <stdarg.h>
#include <windows.h>

#include "winutils.h"
#include "misc.h"
#include "puttymem.h"

/*
 * Print a modal (Really Bad) message box and perform a fatal exit.
 */
void modalfatalbox(const char *fmt, ...)
{
    va_list ap;
    char *buf;

    va_start(ap, fmt);
    buf = dupvprintf(fmt, ap);
    va_end(ap);
    HWND hwnd = NULL;
    MessageBoxA(hwnd, buf, "Pageant Fatal Error",
		MB_SYSTEMMODAL | MB_ICONERROR | MB_OK);
    sfree(buf);
    exit(1);
}
