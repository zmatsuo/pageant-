
#pragma once

#include <vector>

#include "ssh.h"	// for ssh2_userkey

class ckey {
public:
    ckey();
    ckey(const ckey &rhs);
    ckey(ssh2_userkey *key);		// keyが取り込まれる、keyのfreeは不要
    ckey &operator=(const ckey &rhs);
    ~ckey();
	ssh2_userkey *release();
	ssh2_userkey *get() const;
	void set(ssh2_userkey *key);
    std::string fingerprint() const;
    std::string alg_name() const;
    int bits() const;
    std::string fingerprint_md5() const;
    std::string fingerprint_sha1() const;
    std::string fingerprint_sha256() const;
    std::string fingerprint_md5_comp() const;
    std::string key_comment() const;
    void set_fname(const char *fn);
    std::string key_comment2() const;
	void set_comment(const char *comment);
    bool parse_one_public_key(
		const std::vector<uint8_t> &blob,
		const char **fail_reason);
    bool parse_one_public_key(
		const std::vector<uint8_t> &blob, size_t &pos,
		const char **fail_reason);
    bool parse_one_public_key(
		const void *data, size_t len,
		const char **fail_reason);
    bool parse_one_private_key(
		const void *data, size_t len,
		const char **fail_reason, size_t *key_len);
    void get_raw_key(ssh2_userkey **key) const;
	void dump() const;
	static ckey create(const ssh2_userkey *key);
	static void dump_keys(const std::vector<ckey> &keys);
	std::vector<uint8_t> public_blob_v() const;

private:
    ssh2_userkey *key_;
    RSAKey *debug_rsa_;

	void free();
    unsigned char *public_blob(int *len) const;
    void copy(const ckey &rhs);
    void RSAKey_copy(const RSAKey &rhs);
    static void copy_ssh2_userkey(ssh2_userkey &dest, const ssh2_userkey &src);
    static void copy_RSAKey(RSAKey &dest, const RSAKey &src);
};

bool parse_public_keys(
	const void *data, size_t len,	
	std::vector<ckey> &keys,
	const char **fail_reason);

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
