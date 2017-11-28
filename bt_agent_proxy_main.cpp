
#include <stdint.h>
#include <vector>

#include "bt_agent_proxy.h"

#include "bt_agent_proxy_main.h"


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
    bta_exit(hBta_);
    hBta_ = nullptr;
}

void bt_agent_proxy_main_send(const uint8_t *data, size_t len)
{
    bta_send(hBta_, data, len);
}

void *bt_agent_proxy_main_handle_msg(const void *msgv, size_t *replylen)
{
    receive_event = false;
    bt_agent_proxy_main_send((uint8_t *)msgv, (size_t)*replylen);
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

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
