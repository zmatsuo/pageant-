
#ifndef _WINUTILS_H_
#define _WINUTILS_H_

#include <windows.h>	// LPCTSTR, DWORD
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

//typedef struct filereq_tag filereq; /* cwd for file requester */
//BOOL request_file(filereq *state, OPENFILENAME *of, int preserve, int save);
//filereq *filereq_new(void);
//void filereq_free(filereq *state);
void split_into_argv(char *, int *, char ***, char ***);

__declspec(noreturn)
void modalfatalbox(const char *fmt, ...);

#include "gui_stuff.h"
    
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	//  _WINUTILS_H_

// Local Variables:
// mode: c++
// coding: utf-8
// tab-width: 8
// End:
