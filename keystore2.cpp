
#include "keystore2.h"

#if 0
struct pageant_pubkey *pageant_pubkey_copy(struct pageant_pubkey *key)
{
	struct pageant_pubkey *ret = snew(struct pageant_pubkey);
	ret->blob = snewn(key->bloblen, unsigned char);
	memcpy(ret->blob, key->blob, key->bloblen);
	ret->bloblen = key->bloblen;
	ret->comment = key->comment ? dupstr(key->comment) : NULL;
	ret->ssh_version = key->ssh_version;
	return ret;
}

void pageant_pubkey_free(struct pageant_pubkey *key)
{
	sfree(key->comment);
	sfree(key->blob);
	sfree(key);
}
#endif

