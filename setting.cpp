
#include "pageant+.h"
#include "winmisc.h"
#include "debug.h"
#include "setting.h"
#include "winhelp_.h"
#include "misc_cpp.h"

#define REG_HKCU_APP_PATH   "Software\\pageant+"
#define PUTTY_DEFAULT       L"Default%20Settings"
#define PUTTY_REGKEY        L"Software\\SimonTatham\\PuTTY\\Sessions"
#define AGENT_PIPE_ID		"\\\\.\\pipe\\openssh-ssh-agent"

class settings;

static std::wstring exe_fullpath_;
static std::wstring putty_path;         // putty.exe
static settings *ini_;
static settings *ini_putty_;


static bool str_2_bool(const wchar_t *str)
{
    if (str == nullptr)
        return false;
    if (_wcsicmp(str, L"true") == 0)
        return true;
    if (wcscmp(str, L"1") == 0)
        return true;
    return false;
}

class settings {
public:
    enum type_t {
        TYPE_REG,
        TYPE_INI,
    };
    settings(const wchar_t *base, type_t type) {
        base_ = base;
        type_ = type;
    }
    virtual ~settings() {}
private:
    static void get_key_sec(const char *unikey, std::wstring &section, std::wstring &key)
    {
        std::wstring key_sec = mb_to_wc(std::string(unikey));
        size_t slash_pos = key_sec.find_first_of('/');
        if (slash_pos != std::string::npos) {
            section = key_sec.substr(0, slash_pos);
            key = key_sec.substr(slash_pos+1);
        } else {
            // section only
            section = key_sec;
            key.clear();
        }
    }

    bool setting_get_str_ini(
        const wchar_t *section, const wchar_t *key,
        const wchar_t *_default, std::wstring &str)
    {
        bool r = _GetPrivateProfileString(section, key, base_.c_str(), str);
        if (r == false) {
            if (_default == nullptr) {
                str.clear();
            } else {
                str = _default;
            }
            return false;
        }
        return true;
    }

    bool setting_get_str_reg(
        const wchar_t *section, const wchar_t *key,
        const wchar_t *_default, std::wstring &str)
    {
        std::wstring base = base_;
        base += L"\\";
        base += section;
        bool r = reg_read_cur_user(base.c_str(), key, str);
        if (r == false) {
            if (_default == nullptr) {
                str.clear();
            } else {
                str = _default;
            }
            return false;
        }
        return true;
    }

    bool setting_get_strs_reg(
        const wchar_t *section, const wchar_t *key,
        std::vector<std::wstring> &strs)
    {
        std::wstring base = base_;
        base += L"\\";
        base += section;
        bool r = reg_read_cur_user(base.c_str(), key, strs);
        if (r == false) {
            strs.clear();
            return false;
        }
        return true;
    }

    bool setting_get_strs_ini(
        const wchar_t *section, const wchar_t *key,
        std::vector<std::wstring> &strs)
    {
        strs.clear();
        int n = 0;
        int read_error_count = 0;
        while(1) {
            std::wstring key_local = key;
            key_local += L"_" + std::to_wstring(n);
            std::wstring str;
            bool r = _GetPrivateProfileString(section, key_local.c_str(), base_.c_str(), str);
            if (r == false) {
                read_error_count++;
                if (read_error_count == 10)
                    break;
            } else {
                strs.push_back(str);
            }
            n++;
        }
        return true;
    }
    
public:
    bool setting_remove_all()
    {
        if (type_ == TYPE_REG) {
            bool r = reg_delete_tree(HKEY_CURRENT_USER, base_.c_str());
            return r;
        }
        if (type_ == TYPE_INI) {
            BOOL r = DeleteFileW(base_.c_str());
            return r == FALSE ? false : true;
        }
        return false;
    }

    bool setting_get_strs(const char *_key, std::vector<std::wstring> &strs)
    {
        std::wstring section;
        std::wstring key;
        get_key_sec(_key, section, key);

        bool r;
        if (type_ == TYPE_REG) {
            r = setting_get_strs_reg(
                section.c_str(), key.c_str(), strs);
            return r;
        }
        if (type_ == TYPE_INI) {
            r = setting_get_strs_ini(
                section.c_str(), key.c_str(), strs);
            return r;
        }
        strs.clear();
        return false;
    }

    bool setting_get_str(const char *_key, const wchar_t *_default, std::wstring &str)
    {
        std::wstring section;
        std::wstring key;
        get_key_sec(_key, section, key);

        bool r;
        if (type_ == TYPE_REG) {
            r = setting_get_str_reg(
                section.c_str(), key.c_str(), _default, str);
            return r;
        }
        if (type_ == TYPE_INI) {
            r = setting_get_str_ini(
                section.c_str(), key.c_str(), _default, str);
            return r;
        }
        str.clear();
        return false;
    }

    std::wstring setting_get_str(const char *_key, const wchar_t *_default)
    {
        std::wstring str;
        setting_get_str(_key, _default, str);
        return str;
    }

    bool setting_set_strs(const char *_key, const std::vector<std::wstring> &strs)
    {
        std::wstring section;
        std::wstring key;
        get_key_sec(_key, section, key);

        if (type_ == TYPE_REG) {
            std::wstring base = base_;
            base += L"\\";
            base += section;
            if (!strs.empty()) {
                bool r = reg_write_cur_user(base.c_str(), key.c_str(), strs);
                return r;
            } else {
                bool r = reg_delete_cur_user(base.c_str());
                return r;
            }
        }
        if (type_ == TYPE_INI) {
            // 全削除
            _WritePrivateProfileString(section.c_str(), nullptr, base_.c_str(), nullptr);
            std::wstring key_local = key;
            // 個数書き込み
            key_local += L"_count";
            _WritePrivateProfileString(section.c_str(), key_local.c_str(), base_.c_str(),
                                       std::to_wstring(strs.size()).c_str());
            // 個数分キーを書き込み
            int n = 0;
            for (auto s : strs) {
                key_local = key;
                key_local += L"_" + std::to_wstring(n);
                _WritePrivateProfileString(section.c_str(), key_local.c_str(), base_.c_str(), s.c_str());
                n++;
            }
        }
        return false;
    }

    /**
     *  使い方1
     *      section/keyへ文字列設定
     *
     *  @param _key     "section/key" の文字列
     *  @param  str     書き込む文字列
     *
     *  使い方2
     *      section/keyの文字列削除
     *
     *  @param _key     "section/key" の文字列
     *  @param  str     nullptr
     *                  
     *  使い方2
     *      sectionの文字列削除
     *
     *  @param _key     "section" の文字列
     *  @param  str     nullptr
     *                  
     */
    bool setting_set_str(const char *_key, const wchar_t *str)
    {
        std::wstring section;
        std::wstring key;
        get_key_sec(_key, section, key);

        if (type_ == TYPE_REG) {
            std::wstring base = base_;
            base += L"\\";
            base += section;
            if (!key.empty()) {
                bool r = reg_write_cur_user(base.c_str(), key.c_str(), str);
                return r;
            } else {
                bool r = reg_delete_cur_user(base.c_str());
                return r;
            }
        }
        if (type_ == TYPE_INI) {
            bool r = _WritePrivateProfileString(
                section.c_str(), key.empty() ? NULL : key.c_str(), base_.c_str(), str);
            return r;
        }
        return false;
    }

    bool setting_get_int(const char *_key, int &i)
    {
        std::wstring section;
        std::wstring key;
        get_key_sec(_key, section, key);

        if (type_ == TYPE_INI) {
            std::wstring str;
            bool r = setting_get_str_ini(
                section.c_str(), key.c_str(), nullptr, str);
            if (r == false) {
                i = 0;
                return false;
            } else {
                try {
                    i = std::stoi(str);
                } catch (...) {
                    i = 0;
                    r = false;
                }
            }
            return r;
        }
        if (type_ == TYPE_REG) {
            std::wstring base = base_;
            base += L"\\";
            base += section;
            bool r = reg_read_cur_user(base.c_str(), key.c_str(), i);
            if (r == false) {
                i = 0;
            }
            return r;
        }
        return false;
    }

    int setting_get_int(const char *key)
    {
        int i;
        bool r = setting_get_int(key, i);
        if (r == false) {
            return 0;
        }
        return i;
    }

    bool setting_set_int(const char *_key, int i)
    {
        std::wstring section;
        std::wstring key;
        get_key_sec(_key, section, key);

        if (type_ == TYPE_INI) {
            std::wstring str = std::to_wstring(i);
            bool r = _WritePrivateProfileString(
                section.c_str(), key.c_str(), base_.c_str(), str.c_str());
            return r;
        }
        if (type_ == TYPE_REG) {
            std::wstring base = base_;
            base += L"\\";
            base += section;
            bool r = reg_write_cur_user(base.c_str(), key.c_str(), i);
            return r;
        }
        return false;
    }

    bool setting_get_bool(const char *_key, bool &_bool)
    {
        std::wstring section;
        std::wstring key;
        get_key_sec(_key, section, key);

        if (type_ == TYPE_INI) {
            std::wstring str;
            bool r = setting_get_str_ini(
                section.c_str(), key.c_str(), nullptr, str);
            if (r == false) {
                _bool = false;
                return false;
            } else {
                _bool = str_2_bool(str.c_str());
                return true;
            }
        }
        if (type_ == TYPE_REG) {
            std::wstring base = base_;
            base += L"\\";
            base += section;
            DWORD d;
            bool r = reg_read_cur_user(base.c_str(), key.c_str(), d);
            if (r == false) {
                _bool = false;
                return false;
            } else {
                _bool = d == 0 ? false : true;
                return true;
            }
        }
        return false;
    }

    bool setting_get_bool_default(const char *_key, bool _default)
    {
        bool _bool;
        bool r = setting_get_bool(_key, _bool);
        if (r == false) {
            return _default;
        }
        return _bool;
    }

    bool setting_set_bool(const char *_key, bool _bool)
    {
        std::wstring section;
        std::wstring key;
        get_key_sec(_key, section, key);

        if (type_ == TYPE_INI) {
            const wchar_t *str = _bool ? L"true" : L"false";
            bool r = _WritePrivateProfileString(
                section.c_str(), key.c_str(), base_.c_str(), str);
            return r;
        }
        if (type_ == TYPE_REG) {
            std::wstring base = base_;
            base += L"\\";
            base += section;
            DWORD d = (DWORD)_bool;
            bool r = reg_write_cur_user(base.c_str(), key.c_str(), d);
            return r;
        }
        return false;
    }

    bool setting_isempty(const char *_key)
    {
        std::wstring section;
        std::wstring key;
        get_key_sec(_key, section, key);

        if (type_ == TYPE_INI) {
            std::wstring str;
            bool r = _GetPrivateProfileString(
                section.c_str(), key.c_str(), base_.c_str(), str);
            return r == false  ? true : false;
        }
        if (type_ == TYPE_REG) {
            std::wstring base = base_;
            base += L"\\";
            base += section;
            std::wstring str;
            bool r = reg_read_cur_user(base_.c_str(), key.c_str(), str);
            return r == false ? true : false;
        }
        return true;
    }

    std::wstring get_base() const
    {
        if (type_ == TYPE_INI) {
            // パス
            return base_;
        } else {
            std::wstring path = L"HKEY_CURRENT_USER\\";
            path += base_;
            return path;
        }
    }

    bool isRegistory() const
    {
        return type_ == TYPE_REG;
    }

private:
    type_t type_;
    std::wstring base_;
};

int get_use_inifile(void)
{
    return !ini_putty_->isRegistory();
}

std::wstring setting_get_inifile()
{
    return ini_->get_base();
}

bool setting_set_str(const char *key, const wchar_t *str)
{
    return ini_->setting_set_str(key, str);
}

