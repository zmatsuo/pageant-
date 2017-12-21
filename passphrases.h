/**
   passphrases.h

   Copyright (c) 2017 zmatsuo

   This software is released under the MIT License.
   http://opensource.org/licenses/mit-license.php
*/

#ifdef __cplusplus
#include <vector>
#include "sstring.h"

void passphrase_init();
void passphrase_exit();
std::vector<sstring> passphrase_get_array();
void passphrase_add(const char *passphrase);
void passphrase_forget();
void passphrase_load_setting();
void passphrase_save_setting(const char* passphrase);
void passphrase_remove_setting();

#endif

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
