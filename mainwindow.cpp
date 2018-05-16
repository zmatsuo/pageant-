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
//#define ENABLE_DEBUG_PRINT
#include "debug.h"
#include "winhelp_.h"
#include "misc.h"
#include "misc_cpp.h"
#include "winmisc.h"
#include "setting.h"
#include "gui_stuff.h"
#include "ssh-agent_emu.h"
#include "ssh-agent_ms.h"
#include "pageant.h"
#include "winutils.h"
#include "winutils_qt.h"
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
#include "keyfile.h"
#include "smartcard.h"

#include "ui_mainwindow.h"

#if defined(_MSC_VER)
#pragma comment(lib,"Wtsapi32.lib")
#endif

#define APPNAME			APP_NAME	// in pageant+.h

static MainWindow *gWin;
void setToolTip(const wchar_t *str);

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    createTrayIcon();
    tray_icon_->show();
    ui->setupUi(this);
	gWin = this;
	thread_id_ = GetCurrentThreadId();

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
	tray_icon_->hide();
	delete tray_icon_;
	tray_icon_ = nullptr;

    delete ui;
}

void MainWindow::createTrayIcon()
{
    QIcon icon = QIcon(":/images/pageant.png");
    tray_icon_ = new QSystemTrayIcon(this);
    tray_icon_->setIcon(icon);

	setToolTip(nullptr);

	connect(tray_icon_, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
			this, SLOT(trayClicked(QSystemTrayIcon::ActivationReason)));
}

void MainWindow::trayIconMenu()
{
    QMenu *tray_menu = new QMenu(this);

	if (!get_putty_path().empty()) {
		// putty
		QMenu *session_menu = tray_menu->addMenu( "putty" );
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

		tray_menu->addSeparator();
	}

    tray_menu->addAction(
		tr("&View Keys"),
		this, SLOT(on_viewKeys()));

    tray_menu->addAction(
		tr("Add &Key"),
		this, SLOT(on_pushButtonAddKey_clicked()));

	if (setting_get_bool("bt/enable", false)) {
		tray_menu->addAction(
			tr("Add BT Key"),
			this, SLOT(on_pushButtonAddBTKey_clicked()));
	}

	if (setting_get_bool("rdpvc-relay-server/enable", false) &&
		rdpSshRelayCheckServer() &&
		rdpSshRelayIsRemoteSession() &&
		rdpSshRelayCheckCom())
	{
		QString s = tr("Add RDP Key");
		s += "(" + QString::fromStdWString(rdpSshRelayGetClientName()) + ")";
		tray_menu->addAction(
			s,
			this, SLOT(on_pushButtonAddRdpKey_clicked()));
	}

	tray_menu->addAction(
		tr("&Setting"),
		this, SLOT(on_actionsetting_triggered()));

	if (has_help()) {
		tray_menu->addAction(
			tr("help"),
			this, SLOT(on_help_clicked()));
	}

    tray_menu->addSeparator();

	tray_menu->addAction(
		tr("About"),
		this, SLOT(on_actionAboutDlg()));

    tray_menu->addSeparator();

	tray_menu->addAction(
		tr("&Quit"),
		qApp, &QCoreApplication::quit);

	QAction *ret = tray_menu->exec(QCursor::pos());
	(void)ret;

	delete tray_menu;
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

	if (flist.size() == 0 ||
		std::find(flist.begin(), flist.end(), fn) == flist.end())
	{
		std::wostringstream oss;
		oss << L"次回起動時に読み込みますか?\n"
			<< fn;
		QWidget *w = getDispalyedWindow();
		int r = message_box(w, oss.str().c_str(), L"pageant+", MB_YESNO);
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
		"All Files (*.*)" ";;"
		"Putty (*.ppk)"
		);

    QStringList files = QFileDialog::getOpenFileNames(this, caption, folder, filter);

    dbgprintf("select count %d\n", files.size());
	if (files.size() == 0) {
		// cancel
		return;
	}

	std::vector<std::wstring> files_ws;
	for (auto &f: files) {
		std::wstring f_ws = f.toStdWString();
		ckey key;
		if (load_keyfile(f_ws.c_str(), key)) {
			keystore_add(key);
			files_ws.push_back(f_ws);
		} else {
			QWidget *w = getDispalyedWindow();
			std::wstring msg = L"load error " + f.toStdWString();
			message_box(w, msg.c_str(), L"pageant+", MB_OK|MB_ICONERROR);
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
	BtSelectDlg *dlg = BtSelectDlg::createInstance(0);
	if (dlg->isVisible()) {
		dlg->raise();
		dlg->activateWindow();
		return;
	}
	PushDisplayedWindow(dlg);
	dlg->exec();
	PopDisplayedWindow();
	delete dlg;
}

void MainWindow::on_pushButtonAddRdpKey_clicked()
{
	dbgprintf("add rdp key\n");
	RdpKeyDlg *dlg = RdpKeyDlg::createInstance(0);
	if (dlg->isVisible()) {
		dlg->raise();
		dlg->activateWindow();
		return;
	}
	PushDisplayedWindow(dlg);
	dlg->exec();
	PopDisplayedWindow();
	delete dlg;
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
	AboutDlg *dlg = AboutDlg::createInstance(0);
	if (dlg->isVisible()) {
		dlg->raise();
		dlg->activateWindow();
		return;
	}
	PushDisplayedWindow(dlg);
    dlg->exec();
	PopDisplayedWindow();
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
	QWidget *w = getDispalyedWindow();
    PassphraseDlg dlg(w, info);
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

    keyviewdlg *dlg = keyviewdlg::createInstance(0);
	if (dlg->isVisible()) {
		dlg->raise();
		dlg->activateWindow();
		return;
	}
	PushDisplayedWindow(dlg);
	dlg->exec();
	PopDisplayedWindow();
	delete dlg;
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
	HWND hWnd = reinterpret_cast<HWND>(winId());
	auto path = SmartcardSelectCAPI(hWnd);
	if (path.empty()) {
		// canceled
		return;
	}
	ckey key;
	if (!SmartcardLoad(path.c_str(), key)) {
		return;
	}
	if (!keystore_add(key)) {
		return;
	}
	addStartupKeyfile(utf8_to_wc(path));
}

// PKCS Cert @@
void MainWindow::on_pushButton_2_clicked()
{
	dbgprintf("PKCS Cert\n");
	HWND hWnd = reinterpret_cast<HWND>(winId());
	auto path = SmartcardSelectPKCS(hWnd);
	if (path.empty()) {
		// canceled
		return;
	}
	ckey key;
	if (!SmartcardLoad(path.c_str(), key)) {
		return;
	}
	if (!keystore_add(key)) {
		return;
	}
	addStartupKeyfile(utf8_to_wc(path));
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
		std::wstring sock_path = setting_get_str("ssh-agent/sock_path_ms", nullptr);
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

#if 0
	{
		std::ostringstream oss;
		oss << "pagent:"
			<< (setting_get_bool("ssh-agent/pageant") ? "on" : "off")
			<< "\n";
		oss << "native unix socket:"
			<< (setting_get_bool("ssh-agent/native_unix_socket") ? "on" : "off")
			<< "\n";
		oss << "cygwin unix socket:"
			<< (setting_get_bool("ssh-agent/cygwin_sock") ? "on" : "off")
			<< "\n";
		oss << "ms ssh:"
			<< (setting_get_bool("ssh-agent/ms_ssh") ? "on" : "off")
			<< "\n";
		oss << "bt:"
			<< (setting_get_bool("bt/enable") ? "on" : "off")
			<< "\n";
		oss << "tcp:"
			<< (setting_get_bool("ssh-agent_tcp/enable") ? "on" : "off")
			<< "\n";
		oss << "rdp server:"
			<< (setting_get_bool("rdpvc-relay-server/enable") ? "on" : "off")
			<< "\n";

		setToolTip(utf8_to_wc(oss.str()).c_str());
	}
#endif
}
	
void MainWindow::on_actionsetting_triggered()
{
    dbgprintf("setting\n");

	SettingDlg *dlg = SettingDlg::createInstance(0);
	if (dlg->isVisible()) {
		dlg->raise();
		dlg->activateWindow();
		return;
	}
	PushDisplayedWindow(dlg);
	dlg->exec();
	PopDisplayedWindow();
	delete dlg;

	agents_stop();
	agents_start();
}

void MainWindow::lockTerminal()
{
	if (setting_get_bool("key/forget_when_terminal_locked", false)) {
		setting_clear_keyfiles();
		keystore_remove_all();
	}
	if (setting_get_bool("Passphrase/forget_when_terminal_locked", false)) {
		passphrase_forget();
		passphrase_remove_setting();
	}
	if (setting_get_bool("SmartCardPin/forget_when_terminal_locked", false)) {
		SmartcardForgetPin();
	}
}

bool MainWindow::nativeEventFilter(const QByteArray &eventType, void *message, long *result)
{
	(void)result;
	if (eventType == "windows_generic_MSG") {
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
			SmartcardUnloadPKCS11dll();
			break;
		case WM_APP:
			on_viewKeys();
			break;
		case WM_APP + 1:
			exit(1);
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

void MainWindow::showTrayMessage(
	const wchar_t *title,
	const wchar_t *message)
{
	tray_icon_->showMessage(
		QString::fromStdWString(title),
		QString::fromStdWString(message));
}

void MainWindow::setToolTip(const wchar_t *str)
{
    QString s;
	s = APP_NAME;
	s += tr(TASK_TRAY_TIP);
#if defined(DEVELOP_VERSION)
	s += "\ndevelop version";
#endif

	if (str != nullptr) {
		auto s2 = QString::fromStdWString(str);
		s += "\n" + s2;
	}

	tray_icon_->setToolTip(s);
}

DWORD MainWindow::threadId()
{
	return thread_id_;
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

DIALOG_RESULT_T ShowPassphraseDlg(struct PassphraseDlgInfo *info)
{
	int r;
	const DWORD thread_id = GetCurrentThreadId();
	if (gWin->threadId() == thread_id)
	{
		QWidget *parent = getDispalyedWindow();
		PassphraseDlg dlg(parent, info);
		r = dlg.exec();
	}
	else
	{
		r = gWin->passphraseDlg(info);
	}
	return r == QDialog::Accepted ? DIALOG_RESULT_OK : DIALOG_RESULT_CANCEL;
}

void showTrayMessage(
	const wchar_t *title,
	const wchar_t *message)
{
	gWin->showTrayMessage(
		title,
		message);
}

void setToolTip(const wchar_t *str)
{
	if (gWin != nullptr) {
		gWin->setToolTip(str);
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
#if 0
	HWND hwnd = get_hwnd();
    MessageBox(hwnd, message, mbtitle, MB_OK);
#endif
	QWidget *w = getDispalyedWindow();
	message_box(w, utf8_to_wc(message).c_str(), utf8_to_wc(mbtitle).c_str(), MB_OK, 0);
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
