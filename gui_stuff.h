
#pragma once

#include <windows.h>		// for MB_OK, IDOK, etc  :(

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	DIALOG_RESULT_OK,
	DIALOG_RESULT_CANCEL
} DIALOG_RESULT_T;

struct ConfirmAcceptDlgInfo {
	const char *title;
    const char *fingerprint;
    int dont_ask_again;			// 問い合わせする?初期値 0/1 = そのまま/今後問い合わせ不要
	int dont_ask_again_available;
    int timeout;				// 秒,0でタイムアウトなし
};

DIALOG_RESULT_T confirmAcceptDlg(struct ConfirmAcceptDlgInfo *info);

struct PassphraseDlgInfo {
    char **passphrase;			// 入力されたパスフレーズへのポインタ 利用したらfree()すること
	const char *caption;
    const char *text;
    int save;
	int saveAvailable;
};

DIALOG_RESULT_T passphraseDlg(struct PassphraseDlgInfo *info);

int message_boxA(const char *text, const char *caption, DWORD style, DWORD helpctxid);
int message_boxW(const wchar_t *text, const wchar_t *caption, DWORD style, DWORD helpctxid);

int task_dialog(
	const wchar_t *WindowTitle,
	const wchar_t *MainInstruction,
	const wchar_t *Content,
	const wchar_t *ExpandedInformation,
	const wchar_t *footer,
	const wchar_t *icon,
	int CommandButtons,
	int defaultButton,
	int helpid);

char *pin_dlg(const wchar_t *text, const wchar_t *caption, HWND hWnd, BOOL *pSavePassword);

void addFileCert();
void addCAPICert();
void addPKCSCert();
void addBtCert();

#ifdef __cplusplus
}
#endif

#if defined(__cplusplus)
extern "C++" {
int message_box(const char *text, const char *caption, DWORD style, DWORD helpctxid = 0);
int message_box(const wchar_t *text, const wchar_t *caption, DWORD style, DWORD helpctxid = 0);
}
#endif

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
