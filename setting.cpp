
#include <algorithm>

#include "pageant+.h"
#include "winmisc.h"
#include "debug.h"
#include "setting.h"
#include "db.h"
#include "winhelp_.h"
#include "misc_cpp.h"
#include "puttymem.h"
#include "tree234.h"

#define APPNAME "Pageant"			// access ini and registory
#define INI_FILE	APP_NAME ".ini"
#define INI_FILE_W	L"pageant+.ini"

extern "C" tree234 *passphrases;

static std::wstring putty_path;			// putty.exe

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

public:
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

	/**
	 *	@param _key		"section/key" の文字列
	 *					削除時(str==nullptr)、"section"(key省略)の場合、セクションを全て削除
	 *	@param	str		書き込む文字列
	 *					nullptr時、キー(orセクション)を削除
	 *	TODO レジストリ時削除未テスト
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

static settings *ini_;
static settings *ini_putty_;

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

bool setting_get_bool(const char *key)
{
	return ini_->setting_get_bool_default(key, false);
}

bool setting_get_bool(const char *key, bool _default)
{
	return ini_->setting_get_bool_default(key, _default);
}

void setting_set_bool(const char *key, bool _bool)
{
	ini_->setting_set_bool(key, _bool);
}

#if 0
int setting_get_int(const char *key)
{
	return ini_->setting_get_int(key);
}
#endif

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


/**
 *	@param _use_ini		0/1/2	レジストリ/iniファイル/自動
 *	@param _in_file		iniの場合のファイル名
 *
 *	
 */
void setting_init(int _use_ini, const wchar_t *_ini_file)
{
	bool ok = false;
	std::wstring ini_file_w;

	// レジストリにiniファイル強制指定
	bool r = reg_read_cur_user(L"Software\\pageant+", L"force_ini", ini_file_w);
	if (r == true) {
		// iniファイルを強制指定
		_use_ini = 1;
		if (!ini_file_w.empty()) {
			// ファイル名も指定があれば上書き
			_ini_file = ini_file_w.c_str();
		}
	}
	
	// iniファイル強制
	if (_use_ini == 1) {
		if (_ini_file == NULL) {
			std::wstring f = _GetModuleFileName(NULL);
			size_t pos = f.rfind(L'\\');
			if (pos != std::string::npos) {
				f = f.substr(0, pos+1);
			}
			f = f + INI_FILE_W;
			ini_file_w = f;
		} else {
			ini_file_w = _ini_file;
		}
		_WritePrivateProfileString(L"Generic", L"UseIniFile", ini_file_w.c_str(), L"true");
		ok = true;
		ini_ = new settings(ini_file_w.c_str(), settings::TYPE_INI);
	}

	// iniファイル自動
	if (!ok) {
		if (_use_ini ==2) {
			std::wstring ini_file = get_full_path(INI_FILE_W, false);
			if (!ini_file.empty()) {
				// iniファイルがあった
				ini_file_w = ini_file;
				std::wstring str;
				_GetPrivateProfileString(L"Generic", L"UseIniFile", ini_file_w.c_str(), str);
				if (str_2_bool(str.c_str()) == false) {
					// 使うな指定
					ini_file.clear();
				} else {
					ok = true;
					ini_ = new settings(ini_file_w.c_str(), settings::TYPE_INI);
				}
			}
		}
	}

	
	// レジストリ TODO
	if (!ok) {
		ini_file_w = L"Software\\pageant+";
		ini_ = new settings(ini_file_w.c_str(), settings::TYPE_REG);
	}

	///////
	std::wstring ws;
	r = ini_->setting_get_str("putty/exe", nullptr, ws);
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

	std::wstring putty_inifile_w;	// putty.ini
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
			putty_inifile_w = L"Software\\SimonTatham\\PuTTY";	// "\\Pageant"
			ini_putty_ = new settings(putty_inifile_w.c_str(), settings::TYPE_REG);
		} else {
			putty_inifile_w = ws;
			ini_putty_ = new settings(putty_inifile_w.c_str(), settings::TYPE_INI);
		}

#define PUTTY_HELP_FILE		L"putty.hlp"
#define PUTTY_CHM_FILE		L"putty.chm"
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

	
	set_default();
	
	dbgprintf("setting_init() leave\n");
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
	std::wstring v = mb_to_wc(std::string(value));
	setting_set_str(key.c_str(), v.c_str());
}

/**
 *	@param[in]	keyname
 *	@param[out]	value		"accept" or "refuce" or "\0"
 *	@param[in]	value_size	このサイズは'\0'も含む
 */
void setting_get_confirm_info(const char *keyname, char *value_ptr, size_t value_size)
{
    if (value_size <= 0)
		return;
	value_ptr[0] = '\0';
    if (keyname == NULL) {
		return;
    }
	std::string key = "ssh-agent_confirm/";
	key += keyname;

	std::wstring ws = ini_->setting_get_str(key.c_str(), nullptr);
	if (ws.empty() && ini_putty_ != nullptr) {
		ws = ini_putty_->setting_get_str(key.c_str(), nullptr);
	}

	if (!ws.empty()) {
		std::string s = wc_to_mb(ws);
        const size_t len = (size_t)s.length() + 1;		// '\0'も含む長さ
        if (len < value_size) {
            memcpy(value_ptr, s.c_str(), len);
		}
	}
}

/**
 *	@retval		0-3600[sec]
 *	@retval		-1	no setting
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

void setting_get_keyfiles(std::vector<std::wstring> &list)
{
	int n = 1;
	while(1) {
		std::string key = "Keyfile/file" + std::to_string(n);
		std::wstring fn;
		bool r = ini_->setting_get_str(key.c_str(), nullptr, fn);
		if (r == false) {
			break;
		}
		list.push_back(fn);
		n++;
	}
}

void setting_add_keyfile(const wchar_t *_file)
{
	std::vector<std::wstring> list;
	setting_get_keyfiles(list);

	std::wstring file(_file);
	for (auto &f: list) {
		if (f == file) {
			// すでにあった
			return;
		}
	}
	list.push_back(file);

	int n = 1;
	for (auto &f: list) {
		std::string key = "Keyfile/file" + std::to_string(n);
		ini_->setting_set_str(key.c_str(), f.c_str());
		n++;
	}
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



#define PUTTY_DEFAULT     L"Default%20Settings"
#define PUTTY_REGKEY      L"Software\\SimonTatham\\PuTTY\\Sessions"

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

	for(auto &sec : sectionAry) {
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

void load_passphrases()
{
	if (passphrases == NULL) {
		passphrases = newtree234(NULL);
	}

	std::vector<std::string> passphraseAry;

	// ini or reg から読み出す
    int i = 1;
	while (1) {
		std::string key = "Passphrases/crypto" + std::to_string(i++);
		std::wstring ws_c_pass;
		bool r = ini_->setting_get_str(key.c_str(), nullptr, ws_c_pass);
		if (r == false) {
			break;
		}
		passphraseAry.push_back(wc_to_mb(ws_c_pass));
	}
	if (!get_putty_path().empty()) {
		i = 1;
		while (1) {
			std::string key = "Passphrases/crypto" + std::to_string(i++);
			std::wstring ws_c_pass;
			bool r = ini_putty_->setting_get_str(key.c_str(), nullptr, ws_c_pass);
			if (r == false) {
				break;
			}
			passphraseAry.push_back(wc_to_mb(ws_c_pass));
		}
	}

	// 重複(暗号化済み)パスフレーズを削除する
	std::sort(passphraseAry.begin(), passphraseAry.end());
	passphraseAry.erase(
		std::unique(passphraseAry.begin(), passphraseAry.end()),
		passphraseAry.end());
	
	// パスフレーズの暗号化を解除
	for (std::string &c_passphrase : passphraseAry) {
		const char *buffer = c_passphrase.c_str();
		int original_len = decrypto(buffer, NULL);
		char *original = (char*) malloc(original_len + 1);
		if (original != NULL) {
			decrypto(buffer, original);
			addpos234(passphrases, original, 0);
		}
	}
}

void save_passphrases(const char* passphrase)
{
	if (passphrase == NULL) {
		return;
	}
	size_t plen = strlen(passphrase);
	if (plen == 0) {
		return;
	}
	std::vector<char> buf(plen+1);
	memcpy(&buf[0], passphrase, plen+1);
    int encrypted_len = encrypto(&buf[0], NULL);
    char* encrypted = snewn(encrypted_len + 1, char);
    encrypto(&buf[0], encrypted);
	smemclr(&buf[0], plen+1);
	std::wstring ws_encrypted = mb_to_wc(std::string(encrypted));

	int i = 1;
	while (1) {
		std::string key = "Passphrases/crypto" + std::to_string(i++);
		std::wstring ws_c_pass;
		bool r = ini_->setting_get_str(key.c_str(), nullptr, ws_c_pass);
		if (r == false) {
			// 見つからず、最後に書き込み
			r = ini_->setting_set_str(key.c_str(), ws_encrypted.c_str());
			break;
		}
		if (ws_encrypted == ws_c_pass) {
			// すでにあった、終了
			break;
		}
	}

	smemclr(encrypted, encrypted_len);
	sfree(encrypted);
}

void setting_get_passphrases(std::vector<std::string> &passphraseAry)
{
	passphraseAry.clear();

    if (!passphrases)
        return;

	int n = 0;
	for(;;) {
		const char *pp = (char *)index234(passphrases, n++);
		if (pp == NULL) {
			return;
		}
		passphraseAry.push_back(std::string(pp));
    }
}

void setting_remove_passphrases()
{
	std::string key = "Passphrases";
	ini_->setting_set_str(key.c_str(), nullptr);
}

void setting_remove_confirm_info()
{
	std::string key = "ssh-agent_confirm";
	ini_->setting_set_str(key.c_str(), nullptr);
}


// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
