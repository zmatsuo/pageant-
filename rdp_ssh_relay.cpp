#define _UNICODE

#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <wtsapi32.h>
#include <tchar.h>
#include <lmcons.h>
#include <vector>
#include <thread>
#include <sstream>

#include "rdp_ssh_relay_def.h"
#define ENABLE_DEBUG_PRINT
#include "debug.h"
#include "winmisc.h"		// for setThreadName();

#include "rdp_ssh_relay.h"

static std::thread *thread_;
static HANDLE ghEventEnd;
static HANDLE ghEventReconnect;
static HANDLE ghEventDisconnect;
static HANDLE hVirtChannel_;
static HANDLE eventNotify_;
static bool exitFlag_;
static bool sendFlag_;
static const uint8_t *sendPtr_;
static size_t sendSize_;
static std::vector<uint8_t> receiveBuf_;

#define EVENT_BASE_NAME	"pageant"	// eventの名前のベース
#define RDP_CLIENT_DLL_FILE_NAME	"pageant+_rdp_client.dll"

static std::wstring rdp_client_dll_;
static const wchar_t rdpClientRegistryKey[] =
	L"Software\\Microsoft\\Terminal Server Client\\Default\\AddIns\\pageant+";
static const wchar_t entry[] = 
	L"SYSTEM\\CurrentControlSet\\Control\\Terminal Server\\AddIns\\pageant+";

static std::wstring _WTSQuerySessionInformation(
	HANDLE hServer,
	DWORD SessionId,
	WTS_INFO_CLASS WTSInfoClass)
{
	LPWSTR buffer;
	DWORD bytesReturend;
	BOOL r = ::WTSQuerySessionInformationW(
		hServer,
		SessionId,
		WTSInfoClass,
		&buffer, &bytesReturend);
	if (r == FALSE) {
		return L"";
	}
	std::wstring s(buffer, bytesReturend / sizeof(wchar_t) - 1);
	::WTSFreeMemory(buffer);
	return s;
}

static std::wstring getClientInfo()
{
	std::wstring s1, s2;
	s1 = _WTSQuerySessionInformation(
		WTS_CURRENT_SERVER_HANDLE,
		WTS_CURRENT_SESSION,
		WTSClientName);
	s2 = _WTSQuerySessionInformation(
		WTS_CURRENT_SERVER_HANDLE,
		WTS_CURRENT_SESSION,
		WTSDomainName);
	if (!s2.empty()) {
		s1 += L"." + s2;
	}
	return s1;
}

/*
 *	リモート接続かどうかを取得する
 */
static BOOL IsRemoteSession()
{
	// リモート接続フラグ
	BOOL bRemoteSession = FALSE;

	WTS_CLIENT_ADDRESS* pClientAddress = NULL;
	DWORD dwBytes = 0;

	// 指定したターミナルサーバー上の、指定したセッションの情報を取得
	if ( 0 != ::WTSQuerySessionInformationW(
			 WTS_CURRENT_SERVER_HANDLE	// アプリケーションを実行中のターミナルサーバー
			 , WTS_CURRENT_SESSION		// カレントセッション
			 , WTSClientAddress
			 , (LPWSTR*)&pClientAddress
			 , &dwBytes
			 ) ) {

		if ( NULL != pClientAddress ) {

			// AF_UNSPEC以外だったらリモート接続
			if ( AF_UNSPEC != pClientAddress->AddressFamily ) {

				// リモート接続でした
				bRemoteSession = TRUE;
			}

			// メモリの解放
			::WTSFreeMemory( pClientAddress );
		}
	}

	// リモート接続かどうかを返す
	return bRemoteSession;
}

// https://technet.microsoft.com/ja-jp/aa380798
#define TERMINAL_SERVER_KEY _T("SYSTEM\\CurrentControlSet\\Control\\Terminal Server\\")
#define GLASS_SESSION_ID    _T("GlassSessionId")

static BOOL IsCurrentSessionRemoteable()
{
    BOOL fIsRemoteable = FALSE;
                                       
    if (GetSystemMetrics(SM_REMOTESESSION)) 
    {
        fIsRemoteable = TRUE;
    }
    else
    {
        HKEY hRegKey = NULL;
        LONG lResult;

        lResult = RegOpenKeyEx(
            HKEY_LOCAL_MACHINE,
            TERMINAL_SERVER_KEY,
            0, // ulOptions
            KEY_READ,
            &hRegKey
            );

        if (lResult == ERROR_SUCCESS)
        {
            DWORD dwGlassSessionId;
            DWORD cbGlassSessionId = sizeof(dwGlassSessionId);
            DWORD dwType;

            lResult = RegQueryValueEx(
                hRegKey,
                GLASS_SESSION_ID,
                NULL, // lpReserved
                &dwType,
                (BYTE*) &dwGlassSessionId,
                &cbGlassSessionId
                );

            if (lResult == ERROR_SUCCESS)
            {
                DWORD dwCurrentSessionId;

                if (ProcessIdToSessionId(GetCurrentProcessId(), &dwCurrentSessionId))
                {
                    fIsRemoteable = (dwCurrentSessionId != dwGlassSessionId);
                }
            }
        }

        if (hRegKey)
        {
            RegCloseKey(hRegKey);
        }
    }

    return fIsRemoteable;
}

static bool sendReceive(const uint8_t *sendPtr, size_t sendLen,
						std::vector<uint8_t> &receive)
{
	dbgprintf("send 0x%x(%d) bytes\n", sendLen, sendLen);
	dmemdumpl(sendPtr, sendLen);
    ULONG bytesWritten;
	bool r = WTSVirtualChannelWrite(hVirtChannel_,
									(PCHAR)sendPtr,
									(ULONG)sendLen,
									&bytesWritten);
	if (r == 0 || sendLen != bytesWritten) {
		dbgprintf("WTSVirtualChannelWrite %S\n",
				  _FormatMessage(GetLastError()).c_str());
		receive.clear();
		return false;
	}

	receive.resize(8*1024);		// todo: check CHANNEL_CHUNK_LENGTH 
    ULONG bytesRead;
	r = WTSVirtualChannelRead(hVirtChannel_,
							  INFINITE,
							  (PCHAR)&receive[0],
							  (ULONG)receive.size(),
							  &bytesRead);
	if (r == 0) {
		dbgprintf("WTSVirtualChannelRead error\n");
		receive.clear();
		return false;
	}

	receive.resize(bytesRead);
	dbgprintf("receive 0x%x(%d) bytes\n", bytesRead, bytesRead);
	dmemdumpl(&receive[0], receive.size());

	return true;
}

static void vcOpen()
{
	LPCSTR channelName = CHANNELNAME;
	HANDLE handle =
		WTSVirtualChannelOpenEx(WTS_CURRENT_SESSION, (LPSTR)channelName, 0);
	dbgprintf("WTSVirtualChannelOpenEx() pVirtualName='%s' %p(%S)\n",
			  channelName,
			  handle,
			  handle != NULL ? L"ok" : _FormatMessage(GetLastError()).c_str());
	if (handle == NULL) {
		dbgprintf("client %S\n",
				  getClientInfo().c_str());
	} else {
		hVirtChannel_ = handle;
	}
}

static void vcClose()
{
    BOOL r = WTSVirtualChannelClose(hVirtChannel_);
	dbgprintf("WTSVirtualChannelClose() %d\n", r);
    hVirtChannel_ = NULL;
}


