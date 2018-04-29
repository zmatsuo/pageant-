/**
   keystore.cpp

   Copyright (c) 2018 zmatsuo

   This software is released under the MIT License.
   http://opensource.org/licenses/mit-license.php
*/

#include "keystore.h"

#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include <algorithm>

//#define ENABLE_DEBUG_PRINT
#include "puttymem.h"
#include "tree234.h"
#include "ssh.h"
#include "pageant.h"
#include "pageant_.h"
#include "setting.h"
#include "ckey.h"
#include "debug.h"


static tree234 *ssh2keys;

static std::vector<KeystoreListener *> listeners;

/*
 * Blob structure for passing to the asymmetric SSH-2 key compare
 * function, prototyped here.
 */
struct blob {
    const unsigned char *blob_;
    size_t len;
};

static void callListeners()
{
	// TODO: lock
	for (auto listener : listeners) {
		listener->change();
	}
}

void keystoreRegistListener(KeystoreListener *listener)
{
	listeners.push_back(listener);
}

void keystoreUnRegistListener(KeystoreListener *listener)
{
	listeners.erase(std::remove(listeners.begin(), listeners.end(), listener), listeners.end());
}

/*
 * Key comparison function for the 2-3-4 tree of SSH-2 keys.
 */
static int cmpkeys_ssh2(const void *av, const void *bv)
{
    const struct ssh2_userkey *a = (struct ssh2_userkey *) av;
    const struct ssh2_userkey *b = (struct ssh2_userkey *) bv;
    int i;
    int alen, blen;
    unsigned char *ablob, *bblob;
    int c;

    /*
     * Compare purely by public blob.
     */
    ablob = a->alg->public_blob(a->data, &alen);
    bblob = b->alg->public_blob(b->data, &blen);

    c = 0;
    for (i = 0; i < alen && i < blen; i++) {
		if (ablob[i] < bblob[i]) {
			c = -1;
			break;
		} else if (ablob[i] > bblob[i]) {
			c = +1;
			break;
		}
    }
    if (c == 0 && i < alen)
		c = +1;			       /* a is longer */
    if (c == 0 && i < blen)
		c = -1;			       /* a is longer */

    sfree(ablob);
    sfree(bblob);

    return c;
}

/*
 * Key comparison function for looking up a blob in the 2-3-4 tree
 * of SSH-2 keys.
 */
static int cmpkeys_ssh2_asymm(const void *av, const void *bv)
{
    const struct blob *a = (struct blob *) av;
    const struct ssh2_userkey *b = (struct ssh2_userkey *) bv;
    int i;
    size_t alen;
	int blen;
    const unsigned char *ablob;
    unsigned char *bblob;
    int c;

    /*
     * Compare purely by public blob.
     */
    ablob = a->blob_;
    alen = a->len;
    bblob = b->alg->public_blob(b->data, &blen);

    c = 0;
    for (i = 0; i < alen && i < blen; i++) {
		if (ablob[i] < bblob[i]) {
			c = -1;
			break;
		} else if (ablob[i] > bblob[i]) {
			c = +1;
			break;
		}
    }
    if (c == 0 && i < alen)
		c = +1;			       /* a is longer */
    if (c == 0 && i < blen)
		c = -1;			       /* a is longer */

    sfree(bblob);

    return c;
}

static bool pageant_add_ssh2_key(struct ssh2_userkey *skey)
{
	int result = add234(ssh2keys, skey) == skey;
	callListeners();
    return result == 0 ? false : true;
}

bool keystore_add(ckey &key)
{
	struct ssh2_userkey *skey = key.release();
	bool r = pageant_add_ssh2_key(skey);
	if (!r) {
		key.set(skey);
	}
	return r;
}

static void clear_keys()
{
    struct ssh2_userkey *skey;
    while ((skey = (struct ssh2_userkey *)index234(ssh2keys, 0)) != NULL) {
		del234(ssh2keys, skey);
		ckey key(skey);
    }
}

void keystore_remove_all()
{
	clear_keys();
	callListeners();
}

static int pageant_delete_ssh2_key(const struct ssh2_userkey *skey)
{
    struct ssh2_userkey *deleted = (struct ssh2_userkey *)del234(ssh2keys, skey);
    if (!deleted)
        return FALSE;
    assert(deleted == skey);
	ckey key(deleted);
	callListeners();
    return TRUE;
}

#if 0
static int pageant_search_fingerprint(const void *av, const void *bv)
{
	const char *search_fingerprint = (const char *)av;
	const struct ssh2_userkey *key = (struct ssh2_userkey *) bv;
	char *fingerprint = ssh2_fingerprint(key->alg, key->data);
	int c = 1;
	if (strcmp(search_fingerprint, fingerprint) == 0) {
		c = 0;
	}
	sfree(fingerprint);
	return c;
}
#endif

static int pageant_search_fingerprint_sha256(const void *av, const void *bv)
{
	const char *search_fingerprint = (const char *)av;
	const struct ssh2_userkey *key = (struct ssh2_userkey *) bv;
	ckey _key = ckey::create(key);
	std::string fingerprint_s = _key.fingerprint_sha256();
	const char *fingerprint = fingerprint_s.c_str();
	int c = 1;
	if (strcmp(search_fingerprint, fingerprint) == 0) {
		c = 0;
	}
	return c;
}

static struct ssh2_userkey *pageant_get_ssh2_key_from_blob(
	const unsigned char *blob, size_t blob_len)
{
	struct blob b;
	b.blob_ = blob;
	b.len = blob_len;
	return (struct ssh2_userkey *)find234(ssh2keys, (void *)&b, cmpkeys_ssh2_asymm);
}

bool keystore_get_from_blob(const std::vector<uint8_t> &blob, ckey &key)
{
	struct ssh2_userkey *skey =
		pageant_get_ssh2_key_from_blob(&blob[0], blob.size());
	if (skey == nullptr) {
		return false;
	}
	key = ckey::create(skey);
	return true;
}

bool keystore_get(const ckey &public_key, ckey &key)
{
	std::vector<uint8_t> blob_v = public_key.public_blob_v();
	ckey private_key;
	bool r = keystore_get_from_blob(blob_v, private_key);
	key = private_key;
	return r;
}

// @retval	ssh2_userey
// @retval	nullptr		見つからなかった
#if 0
static struct ssh2_userkey *pageant_get_ssh2_key_from_fp(const char *fingerprint)
{
    struct ssh2_userkey *key;
    key = (struct ssh2_userkey *)find234(ssh2keys, fingerprint, pageant_search_fingerprint);
    return key;
}
#endif

static struct ssh2_userkey *pageant_get_ssh2_key_from_fp_sha256(const char *fingerprint)
{
    struct ssh2_userkey *key;
    key = (struct ssh2_userkey *)find234(ssh2keys, fingerprint, pageant_search_fingerprint_sha256);
    return key;
}

void keystore_init()
{
    ssh2keys = newtree234(cmpkeys_ssh2);
}

void keystore_exit()
{
	clear_keys();
    sfree(ssh2keys);
    ssh2keys = nullptr;
}

//////////////////////////////////////////////////////////////////////////////

std::vector<ckey> keystore_get_keys()
{
	std::vector<ckey> keys;

	struct ssh2_userkey *key;
	for (int i = 0; NULL != (key = (struct ssh2_userkey *)index234(ssh2keys, i)); i++) {
		ckey _ckey = ckey::create(key);
		keys.emplace_back(_ckey);
	}

	return keys;
}

bool keystore_get(const char *fingerprint, ckey &key)
{
	std::vector<ckey> keys = keystore_get_keys();

	for (auto &akey : keys) {
		auto sha256 = akey.fingerprint_sha256();
		if (sha256 == fingerprint) {
			struct ssh2_userkey *skey = pageant_get_ssh2_key_from_fp_sha256(fingerprint);
			key = ckey::create(skey);
			return true;
		}
	}
	return false;
}

bool keystore_remove(const char *fingerprint)
{
	std::vector<ckey> keys = keystore_get_keys();

	for (auto &key : keys) {
		auto sha256 = key.fingerprint_sha256();
		if (sha256 == fingerprint) {
			struct ssh2_userkey *skey = pageant_get_ssh2_key_from_fp_sha256(fingerprint);
			pageant_delete_ssh2_key(skey);
			return true;
		}
	}
	return false;
}

bool keystore_remove(const ckey &key)
{
	return keystore_remove(key.fingerprint_sha256().c_str());
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
