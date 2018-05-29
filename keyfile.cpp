/**
 *   keyfile.cpp
 *
 *   Copyright (c) 2018 zmatsuo
 *
 *   This software is released under the MIT License.
 *   http://opensource.org/licenses/mit-license.php
 */

#include "keyfile.h"

#include <assert.h>

#include "filename.h"
#include "codeconvert.h"
#include "keystore.h"
#include "passphrases.h"
#include "gui_stuff.h"
#include "setting.h"
#define ENABLE_DEBUG_PRINT
#include "debug.h"

static std::vector<uint8_t> ssh2_userkey_loadpub(const Filename *filename)
{
    const char *error = NULL;
	int bloblen;
	void *blob = ssh2_userkey_loadpub(
		filename, NULL, &bloblen,
		NULL, &error);
	if (!blob) {
		std::vector<uint8_t> file_blob;
		return file_blob;
	}
	std::vector<uint8_t> file_blob((uint8_t*)blob, ((uint8_t*)blob)+bloblen);
	sfree(blob);
	return file_blob;
}

class PassphraseEnumrator {
public:
	PassphraseEnumrator() {
		passphrases_ = passphrase_get_array();
		dialog_opend_ = false;
	}
	~PassphraseEnumrator() {
		passphrases_.clear();
	}
	sstring Get(const char *comment) {
		if (passphrases_.size() > 0) {
			auto passphrase = passphrases_.back();
			passphrases_.back().clear();
			passphrases_.pop_back();
			return passphrase;
		}
		auto passphrase = PassphraseDlg(comment);
		return passphrase;
	}

	void Save(const char *passphrase) {
		if (dialog_opend_ && save_flag_) {
			passphrase_add(passphrase);
			passphrase_save_setting(passphrase);
		}
	}

private:
	sstring PassphraseDlg(const char *comment) {
		dialog_opend_ = true;
		char *_passphrase;
		struct PassphraseDlgInfo pps;
		pps.passphrase = &_passphrase;
		pps.caption = "pageant+";
		pps.text = comment;
		pps.save = 0;
		pps.saveAvailable = setting_get_bool("Passphrase/save_enable", false);
		DIALOG_RESULT_T r = ShowPassphraseDlg(&pps);
		if (r == DIALOG_RESULT_CANCEL) {
			// cancell
			sstring passphrase;
			return passphrase;
		}
		sstring passphrase = _passphrase;
		smemclr(_passphrase, strlen(_passphrase));
		sfree(_passphrase);
		save_flag_ = pps.save;
		return passphrase;
	}

	std::vector<sstring> passphrases_;
	bool dialog_opend_;
	bool save_flag_;
};

static bool load_putty_key(const Filename *filename, ckey &key)
{
	std::vector<uint8_t> file_blob = ssh2_userkey_loadpub(filename);
	if (file_blob.empty()) {
		// ロードできない
		key.clear();
		return false;
	}

	ckey keystore_key;
	if (keystore_get_from_blob(file_blob, keystore_key)) {
		// すでにキーストアに存在する
		// パスフレーズの入力なしに存在チェックを行える
		key.clear();
		return false;
	}

    int needs_pass = ssh2_userkey_encrypted(filename, NULL);
	if (!needs_pass) {
		const char *error = NULL;
		struct ssh2_userkey *skey = NULL;
		skey = ssh2_load_userkey(filename, NULL, &error);
		if (skey == SSH2_WRONG_PASSPHRASE) {
			return false;
		}
		key.set(skey);
		return true;
	}

	{
		PassphraseEnumrator passphraseEnumrator;
		std::string comment = wc_to_mb(std::wstring(filename->path));
		while(1) {
			auto passphrase = passphraseEnumrator.Get(comment.c_str());
			const char *error = NULL;
			struct ssh2_userkey *skey = NULL;
			skey = ssh2_load_userkey(filename, passphrase.c_str(), &error);
			if (skey != SSH2_WRONG_PASSPHRASE) {
				passphraseEnumrator.Save(passphrase.c_str());
				key.set(skey);
				return true;
			}
		}
	}
}

static bool load_importable_key(const Filename *filename, int type, ckey &key)
{
	char *comment_;
	int result = import_encrypted(filename, type, &comment_);
	std::string comment = comment_;
	sfree(comment_);
	dbgprintf("import_encrypted() comment '%s'\n", comment.c_str());
	if (result ==  0) {
		// load without passphrase
		char const *errmsg;
		struct ssh2_userkey *skey =
			import_ssh2(filename, type, NULL, &errmsg);
		key.set(skey);
		return true;
	} else {
		PassphraseEnumrator passphraseEnumrator;
		char const *errmsg;
		while(1) {
			auto passphrase = passphraseEnumrator.Get(comment.c_str());
			if (passphrase.empty()) {
				return false;
			}
			struct ssh2_userkey *skey =
				import_ssh2(filename, type, (char *)passphrase.c_str(), &errmsg);
			if (skey != &ssh2_wrong_passphrase) {
				passphraseEnumrator.Save(passphrase.c_str());
				key.set(skey);
				return true;
			} else {
				dbgprintf("import_ssh2() error '%s'\n", errmsg);
				//sfree(errmsg);
			}
		}
		return false;
	}
}

bool load_keyfile(const wchar_t *_filename, ckey &key)
{
	Filename *filename = filename_from_wstr(_filename);
	bool loaded = false;
    const int type = key_type(filename);
	if (type == SSH_KEYTYPE_SSH2) {
		loaded = load_putty_key(filename, key);
	} else if (import_possible(type) != 0) {
		loaded = load_importable_key(filename, type, key);
		if (loaded) {
			if (keystore_exist(key)) {
				// すでにキーストアに存在する
				key.clear();
				loaded = false;
			}
		}
	}
	filename_free(filename);
	return loaded;
}

// Local Variables:
// coding: utf-8-with-signature
// tab-width: 4
// End:
