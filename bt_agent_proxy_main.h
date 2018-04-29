
#pragma once

#include <stdlib.h>
#include <string>
#include <vector>

bool bt_agent_proxy_main_init(int timeout);
void bt_agent_proxy_main_exit();
bool bt_agent_proxy_main_handle_msg(
    const std::vector<uint8_t> &request,
    std::vector<uint8_t> &response);
void *bt_agent_proxy_main_handle_msg(const void *msgv, size_t *replylen);
//bool bt_agent_proxy_main_check_connect();
bool bt_agent_proxy_main_connect(const wchar_t *target_device);
//bool bt_agent_proxy_main_connect(const DeviceInfoType &deviceInfo);
bool bt_agent_proxy_main_disconnect(const wchar_t *target_device);

#include "ckey.h"

bool bt_agent_proxy_main_get_key(
    const wchar_t *target_device,
    std::vector<ckey> &keys);
bool bt_agent_proxy_main_add_key(
    const std::vector<std::string> &fnames);

#include "bt_agent_proxy.h"
bool bt_agent_proxy_main_connect(const DeviceInfoType &deviceInfo);
bool bt_agent_proxy_main_disconnect(const DeviceInfoType &deviceInfo);

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
