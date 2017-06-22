#undef UNICODE
//#define STRICT

#include <assert.h>

#include <windows.h>
#include <Wtsapi32.h>

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
#include "db.h"
#include "winhelp_.h"
#include "misc.h"
#include "winmisc.h"
#include "filename_.h"
#include "setting.h"
#include "passphrase.h"
#include "aboutdlg.h"
#include "settingdlg.h"
#include "confirmacceptdlg.h"
#include "gui_stuff.h"
#include "ssh-agent_emu.h"
#include "ssh-agent_ms.h"
#include "pageant.h"
#include "winutils.h"

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

static QStandardItemModel *model;		// TODO: メンバ変数化
static MainWindow *win;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
	quitGurad_ = true;
    createActions();
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
    ui->treeView->setRootIsDecorated(false);
    ui->treeView->setAlternatingRowColors(true);
	setWindowFlags( (windowFlags() | Qt::CustomizeWindowHint) & ~Qt::WindowMaximizeButtonHint);

	setWindowIcon(QIcon(":/images/pageant.png"));
	setWindowTitle(APP_NAME " - Key List");

//    trayIconMenu = NULL;

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
#if 0
    if (trayIconMenu != NULL) {
		delete trayIconMenu;
		trayIconMenu = NULL;
	}
#endif
}

void MainWindow::createActions()
{
//	if (QSystemTrayIcon::isSystemTrayAvailable()) {
		//connect(this, SIGNAL(closed()), this, &QWidget::hide);
		//connect(this, SIGNAL(closed()), this, SLOT(hide()));
//ok	connect(this, SIGNAL(closed()), this, SLOT(hide()));
//		connect(this, SIGNAL(closed()), this, SLOT(on_close()));

	// うまくいかない
#if 0
	connect(this, SIGNAL(onMinimize()), this, SLOT(on_close()), Qt::QueuedConnection);
	connect(this, SIGNAL(onMaximize()), this, SLOT(on_close()), Qt::QueuedConnection);
	connect(this, SIGNAL(onClose()), this, SLOT(on_close()), Qt::QueuedConnection);
	connect(this, SIGNAL(close()), this, SLOT(on_close()), Qt::QueuedConnection);
#endif
	
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
//    trayIcon->setContextMenu(trayIconMenu);

	connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
			this, SLOT(trayClicked(QSystemTrayIcon::ActivationReason)));
}

void MainWindow::createTrayIconMenu()
{
    QMenu *trayIconMenu;
#if 0
	if (trayIconMenu != NULL) {
		delete trayIconMenu;

		for(auto a : sessionActionAry){
			delete a;
		}
		sessionActionAry.resize(0);
	}
#endif

	trayIconMenu = new QMenu(this);
	if (!get_putty_path().empty()) {
		// putty
		QMenu *session_menu = trayIconMenu->addMenu( "putty" );
		std::vector<std::string> session_info = setting_get_putty_sessions();

		for (size_t i=0; i< session_info.size() + 1; i++) {
			QString s = i == 0 ? "&New session" : session_info[i-1].c_str();
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
	}

	trayIconMenu->addSeparator();

    restoreAction = new QAction("&View Keys(Restor)", this);
    connect(restoreAction, &QAction::triggered, this, &QWidget::showNormal);
    trayIconMenu->addAction(restoreAction);

    addKeyAction = new QAction("Add &Key", this);
	connect(addKeyAction, SIGNAL(triggered()), this, SLOT(on_pushButtonAddKey_clicked()));
    trayIconMenu->addAction(addKeyAction);

    confirmAnyAction = new QAction("&Confirm Any Request", this);
    trayIconMenu->addAction(confirmAnyAction);

	if (has_help()) {
		helpAction = new QAction(tr("help"), this);
		connect(helpAction, SIGNAL(triggered()), this, SLOT(on_help_clicked()));
		trayIconMenu->addAction(helpAction);
	}

    trayIconMenu->addSeparator();

    aboutAction = new QAction(tr("about"), this);
	connect(aboutAction, SIGNAL(triggered()), this, SLOT(on_actionAboutDlg()));
    trayIconMenu->addAction(aboutAction);

    trayIconMenu->addSeparator();

	minimizeAction = new QAction(tr("Mi&nimize"), this);
	connect(minimizeAction, &QAction::triggered, this, &QWidget::hide);
    trayIconMenu->addAction(minimizeAction);

    quitAction = new QAction(tr("&Quit"), this);
    connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);
    trayIconMenu->addAction(quitAction);

	QAction *ret = trayIconMenu->exec(QCursor::pos());
	(void)ret;

	delete trayIconMenu;
}

static QString get_ssh_folder()
{
    QByteArray home_ba = qgetenv("HOME");
    QString home = home_ba.constData();
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

#define printf	debug_printf

void MainWindow::keylist_update()
{
	std::vector<KeyListItem> k = keylist_update2();

	const int colum = 1;
	model = new QStandardItemModel(k.size(), colum, this);
	for (size_t i=0; i<k.size(); i++) {
		QStandardItem *item;
#if 0
		item = new QStandardItem(QString("%0").arg(i));
		model->setItem(i, 0, item);
#endif
		item = new QStandardItem(QString::fromUtf8(k[i].name.c_str()));
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

    /*
     * OK, a passphrase is needed, and we've been given the key
     * comment to use in the passphrase prompt.
     */
    while (1) {
		char *passphrase;
        struct PassphraseDlgInfo pps;
		pps.passphrase = &passphrase;
		pps.comment = "comment";
		pps.save = 0;
		int dlgret = slot_passphraseDlg(&pps);
        if (dlgret != QDialog::Accepted) {
			// cancell	
            goto done;
		}

        sfree(err);

        assert(passphrase != NULL);

        ret = pageant_add_keyfile(fn, passphrase, &err);
        if (ret == PAGEANT_ACTION_OK) {
            goto done;
        } else if (ret == PAGEANT_ACTION_FAILURE) {
            goto error;
        }

        smemclr(passphrase, strlen(passphrase));
        sfree(passphrase);
        passphrase = NULL;
    }

error:
    message_box(err, APPNAME, MB_OK | MB_ICONERROR, 0);
done:
    sfree(err);
}

//////////////////////////////////////////////////////////////////////////////

void MainWindow::on_pushButtonAddKey_clicked()
{
	debug_printf("add\n");

    QString folder = get_ssh_folder();
    QString cap =
		//QString::fromUtf8(u8"プライベートキーを開く(複数ok)")
		"Select Private Key File"
		;
    QString filter = QString::fromUtf8(
		u8""
		"ppk (*.ppk);;"
		"All Files (*.*)"
		);

    QStringList files = QFileDialog::getOpenFileNames( this, cap, folder, filter );

    debug_printf("select %d\n", files.size());
    if (files.size() == 0) {

    } else {
        for (int i = 0; i < files.size(); i++) {
#if 0
#if defined(_MSC_VER)
			std::string f = files[i].toStdString();
            debug_printf("file '%s'\n", f.c_str());
            Filename *ff = filename_from_str(f.c_str());
#else
            Filename ff = filename_from_str(files[i].toStdString().c_str());
#endif
//            add_keyfile(ff);
#endif
            add_keyfile(files[i]);
        }
    }

	keylist_update();
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
		quitGurad_)
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

//static void showAndBringFront(QDialog *dlg)
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
	const QModelIndex index = ui->treeView->selectionModel()->currentIndex();

	std::vector<int> selectedArray;
	QItemSelectionModel *selection = ui->treeView->selectionModel();
	QModelIndexList indexes = selection->selectedRows();
	qSort(indexes);
	for (int i = 0; i < indexes.count(); i++) {
		selectedArray.push_back(indexes[i].row());
	}

	pageant_delete_key2(selectedArray.size(), &selectedArray[0]);

	keylist_update();
}

void MainWindow::on_close()
{
	debug_printf("close button?\n");
}

void MainWindow::on_pushButton_close_clicked()
{
    debug_printf("quit button\n");
	quitGurad_ = false;
    this->close();
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

void MainWindow::on_pushButtonTest_clicked()
{
#if 0
	static bool sw = false;
	debug_console_show(sw);
	sw = !sw;
#endif
}

void MainWindow::on_help_clicked()
{
	debug_printf("on_help_clicked()\n");
	launch_help(NULL, NULL);
}

void MainWindow::on_actionhelp_triggered()
{
	debug_printf("on_help_triggerd()\n");
	launch_help(NULL, WINHELP_CTX_pageant_general);
}

void MainWindow::on_actionabout_triggered()
{
	debug_printf("on_actionabout_triggered()\n");
	on_actionAboutDlg();
}

void MainWindow::on_actionquit_triggered()
{
    debug_printf("quit button\n");
//    qApp->QCoreApplication::quit();
	quitGurad_ = false;
    this->close();
}

void MainWindow::on_session(QAction *action)
{
	debug_printf("on_session enter\n");
	int value = action->data().toInt();

	std::wstring param;
	if (value >= 0) {
		std::vector<std::string> session_info = setting_get_putty_sessions();
		param = L"@" + QString(session_info[value].c_str()).toStdWString();
	}

	const wchar_t *putty_path = get_putty_path().c_str();
	exec(putty_path, param.c_str());

	debug_printf("on_session %d leave\n", value);
}

// CAPI Cert
void MainWindow::on_pushButton_clicked()
{
	printf("CAPI Cert\n");
#ifdef PUTTY_CAC
	//HWND hwnd = (HWND)winId();
	HWND hwnd = NULL;
	char * szCert = cert_prompt(IDEN_CAPI, hwnd);
	if (szCert == NULL)
		return;
	QString fn = szCert;
	add_keyfile(fn);		// TODO!!!
	keylist_update();
#else
    MessageBoxA((HWND)winId(), "not support", "(^_^)",
				MB_OK | MB_ICONEXCLAMATION);
#endif
}

// PKCS Cert
void MainWindow::on_pushButton_2_clicked()
{
	printf("PKCS Cert\n");
#ifdef PUTTY_CAC
	//HWND hwnd = (HWND)winId();
	HWND hwnd = NULL;
	char * szCert = cert_prompt(IDEN_PKCS, hwnd);
	if (szCert == NULL)
		return;
	QString fn = szCert;
	add_keyfile(fn);		// TODO!!!
	keylist_update();
#else
    MessageBoxA((HWND)winId(), "not support", "(^_^)",
				MB_OK | MB_ICONEXCLAMATION);
#endif
}

void MainWindow::on_actionregedit_triggered()
{
	const wchar_t *path =
		L"HKEY_CURRENT_USER\\SOFTWARE\\SimonTatham\\PuTTY\\Pageant";
	exec_regedit(path);
}

void MainWindow::on_actionsetting_triggered()
{
    debug_printf("setting\n");
    QString unixdomain_path = getenv("SSH_AUTH_SOCK");
    bool unixdomain_enable_prev = setting_get_bool("ssh-agent/cygwin_sock");
    bool pageant_enable_prev = setting_get_bool("ssh-agent/pageant");
	bool ms_agent_enable_prev = setting_get_bool("ssh-agent/ms_ssh");
	SettingDlg dlg(this);
    dlg.exec();

	// サービス再起動が必要
	bool reboot_services = (unixdomain_path != unixdomain_path);
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
			ssh_agent_server_start(unixdomain_path.toStdWString().c_str());
		} else {
			ssh_agent_server_stop();
		}
	}
	if (ms_agent_enable_prev != ms_agent_enable) {
		if (ms_agent_enable) {
			pipe_th_start(unixdomain_path.toStdWString().c_str());
		} else {
			pipe_th_stop();
		}
	}
}

void MainWindow::on_actionsetting2_triggered()
{
    debug_printf("setting\n");
    SettingDlg dlg(this);
    dlg.exec();

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
			break;
		case WM_WTSSESSION_CHANGE:
			// TODO キーの制御をする
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
			break;
		}
	}
	return false;
}

#ifdef PUTTY_CAC
extern "C" char *test_0606(const char *comment);
#endif

void MainWindow::on_pushButton_3_clicked()
{
    debug_printf("pubkey to clipboard\n");
#ifdef PUTTY_CAC
	const char *comment = "CAPI:dfaflafjlaseifaliejf83u98q4usdlkjfla33bf";
	char *keyString = test_0606(comment);
	if (keyString == NULL) {
		message_box("コメントから公開鍵を作れるはず", APPNAME, MB_OK | MB_ICONERROR, 0);
		return;
	}
	QApplication::clipboard()->setText(QString(keyString));
    sfree(keyString);
#endif
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
	info->result = retval;
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


// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:



