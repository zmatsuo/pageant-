/**
 *	ssh-agent_ms.cpp
 *
 *	Copyright (c) 2018 zmatsuo
 *
 *	This software is released under the MIT License.
 *	http://opensource.org/licenses/mit-license.php
 */

#pragma once

bool pipe_th_start(const wchar_t *sock_path);
void pipe_th_stop();

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
