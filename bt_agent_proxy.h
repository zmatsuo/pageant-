
#pragma once

#include <stdint.h>
#include <vector>

class bt_agent_proxy_t {
public:
	class bt_agent_proxy_impl_t *impl_;
};

typedef enum {
	BTA_NOTIFY_CONNECT,
	BTA_NOTIFY_CONNECT_FAIL,
	BTA_NOTIFY_RECV,
	BTA_NOTIFY_SEND,			// 送信完了
	BTA_NOTIFY_DISCONNECT,		// disconnect from client
	BTA_NOTIFY_CLOSE,			// cleanup
	BTA_NOTIFY_TIMEOUT,
	BTA_NOTIFY_DISCONNECT_COMPLATE,		// 自分で切断完了
	BTA_NOTIFY_DEVICEINFO_UPDATE
} bta_notify_t;

typedef struct bta_notify_param_st {
	bta_notify_t type;
	union {
		struct {
			const wchar_t *name;
			const wchar_t *addr_str;
			bool result;
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
	uint64_t deviceAddr;	// BTH_ADDR=ULONGLONG=uint64_t
	std::wstring deviceAddrStr;
	bool connected;
	bool handled;
} DeviceInfoType;

bt_agent_proxy_t *bta_init(const bta_init_t *init_info);
void bta_exit(bt_agent_proxy_t *hBta);

bool bta_connect(bt_agent_proxy_t *hBta, const uint64_t *deviceAddr);
bool bta_connect_request_cancel(bt_agent_proxy_t *hBta);
bool bta_disconnect(bt_agent_proxy_t *hBta);

bool bta_send(bt_agent_proxy_t *hBta, const uint8_t *data, size_t len);
void bta_deviceinfo(bt_agent_proxy_t *hBta, std::vector<DeviceInfoType> &deivceInfo);

class bta_deviceinfo_listener {
public:
	virtual void update(const std::vector<DeviceInfoType> &deivceInfos) = 0;
};

void bta_regist_deviceinfo_listener(bt_agent_proxy_t *hBta, bta_deviceinfo_listener *listener);
void bta_unregist_deviceinfo_listener(bt_agent_proxy_t *hBta, bta_deviceinfo_listener *listener);


// todo:妥当なヘッダへ!
bt_agent_proxy_t *bt_agent_proxy_main_get_handle();

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
