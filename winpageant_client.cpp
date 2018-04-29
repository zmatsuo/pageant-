
#include "winpageant_client.h"

bool agent_exists(void)
{
    HWND hwnd = ::FindWindow("Pageant", "Pageant");
    if (!hwnd)
		return true;
    else
		return false;
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
