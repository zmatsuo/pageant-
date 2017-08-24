#undef UNICODE
//#define STRICT

#include <assert.h>

#include <windows.h>
#include <Wtsapi32.h>
#include <Dbt.h>

#include <QIcon>
#include <QFileDialog>
#include <QDesktopServices>
#include <QCloseEvent>
#include <QStringListModel>
#include <QClipboard>

#include "pageant+.h"
#include "mainwindow.h"
#include "winpgnt.h"
#include "debug.h"
#include "winhelp_.h"
#include "misc.h"
#include "misc_cpp.h"
#include "winmisc.h"
#include "filename.h"
#include "setting.h"
#include "passphrasedlg.h"
#include "aboutdlg.h"
#include "settingdlg.h"
#include "confirmacceptdlg.h"
#include "gui_stuff.h"
#include "ssh-agent_emu.h"
#include "ssh-agent_ms.h"
#include "pageant.h"
#include "winutils.h"
#include "ssh.h"

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

static MainWindow *win;
void add_keyfile(const Filename *fn);

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
	quitGuard_ = true;
    createTrayIcon();
    trayIcon->show();
    ui->setupUi(this);
	win = this;

	connect(this, SIGNAL(signal_confirmAcceptDlg(struct ConfirmAcceptDlgInfo *)),
			this, SLOT(slot_confirmAcceptDlg(struct ConfirmAcceptDlgInfo *)),
			Qt::BlockingQueuedConnection);
	connect(this, SIGNAL(signal_passphraseDlg(struct PassphraseDlgInfo *)),
			this, SLOT(slot_passphraseDlg(struct PassphraseDlgInfo *)),
			Qt::BlockingQueuedConnection);

    //
//    ui->treeView->setRootIsDecorated(false);
//    ui->treeView->setAlternatingRowColors(true);
	setWindowFlags( (windowFlags() | Qt::CustomizeWindowHint) & ~Qt::WindowMaximizeButtonHint);

	setWindowIcon(QIcon(":/images/pageant.png"));
	setWindowTitle(APP_NAME " - Key List");

#if defined(__MINGW32__)
    BOOL r = WTSRegisterSessionNotification(
        (HWND)winId(), NOTIFY_FOR_ALL_SESSIONS);
    debug("WTSRegisterSessionNotification() %d\n", r);
#else
	BOOL r = WTSRegisterSessionNotificationEx(
		WTS_CURRENT_SERVER, (HWND)winId(), NOTIFY_FOR_ALL_SESSIONS);
	debug("WTSRegisterSessionNotificationEx() %d\n", r);
#endif
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
	s += "(";
	s += tr("PuTTY authentication agent");
	s += ")";
	trayIcon->setToolTip(s);

	connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
			this, SLOT(trayClicked(QSystemTrayIcon::ActivationReason)));
}

void MainWindow::createTrayIconMenu()
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

    QAction *restoreAction = new QAction("&View Keys", this);
    connect(restoreAction, &QAction::triggered, this, &QWidget::showNormal);
    trayIconMenu->addAction(restoreAction);

    QAction *addKeyAction = new QAction("Add &Key", this);
	connect(addKeyAction, SIGNAL(triggered()), this, SLOT(on_pushButtonAddKey_clicked()));
    trayIconMenu->addAction(addKeyAction);

    QAction *settingAction = new QAction("&Setting", this);
	connect(settingAction, SIGNAL(triggered()), this, SLOT(on_actionsetting_triggered()));
    trayIconMenu->addAction(settingAction);

	if (has_help()) {
		QAction *helpAction = new QAction(tr("help"), this);
		connect(helpAction, SIGNAL(triggered()), this, SLOT(on_help_clicked()));
		trayIconMenu->addAction(helpAction);
	}

    trayIconMenu->addSeparator();

    QAction *aboutAction = new QAction(tr("About"), this);
	connect(aboutAction, SIGNAL(triggered()), this, SLOT(on_actionAboutDlg()));
    trayIconMenu->addAction(aboutAction);

    trayIconMenu->addSeparator();

	QAction *quitAction = new QAction(tr("&Quit"), this);
    connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);
    trayIconMenu->addAction(quitAction);

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

void MainWindow::keylist_update()
{
	std::vector<KeyListItem> k = keylist_update2();
	QStandardItemModel *model;

	const int colum = 1;
	model = new QStandardItemModel((int)k.size(), colum, this);
	for (int i=0; i<(int)k.size(); i++) {
		QStandardItem *item;
#if 0
		item = new QStandardItem(QString("%0").arg(i));
		model->setItem(i, 0, item);
#endif
		item = new QStandardItem(QString::fromStdString(k[i].name));
		model->setItem(i, 0, item);
#if 0
		item = new QStandardItem(QString("comment"));
		model->setItem(i, 2, item);
#endif
	}


#if 0
	model->setHeaderData(0, Qt::Horizontal, QObject::tr("no"));
#endif
    model->setHeaderData(0, Qt::Horizontal, QObject::tr("key"));
#if 0
    model->setHeaderData(2, Qt::Horizontal, QObject::tr("test"));
#endif
    ui->treeView->setModel(model);
}

//////////////////////////////////////////////////////////////////////////////

/*
 * This function loads a key from a file and adds it.
 */
void MainWindow::add_keyfile(const QString &filename)
{
	Filename *fn = filename_from_str(filename.toStdString().c_str());
	::add_keyfile(fn);
}

//////////////////////////////////////////////////////////////////////////////

void MainWindow::on_pushButtonAddKey_clicked()
{
	debug_printf("add\n");

    QString folder = get_ssh_folder();
    QString caption = "Select Private Key File";
    QString filter = QString::fromUtf8(
		u8""
		"ppk (*.ppk);;"
		"All Files (*.*)"
		);

    QStringList files = QFileDialog::getOpenFileNames(this, caption, folder, filter);

    debug_printf("select count %d\n", files.size());
	for (auto &f: files) {
		add_keyfile(f);
    }

	keylist_update();

	if (files.size() > 0) {
		int r = message_boxW(L"次回起動時に読み込みますか?", L"pagent+", MB_YESNO, 0);
		if (r == IDYES) {
			for (auto &f: files) {
				setting_add_keyfile(f.toStdWString().c_str());
			}
		}
	}
}

void MainWindow::changeEvent(QEvent *e)
{
	if(e->type() == QEvent::WindowStateChange) {
		QWindowStateChangeEvent* event = static_cast<QWindowStateChangeEvent*>(e);

		if (event->oldState() == Qt::WindowNoState &&
			windowState() == Qt::WindowMinimized &&
			setting_get_bool("general/minimize_to_notification_area"))
		{
			hide();
		}
	}
	e->accept();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
	if (setting_get_bool("general/close_to_notification_area") &&
		quitGuard_)
	{
		event->ignore();
		hide();
#if 0
		{	// TODO: ちょっとした説明
			static bool message_once = false;
			if (!message_once) {
				message_once = true;
				trayIcon->showMessage(tr(u8"title here"), tr(u8"つついたら戻るよ"));
			}
		}
#endif
	}
}

static void showAndBringFront(QWidget *win)
{
#if 1
	Qt::WindowFlags eFlags = win->windowFlags();
	eFlags |= Qt::WindowStaysOnTopHint;
	win->setWindowFlags(eFlags);
	win->show();
	eFlags &= ~Qt::WindowStaysOnTopHint;
	win->setWindowFlags(eFlags);
#endif
#if 0
    win->show();
    win->activateWindow();
    win->raise();
#endif
}

void MainWindow::trayClicked(QSystemTrayIcon::ActivationReason e)
{
	switch (e) {
	case QSystemTrayIcon::Context:
	{
		debug_printf("trayClicked context enter\n");
		createTrayIconMenu();
		debug_printf("trayClicked context leave\n");
		break;
	}
	case QSystemTrayIcon::Trigger:
		if(isVisible() == true)
			hide();
		else {
#if 0
			showAndBringFront(this);
#endif
#if 1
			showNormal();
			show();
			raise();
			activateWindow();
#endif
		}
		break;
	default:
		break;
	}
}

