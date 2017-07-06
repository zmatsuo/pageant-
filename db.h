
#ifndef _DB_H_
#define _DB_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int decrypto(const char* encrypted, char* buffer);
int encrypto(char* original, char* buffer);

char *getfingerprint(int type, const void *key);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#ifdef __cplusplus

#endif

#endif	// _DB_H_

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 8
// End:
