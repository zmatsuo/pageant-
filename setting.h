
#ifdef __cplusplus

#include <string>
#include <QString>

void setting_init(int _use_ini = true, const char *_ini_file = NULL);
void setting_exit();
std::wstring setting_get_inifile();
bool setting_get_use_inifile();
bool setting_isempty(const char *key);
bool setting_get_bool(const char *key);
void setting_set_bool(const char *key, bool _bool);
QString setting_get_my_fullpath();
void setting_get_keyfiles(std::vector<std::wstring> &list);

#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
int setting_get_confirm_timeout();
void setting_write_confirm_info(const char *keyname, const char *value);
void setting_get_confirm_info(const char *keyname, char *value_ptr, size_t value_size);
    
#ifdef __cplusplus
}
#endif /* __cplusplus */

#include "db.h"
