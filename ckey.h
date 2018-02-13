
#pragma once

#include "ssh.h"	// for ssh2_userkey

#if defined(__cplusplus)

class ckey {
public:
    ckey();
    ckey(const ckey &rhs);
    ckey(ssh2_userkey *key);
    ckey &operator=(const ckey &rhs);
    ~ckey();
    std::string fingerprint() const;
    std::string alg_name() const;
    int bits() const;
    std::string fingerprint_md5() const;
    std::string fingerprint_sha1() const;
    std::string fingerprint_sha256() const;
    std::string key_comment() const;
    void set_fname(const char *fn);
    std::string key_comment2() const;
    bool parse_one_public_key(const void *data, size_t len,
							  const char **fail_reason);
    void get_raw_key(ssh2_userkey **key) const;
private:
    ssh2_userkey *key_;
    RSAKey *debug_rsa_;

    unsigned char *public_blob(int *len) const;
    void copy(const ckey &rhs);
    void RSAKey_copy(const RSAKey &rhs);
    static void copy_ssh2_userkey(ssh2_userkey &dest, const ssh2_userkey &src);
    static void copy_RSAKey(RSAKey &dest, const RSAKey &src);
};

#include <vector>
class KeyListItem {
public:
	int no;
	std::string algorithm;
	int size;		// key size(bit)
	std::string name;
	std::string md5;
	std::string sha256;
	std::string comment;
	std::string comment2;
};

std::vector<KeyListItem> keylist_update2();

#endif

#if defined(__cplusplus)
extern "C"
{
#endif

//[[deprecated("use ckey")]]
__declspec(deprecated("use ckey"))
char *ssh2_fingerprint_sha256(const struct ssh2_userkey *key);

bool parse_public_keys(
	const void *data, size_t len,	
	std::vector<ckey> &keys,
	const char **fail_reason);

#if defined(__cplusplus)
}
#endif

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
