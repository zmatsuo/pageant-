/*
 * winhelp.c: centralised functions to launch Windows help files,
 * and to decide whether to use .HLP or .CHM help in any given
 * situation.
 */
#undef UNICODE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <windows.h>
#include <htmlhelp.h>
#include <string>
#include <vector>

#include "pageant+.h"
#include "winhelp_.h"
#include "puttymem.h"
#include "misc.h"
#include "winmisc.h"
#include "misc_cpp.h"
#include "gui_stuff.h"

#define PUTTY_HELP_FILE		L"putty.hlp"
#define PUTTY_CHM_FILE		L"putty.chm"
#define PUTTY_HELP_CONTENTS L"putty.cnt"

#if defined(_MSC_VER)
#pragma comment(lib,"Htmlhelp.lib")
#endif

static bool requested_help;
static std::string help_path;		// TODO wstring
static int help_has_contents;
static std::string chm_path;

#if 0
void init_help(void)
{
	std::wstring f;
	f = get_full_path(PUTTY_HELP_FILE);
	if (!f.empty()) help_path = wc_to_mb(f);
    f = get_full_path(PUTTY_CHM_FILE);
	if (!f.empty()) chm_path = wc_to_mb(f);
}
#endif

void shutdown_help(void)
{
    /* Nothing to do currently.
     * (If we were running HTML Help single-threaded, this is where we'd
     * call HH_UNINITIALIZE.) */
}

int has_help(void)
{
    /*
     * FIXME: it would be nice here to disregard help_path on
     * platforms that didn't have WINHLP32. But that's probably
     * unrealistic, since even Vista will have it if the user
     * specifically downloads it.
     */
    return (!help_path.empty() || !chm_path.empty());
}

void launch_help(HWND hwnd, const char *topic)
{
    if (topic) {
		size_t colonpos = strcspn(topic, ":");

		if (!chm_path.empty()) {
			char *fname;
			assert(topic[colonpos] != '\0');
			fname = dupprintf("%s::/%s.html>main", chm_path.c_str(),
							  topic + colonpos + 1);
			HtmlHelpA(hwnd, fname, HH_DISPLAY_TOPIC, 0);
			sfree(fname);
		} else if (!help_path.empty()) {
			char *cmd = dupprintf("JI(`',`%.*s')", colonpos, topic);
			WinHelp(hwnd, help_path.c_str(), HELP_COMMAND, (ULONG_PTR)cmd);
			sfree(cmd);
		} else {
			message_boxA("ヘルプがおかしい?", APP_NAME, MB_OK, 0);
		}
    } else {
		if (!chm_path.empty()) {
			HtmlHelpA(hwnd, chm_path.c_str(), HH_DISPLAY_TOPIC, 0);
		} else if (!help_path.empty()) {
			WinHelp(hwnd, help_path.c_str(),
					help_has_contents ? HELP_FINDER : HELP_CONTENTS, 0);
		} else {
			message_boxA("ヘルプがおかしい?", APP_NAME, MB_OK, 0);
		}
    }
    requested_help = true;
}

static const struct ctxListSt {
	DWORD dwContextId;
	const char *ContextId;
	const wchar_t *url;
} ctxList[] = {
#define HELP_CTX_TABLE(name)	\
	{ (WINHELP_CTXID_ ## name),	\
	  (WINHELP_CTX_ ## name), \
	  L"https://www.ssh.com/ssh/putty/putty-manuals/0.68/Chapter9.html"}
    HELP_CTX_TABLE(errors_hostkey_absent),
    HELP_CTX_TABLE(errors_hostkey_changed),
    HELP_CTX_TABLE(errors_cantloadkey),
    HELP_CTX_TABLE(option_cleanup),
    HELP_CTX_TABLE(pgp_fingerprints),
#undef HELP_CTX_TABLE
};

static const struct ctxListSt *help_get_ctx_from_ctxid(DWORD_PTR dwContextId)
{
	if (dwContextId == WINHELP_CTXID_no_help) {
		return NULL;
	}
	for(int i=0; i<_countof(ctxList); i++) {
		const struct ctxListSt *p = &ctxList[i];
		if (p->dwContextId == dwContextId) {
			return p;
		}
	}
	return NULL;
}

void launch_help_id(HWND hwnd, DWORD id)
{
	if (!has_help()) {
		message_boxA("ヘルプがありません", APP_NAME, MB_OK, 0);
		return;
	}

	const struct ctxListSt *p = help_get_ctx_from_ctxid(id);
	if (help_path.empty() || chm_path.empty()) {
		const char *topic = p == NULL ? NULL : p->ContextId;
		launch_help(hwnd, topic);
	} else {
		const wchar_t *url =
//			p == NULL ? L"https://www.ssh.com/ssh/putty/putty-manuals/0.68/Chapter9.html" :
			p == NULL ? L"http://d.hatena.ne.jp/nattou_curry_2/20090626/1246022561" :
			p->url;
		exec(url, NULL);
	}
}

void quit_help(HWND hwnd)
{
    if (requested_help) {
		if (!chm_path.empty()) {
			HtmlHelpW(NULL, NULL, HH_CLOSE_ALL, 0);
		} else
			if (!help_path.empty()) {
				WinHelp(hwnd, help_path.c_str(), HELP_QUIT, 0);
			}
		requested_help = false;
    }
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
