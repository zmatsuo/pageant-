#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <wtsapi32.h>
#include <tchar.h>
#include <lmcons.h>
#include <pchannel.h>
#include <cchannel.h>
#include <vector>

#include "rdp_ssh_relay_def.h"
#include "pageant_client.h"
//#define ENABLE_DEBUG_PRINT
#include "debug.h"
#pragma warning( disable : 4996 )
#include "winmisc.h"
#include "setting.h"

//
// GLOBAL variables
//
static HANDLE				 ghThread;		// TODO: closeしてない?
static HANDLE				 ghWriteEvent;
static HANDLE				 ghAlertThread;
static HANDLE				 ghStopThread;
static HANDLE				 ghSynchCodeEvent;
static LPHANDLE				 gphChannel;
static DWORD				 gdwOpenChannel;
static CHANNEL_ENTRY_POINTS  gEntryPoints;
static std::wstring serverName_;
static std::vector<uint8_t> receiveData_;
static std::wstring debugLog_;

#if defined(DEVELOP_VERSION)
static void debugOutputLog(const char *s)
{
	const wchar_t *debugLog = debugLog_.c_str();
	FILE *fp;
	auto e = _wfopen_s(&fp, debugLog, L"a+");
	if (e == 0) {
		fputs(s, fp);	// \n is not output
		fflush(fp);
		fclose(fp);
    }
}

static void debugOutputWin(const char *s)
{
	OutputDebugStringA(s);
}
#endif

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    UNREFERENCED_PARAMETER(lpvReserved);

    if (fdwReason == DLL_PROCESS_ATTACH) {
		std::wstring dll = _GetModuleFileName(hinstDLL);
		setting_init(dll.c_str());
#if defined(DEVELOP_VERSION)
		if (setting_get_bool("debug_rdp_client/console_enable", false)) {
			setlocale(LC_ALL, setlocale(LC_CTYPE, ""));
			debug_console_init(1);
			debug_add_output(debug_console_puts);
		}
		if (setting_get_bool("debug_rdp_client/log", false)) {
			debugLog_ = setting_get_logfile(dll.c_str());
			debug_add_output(debugOutputLog);
		}
		if (setting_get_bool("debug_rdp_client/outputdebugstring", false)) {
			debug_add_output(debugOutputWin);
		}
		debug_set_thread_name("main");
#endif
		dbgprintf("--------------------\n");
		dbgprintf("dll: %S\n", dll.c_str());
		dbgprintf("ini: %S\n", setting_get_inifile().c_str());
		dbgprintf("log: %S\n", debugLog_.c_str());
		dbgprintf("DLL_PROCESS_ATTACH\n");
		if (!setting_get_bool("rdpvc-relay-client/enable")) {
			dbgprintf("rdpvc disabled\n");
			return FALSE;
		}

		return TRUE;
	} else if (fdwReason == DLL_PROCESS_DETACH) {
		dbgprintf("DLL_PROCESS_DETACH\n");
		return TRUE;
	} else {
		return TRUE;
	}
}


static void WaitForTheWrite(HANDLE hEvent)
{
    dbgprintf("worker thread waiting for write to complete\n");

    if (WaitForSingleObject(hEvent, INFINITE) == WAIT_FAILED)
        dbgprintf("WaitForSingleObject");
}

static void pageant_request_identities()
{
#define SSH2_AGENTC_REQUEST_IDENTITIES          11
#define SSH_AGENT_FAILURE                    5
#if 0
	uint8_t request[5];
	request[0] = 0x00;
	request[1] = 0x00;
	request[2] = 0x00;
	request[3] = 0x01;
	request[4] = SSH2_AGENTC_REQUEST_IDENTITIES;
	dmemdump(request, 5);
#endif

	const uint8_t *request_ptr = &receiveData_[0];
	size_t request_len = receiveData_.size();
	dmemdump(request_ptr, request_len);

	void *out;
	int outlen;
	void *p2 = agent_query((void *)request_ptr, request_len, &out, &outlen);
	(void)p2;		// TODO		
	if (out == nullptr) {
		out = malloc(5);
		uint8_t *p = (uint8_t *)out;
		p[0] = 0x00;
		p[1] = 0x00;
		p[2] = 0x00;
		p[3] = 0x01;
		p[4] = SSH_AGENT_FAILURE;
		outlen = 5;
	}
	dmemdump(out, outlen);

    DWORD dwControlCode = 0;
    UINT ui = gEntryPoints.pVirtualChannelWrite(gdwOpenChannel,
												(LPVOID)out,
												outlen,
												(LPVOID)&dwControlCode);
	free(out);
    if (ui != CHANNEL_RC_OK) {
        dbgprintf("worker thread VirtualChannelWrite failed\n");
	} else {
		//
		// wait for the write to complete
		//
		dbgprintf("enter wait\n");
		WaitForTheWrite(ghWriteEvent);
		dbgprintf("leave wait\n");
	}
}

static void WINAPI WorkerThread(void)
{
    HANDLE hEventConnect[2]; // 0 stop, 1 alert
    hEventConnect[0] = ghStopThread;
    hEventConnect[1] = ghAlertThread;

    BOOL   bDone = FALSE;
    while( !bDone ) {

        dbgprintf("worker thread waiting...\n");

		DWORD dw = WaitForMultipleObjects(2, hEventConnect, FALSE, INFINITE);
        switch(dw)
        {
		case WAIT_FAILED:
		{
			dbgprintf("WaitForMultipleObjects");
		}
		break;

		case WAIT_OBJECT_0:
		{
			bDone = TRUE;
		}
		break;

		case WAIT_OBJECT_0 + 1:
		{
			dbgprintf("worker thread wakeup\n");

			//
			// signal
			//		TODO 受信データ保護
			//			データ受取
			//			シグナル
			//			送信
			//
			if (!SetEvent(ghSynchCodeEvent))
				dbgprintf("SetEvent");

			pageant_request_identities();
			dbgprintf("op ok\n");
		}
		break;

		default:
		{
			dbgprintf("worker thread - unknown wait rc\n");
		}
		break;
        }
    }

    dbgprintf("worker thread ending\n");

    ExitThread(0);
}


static void WINAPI VirtualChannelOpenEvent(
	DWORD openHandle, UINT event,
	LPVOID pdata, UINT32 dataLength,
	UINT32 totalLength, UINT32 dataFlags)
{
	dbgprintf("VirtualChannelOpenEvent() enter\n");
	UNREFERENCED_PARAMETER(openHandle);
	UNREFERENCED_PARAMETER(dataFlags);
	UNREFERENCED_PARAMETER(totalLength);

	switch(event)
	{
	case CHANNEL_EVENT_DATA_RECEIVED:
	{
		dbgprintf("CHANNEL_EVENT_DATA_RECEIVED %d %d\n", totalLength, dataLength);
	
		//
		// initialize
		//
		receiveData_.resize(dataLength);
		memcpy(&receiveData_[0], pdata, dataLength);

		//
		// initiate a write
		//
		if (!SetEvent(ghAlertThread))
			dbgprintf("SetEvent");
		else
			dbgprintf("signal to wake up thread\n");

		//
		// wait for worker thread to read control
		//
		if (WaitForSingleObject(ghSynchCodeEvent, INFINITE) == WAIT_FAILED)
			dbgprintf("WaitForSingleObject");
		break;
	}

	case CHANNEL_EVENT_WRITE_COMPLETE:
	{
		LPDWORD pdwControlCode = (LPDWORD)pdata;
		dbgprintf("CHANNEL_EVENT_WRITE_COMPLETE %d\n", *pdwControlCode);

		//
		// set event to notify write is complete
		//
		if (!SetEvent(ghWriteEvent))
			dbgprintf("SetEvent");
		break;
	}

	case CHANNEL_EVENT_WRITE_CANCELLED:
	{
		LPDWORD pdwControlCode = (LPDWORD)pdata;
		dbgprintf("CHANNEL_EVENT_WRITE_CANCELLED %d\n", pdwControlCode);

		//
		// set event to notify write is complete since write was cancelled
		//
		if (!SetEvent(ghWriteEvent))
			dbgprintf("SetEvent");
		break;
	}

	default:
	{
		dbgprintf("unrecognized open event\n");
		break;
	}
	}

	//dbgprintf("data is broken up\n");
	dbgprintf("VirtualChannelOpenEvent() leave\n");
}