std::wstring setting_get_str(const char *key, const wchar_t *_default)
{
    return ini_->setting_get_str(key, _default);
}

bool setting_get_str(const char *key, std::wstring &s)
{
    s = ini_->setting_get_str(key, L"");
    if (s.empty()) {
        s.clear();
        return false;
    }
    return true;
}

std::wstring setting_get_str_putty(const char *key, const wchar_t *_default)
{
    return ini_putty_->setting_get_str(key, _default);
}

bool setting_get_bool(const char *key, bool _default)
{
    return ini_->setting_get_bool_default(key, _default);
}

void setting_set_bool(const char *key, bool _bool)
{
    ini_->setting_set_bool(key, _bool);
}

int setting_get_int(const char *key, int _default)
{
    int i;
    bool r = ini_->setting_get_int(key, i);
    if (r == false) {
        return _default;
    }
    return i;
}

bool setting_set_int(const char *key, int i)
{
    return ini_->setting_set_int(key, i);
}

bool setting_isempty(const char *key)
{
    return ini_->setting_isempty(key);
}

std::wstring setting_get_my_fullpath()
{
    return _GetModuleFileName(NULL);
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
    key = "ssh-agent/sock_path_ms";
	if (setting_isempty(key)) {
		setting_set_str(key, L"" AGENT_PIPE_ID);
	}
	key = "confirm/confirm_any_request";
	if (setting_isempty(key)) {
		setting_set_bool(key, true);
	}
}

static bool init_putty_ini_sub(const wchar_t *ini)
{
    std::wstring str;
    _GetPrivateProfileString(L"Generic", L"UseIniFile", ini, str);
    return str_2_bool(str.c_str());
}

static std::wstring init_putty_ini(const wchar_t *_putty_path)
{
    bool find_ini = false;

    std::wstring s;
    if (!find_ini)
    {
        s = _putty_path;
        size_t pos = s.rfind(L'\\');
        if (pos != std::string::npos) {
            s = s.substr(0, pos+1);
        }
        s = s + L"putty.ini";
        if (init_putty_ini_sub(s.c_str())) {
            return s;
        }
    }

    if (!find_ini)
    {
        s = _SHGetKnownFolderPath(FOLDERID_RoamingAppData);
        s += L"\\PuTTY\\putty.ini";
        if (init_putty_ini_sub(s.c_str())) {
            return s;
        }
    }

    s.clear();
    return s;
}

static std::wstring search_putty_help(const wchar_t *_putty_path, const wchar_t *help_base)
{
    std::wstring s = _putty_path;
    size_t pos = s.rfind(L'\\');
    if (pos != std::string::npos) {
        s = s.substr(0, pos+1);
    }
    s = s + help_base;
    if (_waccess(s.c_str(), 0) == 0) {
        return s;
    }

    return L"";
}

void setting_exit()
{
    delete ini_;
    ini_ = nullptr;
    if (ini_putty_ != nullptr) {
        delete ini_putty_;
        ini_putty_ = nullptr;
    }
}

bool setting_get_use_inifile()
{
    return !ini_->isRegistory();
}

std::wstring get_putty_ini()
{
    return ini_putty_->get_base();
}

void setting_write_confirm_info(const char *keyname, const char *value)
{
    std::string key = "ssh-agent_confirm/";
    key += keyname;
    std::wstring v = utf8_to_wc(std::string(value));
    setting_set_str(key.c_str(), v.c_str());
}

/**
 *  @param[in]  keyname
 *  @param[out] value       "accept" or "refuce" or empty
 */
void setting_get_confirm_info(const char *keyname, std::string &value)
{
    if (keyname == NULL) {
		value.clear();
        return;
    }
    std::string key = "ssh-agent_confirm/";
    key += keyname;

    std::wstring ws = ini_->setting_get_str(key.c_str(), nullptr);
    if (ws.empty() && ini_putty_ != nullptr) {
        ws = ini_putty_->setting_get_str(key.c_str(), nullptr);
    }

    if (ws.empty()) {
		value.clear();
	} else {
		value = wc_to_mb(ws);
    }
}

/**
 *  @retval     0-3600[sec]
 *  @retval     -1  no setting
 */
int setting_get_confirm_timeout()
{
    int timeout = 0;
    const char *key = "ssh-agent/timeout";

    bool r = ini_->setting_get_int(key, timeout);
    if (r == false) {
        // puttyの設定から取得
        if (ini_putty_ != nullptr) {
            ini_putty_->setting_get_int(key, timeout);
        } else {
            timeout = 0;
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

void setting_get_strs(const char *key, std::vector<std::wstring> &strs)
{
    ini_->setting_get_strs(key, strs);
}

bool setting_set_strs(const char *key, const std::vector<std::wstring> &strs)
{
    return ini_->setting_set_strs(key, strs);
}

void setting_get_keyfiles(std::vector<std::wstring> &list)
{
    setting_get_strs("Keyfile/file", list);
}

void setting_add_keyfile(const wchar_t *_file)
{
    std::vector<std::wstring> list;
    setting_get_strs("Keyfile/file", list);

    std::wstring file(_file);
    for (auto &f: list) {
        if (f == file) {
            // すでにあった
            return;
        }
    }
    list.push_back(file);

    ini_->setting_set_strs("Keyfile/file", list);
}

void setting_clear_keyfiles()
{
    ini_->setting_set_str("Keyfile", nullptr);
}

/* Un-munge session names out of the registry. */
static void unmungestr(const wchar_t *in, wchar_t *out, int outlen)
{
    while (*in) {
        if (*in == L'%' && in[1] && in[2]) {
            int i, j;

            i = in[1] - L'0';
            i -= (i > 9 ? 7 : 0);
            j = in[2] - L'0';
            j -= (j > 9 ? 7 : 0);

            *out++ = (wchar_t)((i << 4) + j);
            if (!--outlen)
                return;
            in += 3;
        } else {
            *out++ = *in++;
            if (!--outlen)
                return;
        }
    }
    *out = '\0';
    return;
}

// puttyのsession一覧の読み込み
std::vector<std::wstring> setting_get_putty_sessions()
{
    static const wchar_t HEADER[] = L"Session:";
    std::vector<std::wstring> resultAry;

    std::vector<std::wstring> sectionAry;
    if (get_use_inifile()) {
        _GetPrivateProfileSectionNames(get_putty_ini().c_str(), sectionAry);
    }
    else {
        reg_enum_key(HKEY_CURRENT_USER, PUTTY_REGKEY, sectionAry);
    }

    for (const auto &sec : sectionAry) {
        // remove header
        const wchar_t *wp = sec.c_str();
        if (get_use_inifile()) {
            if (wcsncmp(wp, HEADER, _countof(HEADER) - 1) != 0) {
                continue;
            }
            if(wcscmp(wp, PUTTY_DEFAULT) == 0) {
                continue;
            }
            wp += _countof(HEADER) - 1;
        }

        wchar_t session_name[MAX_PATH + 1];
        unmungestr(wp, session_name, _countof(session_name));
        resultAry.push_back(std::wstring(session_name));
    }
    return resultAry;
}

void setting_remove_confirm_info()
{
    std::string key = "ssh-agent_confirm";
    ini_->setting_set_str(key.c_str(), nullptr);
}

static const wchar_t *startup_subkey =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t *startup_valuename = L"pageant+";

bool setting_get_startup_exe_path(std::wstring &exePath)
{
    return reg_read_cur_user(startup_subkey, startup_valuename, exePath);
}

/**
 *  @retval 0   設定されていない
 *  @retval 1   設定されている
 *  @retval 2   設定されている、でも他のpath
 */
int setting_get_startup()
{
    std::wstring str;
    bool result = reg_read_cur_user(startup_subkey, startup_valuename, str);
    if (!result) {
        return 0;
    }
    if (str == setting_get_my_fullpath()) {
        return 1;
    }
    return 2;
}

void setting_set_startup(bool enable)
{
    if (enable) {
        std::wstring path = setting_get_my_fullpath();
        reg_write_cur_user(startup_subkey, startup_valuename, path.c_str());
    } else {
        reg_delete_cur_user(startup_subkey, startup_valuename);
    }
}

void setting_remove_all()
{
    ini_->setting_remove_all();
}

// @retval path(without '\\' at end of line)
static std::wstring getPath(const wchar_t *exe_fullpath)
{
    std::wstring path = exe_fullpath;
    size_t pos = path.rfind(L'\\');
    if (pos != std::string::npos) {
        path = path.substr(0, pos+1);
    } else {
        path = L"";
    }
    return path;
}

static void list_initfile_paths(std::vector<std::wstring> &inifile_paths)
{
    std::wstring path;
    std::wstring s2;
	DWORD attributes;

	if (_getenv(L"HOME", path)) {
		if (_GetFileAttributes(path.c_str(), attributes) &&
			attributes & FILE_ATTRIBUTE_DIRECTORY )
		{
			path += L"\\";
			inifile_paths.push_back(path);
		}
    }

	if (_getenv(L"HOMEDRIVE", path) && _getenv(L"HOMEPATH", s2)) {
		path += s2;
		if (_GetFileAttributes(path.c_str(), attributes) &&
			attributes & FILE_ATTRIBUTE_DIRECTORY )
		{
			path += L"\\";
			inifile_paths.push_back(path);
		}
	}

	if (_getenv(L"USERPROFILE", path)) {
		if (_GetFileAttributes(path.c_str(), attributes) &&
			attributes & FILE_ATTRIBUTE_DIRECTORY )
		{
			path += L"\\";
			inifile_paths.push_back(path);
		}
	}

	if (_SHGetKnownFolderPath(FOLDERID_RoamingAppData, path)) {
		path += L"\\pageant+";
		if (_GetFileAttributes(path.c_str(), attributes) &&
			attributes & FILE_ATTRIBUTE_DIRECTORY )
		{
			path += L"\\";
			inifile_paths.push_back(path);
		}
	}

	path = _GetCurrentDirectory() + L"\\";
	inifile_paths.push_back(path);
}
    

static bool setting_determine_ini_filename(
    const wchar_t *exe_fullpath,
    std::wstring &inifile)
{
    std::vector<std::wstring> inifile_paths;
	list_initfile_paths(inifile_paths);
	inifile_paths.emplace_back(getPath(exe_fullpath));

    std::vector<std::wstring> inifile_bases;
    inifile_bases.emplace_back(
        L"pageant+_" +
        _GetComputerNameEx(ComputerNameDnsHostname) + L".ini");
    inifile_bases.emplace_back(L"pageant+.ini");

    // 探す
    for (const auto &path : inifile_paths) {
        for (const auto &base : inifile_bases) {
            auto ini = path + base;
			DWORD attributes;
			if (_GetFileAttributes(ini.c_str(), attributes) &&
				(attributes & ~FILE_ATTRIBUTE_DIRECTORY))
			{
                inifile = ini;
                return true;
            }
        }
    }

	inifile.clear();
	return false;
}

static settings::type_t select_storage(
    const wchar_t *exe_fullpath,
    std::wstring &ini_file)
{
    // レジストリにiniファイル指定がある?
    bool r = reg_read_cur_user(L"" REG_HKCU_APP_PATH, L"IniFile", ini_file);
    if (r == true && !ini_file.empty()) {
		if (!ini_file.empty()) {
			return settings::TYPE_INI;
		}
    }
    
    // iniファイル探す
    r = setting_determine_ini_filename(exe_fullpath, ini_file);
    if (r == true) {
        // ファイルが見つかった
        return settings::TYPE_INI;
    }

    // レジストリを使う
    return settings::TYPE_REG;
}

static void setting_putty()
{
    std::wstring ws;
    bool r = ini_->setting_get_str("putty/exe", nullptr, ws);
    if (r == true && _waccess(ws.c_str(), 0) == 0) {
        // putty.exe は iniのとおりでok
        putty_path = ws;
    } else {
        // putty.exeを探す
        ws = get_full_path(L"putty.exe", true);
        if (ws.empty()) {
            // 見つからない
            ini_->setting_set_str("putty/exe", L"");
            putty_path.clear();
        } else {
            // 書き込み
            ini_->setting_set_str("putty/exe", ws.c_str());
            putty_path = ws;
        }
    }       

    std::wstring putty_inifile_w;   // putty.ini
    std::wstring putty_help;

    if (putty_path.empty()) {
        ini_->setting_set_str("putty/ini", L"");
        putty_inifile_w.clear();
        ini_putty_ = nullptr;
        ini_->setting_set_str("putty/help", L"");
        putty_help.clear();
    } else {
        r = ini_->setting_get_str("putty/ini", nullptr, ws);
        if (r == true && (ws == L"reg" || _waccess(ws.c_str(), 0) == 0)) {
            // putty.iniはiniにあり
            putty_inifile_w = ws;
        } else {
            ws = init_putty_ini(putty_path.c_str());
            if (!ws.empty()) {
                ini_->setting_set_str("putty/ini", ws.c_str());
                putty_inifile_w = ws;
            } else {
                ws = L"reg";
            }

        }
        if (ws == L"reg") {
            putty_inifile_w = L"Software\\SimonTatham\\PuTTY";  // "\\Pageant"
            ini_putty_ = new settings(putty_inifile_w.c_str(), settings::TYPE_REG);
        } else {
            putty_inifile_w = ws;
            ini_putty_ = new settings(putty_inifile_w.c_str(), settings::TYPE_INI);
        }

#define PUTTY_HELP_FILE     L"putty.hlp"
#define PUTTY_CHM_FILE      L"putty.chm"
#define PUTTY_HELP_CONTENTS L"putty.cnt"
    
        r = ini_->setting_get_str("putty/help", nullptr, ws);
        if (r == true && _waccess(ws.c_str(), 0) == 0) {
            putty_help = ws;
        } else {
            //init_help();

            ws = search_putty_help(putty_path.c_str(), PUTTY_CHM_FILE);
            if (!ws.empty()) {
                putty_help = ws;
            } else {
                ws = search_putty_help(putty_path.c_str(), PUTTY_HELP_FILE);
                if (!ws.empty()) {
                    putty_help = ws;
                } else {
                    putty_help.clear();
                }
            }

            ini_->setting_set_str("putty/help", ws.c_str());
        }
    }
}

void setting_init(const wchar_t *exe_fullpath)
{
	exe_fullpath_ = exe_fullpath;
	
    std::wstring ini_file;
    switch (select_storage(exe_fullpath, ini_file)) {
    case settings::TYPE_INI:
        ini_ = new settings(ini_file.c_str(), settings::TYPE_INI);
        break;
    case settings::TYPE_REG:
    default:
        ini_ = new settings(L"" REG_HKCU_APP_PATH, settings::TYPE_REG);
        break;
    }

    setting_putty();
    set_default();
}

std::wstring setting_get_logfile(const wchar_t *exe_fullpath)
{
	std::wstring drive;
	std::wstring dir;
	std::wstring fname;
	_splitpath(exe_fullpath, &drive, &dir, &fname, nullptr);

	std::wstring path;
	if (ini_->isRegistory()) {
		path = _SHGetKnownFolderPath(FOLDERID_RoamingAppData);
		path += L"\\pageant+";
		bool create_dir = false;
		DWORD attributes;
		if (!_GetFileAttributes(path.c_str(), attributes)) {
			create_dir = true;
		} else {
			if (!(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
				DeleteFileW(path.c_str());
				create_dir = true;
			}
		}
		if (create_dir) {
			CreateDirectoryW(path.c_str(), NULL);
		}
		path += L"\\";
	} else {
		path = drive + dir;
	}

	std::wstring logfname = path +
		fname + L"_" +
		_GetComputerNameEx(ComputerNameDnsHostname) + L".log";

	return logfname;
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
