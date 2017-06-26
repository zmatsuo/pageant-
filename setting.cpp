
#include <QCoreApplication>
#include <QSettings>
#include <QApplication>
#include <QDir>

#include "pageant+.h"
#include "winmisc.h"
#include "debug.h"
#include "setting.h"
#include "db.h"
#include "winhelp_.h"

#define APPNAME "Pageant"			// access ini and registory
#define INI_FILE	APP_NAME ".ini"

static QSettings *settings;
static bool use_inifile;
static QString ini_file;
static std::wstring putty_path;		// putty.exe

static bool decide_ini_path(QString &ini)
{
    // カレントを探す
    QDir cur_dir = QDir::current();
    if (cur_dir.exists(INI_FILE)) {
		ini = cur_dir.absolutePath() + "/" + INI_FILE;
		return true;
    }
    // exeと同じフォルダを探す
    QDir exe_dir(QApplication::applicationDirPath());
    if (cur_dir.exists(INI_FILE)) {
		ini = exe_dir.absolutePath() + "/" + INI_FILE;
		return true;
    }
    ini.clear();
    return false;
}

QString setting_get_inifile()
{
    return ini_file;
}

bool setting_get_bool(const char *key)
{
    return settings->value(key, true).toBool();
}

void setting_set_bool(const char *key, bool _bool)
{
	settings->setValue(key, _bool);
	settings->sync();
}

bool setting_isempty(const char *key)
{
	return (settings->value(key) == QVariant());
}

bool setting_isempty(const QString &key)
{
	return (settings->value(key) == QVariant());
}

QString setting_get_my_fullpath()
{
	return QString::fromStdWString(_GetModuleFileName(NULL));
}

static void set_default()
{
	const char *key;
	key = "ssh-agent/pageant";
	if (setting_isempty(key)) {
		setting_set_bool(key, true);
	}
	key = "ssh-agent/cygwin_sock";
	if (setting_isempty(key)) {
		setting_set_bool(key, true);
	}
	key = "ssh-agent/ms_ssh";
	if (setting_isempty(key)) {
		setting_set_bool(key, true);
	}
	key = "general/minimize_to_notification_area";
	if (setting_isempty(key)) {
		setting_set_bool(key, true);
	}
	key = "general/close_to_notification_area";
	if (setting_isempty(key)) {
		setting_set_bool(key, true);
	}
}

/**
 *	@param _use_ini		0/1/2	レジストリ/iniファイル/自動
 *	@param _in_file		iniの場合のファイル名
 */
void setting_init(int _use_ini, const char *_ini_file)
{
	db_init();
    init_help();

	bool ok = false;

	// iniファイル強制
	if (_use_ini == 1) {
		if (_ini_file == NULL) {
			ini_file = QApplication::applicationDirPath() + "/" + INI_FILE;
		} else {
			ini_file = _ini_file;
		}
		use_inifile = true;
		settings = new QSettings(ini_file, QSettings::IniFormat);
		settings->setValue("Generic/UseIniFile", true);
		settings->sync();
		ok = true;
	}

	// iniファイル自動
	if (!ok) {
		if (_use_ini ==2) {
			if (decide_ini_path(ini_file)) {
				// iniファイルが有った
				settings = new QSettings(ini_file, QSettings::IniFormat);
				if (settings->value("Generic/UseIniFile", "0").toBool()) {
					// 使うな指定
					ini_file.clear();
					delete settings;
				} else {
					use_inifile = true;
					ok = true;
				}
			}
		}
	}

	// レジストリ
	if (!ok) {
		ini_file = "HKEY_CURRENT_USER\\SOFTWARE\\pageant+";
		settings = new QSettings(ini_file, QSettings::NativeFormat);
		use_inifile = false;
	}

	set_default();

	///////
	putty_path = get_full_path(L"putty.exe", true);

	
	debug_printf("setting_init()\n");
}

void setting_exit()
{
	delete settings;
	settings = nullptr;
}

bool setting_get_use_inifile()
{
	return use_inifile;
}

void setting_write_confirm_info(const char *keyname, const char *value)
{
	const QString key = "ssh-agent_confirm/" + QString(keyname);
    settings->setValue(key, value);
	settings->sync();
//	write_confirm_info(keyname, value);
}

/**
 *	@param	value_size	このサイズは'\0'も含む
 */
void setting_get_confirm_info(const char *keyname, char *value_ptr, size_t value_size)
{
    if (value_size <= 0)
		return;
	value_ptr[0] = '\0';
    if (keyname == NULL) {
		return;
    }

	const QString key = "ssh-agent_confirm/" + QString(keyname);
	if (!setting_isempty(key)) {
        const std::string s = settings->value(key, "").toString().toStdString();
        const char *ps = s.c_str();
        const size_t len = (size_t)s.length() + 1;		// '\0'も含む長さ
        if (len < value_size) {
            memcpy(value_ptr, ps, len);
		}
    } else {
        // 以前の設定から取得
        get_confirm_info(keyname, value_ptr, value_size);
    }
}

/**
 *	@retval		0-3600[sec]
 *	@retval		-1	no setting
 */
int setting_get_confirm_timeout()
{
	int timeout;
	const char *key = "ssh-agent/timeout";

	if (!setting_isempty(key)) {
		timeout = settings->value(key, -1).toInt();
	} else {
		// puttyの設定から取得
		if (get_use_inifile()) {
			std::string inifile = get_putty_ini();
			timeout = GetPrivateProfileIntA(APPNAME, "ConfirmTimeout", -1, inifile.c_str());
		} else {
			bool r = reg_read_cur_user(L"Software\\SimonTatham\\PuTTY\\" APPNAME, L"ConfirmTimeout", timeout);
			if (r == false) timeout = -1;
		}
	}

	// clip
	if (timeout < 0 || 3600 < timeout)
		timeout = 30;

	return timeout;
}	

std::wstring get_putty_path()
{
    return putty_path;
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