void MainWindow::on_pushButtonRemoveKey_clicked()
{
    debug_printf("remove\n");

	QItemSelectionModel *selection = ui->treeView->selectionModel();
	QModelIndexList indexes = selection->selectedRows();
	const int count = indexes.count();
	if (count == 0) {
		message_boxW(L"鍵が選択されていません", L"pagent+", MB_OK, 0);
	} else {
		std::vector<int> selectedArray;
		for (int i = 0; i < count; i++) {
			selectedArray.push_back(indexes[i].row());
		}
		std::sort(selectedArray.begin(), selectedArray.end());

		pageant_delete_key2((int)selectedArray.size(), &selectedArray[0]);
	}
	keylist_update();
}

void MainWindow::on_pushButton_close_clicked()
{
    debug_printf("quit button\n");
	quitGuard_ = true;
    close();
//	qApp->QCoreApplication::quit();
}

void MainWindow::on_actionAboutDlg()
{
    debug_printf("AboutDlg\n");
	AboutDlg dlg(this);
    dlg.exec();
}

int MainWindow::slot_confirmAcceptDlg(struct ConfirmAcceptDlgInfo *info)
{
	debug_printf("MainWindow::slot_confirmAcceptDlg() enter\n");
    ConfirmAcceptDlg dlg(this, info);
	showAndBringFront(&dlg);
    int r = dlg.exec();
	debug_printf("MainWindow::slot_confirmAcceptDlg() leave\n");
	return r;
}

int MainWindow::slot_passphraseDlg(struct PassphraseDlgInfo *info)
{
	debug_printf("MainWindow::slot_passphraseDlg() enter\n");
    passphrase dlg(this, info);
	showAndBringFront(&dlg);
    int r = dlg.exec();
	debug_printf("MainWindow::slot_passphraseDlg() leave\n");
	return r;
}

void MainWindow::on_actionhelp_triggered()
{
	debug_printf("on_help_triggerd()\n");
    launch_help_id(NULL, WINHELP_CTXID_errors_hostkey_absent);
}

void MainWindow::on_actionabout_triggered()
{
	debug_printf("on_actionabout_triggered()\n");
	on_actionAboutDlg();
}

void MainWindow::on_session(QAction *action)
{
	debug_printf("on_session enter\n");
	int value = action->data().toInt();

	std::wstring param;
	if (value >= 0) {
		std::vector<std::wstring> session_info = setting_get_putty_sessions();
		param = L"@" + session_info[value];
	}

	const std::wstring putty_path = get_putty_path();
	exec(putty_path.c_str(), param.c_str());

	debug_printf("on_session %d leave\n", value);
}

// CAPI Cert
void MainWindow::on_pushButton_clicked()
{
	debug_printf("CAPI Cert\n");
#ifdef PUTTY_CAC
	//HWND hwnd = (HWND)winId();
	HWND hwnd = NULL;
	char * szCert = cert_prompt(IDEN_CAPI, hwnd);
	if (szCert == NULL)
		return;
	Filename *fn = filename_from_str(szCert);
	::add_keyfile(fn);
	keylist_update();

	int r = message_boxW(L"次回起動時に読み込みますか?", L"pagent+", MB_YESNO, 0);
	if (r == IDYES) {
		setting_add_keyfile(fn->path);
	}

	filename_free(fn);
#else
    MessageBoxA((HWND)winId(), "not support", "(^_^)",
				MB_OK | MB_ICONEXCLAMATION);
#endif
}

// PKCS Cert @@
void MainWindow::on_pushButton_2_clicked()
{
	debug_printf("PKCS Cert\n");
#if 1
	{
		//HWND hwnd = (HWND)winId();
		HWND hwnd = NULL;
		char * szCert = cert_prompt(IDEN_PKCS, hwnd);
		if (szCert == NULL)
			return;
		Filename *fn = filename_from_str(szCert);
		::add_keyfile(fn);
		keylist_update();

		int r = message_boxW(L"次回起動時に読み込みますか?", L"pagent+", MB_YESNO, 0);
		if (r == IDYES) {
			setting_add_keyfile(fn->path);
		}

		filename_free(fn);
	}
#endif
#if 0
    MessageBoxA((HWND)winId(), "not support", "(^_^)",
				MB_OK | MB_ICONEXCLAMATION);
#endif
}

void MainWindow::on_actionsetting_triggered()
{
    debug_printf("setting\n");
	std::wstring unixdomain_path = _getenv(L"SSH_AUTH_SOCK");
    bool unixdomain_enable_prev = setting_get_bool("ssh-agent/cygwin_sock");
    bool pageant_enable_prev = setting_get_bool("ssh-agent/pageant");
	bool ms_agent_enable_prev = setting_get_bool("ssh-agent/ms_ssh");
	SettingDlg dlg(this);
    dlg.exec();

	// サービス再起動が必要
	const bool reboot_services = (unixdomain_path != _getenv(L"SSH_AUTH_SOCK"));
    if (reboot_services) {
		// すべて止める
		winpgnt_stop();
		ssh_agent_server_stop();
		pipe_th_stop();
		pageant_enable_prev = false;
		unixdomain_enable_prev = false;
		ms_agent_enable_prev = false;
	}

    bool unixdomain_enable = setting_get_bool("ssh-agent/cygwin_sock");
    bool pageant_enable = setting_get_bool("ssh-agent/pageant");
	bool ms_agent_enable = setting_get_bool("ssh-agent/ms_ssh");
    if (pageant_enable_prev != pageant_enable) {
		if (pageant_enable) {
			winpgnt_start();
		} else {
			winpgnt_stop();
		}
	}
	if (unixdomain_enable_prev != unixdomain_enable) {
		if (unixdomain_enable) {
			ssh_agent_server_start(unixdomain_path.c_str());
		} else {
			ssh_agent_server_stop();
		}
	}
	if (ms_agent_enable_prev != ms_agent_enable) {
		if (ms_agent_enable) {
			pipe_th_start(unixdomain_path.c_str());
		} else {
			pipe_th_stop();
		}
	}
}

bool MainWindow::nativeEventFilter(const QByteArray &eventType, void *message, long *result)
{
	(void)result;
	if (eventType == "windows_generic_MSG"){
		MSG *msg = static_cast<MSG *>(message);
		WPARAM w = msg->wParam;
		switch (msg->message) {
		case WM_POWERBROADCAST:
			debug_printf("WM_POWERBROADCAST(%d) wParam=%s(%d)\n",
						 msg->message,
						 w == PBT_APMPOWERSTATUSCHANGE ? "PBT_APMPOWERSTATUSCHANGE" :
						 w == PBT_APMRESUMEAUTOMATIC ? "PBT_APMRESUMEAUTOMATIC" :
						 w == PBT_APMRESUMESUSPEND ? "PBT_APMRESUMESUSPEND" :
						 w == PBT_APMSUSPEND ? "PBT_APMSUSPEND" :
						 w == PBT_POWERSETTINGCHANGE ? "PBT_POWERSETTINGCHANGE" :
						 "??",
						 w);
			if (w == PBT_APMSUSPEND) {
				if (setting_get_bool("Passphrase/forget_when_terminal_locked", false)) {
					pageant_forget_passphrases();
					setting_remove_passphrases();
				}
				if (setting_get_bool("SmartCardPin/forget_when_terminal_locked", false)) {
					cert_forget_pin();
				}
			}
			break;
		case WM_WTSSESSION_CHANGE:
			debug_printf("WM_WTSSESSION_CHANGE(%d) wParam=%s(%d)\n",
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
						 w);
			if (w == WTS_SESSION_LOCK) {
				if (setting_get_bool("Passphrase/forget_when_terminal_locked", false)) {
					pageant_forget_passphrases();
					setting_remove_passphrases();
				}
				if (setting_get_bool("SmartCardPin/forget_when_terminal_locked", false)) {
					cert_forget_pin();
				}
			}
			break;
		case WM_DEVICECHANGE:
			debug_printf("WM_DEVICECHANGE(%d) wParam=%s(0x%04x)\n",
						 msg->message,
						 w == DBT_DEVICEREMOVECOMPLETE ? "DBT_DEVICEREMOVECOMPLETE" :
						 "??",
						 w);
			cert_pkcs11dll_finalize();
			break;
		}
	}
	return false;
}

// clipboard
void MainWindow::on_pushButton_3_clicked()
{
    debug_printf("pubkey to clipboard\n");

	const QStandardItemModel *model = (QStandardItemModel *)ui->treeView->model();
	QItemSelectionModel *selection = ui->treeView->selectionModel();
	QModelIndexList indexes = selection->selectedRows();
	switch (indexes.count()) {
	case 0:
		message_boxW(L"鍵が選択されていません", L"pagent+", MB_OK, 0);
		break;
	case 1:
	{
		const QModelIndex index = selection->currentIndex();
		QStandardItem *item = model->itemFromIndex(index);
		if (item == nullptr)
			return;
		QString qs = item->text();

		std::string s = qs.toStdString();
		std::string::size_type p = s.find_last_of(' ');
		s = s.substr(0, p);

		char *pubkey = pageant_get_pubkey(s.c_str());
		QApplication::clipboard()->setText(QString(pubkey));
		free(pubkey);

		message_boxW(L"OpenSSH形式の公開鍵をクリップボードにコピーしました", L"pagent+", MB_OK, 0);

		break;
	}
	default:
		message_boxW(L"1つだけ選択してください", L"pagent+", MB_OK, 0);
		break;
	}
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
	int r = win->confirmAcceptDlg(info);
	debug_printf("r=%s(%d)\n",
				 r == QDialog::Accepted ? "QDialog::Accepted" :
				 r == QDialog::Rejected ? "QDialog::Rejected" :
				 "??",
				 r);
	DIALOG_RESULT_T retval = (r == QDialog::Accepted) ? DIALOG_RESULT_OK : DIALOG_RESULT_CANCEL;
	return retval;
}

// return
DIALOG_RESULT_T passphraseDlg(struct PassphraseDlgInfo *info)
{
	int r = win->passphraseDlg(info);
	return r == QDialog::Accepted ? DIALOG_RESULT_OK : DIALOG_RESULT_CANCEL;
}


void keylist_update(void)
{
	win->keylist_update();
}

HWND get_hwnd()
{
	return (HWND)win->winId();
}

void add_keyfile(const Filename *fn)
{
	std::string comment;
	/*
     * Try loading the key without a passphrase. (Or rather, without a
     * _new_ passphrase; pageant_add_keyfile will take care of trying
     * all the passphrases we've already stored.)
     */
    char *err;
    int ret = pageant_add_keyfile(fn, NULL, &err);
    if (ret == PAGEANT_ACTION_OK) {
        goto done;
    } else if (ret == PAGEANT_ACTION_FAILURE) {
		goto error;
    }
	// PAGEANT_ACTION_NEED_PP

    /*
     * OK, a passphrase is needed, and we've been given the key
     * comment to use in the passphrase prompt.
     */
	comment = err;
	comment += "\n";
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
			passphrase dlg(win, &pps);
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
				add_passphrase(_passphrase);
				save_passphrases(_passphrase);
			}
			goto done;
        } else if (ret == PAGEANT_ACTION_FAILURE) {
			goto error;
        }

        smemclr(_passphrase, strlen(_passphrase));
        sfree(_passphrase);
        _passphrase = NULL;
    }

error:
    message_box(err, APPNAME, MB_OK | MB_ICONERROR, 0);
done:
    sfree(err);
}

void add_keyfile(const wchar_t *filename)
{
	Filename *fn = filename_from_wstr(filename);
	add_keyfile(fn);
	filename_free(fn);
	keylist_update();
}

void add_keyfile(const std::vector<std::wstring> &keyfileAry)
{
	for(auto &f: keyfileAry) {
		Filename *fn = filename_from_wstr(f.c_str());
		add_keyfile(fn);
		filename_free(fn);
	}
	keylist_update();
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

/*
 * Print a modal (Really Bad) message box and perform a fatal exit.
 */
void modalfatalbox(const char *fmt, ...)
{
    va_list ap;
    char *buf;

    va_start(ap, fmt);
    buf = dupvprintf(fmt, ap);
    va_end(ap);
	HWND hwnd = get_hwnd();
    MessageBox(hwnd, buf, "Pageant Fatal Error",
	       MB_SYSTEMMODAL | MB_ICONERROR | MB_OK);
    sfree(buf);
    exit(1);
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:



