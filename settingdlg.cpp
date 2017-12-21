
#if !defined(UNICODE)
#define UNICODE
#endif

#include <windows.h>

#pragma warning(push)
#pragma warning(disable:4127)
#pragma warning(disable:4251)
#include <QDir>
#include <qmap.h>		// TODO newのdefineでエラーが出るので入れた
#pragma warning(pop)

#include "pageant+.h"
#include "debug.h"
#include "setting.h"
#include "ssh-agent_emu.h"
#include "winmisc.h"
#include "gui_stuff.h"
#include "pageant.h"
#include "misc_cpp.h"
#include "misc.h"
extern "C" {
#include "cert_common.h"	// for cert_forget_pin()
}
#include "passphrases.h"

#include "settingdlg.h"
#pragma warning(push)
#pragma warning(disable:4127)
#pragma warning(disable:4251)
#pragma warning(disable:4244)
#include "ui_settingdlg.h"
#pragma warning(pop)

void SettingDlg::dispSetting()
{
	QString text;
	QString s1;
	text += u8"Qt version: ";
	text += QT_VERSION_STR;
	text += "\n";
	text += u8"Qt dll version: ";
	text += qVersion();
	text += "\n";

	{
		char *buildinfo_text = buildinfo("\n");
		s1 = buildinfo_text;
		sfree(buildinfo_text);
	}

	text += s1;
	
	ui->plainTextEdit->insertPlainText(text);
}

static QString get_recommended_ssh_sock_path()
{
	std::vector<QString> path_list;
	QString s;
	s =  QDir(QString::fromStdWString(_getenv(L"HOME"))).absolutePath() + "\\.ssh";
	s.replace("/","\\");
	path_list.push_back(s);
	s = QString::fromStdWString(_SHGetKnownFolderPath(FOLDERID_Profile))
		+ "\\.ssh";
	s.replace("/","\\");
	path_list.push_back(s);
	path_list.push_back("c:\\cygwin64\\tmp");
	path_list.push_back("c:\\msys64\\tmp");
	path_list.push_back("c:\\tmp");
    path_list.push_back(QString::fromStdWString(_SHGetKnownFolderPath(FOLDERID_LocalAppData)) + "\\temp");

	QDir dir;
	QString path;
	for(size_t i=0; i<path_list.size(); i++) {
		dir.setPath(path_list[i]);
		if (dir.exists()) {
			path = dir.absolutePath() + "\\.ssh-pageant";
			path.replace("/","\\");
			break;
		}
	}

	return path;
}

SettingDlg::SettingDlg(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingDlg)
{
    ui->setupUi(this);

    {
		QString text =
			QString(u8"環境変数") + "\n" +
			"%SSH_AUTH_SOCK% = " +
			_ExpandEnvironmentStrings("%SSH_AUTH_SOCK%").c_str() + "\n" +
			"%HOME% = " +
			_ExpandEnvironmentStrings("%HOME%").c_str() + "\n" +
			"%USERPROFILE% = " + 
			_ExpandEnvironmentStrings("%USERPROFILE%").c_str() + "\n" +
			"%HOMEDRIVE% = "+
			_ExpandEnvironmentStrings("%HOMEDRIVE%").c_str() + "\n" +
			"%HOMEPATH% = " +
			_ExpandEnvironmentStrings("%HOMEPATH%").c_str() + "\n" +
			"%TMP% = " +
			_ExpandEnvironmentStrings("%TMP%").c_str() + "\n" +
			"%TEMP% = " +
			_ExpandEnvironmentStrings("%TEMP%").c_str();
		ui->label_3->setText(text);
    }

	dispSetting();

	// startup
	{
		const int startup = setting_get_startup();
		ui->checkBox_13->setChecked(startup == 1);
	}
	
    ui->checkBox->setChecked(setting_get_bool("ssh-agent/cygwin_sock"));
	ui->checkBox_3->setChecked(setting_get_bool("ssh-agent/pageant"));
	ui->checkBox_2->setChecked(setting_get_bool("ssh-agent/ms_ssh"));

    std::wstring ssh_auth_sock;
    bool r = reg_read_cur_user(L"Environment", L"SSH_AUTH_SOCK", ssh_auth_sock);
	if (r == true) {
		replace(ssh_auth_sock.begin(), ssh_auth_sock.end(), L'/', L'\\');
		ui->lineEdit->setText(QString::fromStdWString(ssh_auth_sock));
	} else {
		QString ssh_auth_sock_rec = get_recommended_ssh_sock_path();
		ui->lineEdit->setText(ssh_auth_sock_rec);
		ui->label->setText(u8"SSH_AUTH_SOCKの値はおすすめの値です");
	}

#if defined(_DEBUG)
	ui->checkBox_4->setChecked(setting_get_bool("debug/console_enable"));
#else
    ui->checkBox_4->setChecked(false);
    ui->checkBox_4->setEnabled(false);
#endif

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
#if !defined(DEVELOP_VERSION)
		ui->checkBox_6->setEnabled(false);
		ui->spinBox->setEnabled(false);
#endif
		ui->checkBox_6->setChecked(
			setting_get_bool("bt/enable", false));
		ui->spinBox->setValue(
			setting_get_int("bt/timeoute", 10));
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
	
	showTab(false);
}

SettingDlg::~SettingDlg()
{
    delete ui;
}

void SettingDlg::dispSockPath(const std::string &path)
{
    std::string win_str = _ExpandEnvironmentStrings(path.c_str());
    replace(win_str.begin(), win_str.end(), '/', '\\');

    std::string win_str_drive;
    std::string win_str_path;
    bool path_ok = false;
    
    if (win_str.size() > 4 &&
		win_str[1] == ':' && win_str[2] == '\\' &&
		win_str[win_str.size()-1] != '\\')
    {
		// ok
		win_str_drive = win_str[0];
		win_str_path = &win_str[2];
		if (!win_str_path.empty()) {
			path_ok = true;
		}
    }

    QString text;
    if (path_ok) {
		std::string cyg_str = "/cygdrive/" + win_str_drive + win_str_path;
		replace(cyg_str.begin(), cyg_str.end(), '\\', '/');

		std::string msys_str = "/" + win_str_drive + win_str_path;
		replace(msys_str.begin(), msys_str.end(), '\\', '/');

		text =	QString(
			"Windows:\n"
			"  %1\n"
			"Cygwin:\n"
			"  %2\n"
			"msys:\n"
			"  %3").arg(win_str.c_str()).arg(cyg_str.c_str()).arg(msys_str.c_str());
    } else {
		text = "path is incomplete";
    }

    ui->label->setText(text);
}

void SettingDlg::on_lineEdit_textChanged(const QString &arg1)
{
    std::string path = arg1.toStdString();
    dispSockPath(path);
}

void SettingDlg::on_buttonBox_accepted()
{
    setting_set_bool("ssh-agent/cygwin_sock", ui->checkBox->isChecked());
	setting_set_bool("ssh-agent/pageant", ui->checkBox_3->isChecked());
	setting_set_bool("ssh-agent/ms_ssh", ui->checkBox_2->isChecked());

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
	{
		if (ui->checkBox_13->isChecked() == false) {
			setting_set_startup(false);
		} else {
			bool setup_startup = true;
			const int startup = setting_get_startup();
			if (startup == 2) {
				std::wstring text =
					L"スタートアップに他のexeのパスが設定されています\n"
					"設定しますか?";
				int r = message_boxW(text.c_str(), L"" "Pageant+", MB_YESNO, 0);
				if (r == IDNO) {
					setup_startup = false;
				}
			}
			if (setup_startup == true) {
				setting_set_startup(true);
			}
		}
	}

	// debug
	{
#if defined(_DEBUG)
		const bool check = ui->checkBox_4->isChecked();
		setting_set_bool("debug/console_enable", check);
		debug_console_show(check);
#endif
	}

	// bluetooth
	{
		setting_set_bool("bt/enable", ui->checkBox_6->isChecked());
		setting_set_int("bt/timeoute", ui->spinBox->value());
	}

	// localhost
	{
		setting_set_bool("ssh-agent_tcp/enable", 
						 ui->checkBox_14->isChecked());
		setting_set_int("ssh-agent_tcp/port_no",
						std::stoi(
							ui->lineEdit_3->text().toStdString()));
	}
	
	//const char *ssh_auth_sock = getenv("SSH_AUTH_SOCK");
    std::wstring ssh_auth_sock;
    reg_read_cur_user(L"Environment", L"SSH_AUTH_SOCK", ssh_auth_sock);
	std::wstring ui_ssh_auth_sock = ui->lineEdit->text().toStdWString();

    if (ui_ssh_auth_sock != ssh_auth_sock)
    {
		std::wstring env = L"SSH_AUTH_SOCK=";
		env += ui_ssh_auth_sock;
		_wputenv(env.c_str());
		reg_write_cur_user(L"Environment", L"SSH_AUTH_SOCK", ui_ssh_auth_sock.c_str());

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

		std::string text;
		int n = 0;
		for(auto &pp : passphraseAry) {
			n++;
			text += std::to_string(n);
			text += ":";
			text += pp;
			text += "\n";
		}
		text = "count " + std::to_string(n) + "\n" + text;
		message_boxA(text.c_str(), "secret pp", MB_OK | MB_ICONERROR, 0);
	} else
#endif
	{
		int r = message_boxA(
			"forgot passphrases OK?", "ok?",
			MB_OKCANCEL | MB_ICONERROR, 0);
		if (r == IDOK) {
			passphrase_forget();
			passphrase_remove_setting();
			cert_forget_pin();
		}
	}
}

void SettingDlg::on_pushButton_13_clicked()
{
	setting_remove_confirm_info();
}

void SettingDlg::on_pushButton_14_clicked()
{
    cert_forget_pin();
}

void SettingDlg::showTab(bool show)
{
	const int index_from = 4;
	const int index_to = 6;
	if (show) {
		// show
		int n = index_to;
		for (const auto &a : invisibleTabs_) {
			ui->tabWidget->insertTab(n, a.tab, a.label);
			n--;
		}
		invisibleTabs_.clear();
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
	}
}

void SettingDlg::on_checkBox_5_clicked()
{
	if (ui->checkBox_5->isChecked()) {
		// show detail
		showTab(true);
	} else {
		showTab(false);
	}
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:

