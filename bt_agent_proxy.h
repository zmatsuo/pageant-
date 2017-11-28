
#pragma once

#include <stdint.h>
#include <winsock2.h>
#include <ws2bth.h>

#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bt_agent_proxy_tag {
	int dummy_;
	void *impl_;
} bt_agent_proxy_t;

typedef enum {
	BTA_NOTIFY_CONNECT,
	BTA_NOTIFY_CONNECT_FAIL,
	BTA_NOTIFY_RECV,
	BTA_NOTIFY_SEND,			// 送信完了
	BTA_NOTIFY_DISCONNECT,		// disconnect from client
	BTA_NOTIFY_CLOSE,			// cleanup
	BTA_NOTIFY_TIMEOUT,
	BTA_NOTIFY_DEVICEINFO_UPDATE
} bta_notify_t;

typedef struct bta_notify_param_st {
	bta_notify_t type;
#if defined(_MSC_VER) || defined(__MINGW32__)
	SOCKET sock;
#else
	int sock;
#endif
	union {
		struct {
			const wchar_t *name;
			const wchar_t *addr_str;
		} connect;
		struct {
			uint8_t *ptr;			// @param[in,out] receive buf / next receive buf
			size_t size;
		} recv;
	} u;
	////
	uint64_t timeout;
} bta_notify_param_t;


typedef struct bta_init_st {
	int size;					// sizeof(bta_init_t)
	int (*notify_func)(
		bt_agent_proxy_t *hBta,
		bta_notify_param_t *notify);
	uint8_t *recv_ptr;			// first receive buf
	size_t recv_size;
	int timeout;				// ms (仮)
} bta_init_t;

typedef struct {
	std::wstring deviceName;
	BTH_ADDR deviceAddr;
	std::wstring deviceAddrStr;
	bool connected;
} DeviceInfoType;

bt_agent_proxy_t *bta_init(const bta_init_t *init_info);
bool bta_connect(bt_agent_proxy_t *hBta, const BTH_ADDR *deviceAddr);
bool bta_disconnect(bt_agent_proxy_t *hBta);
void bta_exit(bt_agent_proxy_t *hBta);
void bta_send(bt_agent_proxy_t *hBta, const uint8_t *data, size_t len);
void bta_deviceinfo(bt_agent_proxy_t *hBta, std::vector<DeviceInfoType> &deivceInfo);


#ifdef __cplusplus
}
#endif


// todo:妥当なヘッダへ!
#if defined(__cplusplus)
bt_agent_proxy_t *bt_agent_proxy_main_get_handle();
#endif


// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
