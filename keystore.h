/**
   keystore.h

   Copyright (c) 2018 zmatsuo

   This software is released under the MIT License.
   http://opensource.org/licenses/mit-license.php
*/

#pragma once

#include "ckey.h"
#include <vector>

void keystore_init();
void keystore_exit();
bool keystore_add(const ckey &key);
bool keystore_remove(const ckey &key);
bool keystore_remove(const char *fingerprint);
void keystore_remove_all();
bool keystore_get(const ckey &public_key, ckey &key);
bool keystore_get(const char *fingerprint, ckey &key);
bool keystore_get_from_blob(const std::vector<uint8_t> &blob, ckey &key);
bool keystore_exist(const ckey &key);

std::vector<ckey> keystore_get_keys();

// 
class KeystoreListener {
public:
    virtual void change() = 0;
};

void keystoreRegistListener(KeystoreListener *listener);
void keystoreUnRegistListener(KeystoreListener *listener);

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
