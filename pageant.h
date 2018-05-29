/*
 * pageant.h: header for pageant.c.
 */

#pragma once

#include "ckey.h"

#include <stdint.h>
#include <vector>

/*
 * FIXME: it would be nice not to have this arbitrary limit. It's
 * currently needed because the Windows Pageant IPC system needs an
 * upper bound known to the client, but it's also reused as a basic
 * sanity check on incoming messages' length fields.
 */
#define AGENT_MAX_MSGLEN  8192

typedef void (*pageant_logfn_t)(void *logctx, const char *fmt, va_list ap);

/*
 * Initial setup.
 */
void pageant_init(void);
void pageant_exit(void);

void pageant_handle_msg(
	const uint8_t *request_ptr, size_t request_len,
	std::vector<uint8_t> &reply);
typedef bool (*agent_query_synchronous_fn)(
	const std::vector<uint8_t> &request,
	std::vector<uint8_t> &response);
int pageant_get_keylist(
	agent_query_synchronous_fn query_func,
	std::vector<ckey> &keys);

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
