
#include <stdio.h>

#include <QApplication>
#include <QStandardItemModel>
#include <QTime>
#include <vector>
#include <string>

#include "winpgnt.h"
#include "tree234.h"
#include "ssh.h"
#include "db.h"
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
    pageant_enum_keys(getKeylist_sub, &ctx, &retstr);
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
// coding: utf-8
// tab-width: 8
// End:
