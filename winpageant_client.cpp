/*
 * Pageant client code.
 */

#include "winpageant_client.h"


#undef UNICODE
#undef _UNICODE
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <windows.h>
#include <aclapi.h>
#include <string>
#include <sstream>
#include <iomanip>

#include "puttymem.h"
#include "misc.h"

//#include "pageant.h" /* for AGENT_MAX_MSGLEN */
#define AGENT_MAX_MSGLEN (8*1024)

typedef struct agent_pending_query agent_pending_query;

#define AGENT_COPYDATA_ID 0x804e50ba   /* random goop */

static PSID get_user_sid(void)
{
	/* Initialised once, then kept around to reuse forever */
	static PSID usersid;

    HANDLE proc = NULL, tok = NULL;
    TOKEN_USER *user = NULL;
    DWORD toklen, sidlen;
    PSID sid = NULL, ret = NULL;

    if (usersid)
        return usersid;

    if ((proc = OpenProcess(MAXIMUM_ALLOWED, FALSE,
                            GetCurrentProcessId())) == NULL)
        goto cleanup;

    if (!OpenProcessToken(proc, TOKEN_QUERY, &tok))
        goto cleanup;

    if (!GetTokenInformation(tok, TokenUser, NULL, 0, &toklen) &&
        GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        goto cleanup;

    if ((user = (TOKEN_USER *)LocalAlloc(LPTR, toklen)) == NULL)
        goto cleanup;

    if (!GetTokenInformation(tok, TokenUser, user, toklen, &toklen))
        goto cleanup;

    sidlen = GetLengthSid(user->User.Sid);

    sid = (PSID)smalloc(sidlen);

    if (!CopySid(sidlen, sid, user->User.Sid))
        goto cleanup;

    /* Success. Move sid into the return value slot, and null it out
     * to stop the cleanup code freeing it. */
    ret = usersid = sid;
    sid = NULL;

cleanup:
    if (proc != NULL)
        CloseHandle(proc);
    if (tok != NULL)
        CloseHandle(tok);
    if (user != NULL)
        LocalFree(user);
    if (sid != NULL)
        sfree(sid);

    return ret;
}

void *agent_query(
    void *in, int inlen, void **out, int *outlen)
{
    unsigned char *p;
    PSECURITY_DESCRIPTOR psd = NULL;

    *out = NULL;
    *outlen = 0;

    HWND hwnd = FindWindowA("Pageant", "Pageant");
    if (!hwnd)
		return NULL;		       /* *out == NULL, so failure */

	std::string mapname;
	{
		std::ostringstream ss;
		ss << "PageantRequest"
		   << std::hex << std::setw(8) << std::setfill('0')
		   << (unsigned)GetCurrentThreadId();
		mapname = ss.str();
	}

    SECURITY_ATTRIBUTES *psa = NULL;
	SECURITY_ATTRIBUTES sa;
	{
		/*
		 * Make the file mapping we create for communication with
		 * Pageant owned by the user SID rather than the default. This
		 * should make communication between processes with slightly
		 * different contexts more reliable: in particular, command
		 * prompts launched as administrator should still be able to
		 * run PSFTPs which refer back to the owning user's
		 * unprivileged Pageant.
		 */
		PSID usersid = get_user_sid();

		if (usersid) {
			psd = (PSECURITY_DESCRIPTOR)LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
			if (psd) {
				if (InitializeSecurityDescriptor(psd, SECURITY_DESCRIPTOR_REVISION) &&
					SetSecurityDescriptorOwner(psd, usersid, FALSE))
				{
					sa.nLength = sizeof(sa);
					sa.bInheritHandle = TRUE;
					sa.lpSecurityDescriptor = psd;
					psa = &sa;
				} else {
					LocalFree(psd);
					psd = NULL;
				}
			}
		}
	}

    HANDLE filemap =
		CreateFileMappingA(INVALID_HANDLE_VALUE, psa, PAGE_READWRITE,
						   0, AGENT_MAX_MSGLEN, mapname.c_str());
	if (filemap == NULL || filemap == INVALID_HANDLE_VALUE) {
		return NULL;		       /* *out == NULL, so failure */
	}
	p = (unsigned char *)MapViewOfFile(filemap, FILE_MAP_WRITE, 0, 0, 0);
	memcpy(p, in, inlen);
    COPYDATASTRUCT cds;
	cds.dwData = AGENT_COPYDATA_ID;
	cds.cbData = DWORD(1 + mapname.size());
	cds.lpData = const_cast<void *>(reinterpret_cast<const void *>(mapname.c_str()));

	/*
	 * The user either passed a null callback (indicating that the
	 * query is required to be synchronous) or CreateThread failed.
	 * Either way, we need a synchronous request.
	 */
	LRESULT id = SendMessage(hwnd, WM_COPYDATA, (WPARAM) NULL, (LPARAM) &cds);
	if (id > 0) {
		int retlen = 4 + GET_32BIT(p);
		unsigned char *ret = (unsigned char *)snewn(retlen, unsigned char);
		if (ret) {
			memcpy(ret, p, retlen);
			*out = ret;
			*outlen = retlen;
		}
	}
	UnmapViewOfFile(p);
	CloseHandle(filemap);
	if (psd)
		LocalFree(psd);
	return NULL;
}

bool agent_exists(void)
{
    HWND hwnd = ::FindWindow("Pageant", "Pageant");
    if (!hwnd)
		return true;
    else
		return false;
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
