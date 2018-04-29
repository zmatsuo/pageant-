/**
   passphrases.cpp

   Copyright (c) 2017 zmatsuo

   This software is released under the MIT License.
   http://opensource.org/licenses/mit-license.php
*/

#include <algorithm>

#include "puttymem.h"
#include "setting.h"
#include "codeconvert.h"
#include "obfuscate.h"	// for decrypto(),encrypto()

#include "passphrases.h"

static std::vector<sstring> passphrases;

#if 0
void passphrase_init()
{
}
#endif

void passphrase_exit()
{
	passphrases.clear();
}

void passphrase_add(const sstring &passphrase)
{
	// TODO 重複検索
	sstring &&pp_enc = encrypto(passphrase);
	passphrases.push_back(pp_enc);
}

void passphrase_add(const char *passphrase)
{
	passphrase_add(sstring(passphrase));
}

void passphrase_forget()
{
	passphrases.clear();
}

static sstring passphrase_get(int n)
{
	if (n >= passphrases.size()) {
		return sstring();
	}
	sstring pp_dec = decrypto(passphrases[n]);
	return pp_dec;
}

std::vector<sstring> passphrase_get_array()
{
	std::vector<sstring> ary;
	int n = 0;
	for(;;) {
		sstring &&pp = passphrase_get(n);
		if (pp.empty())
			break;
		ary.emplace_back(pp);
		n++;
	}
	return ary;
}

void passphrase_load_setting()
{
	std::vector<sstring> passphraseAry;

	int empty_count = 0;
    int i = 1;
	while (1) {
		std::string key = "Passphrases/crypto" + std::to_string(i++);
		std::wstring ws_c_pass = setting_get_str(key.c_str(), nullptr);
		if (!ws_c_pass.empty()) {
			passphraseAry.emplace_back(wc_to_mb(ws_c_pass));
		} else {
			empty_count++;
			if (empty_count == 5) {
				break;
			}
		}
	}

	// puttyの設定から読み出す
	if (!get_putty_path().empty()) {
		empty_count = 0;
		i = 1;
		while (1) {
			std::string key = "Passphrases/crypto" + std::to_string(i++);
			std::wstring ws_c_pass;
			ws_c_pass = setting_get_str_putty(key.c_str(), nullptr);
			if (!ws_c_pass.empty()) {
				passphraseAry.emplace_back(wc_to_mb(ws_c_pass));
			} else {
				empty_count++;
				if (empty_count == 5) {
					break;
				}
			}
		}
	}

	// 難読化された重複パスフレーズを削除する
	std::sort(passphraseAry.begin(), passphraseAry.end());
	passphraseAry.erase(
		std::unique(passphraseAry.begin(), passphraseAry.end()),
		passphraseAry.end());

	for (const auto &c_passphrase : passphraseAry) {
		passphrases.emplace_back(c_passphrase);
	}
}

void passphrase_save_setting(const char* passphrase)
{
	if (passphrase == NULL) {
		return;
	}
	std::wstring ws_encrypted = acp_to_wc(encrypto(sstring(passphrase)));
	
	int i = 1;
	while (1) {
		std::string key = "Passphrases/crypto" + std::to_string(i++);
		std::wstring ws_c_pass = setting_get_str(key.c_str(), nullptr);
		if (ws_c_pass.empty()) {
			// 見つからず、最後に書き込み
			bool r = setting_set_str(key.c_str(), ws_encrypted.c_str());
			break;
		}
		if (ws_encrypted == ws_c_pass) {
			// すでにあった、終了
			break;
		}
	}
	smemclr(&ws_encrypted[0], ws_encrypted.length()*sizeof(wchar_t));
}

void passphrase_remove_setting()
{
	setting_set_str("Passphrases", nullptr);
}

// Local Variables:
// coding: utf-8-with-signature
// tab-width: 4
// End:
