#define DEBUG	// for diagbn()
#include "sshbn.h"

//#define BIGNUM_INTERNAL
//typedef BignumInt *Bignum;

#include "sshbn_cpp.h"
extern "C" {
#include "sshbn_export.h"
}

#include <sstream>

std::string bignum_tostr(Bignum _md)
{
    if (_md == nullptr) {
		return "null";
    }

    BignumInt *md = (BignumInt *)_md;
    std::ostringstream oss;
    int i, nibbles, morenibbles;
    static const char hex[] = "0123456789ABCDEF";

    oss << "0x";

    nibbles = (3 + bignum_bitcount(md)) / 4;
    if (nibbles < 1)
		nibbles = 1;
    morenibbles = 4 * md[0] - nibbles;
    for (i = 0; i < morenibbles; i++)
		oss << "-";
    for (i = nibbles; i--;)
		oss << 
			hex[(bignum_byte(md, i / 2) >> (4 * (i % 2))) & 0xF];

    return oss.str();
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
