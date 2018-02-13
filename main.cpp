
#include <io.h>		// for access()

#pragma warning(push)
#pragma warning(disable:4127)
#pragma warning(disable:4251)
#include <QApplication>
#include <QtCore/QCommandLineParser>
#pragma warning(pop)

#include "pageant+.h"
#include "pageant.h"
#include "mainwindow.h"
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

int main(int argc, char *argv[])
{
#ifdef _DEBUG
	crt_set_debugflag();
//	_CrtSetBreakAlloc(251204);
#endif
#if 0
	test();
	return 0;
#endif	
	QApplication a(argc, argv);
	a.setQuitOnLastWindowClosed(false);


	bool use_inifile = false;
	QString inifile;
	std::vector<std::wstring> keyfileAry;
#if 1
	{
		a.setApplicationName("pageant+");
		a.setApplicationVersion("1.0");

		QCommandLineParser parser;
		const QCommandLineOption helpOption =
			parser.addHelpOption();
		QCommandLineOption iniOption(
			QStringLiteral("ini"),
			QStringLiteral("use inifile."), "ini_file");
		parser.addOption(iniOption);
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

		use_inifile = parser.isSet(iniOption);
		if (use_inifile) {
			inifile = parser.value(iniOption);
			{
				const QStringList args = parser.positionalArguments();
				for (QString s : args) {
					keyfileAry.push_back(s.toStdWString());
				}
			}
		}
	}
#endif

#if defined(DEVELOP_VERSION)
	if (!use_inifile) {
		if (_access("pageant+.exe.forceini", 0) == 0) {
			use_inifile = true;
			std::wstring ws_inifile =
				_GetCurrentDirectory() + L"\\pageant+.ini";
			inifile = QString::fromStdWString(ws_inifile);
		}
	}
#endif
	setting_init(use_inifile, inifile.isEmpty() ? nullptr : inifile.toStdWString().c_str());

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

	if (setting_get_bool("Passphrase/save_enable", false) &&
		setting_get_bool("Passphrase/enable_loading_when_startup", false))
	{
		passphrase_load_setting();
	}

	if (setting_get_bool("ssh-agent/pageant")) {
		bool r = winpgnt_start();
		if (r == false) {
			setting_set_bool("ssh-agent/pageant", false);
		}
	}

	std::wstring sock_path = _getenv(L"SSH_AUTH_SOCK");
	if (!sock_path.empty())
	{
		if (setting_get_bool("ssh-agent/cygwin_sock")) {
			bool r = ssh_agent_cygwin_unixdomain_start(sock_path.c_str());
			if (r == false) {
				setting_set_bool("ssh-agent/cygwin_sock", false);
			}
		}
		if (setting_get_bool("ssh-agent/ms_ssh")) {
			bool r = pipe_th_start(sock_path.c_str());
			if (r == false) {
				setting_set_bool("ssh-agent/ms_ssh", false);
			}
		}
	}

	if (setting_get_bool("bt/enable", false)) {
		bt_agent_proxy_main_init(setting_get_int("bt/timeout", 10*1000));
	}

	if (setting_get_bool("ssh-agent_tcp/enable", false)) {
		const int port_no =
			setting_get_int("ssh-agent_tcp/port_no", 8080);
		ssh_agent_localhost_start(port_no);
	}
	
	set_confirm_any_request(setting_get_bool("confirm/confirm_any_request", false));

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

	// stop threads
	winpgnt_stop();
	ssh_agent_cygwin_unixdomain_stop();
	pipe_th_stop();			// TODO fix
	bt_agent_proxy_main_exit();
	ssh_agent_localhost_stop();

	pageant_exit();
	setting_exit();

	return r;
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
