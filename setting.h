
#ifdef __cplusplus

#include <string>
#include <vector>

void setting_init(int _use_ini = true, const wchar_t *_ini_file = NULL);
void setting_exit();
std::wstring setting_get_inifile();
bool setting_get_use_inifile();
void setting_remove_all();

// General purpose
bool setting_get_bool(const char *key);
bool setting_get_bool(const char *key, bool _default);
void setting_set_bool(const char *key, bool _bool);
void setting_get_strs(const char *key, std::vector<std::wstring> &strs);
bool setting_set_strs(const char *key, const std::vector<std::wstring> &strs);
bool setting_set_str(const char *key, const wchar_t *str);
std::wstring setting_get_str(const char *key, const wchar_t *_default);
bool setting_set_int(const char *key, int val);
int setting_get_int(const char *key, int _default);

// exclusive use
void setting_get_keyfiles(std::vector<std::wstring> &list);
void setting_add_keyfile(const wchar_t *_file);
void setting_clear_keyfiles();
std::wstring setting_get_my_fullpath();
int get_use_inifile(void);
int setting_get_startup();
void setting_set_startup(bool enable);
bool setting_get_startup_exe_path(std::wstring &exePath);

// for putty
std::wstring get_putty_path();
std::wstring get_putty_ini();
std::vector<std::wstring> setting_get_putty_sessions();
std::wstring setting_get_str_putty(const char *key, const wchar_t *_default);

#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
int setting_get_confirm_timeout();
void setting_write_confirm_info(const char *keyname, const char *value);
void setting_get_confirm_info(const char *keyname, char *value_ptr, size_t value_size);
    
void setting_remove_confirm_info();

#ifdef __cplusplus
}
#endif /* __cplusplus */


// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
