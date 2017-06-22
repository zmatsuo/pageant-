

/*
 * Help file stuff in winhelp.c.
 */
#include <windows.h>	// for HWND

#ifdef __cplusplus
extern "C" {
#endif

void init_help(void);
void shutdown_help(void);
int has_help(void);
void launch_help(HWND hwnd, const char *topic);
void launch_help_id(HWND hwnd, DWORD id);
void quit_help(HWND hwnd);

#include "winhelp.h"

#ifdef __cplusplus
}
#endif

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 8
// End:
