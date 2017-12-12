
#include <stdint.h>
#include <vector>
#include <thread>
#include <chrono>
#include <regex>
#include <sstream>

#include <crtdbg.h>

#if defined(_DEBUG)
#define malloc(size)	_malloc_dbg(size,_NORMAL_BLOCK,__FILE__,__LINE__) 
#if defined(__cplusplus)
#define new ::new(_NORMAL_BLOCK, __FILE__, __LINE__)
#endif
#endif

#include "codeconvert.h"

#include "bt_agent_proxy.h"

#include "bt_agent_proxy_main.h"
#include "bt_agent_proxy_main2.h"


static bt_agent_proxy_t *hBta_;
static std::vector<uint8_t> receive_buf(7*1024);
static size_t receive_size;
static bool receive_event;
static bool connect_flag;

static int notify_func(
    bt_agent_proxy_t *hBta,
    bta_notify_param_t *notify)
{
	(void)hBta;
    switch(notify->type)
    {
    case BTA_NOTIFY_CONNECT:
		printf("connect %S!\n", notify->u.connect.name );
		connect_flag = true;
//	notify->send_data = (uint8_t *)"data";
//	notify->send_len = 4;
		break;
    case BTA_NOTIFY_RECV:
    {
		printf("受信 %zd\n", receive_size);
		receive_size = notify->u.recv.size;
		receive_event = true;
		notify->u.recv.ptr = &receive_buf[0];
		notify->u.recv.size = receive_buf.size();
		break;
    }
    case BTA_NOTIFY_SEND:
		printf("送信完了\n");
		break;
    default:
		break;
    }
    return 0;
}

void bt_agent_proxy_main_init()
{
    bta_init_t init_info = {
		/*.size =*/	sizeof(bta_init_t),
    };
    init_info.notify_func = notify_func;
    init_info.recv_ptr = &receive_buf[0];
    init_info.recv_size = receive_buf.size();
    hBta_ = bta_init(&init_info);

	connect_flag = false;
}

void bt_agent_proxy_main_exit()
{
	if (hBta_ != nullptr) {
		bta_exit(hBta_);
		hBta_ = nullptr;
	}
}

bool bt_agent_proxy_main_send(const uint8_t *data, size_t len)
{
    return bta_send(hBta_, data, len);
}

void *bt_agent_proxy_main_handle_msg(const void *msgv, size_t *replylen)
{
    receive_event = false;
    bool r = bt_agent_proxy_main_send((uint8_t *)msgv, (size_t)*replylen);
	if (!r) {
		*replylen = 0;
		return nullptr;
	}
    while(!receive_event) {
		Sleep(10);
    }
    *replylen = receive_size;
    void *p = malloc(receive_size);
    memcpy(p, &receive_buf[0], receive_size);
    return p;
}

bt_agent_proxy_t *bt_agent_proxy_main_get_handle()
{
	return hBta_;
}

bool bt_agent_proxy_main_check_connect()
{
	bool f = connect_flag;
	connect_flag = false;
	return f;
}

//////////////////////////////////////////////////////////////////////////////

bool bt_agent_proxy_main_connect(const DeviceInfoType &deviceInfo)
{
	bt_agent_proxy_t *hBta = bt_agent_proxy_main_get_handle();
	if (hBta == nullptr) {
		printf("bt?\n");
		return false;
	}

	// 接続する
	if (!deviceInfo.connected) {
		BTH_ADDR deviceAddr = deviceInfo.deviceAddr;
		bool r = bta_connect(hBta, &deviceAddr);
		printf("bta_connect() %d\n", r);
		if (r == false) {
			dbgprintf("connect fail\n");
			return false;
		} else {
			// 接続待ち
			while (1) {
				// TODO タイムアウト
				if (bt_agent_proxy_main_check_connect() == true) {
					break;
				}
				Sleep(1);
			}
		}
	}
	return true;
}

bool bt_agent_proxy_main_connect(const wchar_t *target_device)
{
	bt_agent_proxy_t *hBta = bt_agent_proxy_main_get_handle();
	if (hBta == nullptr) {
		printf("bt?\n");
		return false;
	}

	// デバイス一覧を取得
	std::vector<DeviceInfoType> deviceInfos;
	bta_deviceinfo(hBta, deviceInfos);
	if (deviceInfos.size() == 0) {
		auto start = std::chrono::system_clock::now();
		while(1) {
			auto now = std::chrono::system_clock::now();
			auto elapse =
				std::chrono::duration_cast<std::chrono::seconds>(
					now - start);
			if (elapse >= (std::chrono::seconds)10) {
				break;
			}
			std::this_thread::sleep_for(std::chrono::microseconds(10));

			bta_deviceinfo(hBta, deviceInfos);
			if (deviceInfos.size() != 0) {
				break;
			}
		}
		if (deviceInfos.size() == 0) {
			// 見つからなかった
			return false;
		}
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

#if 0
// TODO: BTファイルに持っていく
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
		message_boxW(oss.str().c_str(), L"pageant+", MB_OK, 0);
		return false;
	}

	// BT問い合わせする
	pagent_set_destination(bt_agent_query_synchronous_fn);
	int length;
	void *p = pageant_get_keylist2(&length);
	pagent_set_destination(nullptr);
	std::vector<uint8_t> from_bt((uint8_t *)p, ((uint8_t *)p) + length);
	sfree(p);
	p = &from_bt[0];


	debug_memdump(p, length, 1);

	std::string target_device_utf8 = wc_to_utf8(target_device);

	// 鍵を抽出
	const char *fail_reason = nullptr;
	r = parse_public_keys(p, length, keys, &fail_reason);
	if (r == false) {
		printf("err %s\n", fail_reason);
		return false;
	}

	// ファイル名をセット
	for(ckey &key : keys) {
		std::ostringstream oss;
		oss << "btspp://" << target_device_utf8 << "/" << key.fingerprint_sha1();
		key.set_fname(oss.str().c_str());
	}

	for(const ckey &key : keys) {
		printf("key\n");
		printf(" '%s'\n", key.fingerprint().c_str());
		printf(" md5 %s\n", key.fingerprint_md5().c_str());
		printf(" sha1 %s\n", key.fingerprint_sha1().c_str());
		printf(" alg %s %d\n", key.alg_name().c_str(), key.bits());
		printf(" comment %s\n", key.key_comment().c_str());
		printf(" comment2 %s\n", key.key_comment2().c_str());
	}

	return true;
}

static void bt_agent_query_synchronous_fn(void *in, size_t inlen, void **out, size_t *outlen)
{
	size_t reply_len = inlen;
	void *reply = bt_agent_proxy_main_handle_msg(in, &reply_len);	// todo: size_tに変更
	*out = reply;
	*outlen = reply_len;
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
			return false;
		}

		for(auto fnamae : fnames) {
			for(auto key : keys) {
				if (key.key_comment() == fnamae) {
					ssh2_userkey *key_st;
					key.get_raw_key(&key_st);
					if (!pageant_add_ssh2_key(key_st)) {
						printf("err\n");
					}
				}
			}
		}
	}
	return true;
}
#endif


// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
