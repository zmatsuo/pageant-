
#include <winsock2.h>
#include <ws2bth.h>

#include <stdint.h>

#include <vector>
#include <thread>
#include <chrono>
#include <regex>
#include <sstream>
#include <qwidget.h>

#include <crtdbg.h>

#if defined(_DEBUG)
#define malloc(size)	_malloc_dbg(size,_NORMAL_BLOCK,__FILE__,__LINE__) 
#if defined(__cplusplus)
#define new ::new(_NORMAL_BLOCK, __FILE__, __LINE__)
#endif
#endif

//#define ENABLE_DEBUG_PRINT
#include "debug.h"
#include "codeconvert.h"
#include "gui_stuff.h"
#include "pageant.h"
#include "keystore.h"
#include "winutils_qt.h"

#include "bt_agent_proxy.h"

#include "bt_agent_proxy_main.h"

static bt_agent_proxy_t *hBta_;
//static int timeoutMs;
static std::chrono::milliseconds timeoutMs;
static std::vector<uint8_t> receive_buf(7*1024);
static size_t receive_size;
static bool receive_event;
static bool connect_finish_flag;
static bool connect_result_flag;
static bool disconnect_finish_flag;

static int notify_func(
    bt_agent_proxy_t *hBta,
    bta_notify_param_t *notify)
{
	(void)hBta;
    switch(notify->type)
    {
    case BTA_NOTIFY_CONNECT:
		dbgprintf("connect %S %s\n",
				  notify->u.connect.name,
				  notify->u.connect.result ? "ok" : "fail"
			);
		connect_finish_flag = true;
		connect_result_flag = notify->u.connect.result;
//	notify->send_data = (uint8_t *)"data";
//	notify->send_len = 4;
		break;
    case BTA_NOTIFY_RECV:
    {
		dbgprintf("受信 %zd\n", receive_size);
		receive_size = notify->u.recv.size;
		receive_event = true;
		notify->u.recv.ptr = &receive_buf[0];
		notify->u.recv.size = receive_buf.size();
		break;
    }
    case BTA_NOTIFY_SEND:
		dbgprintf("送信完了\n");
		break;
	case BTA_NOTIFY_DISCONNECT_COMPLATE:
		disconnect_finish_flag = true;
		break;
    default:
		break;
    }
    return 0;
}

bool bt_agent_proxy_main_init(int timeout)
{
    bta_init_t init_info = {
		/*.size =*/	sizeof(bta_init_t),
    };
    init_info.notify_func = notify_func;
    init_info.recv_ptr = &receive_buf[0];
    init_info.recv_size = receive_buf.size();
    hBta_ = bta_init(&init_info);
	if (hBta_ == nullptr) {
		return false;
	}

	timeoutMs = (std::chrono::milliseconds)timeout;
	connect_finish_flag = false;
	return true;
}

void bt_agent_proxy_main_exit()
{
	if (hBta_ != nullptr) {
		bta_exit(hBta_);
		hBta_ = nullptr;
	}
}

static bool bt_agent_proxy_main_send(const uint8_t *data, size_t len)
{
    return bta_send(hBta_, data, len);
}

bool bt_agent_proxy_main_handle_msg(
	const std::vector<uint8_t> &request,
	std::vector<uint8_t> &response)
{
    receive_event = false;
    bool r = bt_agent_proxy_main_send(&request[0], request.size());
	if (!r) {
		response.clear();
		return false;
	}
	auto start = std::chrono::system_clock::now();
    while(!receive_event) {
		std::this_thread::sleep_for(std::chrono::microseconds(1));
		auto now = std::chrono::system_clock::now();
		auto elapse =
			std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
		if (elapse >= timeoutMs) {
			break;
		}
    }
	if (receive_event) {
		response.resize(receive_size);
		memcpy(&response[0], &receive_buf[0], receive_size);
		return true;
	} else {
		response.clear();
		return false;
	}
}

bt_agent_proxy_t *bt_agent_proxy_main_get_handle()
{
	return hBta_;
}

static bool bt_agent_proxy_main_check_connect()
{
	bool f = connect_finish_flag;
	connect_finish_flag = false;
	return f;
}

//////////////////////////////////////////////////////////////////////////////

static void get_device_info(
	bt_agent_proxy_t *hBta,
	std::vector<DeviceInfoType> &deviceInfos)
{
	bta_deviceinfo(hBta, deviceInfos);
	if (deviceInfos.size() == 0) {
		auto start = std::chrono::system_clock::now();
		while(1) {
			auto now = std::chrono::system_clock::now();
			auto elapse =
				std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
			if (elapse >= timeoutMs) {
				break;
			}
			std::this_thread::sleep_for(std::chrono::microseconds(1));

			bta_deviceinfo(hBta, deviceInfos);
			if (deviceInfos.size() != 0) {
				break;
			}
		}
	}
}
	