static void rdp_ssh_relay_main()
{
    dbgprintf("rdp server start\n");
	

    HANDLE			hEventConnect[3]; // 0 reconnect, 1 disconnect, 2 stop
    hEventConnect[0] = ghEventReconnect;
    hEventConnect[1] = ghEventDisconnect;
    hEventConnect[2] = ghEventEnd;

    bool bContinue = true;
    while(bContinue) {
		DWORD dwWait = WaitForMultipleObjects(3, hEventConnect, FALSE, INFINITE);
		switch(dwWait)
		{
		case WAIT_TIMEOUT:
			break;
		case WAIT_OBJECT_0: // reconnect
			//
			// allow query to occur
			//
			dbgprintf("-reconnected- \n");
			vcOpen();
			break;

		case WAIT_OBJECT_0 + 1: // disconnect
			//
			// do not query
			//
			dbgprintf("disconnected\n");
			vcClose();
			break;

		case WAIT_OBJECT_0 + 2: // stop
			if (exitFlag_) {
				bContinue = false;
				vcClose();
			}
			if (sendFlag_) {
				sendReceive(sendPtr_, sendSize_, receiveBuf_);
				SetEvent(eventNotify_);
			}
			break;

		case WAIT_FAILED:
			dbgprintf("WaitForMultipleObjects %S",
					  _FormatMessage(GetLastError()).c_str());
			break;

		default:
			break;
		}
    }

}

bool rdpSshRelayIsRemoteSession()
{
    if (!IsCurrentSessionRemoteable()) {
		dbgprintf("not remote GetSystemMetrics(SM_REMOTESESSION)\n");
		return false;
    }

    if (!IsRemoteSession()) {
		dbgprintf("not remote\n");
		return false;
    }

	return true;
}

std::wstring rdpSshRelayGetClientName()
{
	return getClientInfo();
}

static DWORD getSessionId()
{
	ULONG *sessionId;
	DWORD BytesReturned;
	WTSQuerySessionInformation(
		WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION,
		WTSSessionId,
		(LPTSTR *)&sessionId,
		&BytesReturned);

	dbgprintf("sessionid %d %d\n", *sessionId, BytesReturned);
	return *sessionId;
}

static bool checkTerminalServerRegistoryEntry()
{
	std::vector<uint8_t> data;
	DWORD dwType;
	bool r = reg_read(
		HKEY_LOCAL_MACHINE,
		entry,
		L"Name",
		dwType, data);
	if (r == false || dwType != REG_SZ) {
		return false;
	}
	std::wstring name((wchar_t *)&data[0]);
	if (name != L"" EVENT_BASE_NAME) {
		return false;
	}

	r = reg_read(
		HKEY_LOCAL_MACHINE,
		entry,
		L"Type",
		dwType, data);
	if (r == false || dwType != REG_DWORD) {
		return false;
	}
	DWORD d = *((DWORD*)&data[0]);
	if (d != 3) {
		return false;
	}
	return true;
}

static bool addTerminalServerRegistoryEntry()
{
	const wchar_t *s;
	s = L"" EVENT_BASE_NAME;
	size_t len = wcslen(s);
	bool r = reg_write(
		HKEY_LOCAL_MACHINE,
		entry,
		L"Name",
		REG_SZ, (void *)s, (len+1) * sizeof(wchar_t));
	if (r == false) {
		// 管理者権限がないため書き込めなかった
		return false;
	}

	DWORD type = 3;
	reg_write(
		HKEY_LOCAL_MACHINE,
		entry,
		L"Type",
		REG_DWORD, (void *)&type, sizeof(type));

	return true;
}

static void setRdpClientDll()
{
	std::wstring f = _GetModuleFileName(NULL);
	size_t pos = f.rfind(L'\\');
	if (pos != std::string::npos) {
		f = f.substr(0, pos+1);
	}
	f = f + L"" RDP_CLIENT_DLL_FILE_NAME;
	rdp_client_dll_ = f;
}

static bool checkRdpClientDll()
{
	if (_waccess(rdp_client_dll_.c_str(), 0) != 0) {
		return false;
    }
	return true;
}

static bool checkClientRegistory(std::wstring &dll)
{
	if (!reg_read_cur_user(rdpClientRegistryKey, L"Name", dll)) {
		// 設定されていないので利用できる
		return true;
	}
	if (dll != rdp_client_dll_) {
		// 異なるdllが設定されている
		return false;
	}

	return true;
}

static void clearClientRegistry()
{
	reg_write_cur_user(rdpClientRegistryKey, nullptr, nullptr);
}

