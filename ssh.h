
#pragma once

#include <stdio.h>
#include <string.h>

#include "puttymem.h"
#include "misc.h"
#include "filename.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct ssh_channel;

extern void sshfwd_close(struct ssh_channel *c);
extern int sshfwd_write(struct ssh_channel *c, char *, int);
extern void sshfwd_unthrottle(struct ssh_channel *c, int bufsize);

/*
 * Useful thing.
 */
#ifndef lenof
#define lenof(x) ( (sizeof((x))) / (sizeof(*(x))))
#endif

#define SSH_CIPHER_IDEA		1
#define SSH_CIPHER_DES		2
#define SSH_CIPHER_3DES		3
#define SSH_CIPHER_BLOWFISH	6

#ifdef MSCRYPTOAPI
#define APIEXTRA 8
#else
#define APIEXTRA 0
#endif

#include "sshbn_export.h"

struct RSAKey {
    int bits;
    int bytes;
#ifdef MSCRYPTOAPI
    unsigned long exponent;
    unsigned char *modulus;
#else
    Bignum modulus;
    Bignum exponent;
    Bignum private_exponent;
    Bignum p;
    Bignum q;
    Bignum iqmp;
#endif
    char *comment;
};

struct dss_key {
    Bignum p, q, g, y, x;
};

struct ec_curve;

struct ec_point {
    const struct ec_curve *curve;
    Bignum x, y;
    Bignum z;  /* Jacobian denominator */
    unsigned char infinity;
};

void ec_point_free(struct ec_point *point);

/* Weierstrass form curve */
struct ec_wcurve
{
    Bignum a, b, n;
    struct ec_point G;
};

/* Montgomery form curve */
struct ec_mcurve
{
    Bignum a, b;
    struct ec_point G;
};

/* Edwards form curve */
struct ec_ecurve
{
    Bignum l, d;
    struct ec_point B;
};

struct ec_curve {
    enum { EC_WEIERSTRASS, EC_MONTGOMERY, EC_EDWARDS } type;
    /* 'name' is the identifier of the curve when it has to appear in
     * wire protocol encodings, as it does in e.g. the public key and
     * signature formats for NIST curves. Curves which do not format
     * their keys or signatures in this way just have name==NULL.
     *
     * 'textname' is non-NULL for all curves, and is a human-readable
     * identification suitable for putting in log messages. */
    const char *name, *textname;
    unsigned int fieldBits;
    Bignum p;
    union {
        struct ec_wcurve w;
        struct ec_mcurve m;
        struct ec_ecurve e;
    };
};

const struct ssh_signkey *ec_alg_by_oid(int len, const void *oid,
                                        const struct ec_curve **curve);
const unsigned char *ec_alg_oid(const struct ssh_signkey *alg, int *oidlen);
extern const int ec_nist_curve_lengths[], n_ec_nist_curve_lengths;
int ec_nist_alg_and_curve_by_bits(int bits,
                                  const struct ec_curve **curve,
                                  const struct ssh_signkey **alg);
int ec_ed_alg_and_curve_by_bits(int bits,
                                const struct ec_curve **curve,
                                const struct ssh_signkey **alg);

struct ssh_signkey;

struct ec_key {
    const struct ssh_signkey *signalg;
    struct ec_point publicKey;
    Bignum privateKey;
};

struct ec_point *ec_public(const Bignum privateKey, const struct ec_curve *curve);

int makekey(const unsigned char *data, int len, struct RSAKey *result,
	    const unsigned char **keystr, int order);
int makeprivate(const unsigned char *data, int len, struct RSAKey *result);
int rsaencrypt(unsigned char *data, int length, struct RSAKey *key);
Bignum rsadecrypt(Bignum input, struct RSAKey *key);
void rsasign(unsigned char *data, int length, struct RSAKey *key);
void rsasanitise(struct RSAKey *key);
int rsastr_len(struct RSAKey *key);
void rsastr_fmt(char *str, struct RSAKey *key);
void rsa_fingerprint(char *str, int len, const struct RSAKey *key);
int rsa_verify(struct RSAKey *key);
unsigned char *rsa_public_blob(struct RSAKey *key, int *len);
int rsa_public_blob_len(void *data, int maxlen);
void freersakey(struct RSAKey *key);

#ifndef PUTTY_UINT32_DEFINED
/* This makes assumptions about the int type. */
typedef unsigned int uint32;
#define PUTTY_UINT32_DEFINED
#endif
typedef uint32 word32;

unsigned long crc32_compute(const void *s, size_t len);
unsigned long crc32_update(unsigned long crc_input, const void *s, size_t len);

/* SSH CRC compensation attack detector */
void *crcda_make_context(void);
void crcda_free_context(void *handle);
int detect_attack(void *handle, unsigned char *buf, uint32 len,
		  unsigned char *IV);

/*
 * SSH2 RSA key exchange functions
 */
struct ssh_hash;
void *ssh_rsakex_newkey(char *data, int len);
void ssh_rsakex_freekey(void *key);
int ssh_rsakex_klen(void *key);
void ssh_rsakex_encrypt(const struct ssh_hash *h, unsigned char *in, int inlen,
                        unsigned char *out, int outlen,
                        void *key);

/*
 * SSH2 ECDH key exchange functions
 */
struct ssh_kex;
const char *ssh_ecdhkex_curve_textname(const struct ssh_kex *kex);
void *ssh_ecdhkex_newkey(const struct ssh_kex *kex);
void ssh_ecdhkex_freekey(void *key);
char *ssh_ecdhkex_getpublic(void *key, int *len);
Bignum ssh_ecdhkex_getkey(void *key, char *remoteKey, int remoteKeyLen);

/*
 * Helper function for k generation in DSA, reused in ECDSA
 */
Bignum *dss_gen_k(const char *id_string, Bignum modulus, Bignum private_key,
                  unsigned char *digest, int digest_len);

typedef struct {
    uint32 h[4];
} MD5_Core_State;

struct MD5Context {
#ifdef MSCRYPTOAPI
    unsigned long hHash;
#else
    MD5_Core_State core;
    unsigned char block[64];
    int blkused;
    uint32 lenhi, lenlo;
#endif
};

void MD5Init(struct MD5Context *context);
void MD5Update(struct MD5Context *context, unsigned char const *buf,
	       unsigned len);
void MD5Final(unsigned char digest[16], struct MD5Context *context);
void MD5Simple(void const *p, unsigned len, unsigned char output[16]);

void *hmacmd5_make_context(void *);
void hmacmd5_free_context(void *handle);
void hmacmd5_key(void *handle, void const *key, int len);
void hmacmd5_do_hmac(void *handle, unsigned char const *blk, int len,
		     unsigned char *hmac);

typedef struct {
    uint32 h[5];
    unsigned char block[64];
    int blkused;
    uint32 lenhi, lenlo;
} SHA_State;
void SHA_Init(SHA_State * s);
void SHA_Bytes(SHA_State * s, const void *p, int len);
void SHA_Final(SHA_State * s, unsigned char *output);
void SHA_Simple(const void *p, int len, unsigned char *output);

void hmac_sha1_simple(void *key, int keylen, void *data, int datalen,
		      unsigned char *output);
typedef struct {
    uint32 h[8];
    unsigned char block[64];
    int blkused;
    uint32 lenhi, lenlo;
} SHA256_State;
void SHA256_Init(SHA256_State * s);
void SHA256_Bytes(SHA256_State * s, const void *p, int len);
void SHA256_Final(SHA256_State * s, unsigned char *output);
void SHA256_Simple(const void *p, int len, unsigned char *output);

typedef struct {
    unsigned long hi, lo;
} uint64;
typedef struct {
    uint64 h[8];
    unsigned char block[128];
    int blkused;
    uint32 len[4];
} SHA512_State;
#define SHA384_State SHA512_State
void SHA512_Init(SHA512_State * s);
void SHA512_Bytes(SHA512_State * s, const void *p, int len);
void SHA512_Final(SHA512_State * s, unsigned char *output);
void SHA512_Simple(const void *p, int len, unsigned char *output);
void SHA384_Init(SHA384_State * s);
#define SHA384_Bytes(s, p, len) SHA512_Bytes(s, p, len)
void SHA384_Final(SHA384_State * s, unsigned char *output);
void SHA384_Simple(const void *p, int len, unsigned char *output);

struct ssh_mac;
struct ssh_cipher {
    void *(*make_context)(void);
    void (*free_context)(void *);
    void (*sesskey) (void *, unsigned char *key);	/* for SSH-1 */
    void (*encrypt) (void *, unsigned char *blk, int len);
    void (*decrypt) (void *, unsigned char *blk, int len);
    int blksize;
    const char *text_name;
};

struct ssh2_cipher {
    void *(*make_context)(void);
    void (*free_context)(void *);
    void (*setiv) (void *, unsigned char *key);	/* for SSH-2 */
    void (*setkey) (void *, unsigned char *key);/* for SSH-2 */
    void (*encrypt) (void *, unsigned char *blk, int len);
    void (*decrypt) (void *, unsigned char *blk, int len);
    /* Ignored unless SSH_CIPHER_SEPARATE_LENGTH flag set */
    void (*encrypt_length) (void *, unsigned char *blk, int len, unsigned long seq);
    void (*decrypt_length) (void *, unsigned char *blk, int len, unsigned long seq);
    const char *name;
    int blksize;
    /* real_keybits is the number of bits of entropy genuinely used by
     * the cipher scheme; it's used for deciding how big a
     * Diffie-Hellman group is needed to exchange a key for the
     * cipher. */
    int real_keybits;
    /* padded_keybytes is the number of bytes of key data expected as
     * input to the setkey function; it's used for deciding how much
     * data needs to be generated from the post-kex generation of key
     * material. In a sensible cipher which uses all its key bytes for
     * real work, this will just be real_keybits/8, but in DES-type
     * ciphers which ignore one bit in each byte, it'll be slightly
     * different. */
    int padded_keybytes;
    unsigned int flags;
#define SSH_CIPHER_IS_CBC	1
#define SSH_CIPHER_SEPARATE_LENGTH      2
    const char *text_name;
    /* If set, this takes priority over other MAC. */
    const struct ssh_mac *required_mac;
};

struct ssh2_ciphers {
    int nciphers;
    const struct ssh2_cipher *const *list;
};

struct ssh_mac {
    /* Passes in the cipher context */
    void *(*make_context)(void *);
    void (*free_context)(void *);
    void (*setkey) (void *, unsigned char *key);
    /* whole-packet operations */
    void (*generate) (void *, unsigned char *blk, int len, unsigned long seq);
    int (*verify) (void *, unsigned char *blk, int len, unsigned long seq);
    /* partial-packet operations */
    void (*start) (void *);
    void (*bytes) (void *, unsigned char const *, int);
    void (*genresult) (void *, unsigned char *);
    int (*verresult) (void *, unsigned char const *);
    const char *name, *etm_name;
    int len, keylen;
    const char *text_name;
};

struct ssh_hash {
    void *(*init)(void); /* also allocates context */
    void *(*copy)(const void *);
    void (*bytes)(void *, const void *, int);
    void (*final)(void *, unsigned char *); /* also frees context */
    void (*free)(void *);
    int hlen; /* output length in bytes */
    const char *text_name;
};   

struct ssh_kex {
    const char *name, *groupname;
    enum { KEXTYPE_DH, KEXTYPE_RSA, KEXTYPE_ECDH } main_type;
    const struct ssh_hash *hash;
    const void *extra;                 /* private to the kex methods */
};

struct ssh_kexes {
    int nkexes;
    const struct ssh_kex *const *list;
};

struct ssh_signkey {
    void *(*newkey) (const struct ssh_signkey *self,
                     const char *data, int len);
    void (*freekey) (void *key);
    char *(*fmtkey) (void *key);
    unsigned char *(*public_blob) (void *key, int *len);
    unsigned char *(*private_blob) (void *key, int *len);
    void *(*createkey) (const struct ssh_signkey *self,
                        const unsigned char *pub_blob, int pub_len,
			const unsigned char *priv_blob, int priv_len);
    void *(*openssh_createkey) (const struct ssh_signkey *self,
                                const unsigned char **blob, int *len);
    int (*openssh_fmtkey) (void *key, unsigned char *blob, int len);
    /* OpenSSH private key blobs, as created by openssh_fmtkey and
     * consumed by openssh_createkey, always (at least so far...) take
     * the form of a number of SSH-2 strings / mpints concatenated
     * end-to-end. Because the new-style OpenSSH private key format
     * stores those blobs without a containing string wrapper, we need
     * to know how many strings each one consists of, so that we can
     * skip over the right number to find the next key in the file.
     * openssh_private_npieces gives that information. */
    int openssh_private_npieces;
    int (*pubkey_bits) (const struct ssh_signkey *self,
                        const void *blob, int len);
    int (*verifysig) (void *key, const char *sig, int siglen,
		      const char *data, int datalen);
    unsigned char *(*sign) (void *key, const char *data, int datalen,
			    int *siglen);
    const char *name;
    const char *keytype;               /* for host key cache */
    const void *extra;                 /* private to the public key methods */
    unsigned char *(*sign_rsa) (void *key, const char *data, int datalen,
			       int *siglen, int flag);
};

struct ssh2_userkey {
    const struct ssh_signkey *alg;     /* the key algorithm */
    void *data;			       /* the key data */
    char *comment;		       /* the key comment */
};

/* The maximum length of any hash algorithm used in kex. (bytes) */
#define SSH2_KEX_MAX_HASH_LEN (64) /* SHA-512 */

extern const struct ssh_cipher ssh_3des;
extern const struct ssh_cipher ssh_des;
extern const struct ssh_cipher ssh_blowfish_ssh1;
extern const struct ssh2_ciphers ssh2_3des;
extern const struct ssh2_ciphers ssh2_des;
extern const struct ssh2_ciphers ssh2_aes;
extern const struct ssh2_ciphers ssh2_blowfish;
extern const struct ssh2_ciphers ssh2_arcfour;
extern const struct ssh2_ciphers ssh2_ccp;
extern const struct ssh_hash ssh_sha1;
extern const struct ssh_hash ssh_sha256;
extern const struct ssh_hash ssh_sha384;
extern const struct ssh_hash ssh_sha512;
extern const struct ssh_kexes ssh_diffiehellman_group1;
extern const struct ssh_kexes ssh_diffiehellman_group14;
extern const struct ssh_kexes ssh_diffiehellman_gex;
extern const struct ssh_kexes ssh_rsa_kex;
extern const struct ssh_kexes ssh_ecdh_kex;
extern const struct ssh_signkey ssh_dss;
extern const struct ssh_signkey ssh_rsa;
extern const struct ssh_signkey ssh_ecdsa_ed25519;
extern const struct ssh_signkey ssh_ecdsa_nistp256;
extern const struct ssh_signkey ssh_ecdsa_nistp384;
extern const struct ssh_signkey ssh_ecdsa_nistp521;
extern const struct ssh_mac ssh_hmac_md5;
extern const struct ssh_mac ssh_hmac_sha1;
extern const struct ssh_mac ssh_hmac_sha1_buggy;
extern const struct ssh_mac ssh_hmac_sha1_96;
extern const struct ssh_mac ssh_hmac_sha1_96_buggy;
extern const struct ssh_mac ssh_hmac_sha256;

void *aes_make_context(void);
void aes_free_context(void *handle);
void aes128_key(void *handle, unsigned char *key);
void aes192_key(void *handle, unsigned char *key);
void aes256_key(void *handle, unsigned char *key);
void aes_iv(void *handle, unsigned char *iv);
void aes_ssh2_encrypt_blk(void *handle, unsigned char *blk, int len);
void aes_ssh2_decrypt_blk(void *handle, unsigned char *blk, int len);

/*
 * PuTTY version number formatted as an SSH version string. 
 */
extern char sshver[];

/*
 * Gross hack: pscp will try to start SFTP but fall back to scp1 if
 * that fails. This variable is the means by which scp.c can reach
 * into the SSH code and find out which one it got.
 */
extern int ssh_fallback_cmd(void *handle);

#ifndef MSCRYPTOAPI
void SHATransform(word32 * digest, word32 * data);
#endif

int random_byte(void);
void random_add_noise(void *noise, int length);
void random_add_heavynoise(void *noise, int length);

void logevent(void *, const char *);

#if 0
/* Allocate and register a new channel for port forwarding */
void *new_sock_channel(void *handle, Socket s);
void ssh_send_port_open(void *channel, char *hostname, int port, char *org);

/* Exports from portfwd.c */
extern const char *pfd_newconnect(Socket * s, char *hostname, int port,
				  void *c, const Config *cfg,
				  int addressfamily);
/* desthost == NULL indicates dynamic (SOCKS) port forwarding */
extern const char *pfd_addforward(char *desthost, int destport, char *srcaddr,
				  int port, void *backhandle,
				  const Config *cfg, void **sockdata,
				  int address_family);
extern void pfd_close(Socket s);
extern void pfd_terminate(void *sockdata);
extern int pfd_send(Socket s, char *data, int len);
extern void pfd_confirm(Socket s);
extern void pfd_unthrottle(Socket s);
extern void pfd_override_throttle(Socket s, int enable);
#endif

int loadrsakey(const Filename *filename, struct RSAKey *key,
	       const char *passphrase, const char **errorstr);
int rsakey_encrypted(const Filename *filename, char **comment);
int rsakey_pubblob(const Filename *filename, void **blob, int *bloblen,
		   char **commentptr, const char **errorstr);

int saversakey(const Filename *filename, struct RSAKey *key, char *passphrase);

extern int base64_decode_atom(const char *atom, unsigned char *out);
extern int base64_lines(int datalen);
extern void base64_encode_atom(const unsigned char *data, int n, char *out);
extern void base64_encode(FILE *fp, const unsigned char *data, int datalen,
                          int cpl);

/* ssh2_load_userkey can return this as an error */
extern struct ssh2_userkey ssh2_wrong_passphrase;
#define SSH2_WRONG_PASSPHRASE (&ssh2_wrong_passphrase)

int ssh2_userkey_encrypted(const Filename *filename, char **comment);
struct ssh2_userkey *ssh2_load_userkey(const Filename *filename,
				       const char *passphrase,
                                       const char **errorstr);
unsigned char *ssh2_userkey_loadpub(const Filename *filename, char **algorithm,
				    int *pub_blob_len, char **commentptr,
				    const char **errorstr);
int ssh2_save_userkey(const Filename *filename, struct ssh2_userkey *key,
		      char *passphrase);
const struct ssh_signkey *find_pubkey_alg(const char *name);
const struct ssh_signkey *find_pubkey_alg_len(int namelen, const char *name);

enum {
    SSH_KEYTYPE_UNOPENABLE,
    SSH_KEYTYPE_UNKNOWN,
    SSH_KEYTYPE_SSH1, SSH_KEYTYPE_SSH2,
    /*
     * The OpenSSH key types deserve a little explanation. OpenSSH has
     * two physical formats for private key storage: an old PEM-based
     * one largely dictated by their use of OpenSSL and full of ASN.1,
     * and a new one using the same private key formats used over the
     * wire for talking to ssh-agent. The old format can only support
     * a subset of the key types, because it needs redesign for each
     * key type, and after a while they decided to move to the new
     * format so as not to have to do that.
     *
     * On input, key files are identified as either
     * SSH_KEYTYPE_OPENSSH_PEM or SSH_KEYTYPE_OPENSSH_NEW, describing
     * accurately which actual format the keys are stored in.
     *
     * On output, however, we default to following OpenSSH's own
     * policy of writing out PEM-style keys for maximum backwards
     * compatibility if the key type supports it, and otherwise
     * switching to the new format. So the formats you can select for
     * output are SSH_KEYTYPE_OPENSSH_NEW (forcing the new format for
     * any key type), and SSH_KEYTYPE_OPENSSH_AUTO to use the oldest
     * format supported by whatever key type you're writing out.
     *
     * So we have three type codes, but only two of them usable in any
     * given circumstance. An input key file will never be identified
     * as AUTO, only PEM or NEW; key export UIs should not be able to
     * select PEM, only AUTO or NEW.
     */
    SSH_KEYTYPE_OPENSSH_AUTO,
    SSH_KEYTYPE_OPENSSH_PEM,
    SSH_KEYTYPE_OPENSSH_NEW,
    SSH_KEYTYPE_SSHCOM,
    /*
     * Public-key-only formats, which we still want to be able to read
     * for various purposes.
     */
    SSH_KEYTYPE_SSH1_PUBLIC,
    SSH_KEYTYPE_SSH2_PUBLIC_RFC4716,
    SSH_KEYTYPE_SSH2_PUBLIC_OPENSSH
};
char *ssh1_pubkey_str(struct RSAKey *ssh1key);
void ssh1_write_pubkey(FILE *fp, struct RSAKey *ssh1key);
char *ssh2_pubkey_openssh_str(struct ssh2_userkey *key);
void ssh2_write_pubkey(FILE *fp, const char *comment,
                       const void *v_pub_blob, int pub_len,
                       int keytype);
