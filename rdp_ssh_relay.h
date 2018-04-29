// rdp_ssh_relay.h

#pragma once

#include <stdint.h>
#include <vector>

bool rdpSshRelayInit();

// server
bool rdpSshRelayCheckServer();
bool rdpSshRelaySetupServer();		// UAC
bool rdpSshRelayTeardownServer();	// UAC
bool rdpSshRelayServerStart();
void rdpSshRelayServerStop();
bool rdpSshRelayIsRemoteSession();
bool rdpSshRelayCheckCom();
std::wstring rdpSshRelayGetClientName();
bool rdpSshRelaySendReceive(
	const std::vector<uint8_t> &send,
	std::vector<uint8_t> &receive);

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
