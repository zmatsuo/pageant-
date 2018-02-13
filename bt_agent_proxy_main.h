
#pragma once

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void bt_agent_proxy_main_init(int timeout);
void bt_agent_proxy_main_exit();
void *bt_agent_proxy_main_handle_msg(const void *msgv, size_t *replylen);
//bool bt_agent_proxy_main_check_connect();
bool bt_agent_proxy_main_connect(const wchar_t *target_device);
//bool bt_agent_proxy_main_connect(const DeviceInfoType &deviceInfo);
bool bt_agent_proxy_main_disconnect(const wchar_t *target_device);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#ifdef __cplusplus

#include <string>
#include <vector>

#include "ckey.h"

bool bt_agent_proxy_main_get_key(
    const wchar_t *target_device,
    std::vector<ckey> &keys);
bool bt_agent_proxy_main_add_key(
    const std::vector<std::string> &fnames);

#endif

#include "bt_agent_proxy.h"
bool bt_agent_proxy_main_connect(const DeviceInfoType &deviceInfo);
bool bt_agent_proxy_main_disconnect(const DeviceInfoType &deviceInfo);
