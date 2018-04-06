// rdp_ssh_relay.h

#pragma once

#include <stdint.h>
#include <vector>

bool rdpSshRelayInit();

// server
bool rdpSshRelayCheckServer();
bool rdpSshRelayServerStart();
void rdpSshRelayServerStop();
bool rdpSshRelaySendReceive(
	const std::vector<uint8_t> &send,
	std::vector<uint8_t> &receive);
bool rdpSshRelayIsRemoteSession();
std::wstring rdpSshRelayGetClientName();

// client
bool rdpSshRelayCheckClient();
bool rdpSshRelayCheckClientRegistry(std::wstring &dll);
bool rdpSshRelayCheckClientDll(std::wstring &dll);
bool rdpSshRelaySetupClient();
void rdpSshRelayTeardownClient();


// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
