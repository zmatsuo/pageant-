
#if !defined(UNICODE)
#define UNICODE
#endif

#include <windows.h>
#include <sstream>

#include "pageant+.h"
#include "debug.h"
#include "setting.h"
#include "ssh-agent_emu.h"
#include "winmisc.h"
#include "gui_stuff.h"
#include "misc_cpp.h"
#include "misc.h"
#include "passphrases.h"
#include "rdp_ssh_relay.h"

#include "settingdlg.h"
#pragma warning(push)
#pragma warning(disable:4251)
#include "ui_settingdlg.h"
#pragma warning(pop)

#include "pageant.h"
#include "winutils_qt.h"
#include "smartcard.h"

static std::wstring get_recommended_ssh_sock_path()
{
	std::vector<std::wstring> paths;
	std::wstring s;

	s = _getenv(L"HOME");
	if (!s.empty()) {
		s += L"\\.ssh";
		replace(s.begin(), s.end(), L'/', L'\\');
		paths.emplace_back(s);
	}

	s = _SHGetKnownFolderPath(FOLDERID_Profile) + L"\\.ssh";
	replace(s.begin(), s.end(), L'/', L'\\');
	paths.emplace_back(s);

	bool r = reg_read_cur_user(L"Environment", L"SSH_AUTH_SOCK", s);
	if (r == true) {
		replace(s.begin(), s.end(), L'/', L'\\');
		auto pos = s.rfind('\\');
		if (pos != std::string::npos) {
			s = s.substr(0, pos);
			paths.emplace_back(s);
		}
	}

    paths.emplace_back(_SHGetKnownFolderPath(FOLDERID_LocalAppData) + L"\\temp");
	paths.emplace_back(L"c:\\cygwin64\\tmp");
	paths.emplace_back(L"c:\\msys64\\tmp");
	paths.emplace_back(L"c:\\tmp");

	for (const auto &path : paths) {
		if (_waccess(path.c_str(), 0) == 0) {
			return path;
		}
	}

	return L"c:\\tmp";
}

SettingDlg::SettingDlg(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::SettingDlg)
{
	ui->setupUi(this);

	{
		static const wchar_t *env[] = {
			L"SSH_AUTH_SOCK",
			L"HOME",
			L"HOMEDRIVE",
			L"HOMEPATH",
			L"USERPROFILE",
			L"APPDATA",
			L"TMP",
			L"TEMP",
		};
			
		std::wostringstream oss;
		oss << L"環境変数" << L"\n";
		for (const auto &s : env) {
			oss << s << " = "
				<< _getenv(s) << L"\n";
		};
		ui->label_3->setText(QString::fromStdWString(oss.str()));
	}

	// startup
	{
		const int startup = setting_get_startup();
		ui->checkBox_13->setChecked(startup == 1);
	}

	ui->checkBox->setChecked(setting_get_bool("ssh-agent/cygwin_sock"));
	ui->checkBox_3->setChecked(setting_get_bool("ssh-agent/pageant"));
	ui->checkBox_2->setChecked(setting_get_bool("ssh-agent/ms_ssh"));
	ui->checkBox_19->setChecked(
		setting_get_bool("ssh-agent/native_unix_socket"));

	{
		auto ssh_auth_sock_rec = get_recommended_ssh_sock_path();
		std::wstring s;
		if (!setting_get_str("ssh-agent/sock_path_cygwin", s)) {
			s = ssh_auth_sock_rec + L"\\.ssh-agent-cygwin";
		}
		ui->lineEdit->setText(QString::fromStdWString(s));
		
		if (!setting_get_str("ssh-agent/sock_path", s)) {
			s = ssh_auth_sock_rec + L"\\.ssh-agent";
		}
		ui->lineEdit_2->setText(QString::fromStdWString(s));
		setting_get_str("ssh-agent/sock_path_ms", s);
		ui->lineEdit_6->setText(QString::fromStdWString(s));
	}

	// debug tab
	{
		// debug
		{
			// pageant+
			ui->checkBox_4 ->setChecked(setting_get_bool("debug/console_enable"));
			ui->checkBox_20->setChecked(setting_get_bool("debug/log"));
			ui->checkBox_21->setChecked(setting_get_bool("debug/outputdebugstring"));
			ui->lineEdit_4->setText(
				QString::fromStdWString(setting_get_logfile(_GetModuleFileName(nullptr).c_str())));

			// rdp
			std::wstring rdp_client_dll;
			rdpSshRelayCheckClientDll(rdp_client_dll);
			ui->checkBox_24->setChecked(setting_get_bool("debug_rdp_client/console_enable"));
			ui->checkBox_23->setChecked(setting_get_bool("debug_rdp_client/log"));
			ui->checkBox_22->setChecked(setting_get_bool("debug_rdp_client/outputdebugstring"));
			ui->lineEdit_5->setText(
				QString::fromStdWString(setting_get_logfile(rdp_client_dll.c_str())));
		}
	}

	ui->label_6->setText(QString::fromStdWString(setting_get_my_fullpath()));

	ui->label_4->setText(QString::fromStdWString(setting_get_inifile()));

	// putty
	QString qs = QString::fromStdWString(get_putty_path());
    if (qs.isEmpty()) {
		qs = u8"putty.exeは見つかりませんでした";
		ui->label_7->setText(qs);
		ui->label_5->setText(qs);
	} else {
		ui->label_7->setText(qs);
		ui->label_5->setText(QString::fromStdWString(get_putty_ini()));
	}

	// passphrase系
	{
        const bool enable = setting_get_bool("Passphrase/save_enable", false);
        ui->checkBox_7->setChecked(enable);
		ui->checkBox_9->setChecked(
			setting_get_bool("Passphrase/enable_loading_when_startup", false));
		ui->checkBox_8->setChecked(
			setting_get_bool("Passphrase/forget_when_terminal_locked", false));
		arrengePassphraseUI(enable);
	}

	// pin系
	{
		ui->checkBox_12->setChecked(
			setting_get_bool("SmartCardPin/forget_when_terminal_locked", false));
	}

	// confirm系
	{
		ui->checkBox_10->setChecked(
			get_confirm_any_request());
	}

	// relay系
	{
		ui->checkBox_6->setChecked(
			setting_get_bool("bt/enable", false));
		ui->spinBox->setValue(
			setting_get_int("bt/timeoute", 10));
		ui->checkBox_17->setChecked(
			setting_get_bool("bt/auto_connect", false));
	}

	// localhost
	{
		ui->checkBox_14->setChecked(
			setting_get_bool("ssh-agent_tcp/enable", false));
		ui->lineEdit_3->setText(
			QString::fromStdString(
				std::to_string(
					setting_get_int("ssh-agent_tcp/port_no", 8080))));
	}

	// key
	{
		ui->checkBox_16->setChecked(
			setting_get_bool("key/enable_loading_when_startup", false));
		ui->checkBox_15->setChecked(
			setting_get_bool("key/forget_when_terminal_locked", false));
	}

	// rdp ssh relay
	{
		bool r = 
			setting_get_bool("rdpvc-relay-client/enable", false) &&
			rdpSshRelayCheckClient() ? true : false;

		ui->checkBoxSshRDPRelayClient->setChecked(r);
		ui->checkBoxSshRDPRelayServer->setChecked(
			setting_get_bool("rdpvc-relay-server/enable", false));
	}

	// debug tab
#if defined(DEVELOP_VERSION)
	if ((GetAsyncKeyState(VK_LSHIFT) & 0x8000) == 0)
#endif
	{
		ui->tabWidget->setCurrentIndex(0);
		for (int i = 0 ; i < ui->tabWidget->count(); i++) {
			if (ui->tabWidget->tabText(i) == "debug") {
				ui->tabWidget->removeTab(i);
				break;
			}
		}
	}

	showDetail(false);
}

SettingDlg *SettingDlg::instance = nullptr;

SettingDlg::~SettingDlg()
{
    delete ui;
	instance = nullptr;
}

SettingDlg *SettingDlg::createInstance(QWidget *parent)
{
	if (instance != nullptr) {
		return instance;
	}
	instance = new SettingDlg(parent);
	return instance;
}

void SettingDlg::on_buttonBox_accepted()
{
	setting_set_bool("ssh-agent/pageant", ui->checkBox_3->isChecked());

    setting_set_bool("ssh-agent/cygwin_sock", ui->checkBox->isChecked());
	setting_set_bool("ssh-agent/ms_ssh", ui->checkBox_2->isChecked());
    setting_set_bool("ssh-agent/native_unix_socket",
					 ui->checkBox_19->isChecked());
	setting_set_str("ssh-agent/sock_path_cygwin",
					ui->lineEdit->text().toStdWString().c_str());
	setting_set_str("ssh-agent/sock_path",
					ui->lineEdit_2->text().toStdWString().c_str());
	setting_set_str("ssh-agent/sock_path_ms",
					ui->lineEdit_6->text().toStdWString().c_str());
	

	
	// passphrase系
	setting_set_bool("Passphrase/save_enable",
					 ui->checkBox_7->isChecked());
	setting_set_bool("Passphrase/enable_loading_when_startup",
					 ui->checkBox_9->isChecked());
	setting_set_bool("Passphrase/forget_when_terminal_locked",
					 ui->checkBox_8->isChecked());

	// pin系
	setting_set_bool("SmartCardPin/forget_when_terminal_locked",
					 ui->checkBox_12->isChecked());

	// confirm系
	{
		const bool confirm_any_request = ui->checkBox_10->isChecked();
		setting_set_bool("confirm/confirm_any_request", confirm_any_request);
		set_confirm_any_request(confirm_any_request);
	}
	
	// startup
	setting_set_startup(ui->checkBox_13->isChecked());

	// debug tab
	{
		// debug
		{
			const bool check = ui->checkBox_4->isChecked();
			setting_set_bool("debug/console_enable", check);
#if defined(DEVELOP_VERSION)
			debug_console_show(check);
#endif
			setting_set_bool("debug/log",ui->checkBox_20->isChecked());
			setting_set_bool("debug/outputdebugstring", ui->checkBox_21->isChecked());

			setting_set_bool("debug_rdp_client/console_enable",ui->checkBox_24->isChecked());
			setting_set_bool("debug_rdp_client/log",ui->checkBox_23->isChecked());
			setting_set_bool("debug_rdp_client/outputdebugstring",ui->checkBox_22->isChecked());
		}
	}

	// bluetooth
	{
		setting_set_bool("bt/enable", ui->checkBox_6->isChecked());
		setting_set_int("bt/timeoute", ui->spinBox->value());
		setting_set_bool("bt/auto_connect", ui->checkBox_17->isChecked());
	}

	// localhost
	{
		setting_set_bool("ssh-agent_tcp/enable", 
						 ui->checkBox_14->isChecked());
		setting_set_int("ssh-agent_tcp/port_no",
						std::stoi(
							ui->lineEdit_3->text().toStdString()));
	}
	
	// key
	{
		setting_set_bool("key/enable_loading_when_startup",
						 ui->checkBox_16->isChecked());
		setting_set_bool("key/forget_when_terminal_locked", 
						 ui->checkBox_15->isChecked());
	}

	// rdp ssh relay
	{
		if (ui->checkBoxSshRDPRelayClient->isChecked()) {
			bool r = rdpSshRelaySetupClient();
			setting_set_bool("rdpvc-relay-client/enable", r);
		} else {
			rdpSshRelayTeardownClient();
			setting_set_bool("rdpvc-relay-client/enable", false);
		}

		if (ui->checkBoxSshRDPRelayServer->isChecked()) {
			bool r = true;
			if (!rdpSshRelayCheckServer()) {
				r = rdpSshRelaySetupServer();
			}
			setting_set_bool("rdpvc-relay-server/enable", r);
		} else {
			if (rdpSshRelayCheckServer()) {
				bool r = rdpSshRelayTeardownServer();
				setting_set_bool("rdpvc-relay-server/enable", !r);
			}
		}
	}

	// 環境変数を設定する
	if (ui->checkBox_18->isChecked())
	{
		std::wstring ssh_auth_sock_env;
		reg_read_cur_user(L"Environment", L"SSH_AUTH_SOCK", ssh_auth_sock_env);
		std::wstring ssh_auth_sock_setting =
			setting_get_str("ssh-agent/sock_path_cygwin", nullptr);
		if (ssh_auth_sock_setting != ssh_auth_sock_env)
		{
			std::wstring env = L"SSH_AUTH_SOCK=";
			env += ssh_auth_sock_setting;
			_wputenv(env.c_str());
			reg_write_cur_user(L"Environment", L"SSH_AUTH_SOCK", ssh_auth_sock_setting.c_str());

			const wchar_t *expandedInformation =
				L"環境変数は各プログラムごとに管理されていて、\n"
				"他のプログラムからは変更することができません。\n"
				"環境変数の最初の値はプログラムを起動した親プログラムのから引き継ぎます。\n"
				"プログラムによっては自分の環境変数を設定しますが\n"
				"(bashなどのシェルプログラムは設定ファイルに従って環境変数を設定します)\n"
				"初期値(設定前)は親プログラムが渡した値となります。\n"
				"そしてユーザーが起動するプログラムの大半の親プログラムは、\n"
				"デスクトップやスタートボタンを管理しているexplorerです\n"
				"\n"
				"explorerを含む一部のプログラムは\n"
				"他のプログラムからの環境変数変更通知(Windowメッセージ WM_SETTINGCHANGE)を処理することで、\n"
				"環境変数の再設定が可能です。\n"
				"\n"
				"すべてのウィンドウにメッセージを送信して環境変数の再設定をリクエストします\n"
				"\n"
				"<A HREF=\"http://d.hatena.ne.jp/ganaware/20111102/1320222616\">win-ssh-agent</a>の説明";
			int ret = task_dialog(
				L"pageant+",
				L"環境変数の変更を全てのウィンドウに通知しますか?",
				L"通知する場合は[yes]を押してください",
				expandedInformation,
				NULL,
				TD_INFORMATION_ICON,
				TDCBF_YES_BUTTON | TDCBF_NO_BUTTON,
				TDCBF_YES_BUTTON, 1);

			if (ret == IDYES) {
				DWORD_PTR returnValue;
				LRESULT Ret = SendMessageTimeoutW(
					HWND_BROADCAST, WM_SETTINGCHANGE, 0,
					reinterpret_cast<LPARAM>(L"Environment"),
					SMTO_ABORTIFHUNG,
					5000, &returnValue);
				(void)Ret;
			}
		}
	}
}

// 環境変数
void SettingDlg::on_pushButton_clicked()
{
    std::wstring path = _SHGetKnownFolderPath(FOLDERID_System);
    path += L"\\SystemPropertiesAdvanced.exe";
    ::exec(path.c_str());
}

// regedit 環境変数
void SettingDlg::on_pushButton_2_clicked()
{
    exec_regedit(L"HKEY_CURRENT_USER\\Environment");
}

void SettingDlg::on_pushButton_4_clicked()
{
    std::wstring path = _SHGetKnownFolderPath(FOLDERID_System);
    path += L"\\taskmgr.exe";
    const wchar_t *param = L"/0 /Startup";
    ::exec(path.c_str(), param);
}

// regedit run user
void SettingDlg::on_pushButton_3_clicked()
{
    exec_regedit(L"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");
}

// regedit run common
void SettingDlg::on_pushButton_11_clicked()
{
	exec_regedit(L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run");
}

// exeフォルダをexplorerで開く
void SettingDlg::on_pushButton_8_clicked()
{
	std::wstring s = setting_get_my_fullpath();
	s = s.substr(0, s.rfind(L'\\')+1);
	::exec(s.c_str());
}

// pageant+自身の設定
void SettingDlg::on_pushButton_6_clicked()
{
	if (setting_get_use_inifile() == true) {
		std::wstring ini = setting_get_inifile();
        ::exec(ini.c_str());
    } else {
		std::wstring reg_path = setting_get_inifile();
		exec_regedit(reg_path.c_str());
	}
}


// puttyの設定
void SettingDlg::on_pushButton_7_clicked()
{
	if (get_use_inifile() != 0) {
        std::wstring ini_ws = get_putty_ini();
        ::exec(ini_ws.c_str());
	} else {
        std::wstring reg_path = get_putty_ini();
		exec_regedit(reg_path.c_str());
	}
}

// startup user
void SettingDlg::on_pushButton_9_clicked()
{
	std::wstring path;
	bool r = _SHGetKnownFolderPath(FOLDERID_Startup, path);
	if (r) {
        ::exec(path.c_str());
	}
}

// startup common
void SettingDlg::on_pushButton_10_clicked()
{
	std::wstring path;
	bool r = _SHGetKnownFolderPath(FOLDERID_CommonStartup, path);
	if (r) {
        ::exec(path.c_str());
	}
}

void SettingDlg::arrengePassphraseUI(bool enable)
{
    ui->checkBox_8->setEnabled(enable);
    ui->checkBox_9->setEnabled(enable);
	ui->pushButton_12->setEnabled(enable);
}

// passphrase save enable
void SettingDlg::on_checkBox_7_clicked()
{
	const bool check = ui->checkBox_7->isChecked();
	arrengePassphraseUI(check);
}

void SettingDlg::on_pushButton_12_clicked()
{
#if defined(DEVELOP_VERSION) || defined(_DEBUG)
	if ((GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0) {
		// shiftキーが押されている
		auto passphraseAry = passphrase_get_array();

		std::wstring text;
		int n = 0;
		for(auto &pp : passphraseAry) {
			n++;
			text += std::to_wstring(n);
			text += L":";
			text += utf8_to_wc(pp);
			text += L"\n";
		}
		text = L"count " + std::to_wstring(n) + L"\n" + text;
		message_box(this, text.c_str(), L"secret pp", MB_OK | MB_ICONERROR, 0);
	} else
#endif
	{
		int r = message_box(
			this, 
			L"forgot passphrases OK?", L"ok?",
			MB_OKCANCEL | MB_ICONERROR, 0);
		if (r == IDOK) {
			passphrase_forget();
			passphrase_remove_setting();
			SmartcardForgetPin();
		}
	}
}

void SettingDlg::on_pushButton_13_clicked()
{
	setting_remove_confirm_info();
}

void SettingDlg::on_pushButton_14_clicked()
{
	SmartcardForgetPin();
}

void SettingDlg::showDetail(bool show)
{
	const int index_from = 4;
	const int index_to = ui->tabWidget->count() - 1;
	if (show) {
		// show
		for (const auto &a : invisibleTabs_) {
			ui->tabWidget->insertTab(index_to + 1, a.tab, a.label);
		}
		invisibleTabs_.clear();
		ui->pushButton_5->show();
	} else {
		// hide
		int current_index = ui->tabWidget->currentIndex();
		if (current_index >= index_from) {
			ui->tabWidget->setCurrentIndex(0);
		}
		for (int i = index_to; i >= index_from; i--) {
			invisiableTabsInfo_t info;
			info.tab = ui->tabWidget->widget(i);
			info.label = ui->tabWidget->tabText(i);
			invisibleTabs_.push_back(info);
			ui->tabWidget->removeTab(i);
		}
		ui->pushButton_5->hide();
	}
}

void SettingDlg::on_checkBox_5_clicked()
{
	if (ui->checkBox_5->isChecked()) {
		// show detail
		showDetail(true);
	} else {
		showDetail(false);
	}
}

// キーのクリア
void SettingDlg::on_pushButton_15_clicked()
{
	setting_clear_keyfiles();
}

void SettingDlg::on_pushButton_5_clicked()
{
	setting_remove_all();
	close();
}

// startup
void SettingDlg::on_checkBox_13_clicked()
{
	if (ui->checkBox_13->isChecked() == true) {
		const int startup = setting_get_startup();
		if (startup == 2) {
			std::wstring startupPath;
			setting_get_startup_exe_path(startupPath);
			std::wstring text =
				L"スタートアップに他のexeのパスが設定されています\n"
				"設定しますか?\n";
			text += L"現在の登録されているexe: " + startupPath;
			int r = message_box(this, text.c_str(), L"" "Pageant+", MB_YESNO, 0);
			if (r == IDNO) {
				ui->checkBox_13->setChecked(false);
			}
		}
	}
}

void SettingDlg::on_checkBox_3_clicked()
{
	if (!ui->checkBox_3->isChecked() &&
		ui->checkBoxSshRDPRelayClient->isChecked())
	{
		std::wstring text =
			L"rdp clientが使用します\n"
			"offにするとrdp clientもoffになります\n"
			"よろしいですか?\n";
		int r = message_box(this, text.c_str(), L"" "Pageant+", MB_YESNO, 0);
		if (r == IDYES) {
			ui->checkBoxSshRDPRelayClient->setChecked(false);
		} else {
			ui->checkBox_3->setChecked(true);
		}
	}
}

void SettingDlg::on_checkBoxSshRDPRelayClient_clicked()
{
	if (ui->checkBoxSshRDPRelayClient->isChecked()) {
		std::wstring dll_file;
		std::wstring dll_registory;
		std::wstring text;
		if (!rdpSshRelayCheckClientDll(dll_file)) {
			text =
				L"RDPクライアントdllがありません\n"
				"dll: " + dll_file;
			message_box(this, text.c_str(), L"" "Pageant+", MB_OK, 0);
			ui->checkBoxSshRDPRelayClient->setChecked(false);
		} else {
			if (!rdpSshRelayCheckClientRegistry(dll_registory)) {
				text =
					L"レジストリに他のdllが設定されています\n"
					"設定を上書きますか?\n";
				text += L" 現在の登録されているdll: " + dll_registory + L"\n";
				text += L" 設定するdll: " + dll_file;
				int r = message_box(this, text.c_str(), L"" "Pageant+", MB_YESNO, 0);
				if (r == IDNO) {
					ui->checkBoxSshRDPRelayClient->setChecked(false);
				}
			}
			if (!ui->checkBox_3->isChecked()) {
				text =
					L"Pagent compatible agentを使用します\n"
					"よろしいですか?\n";
				int r = message_box(this, text.c_str(), L"" "Pageant+", MB_YESNO, 0);
				if (r == IDYES) {
					ui->checkBox_3->setChecked(true);
				} else {
					ui->checkBoxSshRDPRelayClient->setChecked(false);
				}
			}
		}
	}
}

void SettingDlg::on_checkBoxSshRDPRelayServer_clicked()
{
	wchar_t uac_msg[] = 
		L"このレジストリの設定には管理者権限が必要となります。\n"
		"昇格確認ダイアログが表示されます。";
	if (ui->checkBoxSshRDPRelayServer->isChecked() == true) {
		if (!rdpSshRelayCheckServer()) {
			std::wstring text =
				L"設定ウィンドウを[ok]を押して閉じる時にレジストリが設定されます。\n\n";
			text += uac_msg;
			message_box(this, text.c_str(), L"" "Pageant+", MB_OK, 0);
		}
	} else {
		if (rdpSshRelayCheckServer()) {
			std::wstring text =
				L"設定ウィンドウを[ok]を押して閉じる時にレジストリが削除されます。\n\n";
			text += uac_msg;
			message_box(this, text.c_str(), L"" "Pageant+", MB_OK, 0);
			ui->checkBoxSshRDPRelayServer->setChecked(false);
		}
	}
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
