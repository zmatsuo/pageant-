
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void keystore_init();
void keystore_exit();

struct RSAKey *pageant_get_ssh1_key(const struct RSAKey *key);

/*
 * Accessor functions for Pageant's internal key lists. Fetch the nth
 * key; count the keys; attempt to add a key (returning true on
 * success, in which case the ownership of the key structure has been
 * taken over by pageant.c); attempt to delete a key (returning true
 * on success, in which case the ownership of the key structure is
 * passed back to the client).
 */
struct RSAKey *pageant_nth_ssh1_key(int i);
struct ssh2_userkey *pageant_nth_ssh2_key(int i);
int pageant_count_ssh1_keys(void);
int pageant_count_ssh2_keys(void);
int pageant_add_ssh1_key(struct RSAKey *rkey);
int pageant_add_ssh2_key(struct ssh2_userkey *skey);
int pageant_delete_ssh1_key(struct RSAKey *rkey);
int pageant_delete_ssh2_key(struct ssh2_userkey *skey);
void pageant_delete_ssh1_key_all();
void pageant_delete_ssh2_key_all();

struct ssh2_userkey *pageant_get_ssh2_key_from_fp_sha256(const char *fingerprint);
struct ssh2_userkey *pageant_get_ssh2_key_from_fp(const char *fingerprint);
struct ssh2_userkey *pageant_get_ssh2_key_from_blob(const unsigned char *blob, size_t blob_len);

/*
 * Construct a list of public keys, just as the two LIST_IDENTITIES
 * requests would have returned them.
 */
void *pageant_make_keylist1(int *length);
void *pageant_make_keylist2(int *length);

void pageant_del_key(const char *fingerprint);
char *pageant_get_pubkey(const char *fingerprint);

// TODO:pageant_enum_keys()
// int pageant_enum_keys(pageant_key_enum_fn_t callback, void *callback_ctx,

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

class KeystoreListener {
public:
    virtual void change() = 0;
};

void keystoreRegistListener(KeystoreListener *listener);
void keystoreUnRegistListener(KeystoreListener *listener);

#endif
