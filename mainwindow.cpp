#undef UNICODE
//#define STRICT

#include "bt_agent_proxy.h"

#include <assert.h>
#include <sstream>
#include <regex>
#include <chrono>
#include <thread>
#include <windows.h>
#include <Wtsapi32.h>
#include <Dbt.h>

#pragma warning(push)
#pragma warning(disable:4127)
#pragma warning(disable:4251)
#include <QIcon>
#include <QFileDialog>
#include <QDesktopServices>
#include <QStringListModel>
#include <QStandardItemModel>
#include <QMenu>
#pragma warning(pop)

#include "pageant+.h"
#include "mainwindow.h"
#include "winpgnt.h"
#define ENABLE_DEBUG_PRINT
#include "debug.h"
#include "winhelp_.h"
#include "misc.h"
#include "misc_cpp.h"
#include "winmisc.h"
#include "filename.h"
#include "setting.h"
#include "gui_stuff.h"
#include "ssh-agent_emu.h"
#include "ssh-agent_ms.h"
#include "pageant.h"
#include "winutils.h"
#include "winutils_qt.h"
#include "ssh.h"
#include "keyviewdlg.h"
#include "bt_agent_proxy_main.h"
#include "codeconvert.h"
#include "ckey.h"
#include "puttymem.h"
#include "passphrases.h"
#pragma warning(push)
#pragma warning(disable:4127)
#pragma warning(disable:4251)
#include "passphrasedlg.h"
#include "aboutdlg.h"
#include "settingdlg.h"
#include "confirmacceptdlg.h"
#include "btselectdlg.h"
#include "rdpkeydlg.h"
#pragma warning(pop)
#include "keystore.h"
#include "rdp_ssh_relay.h"

#include "ui_mainwindow.h"

#ifdef PUTTY_CAC
extern "C" {
#include "cert/cert_common.h"
}
#endif

#if defined(_MSC_VER)
#pragma comment(lib,"Wtsapi32.lib")
#endif

#define APPNAME			APP_NAME	// in pageant+.h

static MainWindow *gWin;
static bool add_keyfile(const Filename *fn);

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    createTrayIcon();
    trayIcon->show();
    ui->setupUi(this);
	gWin = this;

	connect(this, SIGNAL(signal_confirmAcceptDlg(struct ConfirmAcceptDlgInfo *)),
			this, SLOT(slot_confirmAcceptDlg(struct ConfirmAcceptDlgInfo *)),
			Qt::BlockingQueuedConnection);
	connect(this, SIGNAL(signal_passphraseDlg(struct PassphraseDlgInfo *)),
			this, SLOT(slot_passphraseDlg(struct PassphraseDlgInfo *)),
			Qt::BlockingQueuedConnection);

#if defined(__MINGW32__)
    BOOL r = WTSRegisterSessionNotification(
        (HWND)winId(), NOTIFY_FOR_ALL_SESSIONS);
    dbgprintf("WTSRegisterSessionNotification() %d\n", r);
#else
	BOOL r = WTSRegisterSessionNotificationEx(
		WTS_CURRENT_SERVER, (HWND)winId(), NOTIFY_FOR_THIS_SESSION);
	dbgprintf("WTSRegisterSessionNotificationEx() %d\n", r);
#endif
	(void)r;		// TODO check return code
}

MainWindow::~MainWindow()
{
	trayIcon->hide();
	delete trayIcon;
	trayIcon = NULL;

    delete ui;
}

void MainWindow::createTrayIcon()
{
    QIcon icon = QIcon(":/images/pageant.png");
    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(icon);

    QString s;
	s = APP_NAME;
	s += tr(TASK_TRAY_TIP);
#if defined(DEVELOP_VERSION)
	s += "\ndevelop version";
#endif
	trayIcon->setToolTip(s);

	connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
			this, SLOT(trayClicked(QSystemTrayIcon::ActivationReason)));
}

void MainWindow::trayIconMenu()
{
    QMenu *trayIconMenu = new QMenu(this);

	if (!get_putty_path().empty()) {
		// putty
		QMenu *session_menu = trayIconMenu->addMenu( "putty" );
		std::vector<std::wstring> session_info = setting_get_putty_sessions();
		for (size_t i=0; i< session_info.size() + 1; i++) {
			QString s = i == 0 ? "&New session" : QString::fromStdWString(session_info[i - 1]);
			QAction *action = new QAction(s, this);
			action->setData(i-1);
			session_menu->addAction(action);
			sessionActionAry.push_back(action);
			if (i == 0 && session_info.size() > 0) {
				session_menu->addSeparator();
			}
		}
		
		connect(session_menu, SIGNAL(triggered(QAction*)),
				this, SLOT(on_session(QAction*)));

		trayIconMenu->addSeparator();
	}

    trayIconMenu->addAction(
		tr("&View Keys"),
		this, SLOT(on_viewKeys()));

    trayIconMenu->addAction(
		tr("Add &Key"),
		this, SLOT(on_pushButtonAddKey_clicked()));

	if (setting_get_bool("bt/enable", false)) {
		trayIconMenu->addAction(
			tr("Add BT Key"),
			this, SLOT(on_pushButtonAddBTKey_clicked()));
	}

	if (rdpSshRelayIsRemoteSession()) {
		QString s = tr("Add RDP Key");
		s += "(" + QString::fromStdWString(rdpSshRelayGetClientName()) + ")";
		trayIconMenu->addAction(
			s,
			this, SLOT(on_pushButtonAddRdpKey_clicked()));
	}

	trayIconMenu->addAction(
		tr("&Setting"),
		this, SLOT(on_actionsetting_triggered()));

	if (has_help()) {
		trayIconMenu->addAction(
			tr("help"),
			this, SLOT(on_help_clicked()));
	}

    trayIconMenu->addSeparator();

	trayIconMenu->addAction(
		tr("About"),
		this, SLOT(on_actionAboutDlg()));

    trayIconMenu->addSeparator();

	trayIconMenu->addAction(
		tr("&Quit"),
		qApp, &QCoreApplication::quit);

	QAction *ret = trayIconMenu->exec(QCursor::pos());
	(void)ret;

	delete trayIconMenu;
}

static QString get_ssh_folder()
{
    QString home = QString::fromStdWString(_getenv(L"HOME"));
    QDir home_dir(home);
    //QDir home_dir = QDir::home();
    if (home_dir.exists()) {
		QDir home_ssh_dir(home_dir.absolutePath() + "/.ssh");
		if (home_ssh_dir.exists()) {
			return home_ssh_dir.absolutePath();
		}
		return home_ssh_dir.absolutePath();
    }
    return QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
}

//////////////////////////////////////////////////////////////////////////////

static void addStartupKeyfile(const std::wstring &fn)
{
	if (!setting_get_bool("key/enable_loading_when_startup", false)) {
		return;
	}

	std::vector<std::wstring> flist;
	setting_get_keyfiles(flist);

	auto iter = std::find(flist.begin(), flist.end(), fn);
	if (iter == flist.end()) {
		std::wostringstream oss;
		oss << L"次回起動時に読み込みますか?\n"
			<< fn;
		int r = message_box(oss.str().c_str(), L"pageant+", MB_YESNO);
		if (r == IDYES) {
			setting_add_keyfile(fn.c_str());
		}
	}
}

void MainWindow::on_pushButtonAddKey_clicked()
{
	dbgprintf("add\n");

    QString folder = get_ssh_folder();
    QString caption = "Select Private Key File";
    QString filter = QString::fromUtf8(
		u8""
		"ppk (*.ppk);;"
		"All Files (*.*)"
		);

    QStringList files = QFileDialog::getOpenFileNames(this, caption, folder, filter);

    dbgprintf("select count %d\n", files.size());
	if (files.size() == 0) {
		// cancel
		return;
	}

	std::vector<std::wstring> files_ws;
	for (auto &f: files) {
		std::wstring wsf = f.toStdWString();
		Filename *fn = filename_from_wstr(wsf.c_str());
		bool r = ::add_keyfile(fn);
		filename_free(fn);
		if (r == true) {
			files_ws.push_back(wsf);
		}
    }
	if (files_ws.size() > 0) {
		for (const auto &f: files_ws) {
			addStartupKeyfile(f);
		}
	}
}

void MainWindow::on_pushButtonAddBTKey_clicked()
{
	dbgprintf("add bt\n");
	BtSelectDlg dlg(this);
	dlg.exec();
}

void MainWindow::on_pushButtonAddRdpKey_clicked()
{
	dbgprintf("add rdp key\n");
	RdpKeyDlg dlg(this);
	dlg.exec();
}

void MainWindow::trayClicked(QSystemTrayIcon::ActivationReason e)
{
	switch (e) {
	case QSystemTrayIcon::Context:
	{
		dbgprintf("trayClicked context enter\n");
		trayIconMenu();
		dbgprintf("trayClicked context leave\n");
		break;
	}
	case QSystemTrayIcon::Trigger:
		on_viewKeys();
		break;
	default:
		break;
	}
}

void MainWindow::on_pushButton_close_clicked()
{
    dbgprintf("quit button\n");
    close();
//	qApp->QCoreApplication::quit();
}

void MainWindow::on_actionAboutDlg()
{
    dbgprintf("AboutDlg\n");
	AboutDlg dlg(this);
    dlg.exec();
}

int MainWindow::slot_confirmAcceptDlg(struct ConfirmAcceptDlgInfo *info)
{
	dbgprintf("MainWindow::slot_confirmAcceptDlg() enter\n");
    ConfirmAcceptDlg dlg(this, info);
	showAndBringFront(&dlg);
    int r = dlg.exec();
	dbgprintf("MainWindow::slot_confirmAcceptDlg() leave\n");
	return r;
}

int MainWindow::slot_passphraseDlg(struct PassphraseDlgInfo *info)
{
	dbgprintf("MainWindow::slot_passphraseDlg() enter\n");
    passphrase dlg(this, info);
	showAndBringFront(&dlg);
    int r = dlg.exec();
	dbgprintf("MainWindow::slot_passphraseDlg() leave\n");
	return r;
}

void MainWindow::on_actionhelp_triggered()
{
	dbgprintf("on_help_triggerd()\n");
    launch_help_id(NULL, WINHELP_CTXID_errors_hostkey_absent);
}

void MainWindow::on_actionabout_triggered()
{
	dbgprintf("on_actionabout_triggered()\n");
	on_actionAboutDlg();
}

void MainWindow::on_viewKeys()
{
	dbgprintf("on_viewKeys()\n");
    keyviewdlg dlg(this);
	showAndBringFront(&dlg);
    int r = dlg.exec();
    (void)r;
}

void MainWindow::on_session(QAction *action)
{
	dbgprintf("on_session enter\n");
	int value = action->data().toInt();

	std::wstring param;
	if (value >= 0) {
		std::vector<std::wstring> session_info = setting_get_putty_sessions();
		param = L"@" + session_info[value];
	}

	const std::wstring putty_path = get_putty_path();
	exec(putty_path.c_str(), param.c_str());

	dbgprintf("on_session %d leave\n", value);
}

// CAPI Cert
void MainWindow::on_pushButton_clicked()
{
	dbgprintf("CAPI Cert\n");
#ifdef PUTTY_CAC
	//HWND hwnd = (HWND)winId();
	HWND hwnd = NULL;
	char * szCert = cert_prompt(IDEN_CAPI, hwnd);
	if (szCert == NULL)
		return;
	Filename *fn = filename_from_str(szCert);
	::add_keyfile(fn);
	addStartupKeyfile(fn->path);

	filename_free(fn);
#else
    MessageBoxA((HWND)winId(), "not support", "(^_^)",
				MB_OK | MB_ICONEXCLAMATION);
#endif
}

// PKCS Cert @@
void MainWindow::on_pushButton_2_clicked()
{
	dbgprintf("PKCS Cert\n");
#if 1
	{
		//HWND hwnd = (HWND)winId();
		HWND hwnd = NULL;
		char * szCert = cert_prompt(IDEN_PKCS, hwnd);
		if (szCert == NULL)
			return;
		Filename *fn = filename_from_str(szCert);
		::add_keyfile(fn);

		addStartupKeyfile(fn->path);

		filename_free(fn);
	}
#endif
#if 0
    MessageBoxA((HWND)winId(), "not support", "(^_^)",
				MB_OK | MB_ICONEXCLAMATION);
#endif
}

void agents_stop()
{
	winpgnt_stop();
	ssh_agent_native_unixdomain_stop();
	ssh_agent_cygwin_unixdomain_stop();
	pipe_th_stop();
	bt_agent_proxy_main_exit();
	ssh_agent_localhost_stop();
	rdpSshRelayServerStop();
}
		  
void agents_start()
{
	if (setting_get_bool("ssh-agent/pageant")) {
		bool r = winpgnt_start();
		if (r == false) {
			setting_set_bool("ssh-agent/pageant", false);
		}
	}

	if (setting_get_bool("ssh-agent/native_unix_socket")) {
		bool r = false;
		std::wstring sock_path = setting_get_str("ssh-agent/sock_path", nullptr);
			
		if (!sock_path.empty()) {
			r = ssh_agent_native_unixdomain_start(sock_path.c_str());
		}
		if (r == false) {
			setting_set_bool("ssh-agent/native_unix_socket", false);
		}
	}

	if (setting_get_bool("ssh-agent/cygwin_sock")) {
		bool r = false;
		std::wstring sock_path = setting_get_str("ssh-agent/sock_path_cygwin", nullptr);
		if (!sock_path.empty())
		{
			r = ssh_agent_cygwin_unixdomain_start(sock_path.c_str());
		}
		if (r == false) {
			setting_set_bool("ssh-agent/cygwin_sock", false);
		}
	}

	if (setting_get_bool("ssh-agent/ms_ssh")) {
		bool r = false;
		std::wstring sock_path = setting_get_str("ssh-agent/sock_path_cygwin", nullptr);
		if (!sock_path.empty())
		{
			r = pipe_th_start(sock_path.c_str());
		}
		if (r == false) {
			setting_set_bool("ssh-agent/ms_ssh", false);
		}
	}

	if (setting_get_bool("bt/enable")) {
		int timeout = setting_get_int("bt/timeout", 10*1000);
		bool r = bt_agent_proxy_main_init(timeout);
		if (r == false) {
			bt_agent_proxy_main_exit();
			setting_set_bool("bt/enable", false);
		}
	}

	if (setting_get_bool("ssh-agent_tcp/enable")) {
		const int port_no =
			setting_get_int("ssh-agent_tcp/port_no", 8080);
		ssh_agent_localhost_start(port_no);
	}
	
	if (setting_get_bool("rdpvc-relay-server/enable")) {
		bool r = rdpSshRelayServerStart();
		if (r == false) {
			setting_set_bool("rdpvc-relay-server/enable", false);
		}
	}
}
	
void MainWindow::on_actionsetting_triggered()
{
    dbgprintf("setting\n");

	SettingDlg dlg(this);
    dlg.exec();

	agents_stop();
	agents_start();
}

void MainWindow::lockTerminal()
{
	if (setting_get_bool("key/forget_when_terminal_locked", false)) {
		setting_clear_keyfiles();
		pageant_delete_ssh1_key_all();
		pageant_delete_ssh2_key_all();
	}
	if (setting_get_bool("Passphrase/forget_when_terminal_locked", false)) {
		passphrase_forget();
		passphrase_remove_setting();
	}
	if (setting_get_bool("SmartCardPin/forget_when_terminal_locked", false)) {
		cert_forget_pin();
	}
}

bool MainWindow::nativeEventFilter(const QByteArray &eventType, void *message, long *result)
{
	(void)result;
	if (eventType == "windows_generic_MSG"){
		MSG *msg = static_cast<MSG *>(message);
		WPARAM w = msg->wParam;
		LPARAM lParam = msg->lParam;
		switch (msg->message) {
		case WM_POWERBROADCAST:
			dbgprintf("WM_POWERBROADCAST(%d) wParam=%s(%d)\n",
					  msg->message,
					  w == PBT_APMPOWERSTATUSCHANGE ? "PBT_APMPOWERSTATUSCHANGE" :
					  w == PBT_APMRESUMEAUTOMATIC ? "PBT_APMRESUMEAUTOMATIC" :
					  w == PBT_APMRESUMESUSPEND ? "PBT_APMRESUMESUSPEND" :
					  w == PBT_APMSUSPEND ? "PBT_APMSUSPEND" :
					  w == PBT_POWERSETTINGCHANGE ? "PBT_POWERSETTINGCHANGE" :
					  "??",
					  w);
			if (w == PBT_APMSUSPEND) {
				lockTerminal();
			}
			break;
		case WM_WTSSESSION_CHANGE:
			dbgprintf("WM_WTSSESSION_CHANGE(%d) wParam=%s(%d) lParam=%d\n",
					  msg->message,
					  w == WTS_CONSOLE_CONNECT ? "WTS_CONSOLE_CONNECT" :
					  w == WTS_CONSOLE_DISCONNECT ? "WTS_CONSOLE_DISCONNECT" :
					  w == WTS_REMOTE_CONNECT ? "WTS_REMOTE_CONNECT" :
					  w == WTS_REMOTE_DISCONNECT ? "WTS_REMOTE_DISCONNECT" :
					  w == WTS_SESSION_LOGON ? "WTS_SESSION_LOGON" :
					  w == WTS_SESSION_LOGOFF ? "WTS_SESSION_LOGOFF" :
					  w == WTS_SESSION_LOCK ? "WTS_SESSION_LOCK" :
					  w == WTS_SESSION_UNLOCK ? "WTS_SESSION_UNLOCK" :
					  w == WTS_SESSION_REMOTE_CONTROL ? "WTS_SESSION_REMOTE_CONTROL" :
					  w == WTS_SESSION_CREATE ? "WTS_SESSION_CREATE" :
					  w == WTS_SESSION_TERMINATE ? "WTS_SESSION_TERMINATE" :
					  "??",
					  w,
					  lParam);
			if (w == WTS_SESSION_LOCK) {
				lockTerminal();
			}
			break;
		case WM_DEVICECHANGE:
			dbgprintf("WM_DEVICECHANGE(%d) wParam=%s(0x%04x)\n",
					  msg->message,
					  w == DBT_CONFIGCHANGECANCELED ? "DBT_CONFIGCHANGECANCELED":
					  w == DBT_CONFIGCHANGED ? "DBT_CONFIGCHANGED":
					  w == DBT_CUSTOMEVENT ? "DBT_CUSTOMEVENT":
					  w == DBT_DEVICEARRIVAL ? "DBT_DEVICEARRIVAL":
					  w == DBT_DEVICEQUERYREMOVE ? "DBT_DEVICEQUERYREMOVE":
					  w == DBT_DEVICEQUERYREMOVEFAILED ? "DBT_DEVICEQUERYREMOVEFAILED":
					  w == DBT_DEVICEREMOVECOMPLETE ? "DBT_DEVICEREMOVECOMPLETE":
					  w == DBT_DEVICEREMOVEPENDING ? "DBT_DEVICEREMOVEPENDING":
					  w == DBT_DEVICETYPESPECIFIC ? "DBT_DEVICETYPESPECIFIC":
					  w == DBT_DEVNODES_CHANGED ? "DBT_DEVNODES_CHANGED":
					  w == DBT_QUERYCHANGECONFIG ? "DBT_QUERYCHANGECONFIG":
					  w == DBT_USERDEFINED ? "DBT_USERDEFINED":
					  "??",
					  w);
			cert_pkcs11dll_finalize();
			break;
		}
	}
	return false;
}

int MainWindow::confirmAcceptDlg(struct ConfirmAcceptDlgInfo *info)
{
	int r = emit signal_confirmAcceptDlg(info);
	return r;
}

int MainWindow::passphraseDlg(struct PassphraseDlgInfo *info)
{
	int r = emit signal_passphraseDlg(info);
	return r;
}

//////////////////////////////////////////////////////////////////////////////

DIALOG_RESULT_T confirmAcceptDlg(struct ConfirmAcceptDlgInfo *info)
{

	int r = gWin->confirmAcceptDlg(info);
	dbgprintf("r=%s(%d)\n",
				 r == QDialog::Accepted ? "QDialog::Accepted" :
				 r == QDialog::Rejected ? "QDialog::Rejected" :
				 "??",
				 r);
	DIALOG_RESULT_T retval = (r == QDialog::Accepted) ? DIALOG_RESULT_OK : DIALOG_RESULT_CANCEL;
	return retval;
}

//////////////////////////////////////////////////////////////////////////////

void addCAPICert()
{
	gWin->on_pushButton_clicked();
}

void addPKCSCert()
{
	gWin->on_pushButton_2_clicked();
}

void addFileCert()
{
	gWin->on_pushButtonAddKey_clicked();
}

void addBtCert()
{
	gWin->on_pushButtonAddBTKey_clicked();
}

// return
DIALOG_RESULT_T passphraseDlg(struct PassphraseDlgInfo *info)
{
	int r = gWin->passphraseDlg(info);
	return r == QDialog::Accepted ? DIALOG_RESULT_OK : DIALOG_RESULT_CANCEL;
}

HWND get_hwnd()
{
	return (HWND)gWin->winId();
}

static bool add_keyfile(const Filename *fn)
{
	std::string comment;
	bool result = false;
	/*
     * Try loading the key without a passphrase. (Or rather, without a
     * _new_ passphrase; pageant_add_keyfile will take care of trying
     * all the passphrases we've already stored.)
     */
    char *err;
    int ret = pageant_add_keyfile(fn, NULL, &err);
    if (ret == PAGEANT_ACTION_OK) {
		result = true;
        goto done;
    } else if (ret == PAGEANT_ACTION_FAILURE) {
		goto error;
    } else if (ret == PAGEANT_ACTION_NEED_PP) {
		{
			auto passphrases = passphrase_get_array();
			for(auto &&passphrase : passphrases) {
				ret = pageant_add_keyfile(fn, passphrase.c_str(), &err);
				passphrase.clear();
				if (ret == PAGEANT_ACTION_OK) {
					passphrases.clear();
					result = true;
					goto done;
				}
			}
			passphrases.clear();
		}

		/*
		 * OK, a passphrase is needed, and we've been given the key
		 * comment to use in the passphrase prompt.
		 */
//		comment = err;
//		comment += "\n";
		comment += wc_to_mb(std::wstring(fn->path));
		while (1) {
			char *_passphrase;
			struct PassphraseDlgInfo pps;
			pps.passphrase = &_passphrase;
			pps.caption = "pageant+";
			pps.text = comment.c_str();
			pps.save = 0;
			pps.saveAvailable = setting_get_bool("Passphrase/save_enable", false);
			DIALOG_RESULT_T r;
#if 0
			{
				r = passphraseDlg(&pps);
			}
#else
			{
				passphrase dlg(gWin, &pps);
				showAndBringFront(&dlg);
				int r2 = dlg.exec();
				r = r2 == QDialog::Accepted ? DIALOG_RESULT_OK : DIALOG_RESULT_CANCEL;
			}
		
#endif
			if (r == DIALOG_RESULT_CANCEL) {
				// cancell	
				goto done;
			}

			sfree(err);
			err = NULL;

			assert(_passphrase != NULL);

			ret = pageant_add_keyfile(fn, _passphrase, &err);
			if (ret == PAGEANT_ACTION_OK) {
				if (pps.save != 0) {
					passphrase_add(_passphrase);
					passphrase_save_setting(_passphrase);
				}
				result = true;
				goto done;
			} else if (ret == PAGEANT_ACTION_FAILURE) {
				goto error;
			}

			smemclr(_passphrase, strlen(_passphrase));
			sfree(_passphrase);
			_passphrase = NULL;
		}
	}

error:
    message_boxA(err, APPNAME, MB_OK | MB_ICONERROR, 0);
	result = false;
done:
    sfree(err);
	return result;
}

void add_keyfile(const wchar_t *filename)
{
	Filename *fn = filename_from_wstr(filename);
	add_keyfile(fn);
	filename_free(fn);
}

void add_keyfile(const std::vector<std::wstring> &keyfileAry)
{
	std::vector<std::string> bt_files;
	std::vector<std::wstring> normal_files;

	// BTとその他を分離
	for(const auto &f: keyfileAry) {
		if (wcsncmp(L"btspp://", f.c_str(), 8) == 0)  {
			std::string utf8 = wc_to_utf8(f.c_str());
			bt_files.emplace_back(utf8);
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
}


/*
 * Warn about the obsolescent key file format.
 */
void old_keyfile_warning(void)
{
    static const char mbtitle[] = "PuTTY Key File Warning";
    static const char message[] =
	"You are loading an SSH-2 private key which has an\n"
	"old version of the file format. This means your key\n"
	"file is not fully tamperproof. Future versions of\n"
	"PuTTY may stop supporting this private key format,\n"
	"so we recommend you convert your key to the new\n"
	"format.\n"
	"\n"
	"You can perform this conversion by loading the key\n"
	"into PuTTYgen and then saving it again.";

	HWND hwnd = get_hwnd();
    MessageBox(hwnd, message, mbtitle, MB_OK);
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
