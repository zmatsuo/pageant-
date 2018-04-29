
#include <assert.h>
#include <string>
#include <sstream>

#include "ckey.h"

//#define ENABLE_DEBUG_PRINT
#include "debug.h"
#include "puttymem.h"
#include "ssh.h"
#include "sshbn_export.h"
#include "sshbn_cpp.h"

#define _strdup(s)	dupstr(s)

static void free_key(struct ssh2_userkey *key)
{
	if (key == nullptr) {
		return;
	}
	if (key->data != nullptr) {
		key->alg->freekey(key->data);
		key->data = nullptr;
	}
	if (key->comment != nullptr) {
		sfree(key->comment);
		key->comment = nullptr;
	}
	sfree(key);
}

/**
 *	publicキーblobをパースしてssh2_userkeyに取得する
 *
 */
static size_t parse_one_public_key(
	struct ssh2_userkey **_key,
	const void *msg,
	size_t msglen,
	const char **fail_reason)
{
	const unsigned char *p = (const unsigned char *)msg;
	const unsigned char *msgend = p + msglen;

	if (msgend < p+4) {
		*fail_reason = "request truncated before key algorithm";
		return 0;
	}
	int alglen = toint(GET_32BIT(p));
	if (alglen < 0 || alglen > msgend - p) {
		*fail_reason = "request truncated before key algorithm";
		return 0;
	}
	const char *alg = (const char *)(p+4);

	struct ssh2_userkey *key = snew(struct ssh2_userkey);
	memset(key, 0, sizeof(struct ssh2_userkey));
	key->alg = find_pubkey_alg_len(alglen, alg);
	if (!key->alg) {
		sfree(key);
		*fail_reason = "algorithm unknown";
		return 0;
	}

	// ↓todo:内部で呼ぶ void *ssh_rsakex_newkey(char *data, int len)
	// newkey() はpublic key
	size_t blob_size = msgend - p;
	RSAKey *rsa = (RSAKey *)key->alg->newkey(key->alg, (char *)p, (int)blob_size);
	if (rsa == NULL) {
		*fail_reason = "bad data";
		return 0;
	}

	// データの後ろにコメントがついている
#if 0
	{
		p = msgend;
		int commlen = toint(GET_32BIT(p));
		p += 4;
		char *comment = snewn(commlen + 1, char);
		if (comment) {
			memcpy(comment, p, commlen);
			comment[commlen] = '\0';
		}
		rsa->comment = comment;
	}
#endif

	key->data = rsa;
	key->comment = NULL;
	*_key = key;
	return msglen;
}

/**
 *	privateキーblobをパースしてssh2_userkeyに取得する
 *
 */
static bool parse_one_private_key(
	struct ssh2_userkey **_key,
	const void *msg,
	size_t msglen,
	const char **fail_reason,
    size_t *key_len)
{
	const unsigned char *p = (unsigned char *)msg;
	const unsigned char *msgend = p + msglen;
	if (key_len != nullptr) {
		*key_len = 0;
	}

	if (msgend < p+4) {
		*fail_reason = "request truncated before key algorithm";
		return false;
	}
	const int alglen = toint(GET_32BIT(p));
	p += 4;
	if (alglen < 0 || alglen > msgend - p) {
		*fail_reason = "request truncated before key algorithm";
		return false;
	}
	const char *alg = (const char *)p;
	p += alglen;

	struct ssh2_userkey *key = snew(struct ssh2_userkey);
	key->alg = find_pubkey_alg_len(alglen, alg);
	if (!key->alg) {
		sfree(key);
		*fail_reason = "algorithm unknown";
		return false;
	}

	size_t bloblen_s = msgend - p;
	int bloblen = (int)bloblen_s;		// TODO型
	const unsigned char *p_prev = p;
	// createkey() はprivate key
	key->data = key->alg->openssh_createkey(key->alg, &p, &bloblen);
	if (!key->data) {
		sfree(key);
		*fail_reason = "key setup failed";
		return false;
	}
	bloblen = int(p - p_prev);

	/*
	 * p has been advanced by openssh_createkey, but
	 * certainly not _beyond_ the end of the buffer.
	 */
	assert(p <= msgend);

	if (msgend < p+4) {
		key->alg->freekey(key->data);
		sfree(key);
		*fail_reason = "request truncated before key comment";
		return false;
	}
	int commlen = toint(GET_32BIT(p));
	p += 4;

	if (commlen < 0 || commlen > msgend - p) {
		key->alg->freekey(key->data);
		sfree(key);
		*fail_reason = "request truncated before key comment";
		return false;
	}
	char *comment = snewn(commlen + 1, char);
	if (comment) {
		memcpy(comment, p, commlen);
		comment[commlen] = '\0';
	}
	key->comment = comment;

	*_key = key;

	if (key_len != nullptr) {
		*key_len = 4 + alglen + bloblen + 4 + commlen;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////////

ckey::ckey()
{
	key_ = nullptr;
	debug_rsa_ = nullptr;
}
ckey::ckey(const ckey &rhs)
{
	copy(rhs);
}
ckey &ckey::operator=(const ckey &rhs)
{
	assert(!(&rhs == this));
	free();
	copy(rhs);
	return *this;
}

void ckey::free()
{
	if (key_ != nullptr) {
		::free_key(key_);
		key_ = nullptr;
		debug_rsa_ = nullptr;
	}
}

ckey::~ckey()
{
	free();
}

ckey::ckey(ssh2_userkey *key)
{
	key_ = key;
	debug_rsa_ = (RSAKey *)key_->data;
}

// TODO constをもどしても良い?
ssh2_userkey *ckey::get() const
{
	return key_;
}

ssh2_userkey *ckey::release()
{
	ssh2_userkey *key = key_;
	key_ = nullptr;
	debug_rsa_ = nullptr;
	return key;
}

void ckey::set(ssh2_userkey *key)
{
	key_ = key;
}

std::string ckey::fingerprint() const
{
	char *r = ssh2_fingerprint(key_->alg, key_->data);
	std::string s = r;
	sfree(r);
	return s;
}
std::string ckey::alg_name() const
{
	return std::string(key_->alg->name);
}

// bit数 bignum_bitcount(key->modulus)で取れる
int ckey::bits() const
{
	int bloblen;
	unsigned char *blob = public_blob(&bloblen);
	int bits = key_->alg->pubkey_bits(key_->alg, blob, bloblen);
	sfree(blob);
	return bits;
}

// MD5/hex
std::string ckey::fingerprint_md5() const
{
	int bloblen;
	unsigned char *blob = public_blob(&bloblen);

	unsigned char digest[16];
	MD5Simple(blob, bloblen, digest);
	sfree(blob);

	char fingerprint_str[16 * 3];
	for (int i = 0; i < 16; i++)
		sprintf(fingerprint_str + i * 3, "%02x%s", digest[i], i == 15 ? "" : ":");
	return std::string(fingerprint_str);
}

std::string ckey::fingerprint_md5_comp() const
{
	int bloblen;
	unsigned char *blob = public_blob(&bloblen);

	char *f = ssh2_fingerprint_blob(blob, bloblen);
	sfree(blob);

	std::string fingerprint = f;
	sfree(f);

	return fingerprint;
}

// ある?
std::string ckey::fingerprint_sha1() const
{
	int bloblen;
	unsigned char *blob = public_blob(&bloblen);

	unsigned char sha1[20];
	SHA_Simple(blob, bloblen, sha1);
	sfree(blob);

	char sha1_str[40 + 1];
	for (int i = 0; i < 20; i++)
		sprintf(sha1_str + i * 2, "%02x", sha1[i]);

	return std::string(sha1_str);
}

/**
 *	@retval	paddingの長さを含まない出力文字列長
 *		todo: miscの関数と入れ替える
 */
static size_t base64_encode_atom_2(const unsigned char *data, size_t *n, char *out)
{
    static const char base64_chars[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    uint32_t u32;
	size_t output_len = 0;
	if (*n >= 3) {
		u32 = data[0] << 16;
		u32 |= data[1] << 8;
		u32 |= data[2];
		out[0] = base64_chars[(u32 >> 18) & 0x3F];
		out[1] = base64_chars[(u32 >> 12) & 0x3F];
		out[2] = base64_chars[(u32 >> 6) & 0x3F];
		out[3] = base64_chars[u32 & 0x3F];
		output_len = 4;
		*n -= 3;
	} else if (*n == 2) {
		u32 = data[0] << 16;
		u32 |= data[1] << 8;
		out[0] = base64_chars[(u32 >> 18) & 0x3F];
		out[1] = base64_chars[(u32 >> 12) & 0x3F];
		out[2] = base64_chars[(u32 >> 6) & 0x3F];
		out[3] = '=';
		output_len = 3;
		*n = 0;
	} else if (*n == 1) {
		u32 = data[0] << 16;
		out[0] = base64_chars[(u32 >> 18) & 0x3F];
		out[1] = base64_chars[(u32 >> 12) & 0x3F];
		out[2] = '=';
		out[3] = '=';
		output_len = 2;
		*n = 0;
	}
	return output_len;
}

static std::string base64_encode(const void *data, size_t datalen, bool padding_enable)
{
	const uint8_t *src = (uint8_t *)data;
	std::vector<char> buf;
	buf.reserve(datalen*4/3 + 1);
	size_t pos = 0;
	size_t out_len = 0;
    while (datalen > 0) {
		buf.resize(pos+4);
		char *out = &buf[pos];
		out_len = base64_encode_atom_2(src, &datalen, out);
		src += 3;
		pos += out_len;
    }
	if (padding_enable)
		buf.push_back('\0');
	else {
		if (out_len == 4)
			buf.push_back('\0');
		else
			buf[pos] = '\0';		// cut padding
	}
	return &buf[0];
}

#if 0
static unsigned char *public_blob_v2(const struct ssh2_userkey *key, int *len)
{
	return key->alg->public_blob(key->data, len);
}

static char *ssh2_fingerprint_sha256(const struct ssh2_userkey *key)
{
	int len;
	unsigned char *blob;
	blob = public_blob_v2(key, &len);

	unsigned char sha256[256/8];
	SHA256_Simple(blob, len, sha256);
	sfree(blob);

	auto s = base64_encode(sha256, 32, false);
	char *fp_sha256 = snewn(s.length() + 1, char);
	memcpy(fp_sha256, s.c_str(), s.length()+1);

	return fp_sha256;
}
#endif

// SHA256/base64
std::string ckey::fingerprint_sha256() const
{
	int bloblen;
	unsigned char *blob = public_blob(&bloblen);
	if (blob == nullptr) {
		return "";
	}

	unsigned char sha256[256/8];
	SHA256_Simple(blob, bloblen, sha256);
	sfree(blob);

	return base64_encode(sha256, 32, false);
}

// こちらでつけたコメント(=ファイル名)
std::string ckey::key_comment() const
{
	std::string s;
	if (key_ == nullptr) {
		return s;
	}
	if (key_->comment == nullptr) {
		return s;
	}
	s = key_->comment;
	return s;
}

void ckey::set_fname(const char *fn)
{
	if (key_->comment != nullptr) {
		sfree(key_->comment);
		key_->comment = nullptr;
	}
	key_->comment = _strdup(fn);
}

// キーについているコメント
std::string ckey::key_comment2() const
{
	std::string s;
	if (key_ == nullptr) {
		return s;
	}
	const struct RSAKey *rsa = (struct RSAKey *)key_->data;
	if (rsa == nullptr) {
		return s;
	}
	if (rsa->comment == nullptr) {
		return s;
	}
	s = rsa->comment;
	return s;
}

void ckey::set_comment(const char *comment)
{
}

// バイナリデータをパースしてkeyに再構成する
bool ckey::parse_one_public_key(
	const void *data, size_t len,
	const char **fail_reason)
{
	if (key_ != nullptr) {
		free();
	}
	size_t r = ::parse_one_public_key(&key_, data, len, fail_reason);
	if (r == 0) {
		return false;
	}
	debug_rsa_ = (RSAKey *)key_->data;
	return true;
}

bool ckey::parse_one_public_key(
	const std::vector<uint8_t> &blob, size_t &pos,
	const char **fail_reason)
{
	const void *data = reinterpret_cast<const void *>(&blob[pos]);
	size_t len = blob.size() - pos;
	return parse_one_public_key(data, len, fail_reason);
}

bool ckey::parse_one_public_key(
	const std::vector<uint8_t> &blob,
	const char **fail_reason)
{
	size_t pos = 0;
	return parse_one_public_key(blob, pos, fail_reason);
}

bool ckey::parse_one_private_key(
	const void *data, size_t len,
	const char **fail_reason, size_t *key_len)
{
	if (key_ != nullptr) {
		free();
	}
	bool r = ::parse_one_private_key(&key_, data, len, fail_reason, key_len);
	debug_rsa_ = (RSAKey *)key_->data;
	return r;
}

void ckey::get_raw_key(ssh2_userkey **key) const
{
	ssh2_userkey *ret_key = snew(struct ssh2_userkey);
	copy_ssh2_userkey(*ret_key, *key_);
	*key = ret_key;
}

unsigned char *ckey::public_blob(int *len) const
{
	if (key_->data == nullptr)
		return nullptr;
	return key_->alg->public_blob(key_->data, len);
}

std::vector<uint8_t> ckey::public_blob_v() const
{
	int len;
	unsigned char *blob = public_blob(&len);
	std::vector<uint8_t> blob_v(blob, &blob[len]);		// todo
	sfree(blob);
	return blob_v;
}

void ckey::copy(const ckey &rhs)
{
	key_ = snew(struct ssh2_userkey);
	copy_ssh2_userkey(*key_, *rhs.key_);
	debug_rsa_ = (RSAKey *)key_->data;
}

void ckey::RSAKey_copy(const RSAKey &rhs)
{
	RSAKey *rsa = (RSAKey *)key_->data;
	debug_rsa_ = rsa;
	copy_RSAKey(*rsa, rhs);
}

void ckey::copy_ssh2_userkey(ssh2_userkey &dest, const ssh2_userkey &src)
{
	memset(&dest, 0, sizeof(ssh2_userkey));
	dest.alg = src.alg;
	if (src.comment != nullptr) {
		dest.comment = _strdup(src.comment);
	}
	RSAKey *rsa = snew(struct RSAKey);
	dest.data = rsa;
	const RSAKey *src_rsa = (RSAKey *)src.data;
	copy_RSAKey(*rsa, *src_rsa);
}

void ckey::copy_RSAKey(RSAKey &dest, const RSAKey &src)
{
	memset(&dest, 0, sizeof(RSAKey));
	dest.exponent = copybn(src.exponent);
	dest.modulus = copybn(src.modulus);
	if (src.private_exponent != nullptr) {
		dest.private_exponent = copybn(src.private_exponent);
	}
	if (src.p != nullptr) {
		dest.p = copybn(src.p);
	}
	if (src.q != nullptr) {
		dest.q = copybn(src.q);
	}
	if (src.iqmp != nullptr) {
		dest.iqmp = copybn(src.iqmp);
	}
	dest.bits = src.bits;
	dest.bytes = src.bytes;
	if (src.comment != nullptr) {
		dest.comment = _strdup(src.comment);
	}
}

/**
 * keyのfingerprint取得(md5)
 * Return a buffer malloced to be as big as necessary (caller must free).
 */
#if 0
char *getfingerprint(int type, const void *key)
{
    if (type == SSH1_AGENTC_REMOVE_ALL_RSA_IDENTITIES ||
		type == SSH2_AGENTC_REMOVE_ALL_IDENTITIES) {
		return NULL;
    }
    if (key == NULL) {
		return NULL;
    }

    const size_t fingerprint_length = 512;
    char* fingerprint = (char*) malloc(fingerprint_length);
    fingerprint[0] = '\0';
    if (type == SSH1_AGENTC_RSA_CHALLENGE
		|| type == SSH1_AGENTC_ADD_RSA_IDENTITY
		|| type == SSH1_AGENTC_REMOVE_RSA_IDENTITY) {
		strcpy(fingerprint, "ssh1:");
		rsa_fingerprint(fingerprint + 5, fingerprint_length - 5, (struct RSAKey*) key);
    } else {
		struct ssh2_userkey* skey = (struct ssh2_userkey*) key;
//	char* fp = skey->alg->fingerprint(skey->data);
		char test[100];
		strcpy(test, "aslkj sadf adsf aeg adf");
		char* fp = test;
		strncpy(fingerprint, fp, fingerprint_length);
		size_t fp_length = strlen(fingerprint);
		if (fp_length < fingerprint_length - 2) {
			fingerprint[fp_length] = ' ';
			strncpy(fingerprint + fp_length + 1, skey->comment,
					fingerprint_length - fp_length - 1);
		}
    }
    return fingerprint;
}
#endif

bool parse_public_keys(
	const void *data, size_t len,
	std::vector<ckey> &keys,
	const char **fail_reason)
{
	keys.clear();
	const unsigned char *msg = (unsigned char *)data;
	if (len < 4) {
		*fail_reason = "too short";
		return false;
	}
	int count = toint(GET_32BIT(msg));
	msg += 4;
	len -= 4;
	dbgprintf("key count=%d\n", count);

	for(int i=0; i<count; i++) {
		if (len < 4) {
			*fail_reason = "too short";
			return false;
		}
		size_t key_len = toint(GET_32BIT(msg));
		if (len < key_len) {
			*fail_reason = "too short";
			return false;
		}
		msg += 4;
		len -= 4;
		const unsigned char *key_blob = msg;
		ckey key;
		bool r = key.parse_one_public_key(key_blob, key_len, fail_reason);
		if (r == false) {
			return false;
		}
		msg += key_len;
		len -= key_len;
		if (len < 4) {
			*fail_reason = "too short";
			return false;
		}
		size_t comment_len = toint(GET_32BIT(msg));
		msg += 4;
		len -= 4;
		if (len < comment_len) {
			*fail_reason = "too short";
			return false;
		}
		// TODO:コメントを処理していない
		msg += comment_len;
		len -= comment_len;
		keys.push_back(key);
	}

	return true;
}

ckey ckey::create(const ssh2_userkey *key)
{
	ssh2_userkey *k = snew(struct ssh2_userkey);
	copy_ssh2_userkey(*k, *key);
    ckey class_ckey(k);
	return class_ckey;
}

void ckey::dump() const
{
	std::ostringstream oss;
	const RSAKey *rsa = (RSAKey *)key_->data;
	oss
		<< "key\n"
		<< " fingerprint " << fingerprint() << "\n"
		<< " md5 " << fingerprint_md5() << "\n"
		<< " sha256 " << fingerprint_sha256() << "\n"
		<< " alg " << alg_name() << " " << bits() << "\n"
		<< " comment " << key_comment() << "\n"
		<< " comment2 " << key_comment2() << "\n"
		<< " exponent " << bignum_tostr(rsa->exponent) << "\n"
		<< " modulus " << bignum_tostr(rsa->modulus) << "\n"
		<< " private_exponent " << bignum_tostr(rsa->private_exponent) << "\n"
		<< " p " << bignum_tostr(rsa->p) << "\n"
		<< " q " << bignum_tostr(rsa->q) << "\n"
		<< " iqmp " << bignum_tostr(rsa->iqmp) << "\n";
	dbgprintf(oss.str().c_str());
}

void ckey::dump_keys(const std::vector<ckey> &keys)
{
	for(const ckey &key : keys) {
		key.dump();
	}
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
