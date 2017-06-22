
#ifndef _DB_H_
#define _DB_H_

#include "tree234.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void db_init();
    
//extern tree234 *rsakeys;
//extern tree234 *ssh2keys;
extern tree234 *passphrases;

void forget_passphrases(void);
void save_passphrases(char* passphrase);
void load_passphrases();

int decrypto(const char* encrypted, char* buffer);
int encrypto(char* original, char* buffer);
int get_use_inifile(void);
int get_confirm_timeout();

void write_confirm_info(const char *keyname, const char *value);
void get_confirm_info(const char *keyname, char *value_ptr, size_t value_size);

char *getfingerprint(int type, const void *key);


    
#ifdef __cplusplus
}
#endif /* __cplusplus */


#ifdef __cplusplus
#include <vector>
#include <string>

std::wstring get_putty_path();
std::vector<std::string> setting_get_putty_sessions();
std::string get_putty_ini();
#endif

#endif	// _DB_H_

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 8
// End:
