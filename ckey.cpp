
#include <assert.h>
#include <string>

#include "puttymem.h"
#include "ssh.h"

#include "ckey.h"

#define _strdup(s)	dupstr(s)

/**
 *	publicキーblobをパースしてssh2_userkeyに取得する
 *
 *	TODO: ssh key系に持っていく
 */
bool parse_one_public_key(
	struct ssh2_userkey **_key,
	const void *msg,
	size_t msglen,
	const char **fail_reason)
{
	const unsigned char *p = (const unsigned char *)msg;
	const unsigned char *msgend = p + msglen;

	if (msgend < p+4) {
		*fail_reason = "request truncated before key algorithm";
		return false;
	}
	int alglen = toint(GET_32BIT(p));
	if (alglen < 0 || alglen > msgend - p) {
		*fail_reason = "request truncated before key algorithm";
		return false;
	}
	const char *alg = (const char *)(p+4);

	struct ssh2_userkey *key = snew(struct ssh2_userkey);
	memset(key, 0, sizeof(struct ssh2_userkey));
	key->alg = find_pubkey_alg_len(alglen, alg);
	if (!key->alg) {
		sfree(key);
		*fail_reason = "algorithm unknown";
		return false;
	}

	// ↓todo:内部で呼ぶ void *ssh_rsakex_newkey(char *data, int len)
	RSAKey *rsa = (RSAKey *)key->alg->newkey(key->alg, (char *)p, msgend - p);
	if (rsa == NULL) {
		*fail_reason = "bad data";
		return false;
	}

	// データの後ろにコメントがついている
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

	key->data = rsa;
	key->comment = NULL;
	*_key = key;
	return true;
}

ckey::ckey()
{
	key_ = nullptr;
}
ckey::ckey(const ckey &rhs)
{
	copy(rhs);
}
ckey &ckey::operator=(const ckey &rhs)
{
	assert(!(&rhs == this));
	if (key_ != nullptr) {
		key_->alg->freekey(key_);
	}
	copy(rhs);
	return *this;
}

ckey::~ckey()
{
	key_->alg->freekey(key_->data);
	if (key_->comment != nullptr) {
		sfree(key_->comment);
		key_->comment = nullptr;
	}
	sfree(key_);
	key_ = nullptr;
}
ckey::ckey(ssh2_userkey *key)
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

// SHA256/base64
std::string ckey::fingerprint_sha256() const
{
#if 1
	int bloblen;
	unsigned char *blob = public_blob(&bloblen);

	unsigned char sha256[256/8];
	SHA256_Simple(blob, bloblen, sha256);
	sfree(blob);

	return base64_encode(sha256, 32, false);
#endif
#if 0
	const RSAKey *rsa = (RSAKey *)key_->data;
//	rsa->comment = NULL;		// TODO check
	char fingerprint[128];
	rsa_fingerprint(fingerprint, sizeof(fingerprint), rsa);
	return std::string(fingerprint);
#endif
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

// バイナリデータをパースしてkeyに再構成する
bool ckey::parse_one_public_key(const void *data, size_t len,
				const char **fail_reason)
{
	bool r = ::parse_one_public_key(&key_, data, len, fail_reason);
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
	return key_->alg->public_blob(key_->data, len);
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

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
