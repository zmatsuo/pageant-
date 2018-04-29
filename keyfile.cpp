
#include "keyfile.h"

#include <assert.h>

#include "filename.h"
#include "codeconvert.h"
#include "ckey.h"
#include "keystore.h"
#include "passphrases.h"
#include "gui_stuff.h"
#include "setting.h"

/*
 * pageant_add_keyfile() is used to load a private key from a file and
 * add it to the agent. Initially, you should call it with passphrase
 * NULL, and it will check if the key is already in the agent, and
 * whether a passphrase is required. Return values are given in the
 * enum below. On return, *retstr will either be NULL, or a
 * dynamically allocated string containing a key comment or an error
 * message.
 *
 * pageant_add_keyfile() also remembers passphrases with which it's
 * successfully decrypted keys (because if you try to add multiple
 * keys in one go, you might very well have used the same passphrase
 * for keys that have the same trust properties). Call
 * passphrase_forget() to get rid of them all.
 */
enum {
    PAGEANT_ACTION_OK,       /* success; no further action needed */
    PAGEANT_ACTION_FAILURE,  /* failure; *retstr is error message */
    PAGEANT_ACTION_NEED_PP   /* need passphrase: *retstr is key comment */
};
/**
 *	@retval		PAGEANT_ACTION_OK
 *	@retval		PAGEANT_ACTION_FAILURE
 *	@retval		PAGEANT_ACTION_NEED_PP
 */
static int pageant_add_keyfile(
	const Filename *filename,
	const char *passphrase,
	char **retstr)
{
    int needs_pass;
    int ret;
    int attempts;
    const char *error = NULL;

    *retstr = NULL;

    const int type = key_type(filename);
	if (type != SSH_KEYTYPE_SSH2) {
		*retstr = dupprintf("Couldn't load this key (%s)",
                            key_type_to_str(type));
		return PAGEANT_ACTION_FAILURE;
    }

    /*
     * See if the key is already loaded (in the primary Pageant,
     * which may or may not be us).
     */
    {
		int bloblen;
		void *blob = ssh2_userkey_loadpub(
			filename, NULL, &bloblen,
			NULL, &error);
		if (!blob) {
			*retstr = dupprintf("Couldn't load private key (%s)", error);
			return PAGEANT_ACTION_FAILURE;
		}
		std::vector<uint8_t> file_blob((uint8_t*)blob, ((uint8_t*)blob)+bloblen);
		sfree(blob);

		// すでにロードしているか?
		auto keys = keystore_get_keys();
		for (const auto &key : keys) {
			auto key_blob = key.public_blob_v();
			if (key_blob.size() == file_blob.size() &&
				memcmp(&key_blob[0], &file_blob[0], key_blob.size()) == 0)
			{
				return PAGEANT_ACTION_OK;
			}
		}
    }

    error = NULL;
    char *comment = NULL;
	needs_pass = ssh2_userkey_encrypted(filename, &comment);
    attempts = 0;

	if (needs_pass && passphrase == NULL) {
		return PAGEANT_ACTION_NEED_PP;
	}
	
    /*
     * Loop round repeatedly trying to load the key, until we either
     * succeed, fail for some serious reason, or run out of
     * passphrases to try.
     */
    struct ssh2_userkey *skey = NULL;
	skey = ssh2_load_userkey(filename, passphrase, &error);
	if (skey == SSH2_WRONG_PASSPHRASE)
		ret = -1;
	else if (!skey)
		ret = 0;
	else
		ret = 1;

	if (ret == 1) {
		/*
		 * Successfully loaded the key file.
		 */
		;
	} else {
		/*
		 * Failed to load the key file, for some reason other than
		 * a bad passphrase.
		 */
		*retstr = dupstr(error);
		return PAGEANT_ACTION_FAILURE;
    }

    /*
     * If we get here, we've successfully loaded the key into
     * rkey/skey, but not yet added it to the agent.
     */

    if (comment)
		sfree(comment);

	ckey key(skey);
	keystore_add(key);
    return PAGEANT_ACTION_OK;
}

bool add_keyfile(const Filename *fn)
{
	std::string comment;
	bool result = false;
	/*
     * Try loading the key without a passphrase. (Or rather, without a
     * _new_ passphrase; pageant_add_keyfile will take care of trying
     * all the passphrases we've already stored.)
     */
    char *err;
    int ret = pageant_add_keyfile(fn, NULL, &err);
    if (ret == PAGEANT_ACTION_OK) {
		result = true;
        goto done;
    } else if (ret == PAGEANT_ACTION_FAILURE) {
		goto error;
    } else if (ret == PAGEANT_ACTION_NEED_PP) {
		{
			auto passphrases = passphrase_get_array();
			for(auto &&passphrase : passphrases) {
				ret = pageant_add_keyfile(fn, passphrase.c_str(), &err);
				passphrase.clear();
				if (ret == PAGEANT_ACTION_OK) {
					passphrases.clear();
					result = true;
					goto done;
				}
			}
			passphrases.clear();
		}

		/*
		 * OK, a passphrase is needed, and we've been given the key
		 * comment to use in the passphrase prompt.
		 */
		comment += wc_to_mb(std::wstring(fn->path));
		while (1) {
			char *_passphrase;
			struct PassphraseDlgInfo pps;
			pps.passphrase = &_passphrase;
			pps.caption = "pageant+";
			pps.text = comment.c_str();
			pps.save = 0;
			pps.saveAvailable = setting_get_bool("Passphrase/save_enable", false);
			DIALOG_RESULT_T r = ShowPassphraseDlg(&pps);
			if (r == DIALOG_RESULT_CANCEL) {
				// cancell	
				goto done;
			}

			sfree(err);
			err = NULL;

			assert(_passphrase != NULL);

			ret = pageant_add_keyfile(fn, _passphrase, &err);
			if (ret == PAGEANT_ACTION_OK) {
				if (pps.save != 0) {
					passphrase_add(_passphrase);
					passphrase_save_setting(_passphrase);
				}
				result = true;
				goto done;
			} else if (ret == PAGEANT_ACTION_FAILURE) {
				goto error;
			}

			smemclr(_passphrase, strlen(_passphrase));
			sfree(_passphrase);
			_passphrase = NULL;
		}
	}

error:
#if 0
	QWidget *w = getDispalyedWindow();
    message_box(w, utf8_to_wc(err).c_str(), L"" APPNAME, MB_OK | MB_ICONERROR, 0);
#endif
	result = false;
done:
    sfree(err);
	return result;
}

void add_keyfile(const wchar_t *filename)
{
	Filename *fn = filename_from_wstr(filename);
	add_keyfile(fn);
	filename_free(fn);
}

// Local Variables:
// coding: utf-8-with-signature
// tab-width: 4
// End:
