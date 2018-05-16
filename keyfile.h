/**
 *   keyfile.h
 *
 *   Copyright (c) 2018 zmatsuo
 *
 *   This software is released under the MIT License.
 *   http://opensource.org/licenses/mit-license.php
 */

#pragma once

#include "ckey.h"

bool load_keyfile(const wchar_t *filename, ckey &key);

