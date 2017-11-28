
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

typedef struct {
	std::vector<std::string> *keylist;
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
}

void getKeylist(std::vector<std::string> &keylist)
{
	char *retstr;
	getkeylist_ctx ctx;
	ctx.keylist = &keylist;
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
	getKeylist(keylistSimple);

	std::vector<KeyListItem> keylist;
	for(size_t i=0; i< keylistSimple.size(); i++) {
		KeyListItem item;
		item.no = i;
		item.name = keylistSimple[i];
		item.comment = "comment!";
		keylist.push_back(item);
	}

	return keylist;
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature-dos
// tab-width: 4
// End:
