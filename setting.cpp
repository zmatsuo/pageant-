
#include <QCoreApplication>
#include <QSettings>
#include <QApplication>
#include <QDir>

#include "pageant+.h"
#include "winmisc.h"
#include "debug.h"
#include "setting.h"
#include "db.h"

#define INI_FILE	APP_NAME ".ini"

static QSettings *settings;
static bool use_inifile;
static QString ini_file;

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
	if (settings->value(key) != QVariant()) {
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

int setting_get_confirm_timeout()
{
	int timeout;
	const char *key = "ssh-agent/timeout";

	if (settings->value(key) != QVariant()) {
		timeout = settings->value(key, -1).toInt();
	} else {
		// 以前の設定から取得
		timeout = get_confirm_timeout();
	}

	// clip
	if (timeout < 0 || 3600 < timeout)
		timeout = 30;

	return timeout;
}	

//////////////////////////////////////////////////////////////////////////////
#if 0
QString setting_get_unixdomain_path()
{
	return settings->value("ssh-agent/path").toString();
}

void setting_set_unixdomain_path(const QString &path)
{
	QString path2 = path;
	path2.replace("/", "\\");
    settings->setValue("ssh-agent/path", path2);
}

bool setting_get_unixdomain_enable()
{
	return setting_get_bool("ssh-agent/cygwin_sock");
}

void setting_set_unixdomain_enable(bool enable)
{
	setting_set_bool("ssh-agent/cygwin_sock", enable);
}
#endif

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
