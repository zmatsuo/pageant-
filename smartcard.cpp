/**
 *   smartcard.cpp
 *
 *   Copyright (c) 2018 zmatsuo
 *
 *   This software is released under the MIT License.
 *   http://opensource.org/licenses/mit-license.php
 */

#include "smartcard.h"

extern "C" {
#include "cert/cert_common.h"
}
#include "gui_stuff.h"

void SmartcardInit()
{
    cert_set_pin_dlg(pin_dlg);
}

bool SmartcardIsPath(const char *utf8_path)
{
    int r = cert_is_certpath(utf8_path);
	return r == 0 ? false : true;
}

bool SmartcardLoad(const char *utf8_path, ckey &key)
{
	if (!SmartcardIsPath(utf8_path)) {
		return false;
	}
	struct ssh2_userkey *skey = cert_load_key(utf8_path);
	if (skey == NULL) {
		return false;
	}
	key.set(skey);
	return true;
}

static std::string SmartcardSelect(HWND hWndOwner, LPCSTR szIden)
{
	std::string path;
	char * szCert = cert_prompt(szIden, hWndOwner);
	if (szCert == NULL)
		return path;
	path = szCert;
	sfree(szCert);
	return path;
}

std::string SmartcardSelectCAPI(HWND hWndOwner)
{
	return SmartcardSelect(hWndOwner, IDEN_CAPI);
}

std::string SmartcardSelectPKCS(HWND hWndOwner)
{
	return SmartcardSelect(hWndOwner, IDEN_PKCS);
}

void SmartcardForgetPin()
{
	cert_forget_pin();
}

void SmartcardUnloadPKCS11dll()
{
	cert_pkcs11dll_finalize();
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
