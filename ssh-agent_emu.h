/**
   ssh-agent_emu.h

   Copyright (c) 2017 zmatsuo

   This software is released under the MIT License.
   http://opensource.org/licenses/mit-license.php
*/

bool ssh_agent_cygwin_unixdomain_start(const wchar_t *sock_path);
void ssh_agent_cygwin_unixdomain_stop();

bool ssh_agent_localhost_start(int port_no);
void ssh_agent_localhost_stop();


// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