static
VOID VCAPITYPE channelInitEventProc(
	LPVOID pInitHandle, UINT event, LPVOID pData, UINT dataLength)
{
	dbgprintf("channelInitEventProc() enter\n");
	dbgprintf(
		"event=%s(%d)\n",
		event == CHANNEL_EVENT_INITIALIZED ? "CHANNEL_EVENT_INITIALIZED" :
		event == CHANNEL_EVENT_CONNECTED ? "CHANNEL_EVENT_CONNECTED" :
		event == CHANNEL_EVENT_V1_CONNECTED ? "CHANNEL_EVENT_V1_CONNECTED" :
		event == CHANNEL_EVENT_DISCONNECTED  ? "CHANNEL_EVENT_DISCONNECTED" :
		event == CHANNEL_EVENT_TERMINATED ? "CHANNEL_EVENT_TERMINATED" :
		"??",
		event
		);

		DWORD dwThreadId;

    UNREFERENCED_PARAMETER(pInitHandle);
    UNREFERENCED_PARAMETER(dataLength);

    switch(event)
    {
	case CHANNEL_EVENT_INITIALIZED:
		// VirtualChannelInit()が成功したら1度呼ばれる
		break;

	case CHANNEL_EVENT_CONNECTED:
	{	// サーバー側でWTSVirtualChannelOpenEx()が行われると呼ばれる
		//
		// save client machine
		//
		serverName_ = (LPTSTR)pData;
		dbgprintf("channel connected, server '%S'\n",
				  serverName_.c_str());

		//
		// open channel
		//
		dbgprintf("VirtualChannelOpen() ChannelName='%s'\n", CHANNELNAME);
		UINT ui = gEntryPoints.pVirtualChannelOpen(gphChannel,
												   &gdwOpenChannel,
												   (PCHAR)CHANNELNAME,
												   VirtualChannelOpenEvent);
		if (ui != CHANNEL_RC_OK){
			dbgprintf("virtual channel open failed\n");
			return;
		}

		//
		// create write completion event
		//
		ghWriteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (ghWriteEvent == NULL)
			dbgprintf("CreateEvent");

		//
		// create thread wakeup event
		//
		ghAlertThread = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (ghAlertThread == NULL)
			dbgprintf("CreateEvent");

		//
		// create thread stop event
		//
		ghStopThread = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (ghStopThread == NULL)
			dbgprintf("CreateEvent");

		//
		// create thread synch event
		//
		ghSynchCodeEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (ghSynchCodeEvent == NULL)
			dbgprintf("CreateEvent");

		//
		// spawn the worker thread
		//
		ghThread = CreateThread(NULL,
								0,
								(LPTHREAD_START_ROUTINE)WorkerThread,
								NULL,
								0,
								&dwThreadId);
		if (ghThread == NULL)
			dbgprintf("CreateThread");
		break;
	}

	case CHANNEL_EVENT_DISCONNECTED:
	{	// サーバー側でWTSVirtualChannelOpenEx()が行われたRDPが、切断時に呼ばれる
		// サーバー側でWTSVirtualChannelOpenEx()したプログラムが終了したときには呼ばれないようだ
		// CHANNEL_EVENT_TERMINATEDとの差は?

		serverName_.clear();

		//
		// signal worker thread to end
		//
		if (!SetEvent(ghStopThread))
			dbgprintf("SetEvent");

		//
		// wait for thread to die
		//
		DWORD dw = WaitForSingleObject(ghThread, 5000);

		switch(dw)
		{
		case WAIT_TIMEOUT:
			//
			// blow away thread since time expired
			//
			dbgprintf("blowing away worker thread\n");
			if (!TerminateThread(ghThread, 0))
				dbgprintf("TerminateThread");
			break;

		case WAIT_FAILED:
			dbgprintf("WaitForSingleObject");
			break;
		default:
			break;
		}

		dbgprintf("worker thread terminated\n");

		//
		// close handles
		//
		CloseHandle(ghStopThread);
		CloseHandle(ghAlertThread);
		CloseHandle(ghWriteEvent);
		CloseHandle(ghSynchCodeEvent);
		break;
	}

	default:
		break;

    }
	dbgprintf("channelInitEventProc() leave\n");
}


extern "C" _declspec(dllexport) BOOL VCAPITYPE VirtualChannelEntry(PCHANNEL_ENTRY_POINTS pEntryPoints)
{
    dbgprintf("VirtualChannelEntry() enter\n");

    memcpy(&gEntryPoints, pEntryPoints, pEntryPoints->cbSize);

    //
    // initialize CHANNEL_DEF structure
    //
    CHANNEL_DEF channelDef[1] = {0};
	memset(&channelDef, 0, sizeof(channelDef));
    strcpy_s(channelDef[0].name, sizeof(channelDef[0].name), CHANNELNAME); // ANSI ONLY
	dbgprintf("channelDef[0].name='%s'\n", channelDef[0].name);

    //
    // register channel
    //
    UINT uRet = gEntryPoints.pVirtualChannelInit(
		(LPVOID *)&gphChannel,
		channelDef, 1,
		VIRTUAL_CHANNEL_VERSION_WIN2000,
		channelInitEventProc);
	dbgprintf("VirtualChannelInit() %d(%s)\n",
			  uRet, uRet == CHANNEL_RC_OK ? "CHANNEL_RC_OK" : "??");
    if (uRet != CHANNEL_RC_OK) {
		return FALSE;
	}
    if (channelDef[0].options != CHANNEL_OPTION_INITIALIZED) {
		dbgprintf("channelDef[0].options 0x%x\n", channelDef[0].options);
		return FALSE;
	}

	dbgprintf("VirtualChannelEntry() leave\n");
	return TRUE;
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
