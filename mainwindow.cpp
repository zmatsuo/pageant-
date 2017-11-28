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
#include "debug.h"
#include "utf8.h"

#pragma warning(push)
#pragma warning(disable:4127)
#pragma warning(disable:4251)
#include "passphrasedlg.h"
#include "aboutdlg.h"
#include "settingdlg.h"
#include "confirmacceptdlg.h"
#pragma warning(pop)

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
void add_keyfile(const Filename *fn);

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
	for (auto &f: files) {
		add_keyfile(f);
    }
#if 0
	keylist_update();
#endif
	if (files.size() > 0) {
		int r = message_boxW(L"次回起動時に読み込みますか?", L"pageant+", MB_YESNO, 0);
		if (r == IDYES) {
			for (auto &f: files) {
				setting_add_keyfile(f.toStdWString().c_str());
			}
		}
	}
}

class ckey {
public:
	ckey() {
		key_ = nullptr;
	}
	ckey(const ckey &rhs) {
		copy(rhs);
	}
	ckey &operator=(const ckey &rhs) {
        assert(!(&rhs == this));
		if (key_ != nullptr) {
			key_->alg->freekey(key_);
		}
		copy(rhs);
		return *this;
	}
	~ckey()
	{
	    key_->alg->freekey(key_->data);
		if (key_->comment != nullptr) {
			sfree(key_->comment);
			key_->comment = nullptr;
		}
	    sfree(key_);
		key_ = nullptr;
	}
	ckey(ssh2_userkey *key) {
		key_ = key;
	}
	std::string fingerprint() const {
		char *r = ssh2_fingerprint(key_->alg, key_->data);
		std::string s = r;
		sfree(r);
		return s;
	}
	std::string alg_name() const {
		return std::string(key_->alg->name);
	}		
	int bits() const {
		int bloblen;
		unsigned char *blob = public_blob(&bloblen);
		int bits = key_->alg->pubkey_bits(key_->alg, blob, bloblen);
		sfree(blob);
		return bits;
	}
	std::string fingerprint_md5() const {
		int bloblen;
		unsigned char *blob = public_blob(&bloblen);

		unsigned char digest[16];
		MD5Simple(blob, bloblen, digest);
		sfree(blob);

		char fingerprint_str[16 * 3];
		for (int i = 0; i < 16; i++)
			sprintf(fingerprint_str + i * 3, "%02x%s", digest[i], i == 15 ? "" : ":");
		return std::string(fingerprint_str);
	}
	std::string fingerprint_sha1() const {
		int bloblen;
		unsigned char *blob = public_blob(&bloblen);

		unsigned char sha1[20];
		SHA_Simple(blob, bloblen, sha1);
		sfree(blob);

		char sha1_str[40 + 1];
		for (int i = 0; i < 20; i++)
			sprintf(sha1_str + i * 2, "%02x", sha1[i]);

		return std::string(sha1_str);
	}
	// こちらでつけたコメント(=ファイル名)
	std::string key_comment() const {
		std::string s;
		if (key_ == nullptr) {
			return s;
		}
		s = key_->comment;
		return s;
	}
	void set_fname(const char *fn) {
		if (key_->comment != nullptr) {
			sfree(key_->comment);
			key_->comment = nullptr;
		}
		key_->comment = _strdup(fn);
	}
	// キーについているコメント
	std::string key_comment2() const {
		std::string s;
		if (key_ == nullptr) {
			return s;
		}
		struct RSAKey *rsa = (struct RSAKey *)key_->data;
		if (rsa == nullptr) {
			return s;
		}
		s = rsa->comment;
		return s;
	}
	// バイナリデータをパースしてkeyに再構成する
	bool parse_one_public_key(const void *data, size_t len,
							  const char **fail_reason)
	{
		bool r = ::parse_one_public_key(&key_, data, len, fail_reason);
		debug_rsa_ = (RSAKey *)key_->data;
		return r;
	}
	void get_raw_key(ssh2_userkey **key) const {
		ssh2_userkey *ret_key = snew(struct ssh2_userkey);
		copy_ssh2_userkey(*ret_key, *key_);
		*key = ret_key;
	}
private:
	ssh2_userkey *key_;
	RSAKey *debug_rsa_;

	unsigned char *public_blob(int *len) const {
		return key_->alg->public_blob(key_->data, len);
	}

	void copy(const ckey &rhs) {
		key_ = snew(struct ssh2_userkey);
		copy_ssh2_userkey(*key_, *rhs.key_);
	}
	void RSAKey_copy(const RSAKey &rhs) {
		RSAKey *rsa = (RSAKey *)key_->data;
		debug_rsa_ = rsa;
		copy_RSAKey(*rsa, rhs);
	}
	static void copy_ssh2_userkey(ssh2_userkey &dest, const ssh2_userkey &src) {
		memset(&dest, 0, sizeof(ssh2_userkey));
		dest.alg = src.alg;
		if (src.comment != nullptr) {
			dest.comment = _strdup(src.comment);
		}
		RSAKey *rsa = snew(struct RSAKey);
		dest.data = rsa;
		const RSAKey *src_rsa = (RSAKey *)src.data;
		copy_RSAKey(*rsa, *src_rsa);
	}
	static void copy_RSAKey(RSAKey &dest, const RSAKey &src) {
		memset(&dest, 0, sizeof(RSAKey));
		dest.exponent = copybn(src.exponent);
		dest.modulus = copybn(src.modulus);
		if (src.private_exponent != nullptr) {
			dest.private_exponent = copybn(src.private_exponent);
		}
		if (src.p != nullptr) {
			dest.p = copybn(src.p);
		}
		if (src.q != nullptr) {
			dest.q = copybn(src.q);
		}
		if (src.iqmp != nullptr) {
			dest.iqmp = copybn(src.iqmp);
		}
		dest.bits = src.bits;
		dest.bytes = src.bytes;
		if (src.comment != nullptr) {
			dest.comment = _strdup(src.comment);
		}
	}
};

static bool parse_public_keys(
	const void *data, size_t len,	
	std::vector<ckey> &keys,
	const char **fail_reason)
{
	keys.clear();
	const unsigned char *msg = (unsigned char *)data;
	if (len < 4) {
		*fail_reason = "too short";
		return false;
	}
	int count = toint(GET_32BIT(msg));
	msg += 4;
	len -= 4;
	printf("key count=%d\n", count);

	for(int i=0; i<count; i++) {
		if (len < 4) {
			*fail_reason = "too short";
			return false;
		}
		size_t key_len = toint(GET_32BIT(msg));
		if (len < key_len) {
			*fail_reason = "too short";
			return false;
		}
		const unsigned char *key_blob = msg + 4;
		ckey key;
		bool r = key.parse_one_public_key(key_blob, key_len, fail_reason);	// TODO:コメント分の長さを処理していない
		if (r == false) {
			return false;
		}
		keys.push_back(key);
		msg += key_len;
		len -= key_len + 4;
	}

	return true;
}

static void bt_agent_query_synchronous_fn(void *in, size_t inlen, void **out, size_t *outlen)
{
	size_t reply_len = inlen;
	void *reply = bt_agent_proxy_main_handle_msg(in, &reply_len);	// todo: size_tに変更
	*out = reply;
	*outlen = reply_len;
}

// TODO btファイルに持っていく
bool bt_agent_proxy_main_connect(const wchar_t *target_device)
{
	bt_agent_proxy_t *hBta = bt_agent_proxy_main_get_handle();
	if (hBta == nullptr) {
		printf("bt?\n");
		return false;
	}

	// デバイス一覧を取得
	std::vector<DeviceInfoType> deviceInfos;
	bta_deviceinfo(hBta, deviceInfos);
	if (deviceInfos.size() == 0) {
		auto start = std::chrono::system_clock::now();
		while(1) {
			auto now = std::chrono::system_clock::now();
			auto elapse =
				std::chrono::duration_cast<std::chrono::seconds>(
					now - start);
			if (elapse >= (std::chrono::seconds)10) {
				break;
			}
			std::this_thread::sleep_for(std::chrono::microseconds(10));

			bta_deviceinfo(hBta, deviceInfos);
			if (deviceInfos.size() != 0) {
				break;
			}
		}
		if (deviceInfos.size() == 0) {
			// 見つからなかった
			return false;
		}
	}
	
	DeviceInfoType deviceInfo;
	for (const auto &device : deviceInfos) {
		if (device.deviceName == target_device) {
			deviceInfo = device;
			break;
		}
	}
	if (deviceInfo.deviceName != target_device) {
		// 見つからなかった
		return false;
	}

	// 接続する
	if (!deviceInfo.connected) {
		BTH_ADDR deviceAddr = deviceInfo.deviceAddr;
		bool r = bta_connect(hBta, &deviceAddr);
		printf("bta_connect() %d\n", r);
		if (r == false) {
			dbgprintf("connect fail\n");
			return false;
		} else {
			// 接続待ち
			while (1) {
				// TODO タイムアウト
				if (bt_agent_proxy_main_check_connect() == true) {
					break;
				}
				Sleep(1);
			}
		}
	}
	return true;
}

// BTファイルに持っていく
bool bt_agent_proxy_main_get_key(
	const wchar_t *target_device,
	std::vector<ckey> &keys)
{
	keys.clear();

	bool r = bt_agent_proxy_main_connect(target_device);
	if (!r) {
		std::wostringstream oss;
		oss << L"bluetooth 接続失敗\n"
			<< target_device;
		message_boxW(oss.str().c_str(), L"pageant+", MB_OK, 0);
		return false;
	}

	// BT問い合わせする
	pagent_set_destination(bt_agent_query_synchronous_fn);
	int length;
	void *p = pageant_get_keylist2(&length);
	pagent_set_destination(nullptr);
	std::vector<uint8_t> from_bt((uint8_t *)p, ((uint8_t *)p) + length);
	sfree(p);
	p = &from_bt[0];


	debug_memdump(p, length, 1);

	std::string target_device_utf8;
	to_utf8(target_device, target_device_utf8);

	// 鍵を抽出
	const char *fail_reason = nullptr;
	r = parse_public_keys(p, length, keys, &fail_reason);
	if (r == false) {
		printf("err %s\n", fail_reason);
		return false;
	}

	// ファイル名をセット
	for(ckey &key : keys) {
		std::ostringstream oss;
		oss << "btspp://" << target_device_utf8 << "/" << key.fingerprint_sha1();
		key.set_fname(oss.str().c_str());
	}

	for(const ckey &key : keys) {
		printf("key\n");
		printf(" '%s'\n", key.fingerprint().c_str());
		printf(" md5 %s\n", key.fingerprint_md5().c_str());
		printf(" sha1 %s\n", key.fingerprint_sha1().c_str());
		printf(" alg %s %d\n", key.alg_name().c_str(), key.bits());
		printf(" comment %s\n", key.key_comment().c_str());
		printf(" comment2 %s\n", key.key_comment2().c_str());
	}

	return true;
}

// BTファイルに持っていく
bool bt_agent_proxy_main_add_key(
	const std::vector<std::string> &fnames)
{
	// デバイス一覧を取得
	std::vector<std::string> target_devices_utf8;
	{
		std::regex re(R"(btspp://(.+)/)");
		std::smatch match;
		for(auto key : fnames) {
			if (std::regex_search(key, match, re)) {
				target_devices_utf8.push_back(match[1]);
			}
		}
	}
	std::sort(target_devices_utf8.begin(), target_devices_utf8.end());
	target_devices_utf8.erase(
		std::unique(target_devices_utf8.begin(), target_devices_utf8.end()),
		target_devices_utf8.end());

	// デバイスごとに公開鍵を取得
	for (auto target_device_utf8: target_devices_utf8) {

		std::wstring target_device;
		to_wstring(target_device_utf8.c_str(), target_device);

		std::vector<ckey> keys;
		bool r = bt_agent_proxy_main_get_key(target_device.c_str(), keys);
		if (r == false) {
			return false;
		}

		for(auto fnamae : fnames) {
			for(auto key : keys) {
				if (key.key_comment() == fnamae) {
					ssh2_userkey *key_st;
					key.get_raw_key(&key_st);
					if (!pageant_add_ssh2_key(key_st)) {
						printf("err\n");
					}
				}
			}
		}
	}
	return true;
}

void MainWindow::on_pushButtonAddBTKey_clicked()
{
	dbgprintf("add bt\n");

	std::wstring target_device = setting_get_str("bt/device", nullptr);
	if (target_device.empty()) {
		dbgprintf("?device\n");
		return;
	}

	std::vector<ckey> keys;
	bool r = bt_agent_proxy_main_get_key(
		target_device.c_str(), keys);

	for(const ckey &key : keys) {
		ssh2_userkey *key_st;
		key.get_raw_key(&key_st);
		if (!pageant_add_ssh2_key(key_st)) {
			printf("err\n");
		}
	}

	for(const ckey &key : keys) {
		std::wostringstream oss;
		oss << L"次回起動時に読み込みますか?\n"
			<< key.key_comment().c_str();
		int r2 = message_boxW(oss.str().c_str(), L"pageant+", MB_YESNO, 0);
		if (r2 == IDYES) {
			std::wstring fn;
			to_wstring(key.key_comment().c_str(), fn);
			setting_add_keyfile(fn.c_str());
		}
	}
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
#if 0
	keylist_update();
#endif
	int r = message_boxW(L"次回起動時に読み込みますか?", L"pageant+", MB_YESNO, 0);
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
#if 0
		keylist_update();
#endif

		int r = message_boxW(L"次回起動時に読み込みますか?", L"pageant+", MB_YESNO, 0);
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
    dbgprintf("setting\n");
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
		ssh_agent_cygwin_unixdomain_stop();
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
			ssh_agent_cygwin_unixdomain_start(unixdomain_path.c_str());
		} else {
			ssh_agent_cygwin_unixdomain_stop();
		}
	}
	if (ms_agent_enable_prev != ms_agent_enable) {
		if (ms_agent_enable) {
			pipe_th_start(unixdomain_path.c_str());
		} else {
			pipe_th_stop();
		}
	}

	bt_agent_proxy_main_exit();
	if (setting_get_bool("bt/enable", false)) {
		bt_agent_proxy_main_init();
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
			dbgprintf("WM_WTSSESSION_CHANGE(%d) wParam=%s(%d)\n",
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
			dbgprintf("WM_DEVICECHANGE(%d) wParam=%s(0x%04x)\n",
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


// return
DIALOG_RESULT_T passphraseDlg(struct PassphraseDlgInfo *info)
{
	int r = gWin->passphraseDlg(info);
	return r == QDialog::Accepted ? DIALOG_RESULT_OK : DIALOG_RESULT_CANCEL;
}

void keylist_update(void)
{
	// TODO key listが開いているとき、内容の更新を行う
#if 0
	gWin->keylist_update();
#endif
}

HWND get_hwnd()
{
	return (HWND)gWin->winId();
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
#if 0
	keylist_update();
#endif
}

void add_keyfile(const std::vector<std::wstring> &keyfileAry)
{
	std::vector<std::string> bt_files;
	std::vector<std::wstring> normal_files;

	// BTとその他を分離
	for(const auto &f: keyfileAry) {
		if (wcsncmp(L"btspp://", f.c_str(), 8) == 0)  {
			std::string utf8;
			to_utf8(f.c_str(), utf8);
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
		for(const auto &f: normal_files) {
			Filename *fn = filename_from_wstr(f.c_str());
			add_keyfile(fn);
			filename_free(fn);
		}
		bt_agent_proxy_main_add_key(bt_files);
	}

#if 0
	keylist_update();
#endif
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
