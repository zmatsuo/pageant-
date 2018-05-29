
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
#include "encode.h"

#define _strdup(s)	dupstr(s)

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
	clear_i();
}

ckey::ckey(const ckey &rhs)
{
	copy(rhs);
}

ckey::ckey(ssh2_userkey *key)
{
	clear_i();
	key_ = key;
	debug_rsa_ = (RSAKey *)key_->data;
}

ckey &ckey::operator=(const ckey &rhs)
{
	assert(!(&rhs == this));
	free();
	copy(rhs);
	return *this;
}

void ckey::clear_i()
{
	key_ = nullptr;
	debug_rsa_ = nullptr;
	debug_ec_ = nullptr;
	expiration_date_ = 0;
	confirmation_required_ = false;
}

void ckey::clear()
{
	free();
}

void ckey::free()
{
	if (key_ != nullptr) {
		if (key_->data != nullptr) {
			key_->alg->freekey(key_->data);
			key_->data = nullptr;
		}
		if (key_->comment != nullptr) {
			sfree(key_->comment);
			key_->comment = nullptr;
		}
		smemclr(key_, sizeof(*key_));
		sfree(key_);
		key_ = nullptr;
	}
	clear_i();
}

ckey::~ckey()
{
	free();
}

// TODO constをもどしても良い?
ssh2_userkey *ckey::get() const
{
	return key_;
}

ssh2_userkey *ckey::release()
{
	ssh2_userkey *key = key_;
	clear_i();
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
	auto blob = public_blob();
	int bits = key_->alg->pubkey_bits(key_->alg, &blob[0], blob.size());
	return bits;
}

// MD5/hex
std::string ckey::fingerprint_md5() const
{
	auto blob = public_blob();

	unsigned char digest[16];
	MD5Simple(&blob[0], blob.size(), digest);

	return hex_encode(digest, 16, true);
}

std::string ckey::fingerprint_md5_comp() const
{
	auto blob = public_blob();

	char *f = ssh2_fingerprint_blob(&blob[0], blob.size());

	std::string fingerprint = f;
	sfree(f);

	return fingerprint;
}

// ある?
std::string ckey::fingerprint_sha1() const
{
	auto blob = public_blob();

	unsigned char sha1[20];
	SHA_Simple(&blob[0], blob.size(), sha1);

	return hex_encode(sha1, sizeof(sha1), false);
}

// SHA256/base64
std::string ckey::fingerprint_sha256() const
{
	auto blob = public_blob();

	unsigned char sha256[256/8];
	SHA256_Simple(&blob[0], blob.size(), sha256);

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

bool ckey::parse_one_private_key(
	const std::vector<uint8_t> &blob, size_t &pos,
	const char **fail_reason)
{
	size_t key_len;
	bool r = parse_one_private_key(
		&blob[pos], blob.size() - pos,
		fail_reason, &key_len);
	pos += key_len;
	return r;
}

void ckey::get_raw_key(ssh2_userkey **key) const
{
	ssh2_userkey *ret_key = snew(struct ssh2_userkey);
	copy_ssh2_userkey(*ret_key, *key_);
	*key = ret_key;
}

unsigned char *ckey::public_blob_o(int *len) const
{
	if (key_->data == nullptr)
		return nullptr;
	return key_->alg->public_blob(key_->data, len);
}

std::vector<uint8_t> ckey::public_blob() const
{
	int len;
	unsigned char *blob = public_blob_o(&len);
	std::vector<uint8_t> blob_v(blob, &blob[len]);		// todo
	sfree(blob);
	return blob_v;
}

void ckey::copy(const ckey &rhs)
{
	key_ = snew(struct ssh2_userkey);
	if (rhs.key_ != nullptr) {
		copy_ssh2_userkey(*key_, *rhs.key_);
		debug_rsa_ = (RSAKey *)key_->data;
		debug_ec_ = (ec_key *)key_->data;
	}
	expiration_date_ = rhs.expiration_date_;
	confirmation_required_ = rhs.confirmation_required_;
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
	if (dest.alg == &ssh_rsa) {
		// ssh-rsa
		RSAKey *rsa = snew(struct RSAKey);
		dest.data = rsa;
		const RSAKey *src_rsa = (RSAKey *)src.data;
		copy_RSAKey(*rsa, *src_rsa);
	} else if (dest.alg == &ssh_ecdsa_nistp256 ||
			   dest.alg == &ssh_ecdsa_nistp384 ||
			   dest.alg == &ssh_ecdsa_nistp521)
	{
		ec_key *eckey = snew(struct ec_key);
		dest.data = eckey;
		const ec_key *src_eckey = (ec_key *)src.data;
		copy_ec(*eckey, *src_eckey);
	} else {
        assert(0);
		exit(1);
	}
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

void ckey::copy_ec(ec_key &dest, const ec_key &src)
{
	memset(&dest, 0, sizeof(ec_key));
	dest.signalg = src.signalg;
	dest.privateKey = copybn(src.privateKey);

	dest.publicKey.curve = src.publicKey.curve;
	dest.publicKey.infinity = src.publicKey.infinity;
	dest.publicKey.x = copybn(src.publicKey.x);
	dest.publicKey.y = copybn(src.publicKey.y);
	if (src.publicKey.z != nullptr) {
		dest.publicKey.z = copybn(src.publicKey.z);
	}
	dest.privateKey = copybn(src.privateKey);
}

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
	oss << "key\n"
		<< " alg " << alg_name() << " " << bits() << "\n"
		<< " fingerprint " << fingerprint() << "\n"
		<< " md5 " << fingerprint_md5() << "\n"
		<< " sha256 " << fingerprint_sha256() << "\n"
		<< " comment " << key_comment() << "\n"
		<< " comment2 " << key_comment2() << "\n";
	if (key_->alg == &ssh_rsa) {
		const RSAKey *rsa = (RSAKey *)key_->data;
		oss << " exponent " << bignum_tostr(rsa->exponent) << "\n"
			<< " modulus " << bignum_tostr(rsa->modulus) << "\n"
			<< " private_exponent " << bignum_tostr(rsa->private_exponent) << "\n"
			<< " p " << bignum_tostr(rsa->p) << "\n"
			<< " q " << bignum_tostr(rsa->q) << "\n"
			<< " iqmp " << bignum_tostr(rsa->iqmp) << "\n";
	} else if (key_->alg == &ssh_ecdsa_nistp256 ||
			   key_->alg == &ssh_ecdsa_nistp384 ||
			   key_->alg == &ssh_ecdsa_nistp521)
	{
		ec_key *eckey = (ec_key *)key_->data;
		oss << " privateKey " << bignum_tostr(eckey->privateKey) << "\n"
			<< " publicKey.x " << bignum_tostr(eckey->publicKey.x) << "\n"
			<< " publicKey.y " << bignum_tostr(eckey->publicKey.y) << "\n"
			<< " publicKey.z " << bignum_tostr(eckey->publicKey.z) << "\n";
	}
	dbgprintf(oss.str().c_str());
}

void ckey::dump_keys(const std::vector<ckey> &keys)
{
	for(const ckey &key : keys) {
		key.dump();
	}
}

bool ckey::operator==(const ckey & rhs) const
{
	return compare(*this, rhs);
}

bool ckey::compare(const ckey &a, const ckey &b)
{
	if (a.key_->alg != b.key_->alg) {
		return false;
	}

	if (a.key_->alg == &ssh_rsa) {
		const RSAKey *a_rsa = (RSAKey *)a.key_->data;
		const RSAKey *b_rsa = (RSAKey *)b.key_->data;

		if (bignum_cmp(a_rsa->exponent, b_rsa->exponent) == 0 &&
			bignum_cmp(a_rsa->modulus, b_rsa->modulus) == 0)
		{
			return true;
		}
#if 0
	} else if (a.key_->alg == &ssh_ecdsa_nistp256 ||
			   a.key_->alg == &ssh_ecdsa_nistp384 ||
			   a.key_->alg == &ssh_ecdsa_nistp521)
	{
		const ec_key *a_ec = (ec_key *)a.key_->data;
		const ec_key *b_ec = (ec_key *)b.key_->data;
		if (bignum_cmp(a_ec->publicKey.x, b_ec->publicKey.x) == 0 &&
			bignum_cmp(a_ec->publicKey.y, b_ec->publicKey.y) == 0)
		{
			return true;
		}
#endif
	} else {
		auto a_pub = a.public_blob();
		auto b_pub = b.public_blob();
		if (a_pub == b_pub) {
			return true;
		}
	}

	return false;
}

void ckey::set_lifetime(uint32 seconds)
{
	expiration_date_ = time(NULL) + seconds;
}

time_t ckey::expiration_time() const
{
	return expiration_date_;
}

void ckey::require_confirmation(bool require)
{
	confirmation_required_ = require;
}

bool ckey::get_confirmation_required() const
{
	return confirmation_required_;
}

std::string ckey::get_pubkey() const
{
	auto _public_blob = public_blob();

	std::string pubkey_str;
	pubkey_str += alg_name() + " ";
	pubkey_str += base64_encode(_public_blob, true);
    pubkey_str += " ";
    pubkey_str += key_comment();
    pubkey_str += "\r\n";

    return pubkey_str;
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
