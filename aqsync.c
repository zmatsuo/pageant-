/*
 * aqsync.c: the agent_query_synchronous() wrapper function.
 *
 * This is a very small thing to have to put in its own module, but it
 * wants to be shared between back ends, and exist in any SSH client
 * program and also Pageant, and _nowhere else_ (because it pulls in
 * the main agent_query).
 */

#include <assert.h>
#include <stdlib.h>

#include "pageant_.h"

// TODO よく調べる
#if 0
void agent_query_synchronous(void *in, size_t inlen, void **out, size_t *outlen)
{
	agent_pending_query *pending;

	int outlen_i = *outlen;
	pending = agent_query(in, inlen, out, &outlen_i, NULL, 0);
	assert(!pending);
	*outlen = outlen_i;
}
#endif

// Local Variables:
// coding: utf-8-with-signature
// tab-width: 4
// End:
