
#ifdef __cplusplus

#include <string>
#include <vector>

void setting_init(int _use_ini = true, const wchar_t *_ini_file = NULL);
void setting_exit();
std::wstring setting_get_inifile();
bool setting_get_use_inifile();

// General purpose
bool setting_get_bool(const char *key);
bool setting_get_bool(const char *key, bool _default);
void setting_set_bool(const char *key, bool _bool);
bool setting_set_str(const char *key, const wchar_t *str);
std::wstring setting_get_str(const char *key, const wchar_t *_default);
bool setting_set_int(const char *key, int val);
int setting_get_int(const char *key, int _default);

// exclusive use
std::wstring setting_get_my_fullpath();
void setting_get_keyfiles(std::vector<std::wstring> &list);
void setting_add_keyfile(const wchar_t *_file);
std::wstring get_putty_path();
std::wstring get_putty_ini();
int get_use_inifile(void);
std::vector<std::wstring> setting_get_putty_sessions();

void setting_get_passphrases(std::vector<std::string> &passphraseAry);

#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
int setting_get_confirm_timeout();
void setting_write_confirm_info(const char *keyname, const char *value);
void setting_get_confirm_info(const char *keyname, char *value_ptr, size_t value_size);
    
void forget_passphrases(void);
void save_passphrases(const char* passphrase);
void load_passphrases();
void setting_remove_passphrases();
void setting_remove_confirm_info();

#ifdef __cplusplus
}
#endif /* __cplusplus */


// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
