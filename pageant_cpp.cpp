
#include <stdio.h>

#pragma warning(push)
#pragma warning(disable:4127)
#pragma warning(disable:4251)
#include <QApplication>
#include <QStandardItemModel>
#include <QTime>
#pragma warning(pop)

#include <vector>
#include <string>

#include "winpgnt.h"
#include "tree234.h"
#include "ssh.h"
#include "pageant.h"
#include "ckey.h"

typedef struct {
	std::vector<std::string> *keylist;
	std::vector<ckey> *keys;
} getkeylist_ctx;

static void getKeylist_sub(void *ctx,
			   const char *fingerprint,
			   const char *comment,
			   struct pageant_pubkey *key)
{
	getkeylist_ctx *p = (getkeylist_ctx *)ctx;
	std::string s;
	s += fingerprint;
	s += " ";
	s += comment;
	p->keylist->push_back(s);

	ckey Ckey;
	const char *fail_reason;
	Ckey.parse_one_public_key(key->blob, key->bloblen, &fail_reason);
	p->keys->push_back(Ckey);
}

static void getKeylist(
	std::vector<std::string> &keylist,
	std::vector<ckey> &keys)
{
	char *retstr;
	getkeylist_ctx ctx;
	ctx.keylist = &keylist;
	ctx.keys = &keys;
	int r = pageant_enum_keys(getKeylist_sub, &ctx, &retstr);
	if (r != PAGEANT_ACTION_OK) {
		dbgprintf("%s\n", retstr);
		sfree(retstr);
		keylist.clear();
	}
}

// todo 不要かも
typedef struct {
	const char *fingerprint;
} removekeylist_ctx;

static void removeKey_sub(void *ctx,
			   const char *fingerprint,
			   const char *comment,
			   struct pageant_pubkey *key)
{
	removekeylist_ctx *p = (removekeylist_ctx *)ctx;
	if (strcmp(fingerprint, p->fingerprint) == 0) {
		// TODO:ここで削除
		int a = 0;
	}
}

void removeKey(const char *fingerprint)
{
	char *retstr;
	removekeylist_ctx ctx;
	ctx.fingerprint = fingerprint;
	int r = pageant_enum_keys(removeKey_sub, &ctx, &retstr);
	if (r != PAGEANT_ACTION_OK) {
		dbgprintf("%s\n", retstr);
		sfree(retstr);
	}
}


std::vector<KeyListItem> keylist_update2()
{
	std::vector<std::string> keylistSimple;
	std::vector<ckey> ckeys;
	getKeylist(keylistSimple, ckeys);

	std::vector<KeyListItem> keylist;
	for(size_t i=0; i< keylistSimple.size(); i++) {
		const ckey &ckey = ckeys[i];
		KeyListItem item;
		item.no = i;
		item.name = keylistSimple[i];
		item.algorithm = ckey.alg_name();
		item.size = ckey.bits();
		item.md5 = ckey.fingerprint_md5();
		item.sha256 = ckey.fingerprint_sha256();
		item.comment = ckey.key_comment2();
		item.comment2 = ckey.key_comment();
		keylist.push_back(item);
	}

	return keylist;
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature-dos
// tab-width: 4
// End:
