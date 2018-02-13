#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include <vector>
#include <algorithm>

#define ENABLE_DEBUG_PRINT
#include "puttymem.h"
#include "tree234.h"
#include "ssh.h"
#include "pageant.h"
#include "pageant_.h"
#include "setting.h"
#include "ckey.h"
#include "debug.h"

#include "keystore.h"

/*
 * rsakeys stores SSH-1 RSA keys. ssh2keys stores all SSH-2 keys.
 */
static tree234 *rsakeys;
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
 * Key comparison function for the 2-3-4 tree of RSA keys.
 */
static int cmpkeys_rsa(const void *av, const void *bv)
{
    const struct RSAKey *a = (struct RSAKey *) av;
    const struct RSAKey *b = (struct RSAKey *) bv;
    Bignum am, bm;
    int alen, blen;

    am = a->modulus;
    bm = b->modulus;
    /*
     * Compare by length of moduli.
     */
    alen = bignum_bitcount(am);
    blen = bignum_bitcount(bm);
    if (alen > blen)
		return +1;
    else if (alen < blen)
		return -1;
    /*
     * Now compare by moduli themselves.
     */
    alen = (alen + 7) / 8;	       /* byte count */
    while (alen-- > 0) {
		int abyte, bbyte;
		abyte = bignum_byte(am, alen);
		bbyte = bignum_byte(bm, alen);
		if (abyte > bbyte)
			return +1;
		else if (abyte < bbyte)
			return -1;
    }
    /*
     * Give up.
     */
    return 0;
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

/*
 * Create an SSH-1 key list in a malloc'ed buffer; return its
 * length.
 */
void *pageant_make_keylist1(int *length)
{
    int i, nkeys;
	size_t len;
    struct RSAKey *key;
    unsigned char *blob, *p, *ret;
    int bloblen;

    /*
     * Count up the number and length of keys we hold.
     */
    len = 4;
    nkeys = 0;
    for (i = 0; NULL != (key = (struct RSAKey *)index234(rsakeys, i)); i++) {
		nkeys++;
		blob = rsa_public_blob(key, &bloblen);
		len += bloblen;
		sfree(blob);
		len += 4 + strlen(key->comment);
    }

    /* Allocate the buffer. */
    p = ret = snewn(len, unsigned char);
    if (length) *length = len;

    PUT_32BIT(p, nkeys);
    p += 4;
    for (i = 0; NULL != (key = (struct RSAKey *)index234(rsakeys, i)); i++) {
		blob = rsa_public_blob(key, &bloblen);
		memcpy(p, blob, bloblen);
		p += bloblen;
		sfree(blob);
		PUT_32BIT(p, strlen(key->comment));
		memcpy(p + 4, key->comment, strlen(key->comment));
		p += 4 + strlen(key->comment);
    }

    assert(p - ret == len);
    return ret;
}

/*
 * Create an SSH-2 key list in a malloc'ed buffer; return its
 * length.
 */
void *pageant_make_keylist2(int *length)
{
    struct ssh2_userkey *key;
    int i, nkeys;
	size_t len;
    unsigned char *blob, *p, *ret;
    int bloblen;

    /*
     * Count up the number and length of keys we hold.
     */
    len = 4;
    nkeys = 0;
    for (i = 0; NULL != (key = (struct ssh2_userkey *)index234(ssh2keys, i)); i++) {
		nkeys++;
		len += 4;	       /* length field */
		blob = key->alg->public_blob(key->data, &bloblen);
		len += bloblen;
		sfree(blob);
		len += 4 + strlen(key->comment);
    }

    /* Allocate the buffer. */
    p = ret = snewn(len, unsigned char);
    if (length) *length = len;

    /*
     * Packet header is the obvious five bytes, plus four
     * bytes for the key count.
     */
    PUT_32BIT(p, nkeys);
    p += 4;
    for (i = 0; NULL != (key = (struct ssh2_userkey *)index234(ssh2keys, i)); i++) {
		blob = key->alg->public_blob(key->data, &bloblen);
		PUT_32BIT(p, bloblen);
		p += 4;
		memcpy(p, blob, bloblen);
		p += bloblen;
		sfree(blob);
		const char *comment = key->comment;
		size_t comment_len = strlen(comment);
		PUT_32BIT(p, comment_len);
		memcpy(p + 4, comment, comment_len);
		p += 4 + comment_len;
    }

    assert(p - ret == len);
    return ret;
}

struct RSAKey *pageant_nth_ssh1_key(int i)
{
    return (struct RSAKey *)index234(rsakeys, i);
}

struct ssh2_userkey *pageant_nth_ssh2_key(int i)
{
    return (struct ssh2_userkey *)index234(ssh2keys, i);
}

int pageant_count_ssh1_keys(void)
{
    return count234(rsakeys);
}

int pageant_count_ssh2_keys(void)
{
    return count234(ssh2keys);
}

int pageant_add_ssh1_key(struct RSAKey *rkey)
{
	int result = add234(rsakeys, rkey) == rkey;
	callListeners();
	return result;
}

int pageant_add_ssh2_key(struct ssh2_userkey *skey)
{
	int result = add234(ssh2keys, skey) == skey;
	callListeners();
    return result;
}

static void pageant_delete_ssh2_key_all_()
{
    struct ssh2_userkey *skey;
    while ((skey = (struct ssh2_userkey *)index234(ssh2keys, 0)) != NULL) {
		del234(ssh2keys, skey);
		skey->alg->freekey(skey->data);
		sfree(skey);
    }
}

void pageant_delete_ssh2_key_all()
{
	pageant_delete_ssh2_key_all_();
	callListeners();
}

int pageant_delete_ssh1_key(struct RSAKey *rkey)
{
    struct RSAKey *deleted = (struct RSAKey *)del234(rsakeys, rkey);
    if (!deleted)
        return FALSE;
    assert(deleted == rkey);
	callListeners();
    return TRUE;
}

static void pagent_delete_ssh1_key_all_()
{
    struct RSAKey *rkey;
    while ((rkey = (struct RSAKey *)index234(rsakeys, 0)) != NULL) {
		del234(rsakeys, rkey);
		freersakey(rkey);
		sfree(rkey);
    }
}

void pageant_delete_ssh1_key_all()
{
	pagent_delete_ssh1_key_all_();
	callListeners();
}

int pageant_delete_ssh2_key(struct ssh2_userkey *skey)
{
    struct ssh2_userkey *deleted = (struct ssh2_userkey *)del234(ssh2keys, skey);
    if (!deleted)
        return FALSE;
    assert(deleted == skey);
	callListeners();
    return TRUE;
}

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

static int pageant_search_fingerprint_sha256(const void *av, const void *bv)
{
	const char *search_fingerprint = (const char *)av;
	const struct ssh2_userkey *key = (struct ssh2_userkey *) bv;
	char *fingerprint = ssh2_fingerprint_sha256(key);
	int c = 1;
	if (strcmp(search_fingerprint, fingerprint) == 0) {
		c = 0;
	}
	sfree(fingerprint);
	return c;
}

struct RSAKey *pageant_get_ssh1_key(const struct RSAKey *key)
{
    return (struct RSAKey *)find234(rsakeys, key, NULL);
}

struct ssh2_userkey *pageant_get_ssh2_key_from_blob(const unsigned char *blob, size_t blob_len)
{
	struct blob b;
	b.blob_ = blob;
	b.len = blob_len;
	return (struct ssh2_userkey *)find234(ssh2keys, (void *)&b, cmpkeys_ssh2_asymm);
}

// @retval	ssh2_userey
// @retval	nullptr		見つからなかった
struct ssh2_userkey *pageant_get_ssh2_key_from_fp(const char *fingerprint)
{
    struct ssh2_userkey *key;
    key = (struct ssh2_userkey *)find234(ssh2keys, fingerprint, pageant_search_fingerprint);
    return key;
}

struct ssh2_userkey *pageant_get_ssh2_key_from_fp_sha256(const char *fingerprint)
{
    struct ssh2_userkey *key;
    key = (struct ssh2_userkey *)find234(ssh2keys, fingerprint, pageant_search_fingerprint_sha256);
    return key;
}

void keystore_init()
{
    rsakeys = newtree234(cmpkeys_rsa);
    ssh2keys = newtree234(cmpkeys_ssh2);
}

void keystore_exit()
{
	pagent_delete_ssh1_key_all_();
	sfree(rsakeys);
    rsakeys = nullptr;
	pageant_delete_ssh2_key_all_();
    sfree(ssh2keys);
    ssh2keys = nullptr;
}

//int pageant_delete_key(struct pageant_pubkey *key, char **retstr)
//int pageant_delete_ssh2_key(struct ssh2_userkey *skey)

void pageant_del_key(const char *fingerprint)
{
    struct ssh2_userkey *key = pageant_get_ssh2_key_from_fp(fingerprint);
    if (key == NULL) {
		return;
    }

    char *fingerprint2 = ssh2_fingerprint(key->alg, key->data);
    printf("f %s %s\n", fingerprint, fingerprint2);
    sfree(fingerprint2);

    pageant_delete_ssh2_key(key);
}

char *pageant_get_pubkey(const char *fingerprint_sha256)
{
    struct ssh2_userkey *key = pageant_get_ssh2_key_from_fp_sha256(fingerprint_sha256);
    if (key == NULL) {
		return NULL;
    }

    int blob_len;
    unsigned char *blob = key->alg->public_blob(key->data, &blob_len);
    const size_t comment_len = strlen(key->comment);
    const size_t base64_len = 8 + blob_len * 4 / 3 + 1 + comment_len + 2 + 1;
    char *base64_ptr = (char *)malloc(base64_len);

    char *p = base64_ptr;
    memcpy(p, "ssh-rsa ", 8);
    p += 8;
    int i = 0;
    while (i < blob_len) {
		int n = (blob_len - i < 3 ? blob_len - i : 3);
		base64_encode_atom(blob + i, n, p);
		p += 4;
		i += n;
    }
    *p++ = ' ';
    memcpy(p, key->comment, comment_len);
    p += comment_len;
    *p++ = '\r';
    *p++ = '\n';
    *p   = '\0';
    return base64_ptr;
}

//////////////////////////////////////////////////////////////////////////////

void pageant_delete_key2(int selectedCount, const int *selectedArray)
{
    struct RSAKey *rkey;
    struct ssh2_userkey *skey;

    int itemNum = selectedCount;
    if (itemNum == 0) {
		return;
    }

    int rCount = pageant_count_ssh1_keys();
    int sCount = pageant_count_ssh2_keys();
		
    /* go through the non-rsakeys until we've covered them all, 
     * and/or we're out of selected items to check. note that
     * we go *backwards*, to avoid complications from deleting
     * things hence altering the offset of subsequent items
     */
    for (int i = sCount - 1; (itemNum > 0) && (i >= 0); i--) {
		skey = pageant_nth_ssh2_key(i);
			
		if (selectedArray[itemNum-1] == rCount + i) {
			pageant_delete_ssh2_key(skey);
			skey->alg->freekey(skey->data);
			sfree(skey);
			itemNum--;
		}
    }
		
    /* do the same for the rsa keys */
    for (int i = rCount - 1; (itemNum >= 0) && (i >= 0); i--) {
		rkey = pageant_nth_ssh1_key(i);

		if(selectedArray[itemNum] == i) {
			pageant_delete_ssh1_key(rkey);
			freersakey(rkey);
			sfree(rkey);
			itemNum--;
		}
    }
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
