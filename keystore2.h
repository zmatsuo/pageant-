#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pageant_pubkey {
    /* Everything needed to identify a public key found by
     * pageant_enum_keys and pass it back to the agent or other code
     * later */
    void *blob;
    int bloblen;
    char *comment;
    int ssh_version;
};
struct pageant_pubkey *pageant_pubkey_copy(struct pageant_pubkey *key);
void pageant_pubkey_free(struct pageant_pubkey *key);

typedef void (*pageant_key_enum_fn_t)(void *ctx,
                                      const char *fingerprint,
                                      const char *comment,
                                      struct pageant_pubkey *key);
int pageant_enum_keys(pageant_key_enum_fn_t callback, void *callback_ctx,
                      char **retstr);
int pageant_delete_key(struct pageant_pubkey *key, char **retstr);
int pageant_delete_all_keys(char **retstr);

void pageant_delete_key2(int selectedCount, const int *selectedArray);

#ifdef __cplusplus
}
#endif /* __cplusplus */
