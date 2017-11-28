
#if !defined(_UNICODE)
#define _UNICODE
#endif
#include <winsock2.h>
#include <windows.h>
#include <ws2bth.h>

#include <thread>
#include <vector>
#include <list>

#include <assert.h>

#include "bt_agent_proxy.h"
//#define ENABLE_DEBUG_PRINT
#include "debug.h"

typedef struct bt_agent_proxy_impl_tag {
	bt_agent_proxy_t *hBta_;
	//
	SOCKET listen_sock_;
	WSAEVENT hListenEvent_;
	WSAEVENT hEventExit_;
	WSAEVENT hClientEvent_;
	std::thread *hThread_;
	bool threadExitRequest_;
	// クライアント管理
	int connectedCount_;
	// クライアント
	SOCKET clientSocket_;
	// buffer
	std::vector<uint8_t> send_pool_;
	size_t send_top_;
	uint8_t *recv_ptr_;
	size_t recv_size_;
	// notify
	bool pesudo_send_notify_;
	int (*notify_func_)(
		bt_agent_proxy_t *hBta,
		bta_notify_param_t *notify);
	// connect req
	BTH_ADDR connect_req_;
	// disconnect req
	bool disconnect_req_;
	//
	std::vector<DeviceInfoType> deivceInfo_;
} bt_agent_proxy_impl_t;

static bt_agent_proxy_impl_t *get_impl_ptr(bt_agent_proxy_t *hBta)
{
	assert(hBta != NULL);
	bt_agent_proxy_impl_t *p = (bt_agent_proxy_impl_t *)hBta->impl_;
	return p;
}

static std::wstring getAdr(BTH_ADDR addr)
{
	typedef union {
		uint8_t u8[6];
		BTH_ADDR addr_long;		// uint64_t
	} split_t;
	const split_t *p = (split_t *)&addr;
	wchar_t buf[18];
	wsprintf(buf, L"%02X:%02X:%02X:%02X:%02X:%02X",
			 p->u8[5], p->u8[4], p->u8[3],
			 p->u8[2], p->u8[1], p->u8[0] );
	return buf;
}

static std::wstring getAdr(LPSOCKADDR addr, size_t addr_len)
{
	assert(addr->sa_family == 32);
	if (addr_len == 0 ) addr_len = 30;
	wchar_t addressAsString[128];
	DWORD addressSize = _countof(addressAsString);
	int r = WSAAddressToStringW(addr, (DWORD)addr_len, nullptr, addressAsString, &addressSize);
	assert(r == 0);
	return (std::wstring)addressAsString;
}

#if 0
/**
 *	callback for BluetoothSdpEnumAttributes()
 */
static BOOL __stdcall callback(ULONG uAttribId, LPBYTE pValueStream, ULONG cbStreamSize, LPVOID pvParam)
{
	SDP_ELEMENT_DATA element;
	INT r = BluetoothSdpGetElementData(pValueStream, cbStreamSize, &element);
	return r = ERROR_SUCCESS ? TRUE : FALSE;
}

/**
 *	bluetooth deviceにどんなサービスがあるかチェックする
 */
static void quiryInner(const wchar_t *address)
{
	WSAQUERYSETW querySet = { 0 };
	querySet.dwSize = sizeof(querySet);
	GUID protocol = L2CAP_PROTOCOL_UUID;
//	GUID protocol = SerialPortServiceClass_UUID;
	querySet.lpServiceClassId = &protocol;
	querySet.dwNameSpace = NS_BTH;
	querySet.dwNumberOfCsAddrs = 0;
	querySet.lpszContext = (LPWSTR)address;

	DWORD flags =
//		LUP_FLUSHCACHE |
		LUP_RETURN_NAME | LUP_RETURN_TYPE |
		LUP_RETURN_ADDR | LUP_RETURN_BLOB | LUP_RETURN_COMMENT
		;


	// Start service query
	HANDLE hLookup;
	int result = WSALookupServiceBeginW(&querySet, flags, &hLookup);
	if (result != ERROR_SUCCESS)
	{
		DWORD err = GetLastError();
		if (err == WSASERVICE_NOT_FOUND) {
			// remote bluetooth is down..
			return;
		}
		dbgprintf("WSALookupServiceBegin() failed %ld\n", err);
		return;
	}

	int i = 0;
	while (result == 0)
	{
		BYTE buffer1[2000];
		DWORD bufferLength1 = sizeof(buffer1);
		WSAQUERYSETW *pResults = (WSAQUERYSETW *)&buffer1;

		// Next service query
		result = WSALookupServiceNextW(hLookup, flags, &bufferLength1, pResults);
		if(result != ERROR_SUCCESS) {
			DWORD err = WSAGetLastError();
			if (err == WSA_E_NO_MORE) {
				break;
			}
			dbgprintf(" WSALookupServiceNext() failed %ld\n", err);
			break;
		}
		// Populate the service info

		dbgprintf("   service instance name: %S\n",
			   pResults->lpszServiceInstanceName);
		dbgprintf("   comment (if any): %S\n",
			   pResults->lpszComment);
#if 0
		CSADDR_INFO *pCSAddr = (CSADDR_INFO *)pResults->lpcsaBuffer;
		// Extract the sdp info
		if (pResults->lpBlob)
		{
			BLOB *pBlob = (BLOB*)pResults->lpBlob;
			if (!BluetoothSdpEnumAttributes(pBlob->pBlobData, pBlob->cbSize, callback, 0))
			{
				dbgprintf("BluetoothSdpEnumAttributes() failed with error code %ld\n", WSAGetLastError());
			}
			else
			{
				dbgprintf("BluetoothSdpEnumAttributes() #%d is OK!\n", i++);
			}
		}
#endif
	}

	result = WSALookupServiceEnd(hLookup);
	if (result != ERROR_SUCCESS) {
		dbgprintf("WSALookupServiceEnd(hLookup) %ld\n", WSAGetLastError());
	}
}
#endif

/*
 * 接続先を調べる
 */
static std::vector<DeviceInfoType> PerformInquiry()
{
	std::vector<DeviceInfoType> _retDeviceInfoList;
	_retDeviceInfoList.clear();

	// Set the flags for query
	DWORD flags =
		LUP_CONTAINERS |
		LUP_RETURN_NAME |
		LUP_RETURN_ADDR;
	BYTE buf1[3000];
	memset(buf1, 0, sizeof(buf1));
	WSAQUERYSETW *wsaq = (WSAQUERYSETW *)buf1;
	wsaq->dwSize = sizeof(buf1);
	wsaq->dwNameSpace = NS_BTH;

	HANDLE hLookup;
	INT _ret = WSALookupServiceBeginW(wsaq, flags, &hLookup);
	if ( _ret != ERROR_SUCCESS )
	{
		dbgprintf("WSALookupServiceBegin failed %d\r\n", GetLastError());
		return _retDeviceInfoList;
	}

	BYTE buf2[3000];
	DWORD dwSize = sizeof(buf2);
	memset(buf2, 0, dwSize);
	WSAQUERYSETW *pwsaResults = (WSAQUERYSETW *)buf2;
	pwsaResults->dwSize = sizeof(buf2);
	pwsaResults->dwNameSpace = NS_BTH;
	while (1)
	{
		_ret = WSALookupServiceNextW(hLookup, flags, &dwSize, pwsaResults);
		if (_ret != ERROR_SUCCESS) {
			DWORD err = GetLastError();
			if (err == WSAEFAULT) {
				// more buffer;
				break;
			} else if (err == WSA_E_NO_MORE) {
				break;
			} else {
				dbgprintf("WSALookupServiceNext failed %d\r\n", err);
			}
			break;
		}

#if 1
		dbgprintf("InstanceName `%S`\n", pwsaResults->lpszServiceInstanceName);
		dbgprintf("OutputFlags 0x%08lx(%s|%s|%s)\n",
			   pwsaResults->dwOutputFlags,
			   pwsaResults->dwOutputFlags & BTHNS_RESULT_DEVICE_CONNECTED ? "BTHNS_RESULT_DEVICE_CONNECTED" : "",
			   pwsaResults->dwOutputFlags & BTHNS_RESULT_DEVICE_REMEMBERED ? "BTHNS_RESULT_DEVICE_REMEMBERED" : "",
			   pwsaResults->dwOutputFlags & BTHNS_RESULT_DEVICE_AUTHENTICATED ? "BTHNS_RESULT_DEVICE_AUTHENTICATED" : ""
			);
#endif

		if (pwsaResults->lpszServiceInstanceName == nullptr) {
			dbgprintf("名無し\n");
			continue;
		}

		assert(pwsaResults->dwNumberOfCsAddrs == 1);
		if ((pwsaResults->dwOutputFlags & BTHNS_RESULT_DEVICE_AUTHENTICATED) == 0){
			dbgprintf("ペアじゃない?\n");
			continue;
		}
			
		CSADDR_INFO *pCSAddr = (CSADDR_INFO *)pwsaResults->lpcsaBuffer;
		if (pCSAddr == nullptr) {
			dbgprintf("アドレスがない\n");
			continue;
		}


		BTH_ADDR btAddr = ((SOCKADDR_BTH *)pCSAddr->RemoteAddr.lpSockaddr)->btAddr;
		std::wstring remote_address = getAdr(pCSAddr->RemoteAddr.lpSockaddr, pCSAddr->RemoteAddr.iSockaddrLength);
		const bool connected =
			(pwsaResults->dwOutputFlags & BTHNS_RESULT_DEVICE_CONNECTED) == 0 ? false: true;

#if 0
		dbgprintf("%S\n",
			   getAdr(pCSAddr->RemoteAddr.lpSockaddr, pCSAddr->RemoteAddr.iSockaddrLength).c_str());
		dbgprintf("%S\n",
			   getAdr(pCSAddr->LocalAddr.lpSockaddr, pCSAddr->LocalAddr.iSockaddrLength).c_str());

		quiryInner(remote_address_s);
#endif

		////
#if 0
		DeviceInfoType2 info;
		info.deviceAddr = btAddr;
		info.deviceAddrStr = remote_address;
		info.deviceName = pwsaResults->lpszServiceInstanceName;
#endif
		////
		
		DeviceInfoType _deviceInfo;
		_deviceInfo.deviceName = pwsaResults->lpszServiceInstanceName;
		_deviceInfo.deviceAddr = btAddr;
		_deviceInfo.deviceAddrStr = remote_address;
		_deviceInfo.connected = connected;
		_retDeviceInfoList.push_back( _deviceInfo );

#if 0
		dbgprintf("%S\n", info.deviceAddrStr.c_str());
		dbgprintf("%S\n", info.deviceName.c_str());
#endif
	}

	WSALookupServiceEnd(hLookup);
	return _retDeviceInfoList;
}

static bool get_device_name(
	bt_agent_proxy_impl_t *impl,
	const BTH_ADDR addr, std::wstring &name)
{
	for(const auto &l : impl->deivceInfo_) {
		if(l.deviceAddr == addr) {
			name = l.deviceName;
			return true;
		}
	}
	name.clear();
	return false;
}

static bool get_device_adr(
	bt_agent_proxy_impl_t *impl,
	const std::wstring &name, BTH_ADDR &addr)
{
	for(const auto &l : impl->deivceInfo_) {
		if(l.deviceName == name) {
			addr = l.deviceAddr;
			return true;
		}
	}
	addr = 0;
	return false;
}

static bool bta_listen(bt_agent_proxy_impl_t *impl)
{
	SOCKET listen_sock = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
	if (listen_sock == INVALID_SOCKET) {
		return false;
	}
	impl->listen_sock_ = listen_sock;

	SOCKADDR_BTH sa = { 0 };
	sa.addressFamily = AF_BTH;
	sa.port = BT_PORT_ANY;
			
	if (bind(listen_sock, (SOCKADDR *)&sa, sizeof(sa)) == SOCKET_ERROR) {
		return false;
	}
			
	int size = sizeof(sa);
	getsockname(listen_sock, (SOCKADDR *)&sa, &size);
		
	CSADDR_INFO info = { 0 };
	info.LocalAddr.lpSockaddr = (LPSOCKADDR)&sa;
	info.LocalAddr.iSockaddrLength = sizeof(sa);
	info.iSocketType = SOCK_STREAM;
	info.iProtocol = BTHPROTO_RFCOMM;

	WSAQUERYSETW set = { 0 };
	set.dwSize = sizeof(WSAQUERYSET);							// Must be set to sizeof(WSAQUERYSET)
	set.dwOutputFlags = 0;										// Not used
	set.lpszServiceInstanceName = (LPWSTR)L"Server";			// Recommended.
	set.lpServiceClassId = (LPGUID)&SerialPortServiceClass_UUID;// Requred.
	set.lpVersion = NULL;										// Not used.
	set.lpszComment = NULL;										// Optional.
	set.dwNameSpace = NS_BTH;									// Must be NS_BTH.
	set.lpNSProviderId = NULL;									// Not required.
	set.lpszContext = NULL;										// Not used.
	set.dwNumberOfProtocols = 0;								// Not used.
	set.lpafpProtocols = NULL;									// Not used.
	set.lpszQueryString = NULL;									// not used.
	set.dwNumberOfCsAddrs = 1;									// Must be 1.
	set.lpcsaBuffer = &info;									// Pointer to a CSADDR_INFO.
	set.lpBlob = NULL;											// Optional.

	if (WSASetService(&set, RNRSERVICE_REGISTER, 0) != 0) {
		return false;
	}
	
	listen(listen_sock, 0);
	dbgprintf("listen\n");

	return true;
}

static void bta_set_senddata(
	bt_agent_proxy_impl_t *impl,
	const uint8_t *data,
	size_t len)
{
	// データをセットする
	size_t pool_end = impl->send_pool_.size();
	size_t need_pool_size = pool_end + len;
	impl->send_pool_.resize(need_pool_size);
	memcpy(&impl->send_pool_[pool_end], data, len);
}


static void bta_send_i(bt_agent_proxy_impl_t *impl)
{
	if (impl->send_pool_.size() == 0) {
		return;
	}
	SOCKET client = impl->clientSocket_;
	
	const uint8_t *send_ptr = &impl->send_pool_[impl->send_top_];
	const size_t send_len = impl->send_pool_.size() - impl->send_top_;
	int sent_size = send(client, (char *)send_ptr, (int)send_len, 0);
	if (sent_size == SOCKET_ERROR) {
		dbgprintf("error\n");
		return;
	}
	dbgprintf("send %zd bytes, sent %d bytes\n", send_len, sent_size);
	impl->send_top_ += sent_size;
	if (impl->send_top_ == impl->send_pool_.size()) {
		// すべて送信した。送信バッファを空にしてok
		dbgprintf("no need send buffer\n");
		impl->send_pool_.clear();
		impl->send_top_ = 0;
	}
}

static void exec_notify(
	bt_agent_proxy_impl_t *impl,
	bta_notify_param_t *param)
{
	impl->notify_func_(impl->hBta_, param);
	if (param->type == BTA_NOTIFY_RECV) {
		// 受信バッファ更新
		impl->recv_ptr_ = param->u.recv.ptr;
		impl->recv_size_ = param->u.recv.size;
	}
}

static bool connect_i(
	bt_agent_proxy_impl_t *impl,
	std::wstring &name,
	BTH_ADDR &addr)
{
	if (name.empty()) {
		get_device_name(impl, addr, name);
	}
	if (addr == 0) {
		get_device_adr(impl, name, addr);
	}
	std::wstring addr_str = getAdr(impl->connect_req_);
	dbgprintf("接続 %S(%S)\n",
		   name.c_str(),
		   addr_str.c_str());

	bta_notify_param_t notify_param;
	notify_param.type = BTA_NOTIFY_CONNECT;
	notify_param.u.connect.name = name.c_str();
	notify_param.u.connect.addr_str = addr_str.c_str();
	exec_notify(impl, &notify_param);
	return true;
}

static void bta_main(bt_agent_proxy_impl_t *impl)
{
	bta_listen(impl);
	impl->connectedCount_ = 0;
	impl->clientSocket_ = INVALID_SOCKET;

	impl->hListenEvent_ = WSACreateEvent();
	impl->hClientEvent_ = WSACreateEvent();
	WSAEventSelect(impl->listen_sock_, impl->hListenEvent_, FD_ACCEPT);

	DWORD timeout = 1000;		// ms
	// WSA_INFINITE
	
	while(!impl->threadExitRequest_) {
		int eventArrayCount = 2;
		WSAEVENT hEvntArray[3];
		hEvntArray[0] = impl->hEventExit_;
		hEvntArray[1] = impl->hListenEvent_;
		if (impl->connectedCount_ > 0) {
			hEvntArray[2] = impl->hClientEvent_;
			eventArrayCount++;
		}
		dbgprintf("enter WSAWaitForMultipleEvents()\n");
		DWORD dwResult =
			WSAWaitForMultipleEvents(eventArrayCount, hEvntArray, FALSE, timeout, FALSE);
		switch(dwResult) {
		case WSA_WAIT_FAILED:
			// API呼び出し失敗
			// TODO:
			break;
		case WSA_WAIT_TIMEOUT:
		{
			std::vector<DeviceInfoType> deviceInfo;
			deviceInfo = PerformInquiry();
			impl->deivceInfo_ = deviceInfo;

			bta_notify_param_t notify_param;
			notify_param.type = BTA_NOTIFY_DEVICEINFO_UPDATE;
			exec_notify(impl, &notify_param);
			break;
		}
		case WSA_WAIT_EVENT_0 + 0:
			WSAResetEvent(impl->hEventExit_);
			if (impl->threadExitRequest_) {
				// exit or other
				goto finish;
			}
			if (impl->pesudo_send_notify_) {
				impl->pesudo_send_notify_ = false;
				bta_notify_param_t notify_param;
				notify_param.type = BTA_NOTIFY_SEND;
				exec_notify(impl, &notify_param);
			}
			if (impl->connect_req_ != 0) {
				SOCKET client = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);

				SOCKADDR_BTH sa;
				memset(&sa, 0, sizeof(sa));
				sa.addressFamily = AF_BTH;
				sa.btAddr = impl->connect_req_;
				sa.serviceClassId = SerialPortServiceClass_UUID;
				sa.port = BT_PORT_ANY;

				int r = connect(client, (SOCKADDR *)&sa, sizeof(sa));
				if (r == SOCKET_ERROR) {
					dbgprintf("connect失敗\n");
					closesocket(client);
				} else {
					u_long val = 1;
					ioctlsocket(client, FIONBIO, &val);		// non block mode
					WSAEventSelect(client, impl->hClientEvent_, FD_READ|FD_WRITE|FD_CLOSE);
					impl->connectedCount_++;
					impl->clientSocket_ = client;
				}

				std::wstring device_name;
				BTH_ADDR addr = impl->connect_req_;
				connect_i(impl, device_name, addr);
				impl->connect_req_ = 0;
			}
			if (impl->disconnect_req_) {
				SOCKET client = impl->clientSocket_;
				closesocket(client);
				impl->connectedCount_--;
				impl->clientSocket_ = INVALID_SOCKET;
			}

			break;
		case WSA_WAIT_EVENT_0 + 1:
			// listen socket event
		{
			// accept
			dbgprintf("accept\n");
			WSAResetEvent(impl->hListenEvent_);

			SOCKADDR_STORAGE sockAddr;
			int nAddrLen = sizeof(SOCKADDR_STORAGE);
			SOCKET client = accept(impl->listen_sock_, (LPSOCKADDR)&sockAddr, &nAddrLen);
			if (impl->connectedCount_ > 0) {
				// 接続数管理
				// TODO:今のところ1クライアントだけ
				closesocket(client);
			} else {
				u_long val = 1;
				ioctlsocket(client, FIONBIO, &val);		// non block mode
				WSAEventSelect(client, impl->hClientEvent_, FD_READ|FD_WRITE|FD_CLOSE);
				impl->connectedCount_++;
				impl->clientSocket_ = client;

				SOCKADDR_BTH *p = (SOCKADDR_BTH *)&sockAddr;
				BTH_ADDR clientAddr = p->btAddr;
				std::wstring name;
				connect_i(impl, name, clientAddr);
			}
			break;
		}
		case WSA_WAIT_EVENT_0 + 2:
			// client socket event
		{
			SOCKET client = impl->clientSocket_;
			WSANETWORKEVENTS events;
			WSAEnumNetworkEvents(client, impl->hClientEvent_, &events);
			WSAResetEvent(impl->hClientEvent_);

			if (events.lNetworkEvents & FD_CLOSE) {
				closesocket(client);
				impl->clientSocket_ = INVALID_SOCKET;
				impl->connectedCount_--;
				dbgprintf("close\n");
			}
			if (events.lNetworkEvents & FD_READ) {

				int recieved = recv(client, (char *)impl->recv_ptr_, (int)impl->recv_size_, 0);

				bta_notify_param_t notify_param;
				notify_param.type = BTA_NOTIFY_RECV;
				notify_param.u.recv.ptr = impl->recv_ptr_;
				notify_param.u.recv.size = recieved;
				exec_notify(impl, &notify_param);
			}
			if (events.lNetworkEvents & FD_WRITE) {
				if (impl->send_pool_.size() != 0) {
					dbgprintf("FD_WRITE\n");
					impl->send_pool_.clear();
					bta_notify_param_t notify_param;
					notify_param.type = BTA_NOTIFY_SEND;
					exec_notify(impl, &notify_param);
				}
			}
			break;
		}
		default:
			;
			break;
		}
	}
finish:
	WSACloseEvent(impl->hListenEvent_);
	WSACloseEvent(impl->hClientEvent_);
}

bt_agent_proxy_t *bta_init(const bta_init_t *info)
{
	WORD wVersionRequested = WINSOCK_VERSION;	/*MAKEWORD(2,2)*/
	WSADATA wsaData;
	int r = WSAStartup(wVersionRequested, &wsaData);
	if (r != 0) {
		return NULL;
	}

	bt_agent_proxy_t *hBta =
		(bt_agent_proxy_t *)calloc(1, sizeof(bt_agent_proxy_t));
	bt_agent_proxy_impl_t *impl =
		(bt_agent_proxy_impl_t *)calloc(1, sizeof(bt_agent_proxy_impl_t));
	hBta->impl_ = impl;
	impl->hBta_ = hBta;

	//////////

	impl->notify_func_ = info->notify_func;
	
	impl->hEventExit_ = WSACreateEvent();
	impl->threadExitRequest_ = false;
	impl->hThread_ = new std::thread(bta_main, impl);
	impl->send_top_ = 0;
	impl->recv_ptr_ = info->recv_ptr;
	impl->recv_size_ = info->recv_size;
	impl->pesudo_send_notify_ = false;
	impl->connect_req_ = 0;
	impl->disconnect_req_ = false;
	
	return hBta;
}

void bta_exit(bt_agent_proxy_t *hBta)
{
	if (hBta == nullptr) {
		return;
	}
	bt_agent_proxy_impl_t *impl = get_impl_ptr(hBta);
	impl->threadExitRequest_ = true;
	WSASetEvent(impl->hEventExit_);
	impl->hThread_->join();
	WSACloseEvent(impl->hEventExit_);
	free(impl);
	free(hBta);
	WSACleanup();
}

bool bta_connect(bt_agent_proxy_t *hBta, const BTH_ADDR *deviceAddr)
{
	bt_agent_proxy_impl_t *impl = get_impl_ptr(hBta);
	impl->connect_req_ = *deviceAddr;
	WSASetEvent(impl->hEventExit_);
	return true;
}

bool bta_disconnect(bt_agent_proxy_t *hBta)
{
	bt_agent_proxy_impl_t *impl = get_impl_ptr(hBta);
	impl->disconnect_req_ = true;
	WSASetEvent(impl->hEventExit_);
	return true;
}

void bta_send(bt_agent_proxy_t *hBta, const uint8_t *data, size_t len)
{
	if (len == 0)
		return;
	bt_agent_proxy_impl_t *impl = get_impl_ptr(hBta);

	// 送信する
	bta_set_senddata(impl, data, len);
	bta_send_i(impl);

	// 送信が完了したらイベントを発生するようにしておく
	//		まるでブロッキングソケットみたいに
	//		どうも1回のsendで送信完了功するようだ
	if (impl->send_pool_.size() == 0) {
		impl->pesudo_send_notify_ = true;
		WSASetEvent(impl->hEventExit_);
	}
}

void bta_deviceinfo(bt_agent_proxy_t *hBta, std::vector<DeviceInfoType> &deivceInfo)
{
	bt_agent_proxy_impl_t *impl = get_impl_ptr(hBta);
	deivceInfo = impl->deivceInfo_;
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
