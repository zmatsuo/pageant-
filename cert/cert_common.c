#undef UNICODE
#ifdef PUTTY_CAC

#define UMDF_USING_NTSTATUS
#include <ntstatus.h>

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <wincrypt.h>
#include <cryptdlg.h>
#pragma warning(push)
#pragma warning(disable: 4201)
#include <cryptuiapi.h>
#pragma warning(pop)
#include <wincred.h>

#include "cert_pkcs.h"
#include "cert_capi.h"

#define DEFINE_VARIABLES
#include "cert_common.h"
#undef DEFINE_VARIABLES

#ifndef SSH_AGENT_SUCCESS
#include "ssh.h"
#endif

#include "../puttymem.h"

#if defined(_DEBUG)
#define malloc(size)		_malloc_dbg(size,_NORMAL_BLOCK,__FILE__,__LINE__) 
#define calloc(num, size)   _calloc_dbg(num, size, _NORMAL_BLOCK, __FILE__, __LINE__)
#define free(ptr)			_free_dbg(ptr, _NORMAL_BLOCK);
#endif

#define strlwr(p)	_strlwr(p)
#define wcsdup(p)	_wcsdup(p)
#define strdup(p)	_strdup(p)

static char *cert_pin_dlg_default(const wchar_t *text, const wchar_t *caption, HWND hWnd, BOOL *pSavePassword);
static pCertPinDlgFnT pCertPinDlgFn = &cert_pin_dlg_default;

static char *wc_to_mb_simple(const wchar_t *wstr)
{
    const UINT cp = CP_ACP;
    int len = WideCharToMultiByte(cp, 0, wstr, -1, NULL, 0, NULL, NULL);
	len++;
	char *buf = malloc(len);
    WideCharToMultiByte(cp, 0, wstr, -1, buf, len, NULL,NULL);
    return buf;
}

static wchar_t *mb_to_wc_simple(const char *str)
{
    const UINT cp = CP_UTF8;
    int len = MultiByteToWideChar(cp, 0, str, -1, NULL, 0);
	len++;
	wchar_t *buf = malloc(sizeof(wchar_t)*len);
    MultiByteToWideChar(cp, 0, str, -1, buf, len);
    return buf;
}

void cert_reverse_array(LPBYTE pb, DWORD cb)
{
	for (DWORD i = 0, j = cb - 1; i < cb / 2; i++, j--)
	{
		BYTE b = pb[i];
		pb[i] = pb[j];
		pb[j] = b;
	}
}

LPSTR cert_get_cert_hash(LPCSTR szIden, PCCERT_CONTEXT pCertContext, LPCSTR szHint)
{
	BYTE pbThumbBinary[SHA1_BINARY_SIZE];
	DWORD cbThumbBinary = SHA1_BINARY_SIZE;
	if (CertGetCertificateContextProperty(pCertContext, CERT_HASH_PROP_ID, pbThumbBinary, &cbThumbBinary) == FALSE)
	{
		return NULL;
	}

	LPSTR szThumbHex[SHA1_HEX_SIZE + 1];
	DWORD iThumbHexSize = _countof(szThumbHex);
	CryptBinaryToStringA(pbThumbBinary, cbThumbBinary,
		CRYPT_STRING_HEXRAW | CRYPT_STRING_NOCRLF, (LPSTR)szThumbHex, &iThumbHexSize);
	return dupcat(szIden, szThumbHex, (szHint != NULL) ? "=" : "", (szHint != NULL) ? szHint : "", NULL);
}

void cert_prompt_cert(HCERTSTORE hStore, HWND hWnd, LPSTR * szCert, LPCSTR szIden, LPCSTR szHint)
{
	HMODULE hCertDialogLibrary = LoadLibrary("cryptdlg.dll");
	if (hCertDialogLibrary == NULL) return;

	// import the address to the certificate selections dialog
	typedef BOOL(WINAPI *PCertSelectCertificateW)(
		__inout  PCERT_SELECT_STRUCT_W pCertSelectInfo);
	PCertSelectCertificateW CertSelectCertificate = (PCertSelectCertificateW)
		GetProcAddress(hCertDialogLibrary, "CertSelectCertificateW");

	// setup the structure to control which certificates 
	// we want the user to be able to select from
	PCERT_CONTEXT * ppCertContext = calloc(1, sizeof(CERT_CONTEXT*));
	CERT_SELECT_STRUCT_W tCertSelect;
	ZeroMemory(&tCertSelect, sizeof(tCertSelect));
	tCertSelect.dwSize = sizeof(CERT_SELECT_STRUCT_W);
	tCertSelect.hwndParent = hWnd;
	tCertSelect.szTitle = L"PuTTY: Select Certificate for Authentication";
	tCertSelect.cCertStore = 1;
	tCertSelect.arrayCertStore = &hStore;
	tCertSelect.szPurposeOid = szOID_PKIX_KP_CLIENT_AUTH;
	tCertSelect.cCertContext = 1;
	tCertSelect.arrayCertContext = ppCertContext;

	// display the certificate selection dialog
	if (CertSelectCertificate(&tCertSelect) == TRUE && tCertSelect.cCertContext == 1)
	{
		BYTE pbThumbBinary[SHA1_BINARY_SIZE];
		DWORD cbThumbBinary = SHA1_BINARY_SIZE;
		if (CertGetCertificateContextProperty(*ppCertContext, CERT_HASH_PROP_ID, pbThumbBinary, &cbThumbBinary) == TRUE)
		{
			*szCert = cert_get_cert_hash(szIden, *ppCertContext, szHint);
		}

		// cleanup
		CertFreeCertificateContext(*ppCertContext);
	}

	// cleanup 
	free(ppCertContext);
	FreeLibrary(hCertDialogLibrary);
}

LPSTR cert_prompt(LPCSTR szIden, HWND hWnd)
{
	HCERTSTORE hStore = NULL;
	LPCSTR szHint = NULL;

	if (cert_is_capipath(szIden))
	{
		hStore = cert_capi_get_cert_store(&szHint, hWnd);
	}

	if (cert_is_pkcspath(szIden))
	{
		hStore = cert_pkcs_get_cert_store(&szHint, hWnd);
	}

	if (hStore != NULL)
	{
		LPSTR szCert = NULL;
		cert_prompt_cert(hStore, hWnd, &szCert, szIden, szHint);
		return szCert;
	}

	return NULL;
}

LPBYTE cert_sign(const struct ssh2_userkey * userkey, LPCBYTE pDataToSign, int iDataToSignLen, int * iWrappedSigLen, HWND hWnd,int rsaFlag)
{
	LPBYTE pRawSig = NULL;
	int iRawSigLen = 0;
	*iWrappedSigLen = 0;

	// sanity check
	if (userkey->comment == NULL) return NULL;

	if (cert_is_capipath(userkey->comment))
	{
		pRawSig = cert_capi_sign(userkey, pDataToSign, iDataToSignLen, &iRawSigLen, hWnd);
	}

	if (cert_is_pkcspath(userkey->comment))
	{
		pRawSig = cert_pkcs_sign(userkey, pDataToSign, iDataToSignLen, &iRawSigLen, hWnd, rsaFlag);
	}

	// sanity check
	if (pRawSig == NULL) return NULL;

	// used to hold wrapped signature to return to server
	LPBYTE pWrappedSig = NULL;

	if (strstr(userkey->alg->name, "ecdsa-") == userkey->alg->name)
	{
		// the full ecdsa ssh blob is as follows:
		//
		// size of algorithm name (4 bytes in big endian)
		// algorithm name
		// size of padded 'r' and 's' values from windows blob (4 bytes in big endian)
		// size of padded 'r' value from signed structure (4 bytes in big endian)
		// 1 byte of 0 padding in order to ensure the 'r' value is represented as positive
		// the 'r' value (first half of the blob signature returned from windows)
		// 1 byte of 0 padding in order to ensure the 's' value is represented as positive
		// the 's' value (first half of the blob signature returned from windows)
		const BYTE iZero = 0;
		int iAlgName = strlen(userkey->alg->name);
		*iWrappedSigLen = 4 + iAlgName + 4 + 4 + 1 + (iRawSigLen / 2) + 4 + 1 + (iRawSigLen / 2);
		pWrappedSig = snewn(*iWrappedSigLen, unsigned char);
		unsigned char * pWrappedPos = pWrappedSig;
		PUT_32BIT(pWrappedPos, iAlgName); pWrappedPos += 4;
		memcpy(pWrappedPos, userkey->alg->name, iAlgName); pWrappedPos += iAlgName;
		PUT_32BIT(pWrappedPos, iRawSigLen + 4 + 4 + 1 + 1); pWrappedPos += 4;
		PUT_32BIT(pWrappedPos, 1 + iRawSigLen / 2); pWrappedPos += 4;
		memcpy(pWrappedPos, &iZero, 1); pWrappedPos += 1;
		memcpy(pWrappedPos, pRawSig, iRawSigLen / 2); pWrappedPos += iRawSigLen / 2;
		PUT_32BIT(pWrappedPos, 1 + iRawSigLen / 2); pWrappedPos += 4;
		memcpy(pWrappedPos, &iZero, 1); pWrappedPos += 1;
		memcpy(pWrappedPos, pRawSig + iRawSigLen / 2, iRawSigLen / 2); pWrappedPos += iRawSigLen / 2;
	}
	else
	{
		// the full rsa ssh blob is as follows:
		//
		// size of algorithm name (4 bytes in big endian)
		// algorithm name
		// size of binary signature (4 bytes in big endian)
		// binary signature
		const char *algName;
		if (rsaFlag == SSH_AGENT_RSA_SHA2_256) {
			algName = "rsa-sha2-256";
		} else if (rsaFlag == SSH_AGENT_RSA_SHA2_512) {
			algName = "rsa-sha2-512";
		} else {
			algName = userkey->alg->name;
		}
		int iAlgoNameLen = strlen(algName);
		*iWrappedSigLen = 4 + iAlgoNameLen + 4 + iRawSigLen;
		pWrappedSig = snewn(*iWrappedSigLen, unsigned char);
		unsigned char * pWrappedPos = pWrappedSig;
		PUT_32BIT(pWrappedPos, iAlgoNameLen); pWrappedPos += 4;
		memcpy(pWrappedPos, algName, iAlgoNameLen); pWrappedPos += iAlgoNameLen;
		PUT_32BIT(pWrappedPos, iRawSigLen); pWrappedPos += 4;
		memcpy(pWrappedPos, pRawSig, iRawSigLen);
	}

	// cleanup
	sfree(pRawSig);
	return pWrappedSig;
}

struct ssh2_userkey * cert_get_ssh_userkey(LPCSTR szCert, PCERT_CONTEXT pCertContext)
{
	struct ssh2_userkey * pUserKey = NULL;

	// get a convenience pointer to the algorithm identifier 
	LPCSTR sAlgoId = pCertContext->pCertInfo->SubjectPublicKeyInfo.Algorithm.pszObjId;

	// Handle RSA Keys
	if (strstr(sAlgoId, _CRT_CONCATENATE(szOID_RSA, ".")) == sAlgoId)
	{
		// get the size of the space required
		PCRYPT_BIT_BLOB pKeyData = _ADDRESSOF(pCertContext->pCertInfo->SubjectPublicKeyInfo.PublicKey);

		DWORD cbPublicKeyBlob = 0;
		LPBYTE pbPublicKeyBlob = NULL;
		if (CryptDecodeObject(X509_ASN_ENCODING, RSA_CSP_PUBLICKEYBLOB, pKeyData->pbData,
			pKeyData->cbData, 0, NULL, &cbPublicKeyBlob) != FALSE && cbPublicKeyBlob != 0 &&
			CryptDecodeObject(X509_ASN_ENCODING, RSA_CSP_PUBLICKEYBLOB, pKeyData->pbData,
				pKeyData->cbData, 0, pbPublicKeyBlob = malloc(cbPublicKeyBlob), &cbPublicKeyBlob) != FALSE)
		{
			// create a new putty rsa structure fill out all non-private params
			RSAPUBKEY * pPublicKey = (RSAPUBKEY *)(pbPublicKeyBlob + sizeof(BLOBHEADER));
			struct RSAKey * rsa = snew(struct RSAKey);
			rsa->bits = pPublicKey->bitlen;
			rsa->bytes = pPublicKey->bitlen / 8;
			rsa->exponent = bignum_from_long(pPublicKey->pubexp);
			cert_reverse_array((BYTE *)(pPublicKey)+sizeof(RSAPUBKEY), rsa->bytes);
			rsa->modulus = bignum_from_bytes((BYTE *)(pPublicKey)+sizeof(RSAPUBKEY), rsa->bytes);
			rsa->comment = dupstr(szCert);
			rsa->private_exponent = bignum_from_long(0);
			rsa->p = bignum_from_long(0);
			rsa->q = bignum_from_long(0);
			rsa->iqmp = bignum_from_long(0);

			// fill out the user key
			pUserKey = snew(struct ssh2_userkey);
			pUserKey->alg = find_pubkey_alg("ssh-rsa");
			pUserKey->data = rsa;
			pUserKey->comment = dupstr(szCert);
		}

		if (pbPublicKeyBlob != NULL) free(pbPublicKeyBlob);
	}

	// Handle ECC Keys
	else if (strstr(sAlgoId, szOID_ECC_PUBLIC_KEY) == sAlgoId)
	{
		BCRYPT_KEY_HANDLE hBCryptKey = NULL;
		if (CryptImportPublicKeyInfoEx2(X509_ASN_ENCODING, _ADDRESSOF(pCertContext->pCertInfo->SubjectPublicKeyInfo),
			0, NULL, &hBCryptKey) != FALSE)
		{
			DWORD iKeyLength = 0;
			ULONG iKeyLengthSize = sizeof(DWORD);
			LPBYTE pEccKey = NULL;
			ULONG iKeyBlobSize = 0;
			if (BCryptGetProperty(hBCryptKey, BCRYPT_KEY_LENGTH, (PUCHAR)&iKeyLength, iKeyLengthSize, &iKeyLength, 0) == STATUS_SUCCESS &&
				BCryptExportKey(hBCryptKey, NULL, BCRYPT_ECCPUBLIC_BLOB, NULL, iKeyBlobSize, &iKeyBlobSize, 0) == STATUS_SUCCESS && iKeyBlobSize != 0 &&
				BCryptExportKey(hBCryptKey, NULL, BCRYPT_ECCPUBLIC_BLOB, pEccKey = malloc(iKeyBlobSize), iKeyBlobSize, &iKeyBlobSize, 0) == STATUS_SUCCESS)
			{
				struct ec_key *ec = snew(struct ec_key);
				ZeroMemory(ec, sizeof(struct ec_key));
				ec_nist_alg_and_curve_by_bits(iKeyLength, &(ec->publicKey.curve), &(ec->signalg));
				ec->publicKey.infinity = 0;
				int iKeyBytes = (iKeyLength + 7) / 8; // round up
				ec->publicKey.x = bignum_from_bytes(pEccKey + sizeof(BCRYPT_ECCKEY_BLOB), iKeyBytes);
				ec->publicKey.y = bignum_from_bytes(pEccKey + sizeof(BCRYPT_ECCKEY_BLOB) + iKeyBytes, iKeyBytes);
				ec->privateKey = bignum_from_long(0);

				// fill out the user key
				pUserKey = snew(struct ssh2_userkey);
				pUserKey->alg = ec->signalg;
				pUserKey->data = ec;
				pUserKey->comment = dupstr(szCert);
			}

			// cleanup
			if (pEccKey != NULL) free(pEccKey);
		}

		// cleanup
		BCryptDestroyKey(hBCryptKey);
	}

	return pUserKey;
}

struct ssh2_userkey * cert_load_key(LPCSTR szCert)
{
	// sanity check
	if (szCert == NULL) return NULL;

	PCERT_CONTEXT pCertContext = NULL;
	HCERTSTORE hCertStore = NULL;

	if (cert_is_capipath(szCert))
	{
		cert_capi_load_cert(szCert, &pCertContext, &hCertStore);
	}

	if (cert_is_pkcspath(szCert))
	{
		cert_pkcs_load_cert(szCert, &pCertContext, &hCertStore);
	}

	// ensure a valid cert was found
	if (pCertContext == NULL) return NULL;

	// get the public key data
	struct ssh2_userkey * pUserKey = cert_get_ssh_userkey(szCert, pCertContext);
	CertFreeCertificateContext(pCertContext);
	CertCloseStore(hCertStore, 0);
	return pUserKey;
}

LPSTR cert_key_string(LPCSTR szCert)
{
	// sanity check
	if (szCert == NULL || !cert_is_certpath(szCert))
	{
		return NULL;
	}

	PCERT_CONTEXT pCertContext = NULL;
	HCERTSTORE hCertStore = NULL;

	// if capi, get the capi cert
	if (cert_is_capipath(szCert))
	{
		cert_capi_load_cert(szCert, &pCertContext, &hCertStore);
	}

	// if pkcs, get the pkcs cert
	if (cert_is_pkcspath(szCert))
	{
		cert_pkcs_load_cert(szCert, &pCertContext, &hCertStore);
	}

	// sanity check
	if (pCertContext == NULL) return NULL;

	// get the open ssh ekys trings
	struct ssh2_userkey * pUserKey = cert_get_ssh_userkey(szCert, pCertContext);
	char * szKey = ssh2_pubkey_openssh_str(pUserKey);

	// clean and return
	free(pUserKey->comment);
	pUserKey->alg->freekey(pUserKey->data);
	sfree(pUserKey);
	CertFreeCertificateContext(pCertContext);
	CertCloseStore(hCertStore, 0);
	return szKey;
}

VOID cert_display_cert(LPCSTR szCert, HWND hWnd)
{
	PCERT_CONTEXT pCertContext = NULL;
	HCERTSTORE hCertStore = NULL;

	// if capi, get the capi cert
	if (cert_is_capipath(szCert))
	{
		cert_capi_load_cert(szCert, &pCertContext, &hCertStore);
	}

	// if pkcs, get the pkcs cert
	if (cert_is_pkcspath(szCert))
	{
		cert_pkcs_load_cert(szCert, &pCertContext, &hCertStore);
	}

	// sanity check
	if (pCertContext == NULL) return;

	// display cert ui
	CryptUIDlgViewContext(CERT_STORE_CERTIFICATE_CONTEXT,
		pCertContext, hWnd, L"PuTTY Certificate Display", 0, NULL);

	// cleanup
	CertFreeCertificateContext(pCertContext);
	CertCloseStore(hCertStore, 0);
}

int cert_all_certs(LPSTR ** pszCert)
{
	// get a hangle to the cert store
	LPCSTR szHint = NULL;
	HCERTSTORE hCertStore = cert_capi_get_cert_store(&szHint, NULL);

	// enumerate all certs
	CTL_USAGE tItem;
	CHAR * sUsage[] = { szOID_PKIX_KP_CLIENT_AUTH };
	tItem.cUsageIdentifier = 1;
	tItem.rgpszUsageIdentifier = sUsage;
	PCCERT_CONTEXT pCertContext = NULL;

	// first count the number of certs for allocation
	int iCertNum = 0;
	while ((pCertContext = CertFindCertificateInStore(hCertStore, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
		CERT_FIND_VALID_ENHKEY_USAGE_FLAG, CERT_FIND_ENHKEY_USAGE, &tItem, pCertContext)) != NULL) iCertNum++;

	// allocate memory and populate it
	*pszCert = snewn(iCertNum, LPSTR);
	LPSTR * pszCertPos = *pszCert;
	while ((pCertContext = CertFindCertificateInStore(hCertStore, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
		CERT_FIND_VALID_ENHKEY_USAGE_FLAG, CERT_FIND_ENHKEY_USAGE, &tItem, pCertContext)) != NULL)
	{
		*(pszCertPos++) = cert_get_cert_hash(IDEN_CAPI, pCertContext, NULL);
	}

	// cleanup and return
	CertCloseStore(hCertStore, 0);
	return iCertNum;
}

void cert_convert_legacy(LPSTR szCert)
{
	// advance string pass 'CAPI:' if already present
	LPSTR sCompare = szCert;
	if (strstr(szCert, "CAPI:") == szCert)
	{
		sCompare = &szCert[IDEN_CAPI_SIZE];
	}

	// search for 'User\MY\' and replace with 'CAPI:'
	LPSTR szIdenLegacyUsr = "User\\MY\\";
	if (strstr(sCompare, szIdenLegacyUsr) == sCompare)
	{
		strcpy(szCert, IDEN_CAPI);
		strcpy(&szCert[IDEN_CAPI_SIZE], &sCompare[strlen(szIdenLegacyUsr)]);
		strlwr(&szCert[IDEN_CAPI_SIZE]);
	}

	// search for 'System\MY\' and replace with 'CAPI:'
	LPSTR szIdenLegacySys = "Machine\\MY\\";
	if (strstr(sCompare, szIdenLegacySys) == sCompare)
	{
		strcpy(szCert, IDEN_CAPI);
		strcpy(&szCert[IDEN_CAPI_SIZE], &sCompare[strlen(szIdenLegacySys)]);
		strlwr(&szCert[IDEN_CAPI_SIZE]);
	}
}

LPBYTE cert_get_hash(LPCSTR szAlgo, LPCBYTE pDataToHash, DWORD iDataToHashSize, DWORD * iHashedDataSize, BOOL bPrependDigest)
{
	// id-sha1 OBJECT IDENTIFIER 
	static const BYTE OID_SHA1[] = {
		0x30, 0x21, /* type Sequence, length 0x21 (33) */
		0x30, 0x09, /* type Sequence, length 0x09 */
		0x06, 0x05, /* type OID, length 0x05 */
		0x2b, 0x0e, 0x03, 0x02, 0x1a, /* id-sha1 OID */
		0x05, 0x00, /* NULL */
		0x04, 0x14  /* Octet string, length 0x14 (20), followed by sha1 hash */
	};
    static const BYTE OID_SHA2_256[] = {
		0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60,
		0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01,
		0x05, 0x00, 0x04, 0x20,
    };
    static const BYTE OID_SHA2_512[] = {
		0x30, 0x51, 0x30, 0x0d, 0x06, 0x09, 0x60,
		0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03,
		0x05, 0x00, 0x04, 0x40,
    };

	HCRYPTPROV hHashProv = (ULONG_PTR)NULL;
	HCRYPTHASH hHash = (ULONG_PTR)NULL;
	LPBYTE pHashData = NULL;
	*iHashedDataSize = 0;

	// determine algo to use for hashing
	ALG_ID iHashAlg = CALG_SHA1;	// "ssh-rsa"
	if (strcmp(szAlgo, "ecdsa-sha2-nistp256") == 0) iHashAlg = CALG_SHA_256;
	if (strcmp(szAlgo, "ecdsa-sha2-nistp384") == 0) iHashAlg = CALG_SHA_384;
	if (strcmp(szAlgo, "ecdsa-sha2-nistp521") == 0) iHashAlg = CALG_SHA_512;
	if (strcmp(szAlgo, "rsa-sha2-256") == 0) iHashAlg = CALG_SHA_256;
	if (strcmp(szAlgo, "rsa-sha2-512") == 0) iHashAlg = CALG_SHA_512;

	// for sha1, prepend the hash digest if requested
	// this is necessary for some signature algorithms
	DWORD iDigestSize = 0;
	LPBYTE pDigest = NULL;
	if (bPrependDigest)
	{
		if (iHashAlg == CALG_SHA1) {
			iDigestSize = sizeof(OID_SHA1);
			pDigest = (LPBYTE)OID_SHA1;
		} else if (iHashAlg == CALG_SHA_256) {
			iDigestSize = sizeof(OID_SHA2_256);
			pDigest = (LPBYTE)OID_SHA2_256;
		} else if (iHashAlg == CALG_SHA_512) {
			iDigestSize = sizeof(OID_SHA2_512);
			pDigest = (LPBYTE)OID_SHA2_512;
		}
	}

	// acquire crytpo provider, hash data, and export hashed binary data
	if (CryptAcquireContext(&hHashProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT) == FALSE ||
		CryptCreateHash(hHashProv, iHashAlg, 0, 0, &hHash) == FALSE ||
		CryptHashData(hHash, pDataToHash, iDataToHashSize, 0) == FALSE ||
		CryptGetHashParam(hHash, HP_HASHVAL, NULL, iHashedDataSize, 0) == FALSE ||
		CryptGetHashParam(hHash, HP_HASHVAL, (pHashData = snewn(*iHashedDataSize +
			iDigestSize, BYTE)) + iDigestSize, iHashedDataSize, 0) == FALSE)
	{
		// something failed
		if (pHashData != NULL)
		{
			sfree(pHashData);
			pHashData = NULL;
		}
	}

	// prepend the digest
	*iHashedDataSize += iDigestSize;
	memcpy(pHashData, pDigest, iDigestSize);

	// cleanup and return
	if (hHash != (ULONG_PTR)NULL) CryptDestroyHash(hHash);
	if (hHashProv != (ULONG_PTR)NULL) CryptReleaseContext(hHashProv, 0);
	return pHashData;
}

// prompt the user to enter the pin
static char *cert_pin_dlg_default(const wchar_t *text, const wchar_t *caption, HWND hWnd, BOOL *pSavePassword)
{
	// prompt the user to enter the pin
	CREDUI_INFOW tCredInfo;
	ZeroMemory(&tCredInfo, sizeof(CREDUI_INFO));
	tCredInfo.hwndParent = hWnd;
	tCredInfo.cbSize = sizeof(tCredInfo);
	tCredInfo.pszMessageText = text;
	tCredInfo.pszCaptionText = caption;
	WCHAR szUserName[CREDUI_MAX_USERNAME_LENGTH + 1] = L"<Using Smart Card>";
	WCHAR szPassword[CREDUI_MAX_PASSWORD_LENGTH + 1] = L"";
	DWORD dwFlags =
		CREDUI_FLAGS_GENERIC_CREDENTIALS |
		CREDUI_FLAGS_KEEP_USERNAME |				// Do not show "save password" when set this flag
		0;
	if (pSavePassword != NULL) {
		dwFlags |= CREDUI_FLAGS_SHOW_SAVE_CHECK_BOX;
	}
	DWORD r = CredUIPromptForCredentialsW(
		&tCredInfo, L"target", NULL, 0,
		szUserName, _countof(szUserName),
		szPassword, _countof(szPassword),
		pSavePassword,
		dwFlags);
	if (r != ERROR_SUCCESS)
	{
		if (pSavePassword != NULL)
		{
			*pSavePassword = FALSE;
		}
		return NULL;
	}

	if (pSavePassword != NULL)
	{
		UINT type = MB_YESNO;
		if (*pSavePassword == FALSE)
		{
			type |= MB_DEFBUTTON2;
		}
		r = MessageBoxW(hWnd, L"Save Pin?", caption, type);
		if (r == IDYES)
		{
			*pSavePassword = TRUE;
		}
	}

	char *szPasswordA = wc_to_mb_simple(szPassword);
	SecureZeroMemory(szPassword, sizeof(szPassword));
	return szPasswordA;
}

void cert_set_pin_dlg(pCertPinDlgFnT fn)
{
	pCertPinDlgFn = fn;
}

typedef struct CACHE_ITEM
{
	struct CACHE_ITEM * NextItem;
	LPSTR szCert;
	VOID * szPin;
	DWORD iLength;
//	BOOL bUnicode;
	DWORD iSize;
} CACHE_ITEM;

static CACHE_ITEM * PinCacheList = NULL;

void cert_pin_save_cache(LPSTR szCert, LPVOID szPin, DWORD iPinSize)
{
	// determine length of storage (round up to block size)
	DWORD iLength = iPinSize;
	DWORD iCryptLength = CRYPTPROTECTMEMORY_BLOCK_SIZE *
		((iLength / CRYPTPROTECTMEMORY_BLOCK_SIZE) + 1);
	VOID * pEncrypted = memcpy(malloc(iCryptLength), szPin, iLength);

	// encrypt memory
	CryptProtectMemory(pEncrypted, iCryptLength,
					   CRYPTPROTECTMEMORY_SAME_PROCESS);

	// allocate new item in cache and commit the change
	CACHE_ITEM * hItem = (CACHE_ITEM *)calloc(1, sizeof(struct CACHE_ITEM));
	hItem->szCert = strdup(szCert);
	hItem->szPin = pEncrypted;
	hItem->iLength = iCryptLength;
//	hItem->bUnicode = bUnicode;
	hItem->NextItem = PinCacheList;
	PinCacheList = hItem;
}

VOID *cert_pin_load_cache(LPSTR szCert)
{
	// attempt to locate the item in the pin cache
	for (CACHE_ITEM * hCurItem = PinCacheList; hCurItem != NULL; hCurItem = hCurItem->NextItem)
	{
		if (strcmp(hCurItem->szCert, szCert) == 0)
		{
			VOID * pEncrypted = memcpy(malloc(hCurItem->iLength), hCurItem->szPin, hCurItem->iLength);
			CryptUnprotectMemory(pEncrypted, hCurItem->iLength, CRYPTPROTECTMEMORY_SAME_PROCESS);
			return pEncrypted;
		}
	}
	return NULL;
}

PVOID cert_pin(LPSTR szCert, BOOL bUnicode, LPVOID szPin, HWND hWnd)
{
	VOID *cache = cert_pin_load_cache(szCert);
	if (cache != NULL)
	{
		return cache;
	}

	// request to add item to pin cache
	if (szPin != NULL)
	{
		DWORD iLength = ((bUnicode) ? sizeof(WCHAR) : sizeof(CHAR)) *
			(1 + ((bUnicode) ? wcslen(szPin) : strlen(szPin)));
		cert_pin_save_cache(szCert, szPin, iLength);
		return NULL;
	}

	char *szPassword = pCertPinDlgFn(
		L"Please Enter Your Smart Card Credentials",
		L"PuTTY Authentication",
		hWnd, NULL);
	PVOID szReturn = NULL;
	if (bUnicode)
	{
		szReturn = mb_to_wc_simple(szPassword);
		SecureZeroMemory(szPassword, strlen(szPassword));
		free(szPassword);
	}
	else
	{
		szReturn = szPassword;
	}

	return szReturn;
}

EXTERN BOOL cert_cache_enabled(DWORD bEnable)
{
	static BOOL bCacheEnabled = FALSE;
	if (bEnable != -1) bCacheEnabled = bEnable;
	return bCacheEnabled;
}

void cert_forget_pin(void)
{
	if (PinCacheList == NULL) {
		return;
	}
	CACHE_ITEM *p = PinCacheList;
	while(1)
	{
		CACHE_ITEM *next = p->NextItem;
		free(p->szCert);
		free(p->szPin);
		free(p);
		if (next == NULL) {
			break;
		}
		p = next;
	}
	PinCacheList = NULL;
}

#endif // PUTTY_CAC
