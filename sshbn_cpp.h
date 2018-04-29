
#pragma once

#include <string>

#ifndef BIGNUM_INTERNAL
typedef void *Bignum;
#endif

std::string bignum_tostr(Bignum md);