bool bt_agent_proxy_main_connect(const DeviceInfoType &deviceInfo)
{
	bt_agent_proxy_t *hBta = bt_agent_proxy_main_get_handle();
	if (hBta == nullptr) {
		dbgprintf("bt?\n");
		return false;
	}

	if (deviceInfo.connected) {
		// 接続済み
		dbgprintf("接続済みデバイス\n");
		return true;
	}

	// 接続する
	dbgprintf("bta_connect(%S(%S))\n",
		deviceInfo.deviceName.c_str(),
		deviceInfo.deviceAddrStr.c_str());
	BTH_ADDR deviceAddr = deviceInfo.deviceAddr;
	bool r = bta_connect(hBta, &deviceAddr);
	dbgprintf("result %d(%s)\n",
			  r, r == false ? "fail" : "ok");
	if (r == false) {
		dbgprintf("connect fail\n");
		return false;
	}

	// 接続待ち
	auto start = std::chrono::system_clock::now();
	while (1) {
		// TODO タイムアウト
		if (bt_agent_proxy_main_check_connect() == true) {
			break;
		}
		std::this_thread::sleep_for(std::chrono::microseconds(1));
		auto now = std::chrono::system_clock::now();
		auto elapsed =
			std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
		if (elapsed > timeoutMs) {
			bta_connect_request_cancel(hBta);
			connect_result_flag = false;
			dbgprintf("timeout\n");
			break;
		}
	}
	dbgprintf("connect %s\n", connect_result_flag ? "ok" : "ng");
	return connect_result_flag;
}

bool bt_agent_proxy_main_disconnect(const DeviceInfoType &deviceInfo)
{
	bt_agent_proxy_t *hBta = bt_agent_proxy_main_get_handle();
	if (hBta == nullptr) {
		dbgprintf("bt?\n");
		return false;
	}

	if (!deviceInfo.connected) {
		// 未接続
		return true;
	}

	// 切断する
	disconnect_finish_flag = false;
	bool r = bta_disconnect(hBta);
	if (r == false) {
		dbgprintf("切断失敗\n");
		return false;
	}

	// 切断待ち
	while (1) {
		// TODO つくる
		if (disconnect_finish_flag == true) {
			break;
		}
		std::this_thread::sleep_for(std::chrono::microseconds(1));
	}

	return true;
}

bool bt_agent_proxy_main_connect(const wchar_t *target_device)
{
	bt_agent_proxy_t *hBta = bt_agent_proxy_main_get_handle();
	if (hBta == nullptr) {
		dbgprintf("bt?\n");
		return false;
	}

	// デバイス一覧を取得
	std::vector<DeviceInfoType> deviceInfos;
	get_device_info(hBta, deviceInfos);
	if (deviceInfos.size() == 0) {
		// 見つからなかった
		return false;
	}
	
	DeviceInfoType deviceInfo;
	for (const auto &device : deviceInfos) {
		if (device.deviceName == target_device) {
			deviceInfo = device;
			break;
		}
	}
	if (deviceInfo.deviceName != target_device) {
		// 見つからなかった
		return false;
	}

	return bt_agent_proxy_main_connect(deviceInfo);
}

bool bt_agent_proxy_main_disconnect(const wchar_t *target_device)
{
	bt_agent_proxy_t *hBta = bt_agent_proxy_main_get_handle();
	if (hBta == nullptr) {
		dbgprintf("bt?\n");
		return false;
	}

	// デバイス一覧を取得
	std::vector<DeviceInfoType> deviceInfos;
	get_device_info(hBta, deviceInfos);
	if (deviceInfos.size() == 0) {
		// 見つからなかった
		return false;
	}
	
	DeviceInfoType deviceInfo;
	for (const auto &device : deviceInfos) {
		if (device.deviceName == target_device) {
			deviceInfo = device;
			break;
		}
	}
	if (deviceInfo.deviceName != target_device) {
		// 見つからなかった
		return false;
	}

	return bt_agent_proxy_main_disconnect(deviceInfo);
}

// 鍵取得
bool bt_agent_proxy_main_get_key(
	const wchar_t *target_device,
	std::vector<ckey> &keys)
{
	keys.clear();

	bool r = bt_agent_proxy_main_connect(target_device);
	if (!r) {
		std::wostringstream oss;
		oss << L"bluetooth 接続失敗\n"
			<< target_device;
		QWidget *w = getDispalyedWindow();
		message_box(w, oss.str().c_str(), L"pageant+", MB_OK, 0);
		return false;
	}

	//
	int err = pageant_get_keylist(bt_agent_proxy_main_handle_msg, keys);

	// ファイル名をセット TODO:ここでやる?
	std::string target_device_utf8 = wc_to_utf8(target_device);
	for(ckey &key : keys) {
		std::ostringstream oss;
		oss << "btspp://" << target_device_utf8 << "/" << key.fingerprint_sha1();
		key.set_fname(oss.str().c_str());
	}

	ckey::dump_keys(keys);

	return true;
}

// BTファイルに持っていく
bool bt_agent_proxy_main_add_key(
	const std::vector<std::string> &fnames)
{
	// デバイス一覧を取得
	std::vector<std::string> target_devices_utf8;
	{
		std::regex re(R"(btspp://(.+)/)");
		std::smatch match;
		for(auto key : fnames) {
			if (std::regex_search(key, match, re)) {
				target_devices_utf8.push_back(match[1]);
			}
		}
	}
	std::sort(target_devices_utf8.begin(), target_devices_utf8.end());
	target_devices_utf8.erase(
		std::unique(target_devices_utf8.begin(), target_devices_utf8.end()),
		target_devices_utf8.end());

	// デバイスごとに公開鍵を取得
	for (auto target_device_utf8: target_devices_utf8) {

		std::wstring target_device = utf8_to_wc(target_device_utf8);

		std::vector<ckey> keys;
		bool r = bt_agent_proxy_main_get_key(target_device.c_str(), keys);
		if (r == false) {
			continue;
		}

		for(auto fnamae : fnames) {
			for(auto key : keys) {
				if (key.key_comment() == fnamae) {
					if (!keystore_add(key)) {
						dbgprintf("key %s add err\n", key.fingerprint_sha256().c_str());
					}
				}
			}
		}
	}
	return true;
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
