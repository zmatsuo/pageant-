
#pragma warning(push)
#pragma warning(disable:4127)
#pragma warning(disable:4251)
#include <QApplication>
#include <QtCore/QCommandLineParser>
#pragma warning(pop)

#include "pageant+.h"
#include "pageant.h"
#include "mainwindow.h"
//#define ENABLE_DEBUG_PRINT
#include "debug.h"
#include "winpgnt.h"
#include "misc.h"
#include "misc_cpp.h"
#include "setting.h"
#include "ssh-agent_ms.h"
#include "ssh-agent_emu.h"
#include "gui_stuff.h"
extern "C" {
#include "cert/cert_common.h"
}
#include "bt_agent_proxy_main.h"
#include "winmisc.h"
#include "passphrases.h"
#include "rdp_ssh_relay.h"
#include "keystore.h"

#ifdef _DEBUG
static void crt_set_debugflag(void)
{
	int tmpFlag = _CrtSetDbgFlag( _CRTDBG_REPORT_FLAG );
	tmpFlag |= 0
//		| _CRTDBG_DELAY_FREE_MEM_DF
		| _CRTDBG_LEAK_CHECK_DF
		| 0;
	tmpFlag = (tmpFlag & 0x0000FFFF) |
//		_CRTDBG_CHECK_ALWAYS_DF
//		_CRTDBG_CHECK_EVERY_1024_DF // 1024回に1回
		_CRTDBG_CHECK_DEFAULT_DF	// チェックしない(default)
		;
	_CrtSetDbgFlag( tmpFlag );

	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
	_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
	_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
	_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
}
#endif

#if defined(DEVELOP_VERSION)
static std::wstring debug_log_;

static void debugOutputLog(const char *s)
{
	FILE *fp;
	auto e = _wfopen_s(&fp, debug_log_.c_str(), L"a+");
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

static bool CheckDoubleStartup()
{
	std::vector<PROCESSENTRY32W> process_list = _Process();
	std::vector<HWND> hWnds = _EnumWindows();
	const DWORD my_pid = GetCurrentProcessId();

	// pageant.exeチェック
	{
		bool enable_terminate_pageant = false;
		for(const auto &process :process_list) {
			if (wcscmp(process.szExeFile, L"pageant.exe") != 0 ) {
				continue;
			}
			const DWORD target_pid = process.th32ProcessID;
		
			for (const auto hWnd : hWnds) {
				DWORD pid;
				GetWindowThreadProcessId(hWnd, &pid);

				if (pid != target_pid) {
					continue;
				}

				wchar_t name[256];
				GetClassNameW(hWnd, name, sizeof(name));

				if (wcscmp(name, L"Pageant") == 0 ) {
					if (!enable_terminate_pageant) {
						int r = ::MessageBoxW(
							NULL,
							L"pageantが起動しています\n"
							"終了するようメッセージを送信しますか?",
							L"pageant+", MB_YESNO | MB_ICONERROR);
						if (r == IDYES) {
							enable_terminate_pageant = true;
						}
					}
					if (enable_terminate_pageant) {
						PostMessage(hWnd, WM_CLOSE, 0, 0);
					}
				}
			}
		}
	}
	
	// 二重起動対策
	{
		bool found = false;
		HWND target_hWnd = 0;
		for(const auto &process :process_list) {
			if (wcscmp(process.szExeFile, L"pageant+.exe") != 0 ) {
				continue;
			}
			const DWORD target_pid = process.th32ProcessID;
			if (target_pid == my_pid) {
				continue;
			}
		
			for (const auto hWnd : hWnds) {
				DWORD pid;
				GetWindowThreadProcessId(hWnd, &pid);

				if (pid != target_pid) {
					continue;
				}

				wchar_t name[256];
				GetClassNameW(hWnd, name, sizeof(name));

				if (wcscmp(name, L"QTrayIconMessageWindowClass") == 0 ) {
					found = true;
					target_hWnd = hWnd;
					break;
				}
			}
			if (found)
				break;
		}

		if (!found) {
			return false;
		}

		PostMessage(target_hWnd, WM_APP, 0, 0);
		return true;
	}
}

static void add_keyfile(const std::vector<std::wstring> &keyfileAry)
{
	std::vector<std::string> bt_files;
	std::vector<std::string> sc_files;
	std::vector<std::wstring> normal_files;

	// BT,SmartCard,その他(普通のファイル)を分離
	for(const auto &f: keyfileAry) {
		std::string utf8_str = wc_to_utf8(f);
		if (wcsncmp(L"btspp://", f.c_str(), 8) == 0)  {
			bt_files.emplace_back(utf8_str);
		} else if(cert_is_certpath(utf8_str.c_str())) {
			sc_files.emplace_back(utf8_str);
		} else {
			normal_files.emplace_back(f);
		}			
	}

	// 通常ファイルを読み込み
	for(auto &f: normal_files) {
		Filename *fn = filename_from_wstr(f.c_str());
		add_keyfile(fn);
		filename_free(fn);
	}

	// BTを読み込み
	if (setting_get_bool("bt/enable", false)) {
		bt_agent_proxy_main_add_key(bt_files);
	}

	for(auto &f: sc_files) {
		struct ssh2_userkey *skey = cert_load_key(f.c_str());
		if (skey == nullptr) {
			//*retstr = const_cast<char *>("load key from certificate failed");
		} else {
			ckey key(skey);
			key.dump();
			keystore_add(key);
		}
	}
}

int main(int argc, char *argv[])
{
#if defined(DEVELOP_VERSION)
	debug_set_thread_name("main");
	SetThreadName("main");
#endif
#ifdef _DEBUG
	crt_set_debugflag();
#endif

	if (CheckDoubleStartup()) {
		return 0;
	}

	QApplication a(argc, argv);
	a.setQuitOnLastWindowClosed(false);

	std::vector<std::wstring> keyfileAry;
#if 1
	{
		a.setApplicationName("pageant+");
		a.setApplicationVersion("1.0");

		QCommandLineParser parser;
		const QCommandLineOption helpOption =
			parser.addHelpOption();
		parser.addPositionalArgument("path1 path2 ...","keyfile");
	
		QStringList qargv = QCoreApplication::arguments();
		if(!parser.parse(qargv)){
			// まだmessage_box()が使用できない
			QString msg = parser.errorText();
			msg += "\n----------\n";
			msg += parser.helpText();
			::MessageBoxW(NULL, msg.toStdWString().c_str(), L"pageant+", MB_OK | MB_ICONERROR);
			return 0;
		}

		if (parser.isSet(helpOption)) {
			const QString msg = parser.helpText();
			::MessageBoxW(NULL, msg.toStdWString().c_str(), L"pageant+", MB_OK | MB_ICONERROR);
			return 0;
		}

		{
			const QStringList args = parser.positionalArguments();
			for (QString s : args) {
				keyfileAry.push_back(s.toStdWString());
			}
		}
	}
#endif

	setting_init(_GetModuleFileName(nullptr).c_str());

#if defined(DEVELOP_VERSION)
	if (setting_get_bool("debug/console_enable", false)) {
		setlocale(LC_ALL, setlocale(LC_CTYPE, ""));
		debug_console_init(1);	// true
		debug_add_output(debug_console_puts);
	}
	if (setting_get_bool("debug/log", false)) {
		debug_log_ = setting_get_logfile(_GetModuleFileName(nullptr).c_str());
		debug_add_output(debugOutputLog);
	}
	if (setting_get_bool("debug/outputdebugstring", false)) {
		debug_add_output(debugOutputWin);
	}
#endif

	dbgprintf("--------------------\n");
	dbgprintf("exe: %S\n", _GetModuleFileName(nullptr).c_str());
	dbgprintf("ini: %S\n", setting_get_inifile().c_str());
	dbgprintf("main() start\n");

#if 0
    /*
     * If Pageant was already running, we leave now. If we haven't
     * even taken any auxiliary action (spawned a command or added
     * keys), complain.
     */
    if (already_running) {
		if (!command && !added_keys) {
			MessageBox(hwnd, "Pageant is already running", "Pageant Error",
					   MB_ICONERROR | MB_OK);
		}
		return 0;
    }
#endif

    cert_set_pin_dlg(pin_dlg);
	pageant_init();
	rdpSshRelayInit();

	if (setting_get_bool("Passphrase/save_enable", false) &&
		setting_get_bool("Passphrase/enable_loading_when_startup", false))
	{
		passphrase_load_setting();
	}

	set_confirm_any_request(setting_get_bool("confirm/confirm_any_request", false));

	agents_start();

	MainWindow w;
	a.installNativeEventFilter(&w);
	//w.show();

	// 鍵ファイル一覧を設定より取得
	if (setting_get_bool("key/enable_loading_when_startup", false)) {
		setting_get_keyfiles(keyfileAry);
	}

	// 鍵ファイル読み込み
	add_keyfile(keyfileAry);

	int r = a.exec();
	dbgprintf("main leave %d\n", r);

	agents_stop();

	pageant_exit();
	setting_exit();
	passphrase_exit();

	return r;
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