char *ssh2_fingerprint_blob(const void *blob, int bloblen);
char *ssh2_fingerprint(const struct ssh_signkey *alg, void *data);
int key_type(const Filename *filename);
const char *key_type_to_str(int type);

int import_possible(int type);
int import_target_type(int type);
int import_encrypted(const Filename *filename, int type, char **comment);
int import_ssh1(const Filename *filename, int type,
		struct RSAKey *key, char *passphrase, const char **errmsg_p);
struct ssh2_userkey *import_ssh2(const Filename *filename, int type,
				 char *passphrase, const char **errmsg_p);
int export_ssh1(const Filename *filename, int type,
		struct RSAKey *key, char *passphrase);
int export_ssh2(const Filename *filename, int type,
                struct ssh2_userkey *key, char *passphrase);

void des3_decrypt_pubkey(unsigned char *key, unsigned char *blk, int len);
void des3_encrypt_pubkey(unsigned char *key, unsigned char *blk, int len);
void des3_decrypt_pubkey_ossh(unsigned char *key, unsigned char *iv,
			      unsigned char *blk, int len);
void des3_encrypt_pubkey_ossh(unsigned char *key, unsigned char *iv,
			      unsigned char *blk, int len);
void aes256_encrypt_pubkey(unsigned char *key, unsigned char *blk,
			   int len);
void aes256_decrypt_pubkey(unsigned char *key, unsigned char *blk,
			   int len);

void des_encrypt_xdmauth(const unsigned char *key,
                         unsigned char *blk, int len);
void des_decrypt_xdmauth(const unsigned char *key,
                         unsigned char *blk, int len);

void openssh_bcrypt(const char *passphrase,
                    const unsigned char *salt, int saltbytes,
                    int rounds, unsigned char *out, int outbytes);

/*
 * For progress updates in the key generation utility.
 */
#define PROGFN_INITIALISE 1
#define PROGFN_LIN_PHASE 2
#define PROGFN_EXP_PHASE 3
#define PROGFN_PHASE_EXTENT 4
#define PROGFN_READY 5
#define PROGFN_PROGRESS 6
typedef void (*progfn_t) (void *param, int action, int phase, int progress);

int rsa_generate(struct RSAKey *key, int bits, progfn_t pfn,
		 void *pfnparam);
int dsa_generate(struct dss_key *key, int bits, progfn_t pfn,
		 void *pfnparam);
Bignum primegen(int bits, int modulus, int residue, Bignum factor,
		int phase, progfn_t pfn, void *pfnparam);

/*
 * SSH-1 agent messages.
 */
#if 0
#define SSH1_AGENTC_REQUEST_RSA_IDENTITIES    1
#define SSH1_AGENT_RSA_IDENTITIES_ANSWER      2
#define SSH1_AGENTC_RSA_CHALLENGE             3
#define SSH1_AGENT_RSA_RESPONSE               4
#define SSH1_AGENTC_ADD_RSA_IDENTITY          7
#define SSH1_AGENTC_REMOVE_RSA_IDENTITY       8
#define SSH1_AGENTC_REMOVE_ALL_RSA_IDENTITIES 9	/* openssh private? */
#endif

/*
 * Messages common to SSH-1 and OpenSSH's SSH-2.
 */
#define SSH_AGENT_FAILURE                    5
#define SSH_AGENT_SUCCESS                    6

/*
 * OpenSSH's SSH-2 agent messages.
 *	https://tools.ietf.org/html/draft-miller-ssh-agent-02#section-5.1
 */
#define SSH2_AGENTC_REQUEST_IDENTITIES          11
#define SSH2_AGENT_IDENTITIES_ANSWER            12
#define SSH2_AGENTC_SIGN_REQUEST                13
#define SSH2_AGENT_SIGN_RESPONSE                14
#define SSH2_AGENTC_ADD_IDENTITY                17
#define SSH2_AGENTC_REMOVE_IDENTITY             18
#define SSH2_AGENTC_REMOVE_ALL_IDENTITIES       19
#define SSH2_AGENTC_ADD_ID_CONSTRAINED          25

#define	SSH_AGENT_RSA_SHA2_256	2
#define SSH_AGENT_RSA_SHA2_512	4

/*
 * Need this to warn about support for the original SSH-2 keyfile
 * format.
 */
void old_keyfile_warning(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */
