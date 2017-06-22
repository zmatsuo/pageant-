
#if !defined(UNICODE)
#define UNICODE
#endif

#include <windows.h>

#include <QDir>

#include "pageant+.h"
#include "settingdlg.h"
#include "ui_settingdlg.h"
#include "debug.h"
#include "setting.h"
#include "ssh-agent_emu.h"
#include "winmisc.h"
#include "gui_stuff.h"

void SettingDlg::dispSetting()
{
	QString text;
	QString s1;
	text += u8"pagent+:\n";
	text += setting_get_my_fullpath() + "\n";
	text += u8"pagent+ 設定保存場所:\n";
	text += setting_get_inifile() + "\n";
	text += u8"putty path:\n";
	std::wstring ss = get_putty_path();
	if (ss.empty()) {
		text += "putty not found";
	} else {
		// putty有り
		text += QString::fromStdWString(ss);
		text += "\n";
		text += u8"putty 設定保存場所:\n";
		if (get_use_inifile()) {
			text += get_putty_ini().c_str();
		} else {
			text += "registry";
		}
		text += "\n";
	}
	ui->plainTextEdit->insertPlainText(text);
}

static QString get_recommended_ssh_sock_path()
{
	std::vector<QString> path_list;
	QString s;
	s =  QDir(qgetenv("HOME").constData()).absolutePath() + "\\.ssh";
	s.replace("/","\\");
	path_list.push_back(s);
	s = QString::fromStdWString(_SHGetFolderPath(CSIDL_PROFILE)) + "\\.ssh";
	s.replace("/","\\");
	path_list.push_back(s);
	path_list.push_back("c:\\cygwin64\\tmp");
	path_list.push_back("c:\\msys64\\tmp");
	path_list.push_back("c:\\tmp");
    path_list.push_back(QString::fromStdWString(_SHGetFolderPath(CSIDL_LOCAL_APPDATA)) + "\\temp");

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

    setStartup(0);
	dispSetting();

	
	
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

	ui->checkBox_4->setChecked(setting_get_bool("debug/console_enable"));
	ui->checkBox_5->setChecked(setting_get_bool("general/minimize_to_notification_area"));
	ui->checkBox_6->setChecked(setting_get_bool("general/close_to_notification_area"));

	ui->label_6->setText(setting_get_my_fullpath());

	ui->label_4->setText(setting_get_inifile());
    ui->label_5->setText(QString::fromStdString(get_putty_ini()));
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

void SettingDlg::on_checkBox_clicked(bool checked)
{
    debug("checkbox %s\n", checked ? "ON" : "OFF");
}

void SettingDlg::on_buttonBox_accepted()
{
    setting_set_bool("ssh-agent/cygwin_sock", ui->checkBox->isChecked());
	setting_set_bool("ssh-agent/pageant", ui->checkBox_3->isChecked());
	setting_set_bool("ssh-agent/ms_ssh", ui->checkBox_2->isChecked());
	setting_set_bool("general/minimize_to_notification_area",
					 ui->checkBox_5->isChecked());
	setting_set_bool("general/close_to_notification_area",
					 ui->checkBox_6->isChecked());

	//const char *ssh_auth_sock = getenv("SSH_AUTH_SOCK");
    std::wstring ssh_auth_sock;
    reg_read_cur_user(L"Environment", L"SSH_AUTH_SOCK", ssh_auth_sock);
	std::wstring ui_ssh_auth_sock = ui->lineEdit->text().toStdWString();

    if (ui_ssh_auth_sock != ssh_auth_sock)
    {
		std::wstring env = L"SSH_AUTH_SOCK=";
		env += ui_ssh_auth_sock;
		_wputenv(env.c_str());
		reg_write_cur_user(L"Environment", L"SSH_AUTH_SOCK", ui_ssh_auth_sock);

		const wchar_t *expandedInformation =
			L"環境変数は各プログラムごとに管理されていて、\n"
			"プログラムを起動した親プログラムの環境変数をひきつぎます。\n"
			"bashなどのシェルプログラムは設定ファイルに従って環境変数を設定しますが、\n"
			"初期値は親プログラムの値となります\n"
			"そしてユーザーが起動するプログラムの大半の親プログラムは、\n"
			"デスクトップやスタートボタンを管理しているexplorerです\n"
			"\n"
			"explorerを含む一部のプログラムは\n"
			"Windowメッセージ(WM_SETTINGCHANGE)を処理することで\n"
			"環境変数の再設定が可能です\n"
			"\n"
			"すべてのウィンドウにメッセージを送信して環境変数の再設定をリクエストします\n"
			"\n"
			"<A HREF=\"http://d.hatena.ne.jp/ganaware/20111102/1320222616\">win-ssh-agent</a>の説明";
		int ret = task_dialog(
			QString::fromUtf8(u8"pagent+").toStdWString().c_str(),
			QString::fromUtf8(u8"環境変数の変更を全てのウィンドウに通知しますか?").toStdWString().c_str(),
			QString::fromUtf8(u8"通知する場合は[yes]を押してください").toStdWString().c_str(),
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

// regedit
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

void SettingDlg::on_pushButton_3_clicked()
{
    exec_regedit(L"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");
}

/*
 *	@param	action	0/1 = 反映/逆転
 */
void SettingDlg::setStartup(int action)
{
    const wchar_t *subkey =
		L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    const wchar_t *valuename = L"pageant+";
    std::wstring str;
    bool result = reg_read_cur_user(subkey, valuename, str);
    switch (action) {
    case 0:
    default:
		// コントロールに反映させる
		break;
    case 1:
		// 現在の状態を逆にする
		if (result) {
			// 削除する
			reg_delete_cur_user(subkey, valuename);
			result = false;
		} else {
			// 登録する
			str = setting_get_my_fullpath().toStdWString();
			reg_write_cur_user(subkey, valuename, str);
			result = true;
		}
		break;
    }
    QString tip;
    if (result == false) {
		tip = u8"設定されていません";
    } else {
		tip = u8"設定されています\n";
		tip += "'" + QString::fromStdWString(str) + "'";
    }
    ui->pushButton_5->setChecked(result);
    ui->pushButton_5->setToolTip(tip);
}

void SettingDlg::on_pushButton_5_clicked()
{
    setStartup(1);
}

// 自分の設定
void SettingDlg::on_pushButton_6_clicked()
{
	if (setting_get_use_inifile() == true) {
		QString ini = setting_get_inifile();
        std::wstring ini_ws = ini.toStdWString();
        ::exec(ini_ws.c_str());
    } else {
		QString reg_path = setting_get_inifile();
        std::wstring reg_path_ws = reg_path.toStdWString();
		exec_regedit(reg_path_ws.c_str());
	}
}


// puttyの設定
void SettingDlg::on_pushButton_7_clicked()
{
	if (get_use_inifile() != 0) {
		QString ini = get_putty_ini().c_str();
        std::wstring ini_ws = ini.toStdWString();
        ::exec(ini_ws.c_str());
	} else {
        const wchar_t *reg_path = L"HKEY_CURRENT_USER\\Software\\SimonTatham\\PuTTY\\Pageant";
		exec_regedit(reg_path);
	}
}

// debug console
void SettingDlg::on_checkBox_4_clicked()
{
	const bool check = ui->checkBox_4->isChecked();
	setting_set_bool("debug/console_enable", check);
	debug_console_show(check);
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:


