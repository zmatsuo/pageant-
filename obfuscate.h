/**
   obfuscate.h

   Copyright (c) 2017 zmatsuo

   This software is released under the MIT License.
   http://opensource.org/licenses/mit-license.php
*/

#pragma once

size_t decrypto(const char* encrypted, char* buffer);
size_t encrypto(char* original, char* buffer);

#ifdef __cplusplus
#include "sstring.h"
sstring decrypto(const sstring &s);
sstring encrypto(const sstring &s);
#endif
	
// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
