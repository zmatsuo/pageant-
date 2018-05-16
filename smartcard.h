/**
*   smartcard.h
*
*   Copyright (c) 2018 zmatsuo
*
*   This software is released under the MIT License.
*   http://opensource.org/licenses/mit-license.php
*/

#include <string>
#include "ckey.h"

void SmartcardInit();
bool SmartcardIsPath(const char *utf8_path);
bool SmartcardLoad(const char *utf8_path, ckey &key);
#if defined(_WINDOWS_)	// windows.h included
std::string SmartcardSelectCAPI(HWND hWndOwner);
std::string SmartcardSelectPKCS(HWND hWndOwner);
#endif
void SmartcardForgetPin();
void SmartcardUnloadPKCS11dll();

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
