#include <QApplication>
#include <QtCore/QCommandLineParser>

#include "mainwindow.h"
#include "winpgnt.h"
#include "pageant.h"
#include "misc.h"
#include "setting.h"
#include "ssh-agent_ms.h"
#include "ssh-agent_emu.h"

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
}
#endif

int main(int argc, char *argv[])
{
#ifdef _DEBUG
	crt_set_debugflag();
#endif
	QApplication a(argc, argv);

	setting_init();

	debug_printf("main() start\n");

	QCommandLineParser parser;
	parser.setApplicationDescription(QStringLiteral("setApplicationDescription"));
	parser.addHelpOption();

	QCommandLineOption ini(QStringLiteral("ini"), QStringLiteral("use inifile"));
	parser.addOption(ini);

	parser.process(a);

	QString inifile;
	if(parser.isSet(ini)){
		inifile = parser.value(ini);
	}
	

    /*
     * Forget any passphrase that we retained while going over
     * command line keyfiles.
     */
//    forget_passphrases();

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

	pageant_init();
	{
		/*
		 * Initialise storage for RSA keys.
		 */
#if 0
		rsakeys = newtree234(cmpkeys_rsa);
		ssh2keys = newtree234(cmpkeys_ssh2);
#endif

		/*
		 * Initialise storage for short-term passphrase cache.
		 */
		passphrases = newtree234(NULL);

		load_passphrases();
	}


	if (setting_get_bool("ssh-agent/pageant")) {
		bool r = winpgnt_start();
		if (r == false) {
			setting_set_bool("ssh-agent/pageant", false);
		}
	}

	QString sock_path = getenv("SSH_AUTH_SOCK");
	if (!sock_path.isEmpty())
	{
		if (setting_get_bool("ssh-agent/cygwin_sock")) {
			bool r = ssh_agent_server_start(sock_path.toStdWString().c_str());
			if (r == false) {
				setting_set_bool("ssh-agent/cygwin_sock", false);
			}
		}
		if (setting_get_bool("ssh-agent/ms_ssh")) {
			bool r = pipe_th_start(sock_path.toStdWString().c_str());
			if (r == false) {
				setting_set_bool("ssh-agent/ms_ssh", false);
			}
		}
	}
	

	MainWindow w;
	a.installNativeEventFilter(&w);
	w.show();

	int r = a.exec();

	debug_printf("main leave %d\n", r);

	winpgnt_stop();
	ssh_agent_server_stop();
//	pipe_th_stop();			// TODO fix
	setting_exit();

	return r;
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