static bool setRdpClientRegistry()
{
	bool r = reg_write_cur_user(rdpClientRegistryKey, L"Name", rdp_client_dll_.c_str());
	if (r == false) {
		clearClientRegistry();
	}
	return r;
}

//////////////////////////////////////////////////////////////////////////////

bool rdpSshRelayInit()
{
	setRdpClientDll();
	return true;
}

//
// dllが使えるか?
//
// @retvalue	true	利用できる
//
bool rdpSshRelayCheckClientDll(std::wstring &dll)
{
	dll = rdp_client_dll_;
	return checkRdpClientDll();
}

//
// レジストリに設定できるか?
//
// @retvalue	true	利用できる
//
bool rdpSshRelayCheckClientRegistry(std::wstring &dll)
{
	return checkClientRegistory(dll);
}

//
//
//
// @retvalue	true	利用できる
//
bool rdpSshRelayCheckClient()
{
	if (!checkRdpClientDll()) {
		return false;
	}
	std::wstring dll;
	if (!checkClientRegistory(dll)) {
		return false;
	}
	return true;
}

bool rdpSshRelaySetupClient()
{
	return setRdpClientRegistry();
}

void rdpSshRelayTeardownClient()
{
	clearClientRegistry();
}

bool rdpSshRelayCheckServer()
{
	if (checkTerminalServerRegistoryEntry() == false) {
		return false;
	}
	return true;
}

void rdpSshRelaySetServer()
{
	addTerminalServerRegistoryEntry();
}

bool rdpSshRelayServerStart()
{
	if (thread_ != nullptr) {
		return false;
	}

	if (checkTerminalServerRegistoryEntry() == false) {
		dbgprintf("registory entry is not set\n");
		return false;
	}

	// if (!rdpSshRelayIsRemoteSession()) {
	// 	return false;
	// }
	
    DWORD sessionId = getSessionId();
	dbgprintf("sessionid %d\n", sessionId);

    // connection events
	std::wstring s = L"Global\\" EVENT_BASE_NAME "-";
	s += std::to_wstring(sessionId);
	s += L"-Reconnect";
	ghEventReconnect = CreateEvent(NULL, FALSE, FALSE, s.c_str());
	s = L"Global\\" EVENT_BASE_NAME "-";
	s += std::to_wstring(sessionId);
	s += L"-Disconnect";
	ghEventDisconnect = CreateEvent(NULL, FALSE, FALSE, s.c_str());


    //
    // create stop event
    //
    ghEventEnd = CreateEvent(NULL, FALSE, FALSE, NULL);
	eventNotify_ = CreateEvent(NULL, FALSE, FALSE, NULL);


	if (rdpSshRelayIsRemoteSession()) {
		vcOpen();
	}
	
	exitFlag_ = false;
	sendFlag_ = false;
    thread_ = new std::thread(rdp_ssh_relay_main);
	setThreadName(thread_, "rdp ssh relay");

	dbgprintf("rdpSshRelayServerStart ok\n");

	return true;
}

void rdpSshRelayServerStop()
{
	dbgprintf("rdpSshRelayServerStop()\n");

    if (thread_ == nullptr) {
		return;
	}

	exitFlag_ = true;
    SetEvent(ghEventEnd);
	thread_->join();
	delete thread_;
	thread_ = nullptr;

    //
    // close handles
    //
    CloseHandle(ghEventReconnect);
    CloseHandle(ghEventDisconnect);
    CloseHandle(ghEventEnd);
	CloseHandle(eventNotify_);
}

bool rdpSshRelaySendReceive(
	const std::vector<uint8_t> &send,
	std::vector<uint8_t> &receive)
{
	if (thread_ == nullptr) {
		// 動いていない
		return false;
	}
	if (hVirtChannel_ == nullptr) {
		return false;
	}

	sendPtr_ = &send[0];
	sendSize_ = send.size();
	sendFlag_ = true;
	SetEvent(ghEventEnd);
	WaitForSingleObject(eventNotify_, INFINITE);
	receive = receiveBuf_;
	return true;
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
