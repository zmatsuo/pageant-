
#include <QCoreApplication>
#include <QApplication>
#include <QDir>

#include "pageant+.h"
#include "winmisc.h"
#include "debug.h"
#include "setting.h"
#include "db.h"
#include "winhelp_.h"
#include "misc_cpp.h"

#define APPNAME "Pageant"			// access ini and registory
#define INI_FILE	APP_NAME ".ini"

static bool use_inifile;
static QString ini_file;			// TODO削除
static std::wstring ini_file_w;
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

std::wstring setting_get_inifile()
{
    return ini_file_w;
}

static void get_key_sec(const char *unikey, std::wstring &section, std::wstring &key)
{
	std::wstring key_sec = mb_to_wc(std::string(unikey));
	size_t slash_pos = key_sec.find_first_of('/');
	section = key_sec.substr(0, slash_pos);
	key = key_sec.substr(slash_pos+1);
}

QString setting_get_str(const char *key)
{
	std::wstring section;
	std::wstring key2;
	get_key_sec(key, section, key2);

	std::wstring str;
	bool r = _GetPrivateProfileString(
		section.c_str(), key2.c_str(), ini_file_w.c_str(), str);
	(void)r;
	return QString::fromStdWString(str);
}

void setting_set_str(const char *key, const wchar_t *str)
{
	std::wstring section;
	std::wstring key2;
	get_key_sec(key, section, key2);

	bool r = _WritePrivateProfileString(
		section.c_str(), key2.c_str(), ini_file_w.c_str(), str);
	(void)r;
}

static bool str_2_bool(const wchar_t *str)
{
	if (_wcsicmp(str, L"true") == 0) {
		return true;
	}
	if (_wcsicmp(str, L"1") == 0) {
		return true;
	}
	return false;
}

bool setting_get_bool(const char *key)
{
	QString str = setting_get_str(key);
	std::wstring wstr = str.toStdWString();
	return str_2_bool(wstr.c_str());
}

void setting_set_bool(const char *key, bool _bool)
{
	const wchar_t *str = _bool ? L"true" : L"false";

	std::wstring section;
	std::wstring key2;
	get_key_sec(key, section, key2);

	bool r = _WritePrivateProfileString(
		section.c_str(), key2.c_str(), ini_file_w.c_str(), str);
	(void)r;
}

int setting_get_int(const char *key)
{
	QString str = setting_get_str(key);
	try {
		return std::stoi(str.toStdString());
	} catch (...) {
		return 0;
	}
	return 0;
}

void setting_set_int(const char *key, int i)
{
	std::wstring s = std::to_wstring(i);
	setting_set_str(key, s.c_str());
}

bool setting_isempty(const char *key)
{
	std::wstring section;
	std::wstring key2;
	get_key_sec(key, section, key2);

	std::wstring str;
	bool r = _GetPrivateProfileString(
		section.c_str(), key2.c_str(), ini_file_w.c_str(), str);
	if (r == false) {
		// empty
		return true;
	}
	return false;
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
		QString s = ini_file;
		s.replace("/","\\");
		ini_file_w = s.toStdWString();
#if 0
		settings = new QSettings(ini_file, QSettings::IniFormat);
		settings->setValue("Generic/UseIniFile", true);
		settings->sync();
#endif
//		setting_set_bool("Generic/UseIniFile", true);
		_WritePrivateProfileString(L"Generic", L"UseIniFile", L"true", ini_file_w.c_str());
		ok = true;
	}

	// iniファイル自動
	if (!ok) {
		if (_use_ini ==2) {
			if (decide_ini_path(ini_file)) {
				// iniファイルが有った
#if 0
				settings = new QSettings(ini_file, QSettings::IniFormat);
				if (settings->value("Generic/UseIniFile", "0").toBool()) {
					// 使うな指定
					ini_file.clear();
					delete settings;
				} else {
					use_inifile = true;
					ok = true;
				}
#endif
				std::wstring str;
				_GetPrivateProfileString(L"Generic", L"UseIniFile",ini_file_w.c_str(), str);
				if (str_2_bool(str.c_str()) == false) {
					// 使うな指定
					ini_file.clear();
//					delete settings;
				} else {
					use_inifile = true;
					ok = true;
				}
			}
		}
	}

	
	// レジストリ
	if (!ok) {
//		settings = NULL;
#if 0
		ini_file = "HKEY_CURRENT_USER\\SOFTWARE\\pageant+";
		settings = new QSettings(ini_file, QSettings::NativeFormat);
		use_inifile = false;
#endif
	}

	QString s = ini_file;
	s.replace("/","\\");
	ini_file_w = mb_to_wc(s.toStdString());

	set_default();

	///////
	putty_path = get_full_path(L"putty.exe", true);

	
	debug_printf("setting_init()\n");
}

void setting_exit()
{
}

bool setting_get_use_inifile()
{
	return use_inifile;
}

void setting_write_confirm_info(const char *keyname, const char *value)
{
	const QString key = "ssh-agent_confirm/" + QString(keyname);
	std::wstring v = mb_to_wc(std::string(value));
	setting_set_str(key.toStdString().c_str(), v.c_str());
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
	if (!setting_isempty(key.toStdString().c_str())) {
		QString s = setting_get_str(key.toStdString().c_str());
		std::string s2 = s.toStdString();

        const char *ps = s2.c_str();
        const size_t len = (size_t)s2.length() + 1;		// '\0'も含む長さ
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
		timeout = setting_get_int(key);
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

void setting_get_keyfiles(std::vector<std::wstring> &list)
{
	list.clear();

	if (!use_inifile) {
		return;
	}

	int n = 0;
	while(1) {
		std::wstring file = L"file" + std::to_wstring(n);
		std::wstring fn;
		bool r = _GetPrivateProfileString(
			L"Keyfile", file.c_str(), ini_file_w.c_str(), fn);
		if (r == false) {
			break;
		}
		list.push_back(fn);
		n++;
	}
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
