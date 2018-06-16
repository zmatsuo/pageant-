#undef UNICODE

#include <windows.h>
#include <commctrl.h>

#include "winmisc.h"
#include "winhelp.h"
#include "winhelp_.h"
#include "misc_cpp.h"
#include "codeconvert.h"
#include "winutils_qt.h"
#include "gui_stuff.h"

#if defined(_MSC_VER)
#pragma comment(lib,"Comctl32.lib")
#endif

#if defined(_MSC_VER)
#define HAVE_TaskDialogIndirect
#endif

static HWND GetParentHWND(QWidget *w)
{
	HWND hWnd;
	if (w == nullptr) {
		hWnd = GetDesktopWindow();
	} else {
		hWnd = reinterpret_cast<HWND>(w->winId());
	}
	return hWnd;
}

static HWND GetParentHWND()
{
	return GetParentHWND(getDispalyedWindow());
}

static VOID CALLBACK message_box_help_callback(LPHELPINFO lpHelpInfo)
{
	const DWORD_PTR id = lpHelpInfo->dwContextId;
	launch_help_id(NULL, (DWORD)id);
}

/**
 *	@retvalue
 *		IDCANCEL
 *		IDNO
 *		IDOK
 *		IDRETRY
 *		IDYES
 */
int message_box(
	HWND hWndOwner, const wchar_t *text, const wchar_t *caption, DWORD style, DWORD helpctxid)
{
	MSGBOXPARAMSW mbox = {
		sizeof(mbox)
	};
    mbox.hInstance = (HINSTANCE)::GetModuleHandle(NULL);
	mbox.hwndOwner = hWndOwner;
    mbox.dwLanguageId = LANG_NEUTRAL;
    mbox.lpszText = text;
    mbox.lpszCaption = caption;
    mbox.dwContextHelpId = helpctxid;
    mbox.dwStyle = style;
    if (helpctxid != WINHELP_CTXID_no_help &&	// WINHELP_CTXID_no_help == 0
		has_help())
	{
		mbox.dwStyle |= MB_HELP;
		mbox.lpfnMsgBoxCallback = &message_box_help_callback;
	}
    return ::MessageBoxIndirectW(&mbox);
}

int message_boxW(HWND hWndOwner, const wchar_t *text, const wchar_t *caption, DWORD style, DWORD helpctxid)
{
	return message_box(hWndOwner, text, caption, style, helpctxid);
}

int message_boxA(HWND hWndOwner, const char *text, const char *caption, DWORD style, DWORD helpctxid)
{
	std::wstring wtext = acp_to_wc(text);
	std::wstring wcaption = acp_to_wc(caption);
	int r = message_box(hWndOwner, wtext.c_str(), wcaption.c_str(), style, helpctxid);
	return r;
}

int message_boxA(const char *text, const char *caption, DWORD style, DWORD helpctxid)
{
	HWND hWnd = GetParentHWND();
	int r = message_boxA(hWnd, text, caption, style, helpctxid);
	return r;
}

//////////////////////////////////////////////////////////////////////////////

#if defined(HAVE_TaskDialogIndirect)

struct tdst {
	int helpid;
};

static HRESULT CALLBACK tdcb(
	HWND     hwnd,
	UINT     uNotification,
	WPARAM   wParam,
	LPARAM   lParam,
	LONG_PTR dwRefData)
{
	(void)hwnd;
	(void)wParam;
	struct tdst *tddata = (struct tdst *)dwRefData;
	switch (uNotification) {
	case TDN_HYPERLINK_CLICKED:
		exec((wchar_t *)lParam);
		break;
	case TDN_HELP:
	{
		const DWORD_PTR id = tddata->helpid;
		launch_help_id(NULL, (DWORD)id);
		break;
	}
	default:
		break;;
	}
	return S_OK;
}

/**
 *	@retvalue
 *		IDCANCEL
 *		IDNO
 *		IDOK
 *		IDRETRY
 *		IDYES
 */
int task_dialog(
	const wchar_t *WindowTitle,
	const wchar_t *MainInstruction,
	const wchar_t *Content,
	const wchar_t *ExpandedInformation,
	const wchar_t *footer,
	const wchar_t *icon,
	int CommandButtons,
	int defaultButton,
	int helpid)
{
	struct tdst tddata;
	if (helpid != 0) {
		tddata.helpid = helpid;
	}

	int pushedButton;
	TASKDIALOGCONFIG tdc = {
		sizeof(tdc)
	};
	tdc.hwndParent = GetParentHWND();
	tdc.hInstance = (HINSTANCE)::GetModuleHandle(NULL);
	tdc.dwFlags = TDF_ENABLE_HYPERLINKS;
	tdc.dwCommonButtons = CommandButtons;
	tdc.pszWindowTitle = WindowTitle;
	tdc.pszMainIcon = icon;
	tdc.pszMainInstruction = MainInstruction;
	tdc.pszContent = Content;
	tdc.nDefaultButton = defaultButton;
	if (ExpandedInformation != NULL) {
		tdc.pszExpandedInformation = ExpandedInformation;
		tdc.pszExpandedControlText = L"詳細非表示";
		tdc.pszCollapsedControlText = L"詳細表示";
	}
	tdc.pszFooter = footer;
	tdc.pfCallback = &tdcb;
	tdc.lpCallbackData = (LONG_PTR)&tddata;
	HRESULT r = TaskDialogIndirect(&tdc, &pushedButton, NULL, NULL);
	if (r != S_OK) {
		return IDCANCEL;		// same as cancel
	}
	return pushedButton;
}

#else	// HAVE_TaskDialogIndirect

int task_dialog(
	const wchar_t *WindowTitle,
	const wchar_t *MainInstruction,
	const wchar_t *Content,
	const wchar_t *ExpandedInformation,
	const wchar_t *footer,
	const wchar_t *icon,
	int CommandButtons,
	int defaultButton,
	int helpid)
{
	(void)ExpandedInformation;
	(void)footer;
	(void)defaultButton;
	(void)helpid;

#if 0
	int pushedButton;
	HRESULT r = TaskDialog(
        get_hwnd(),
		(HINSTANCE)::GetModuleHandle(NULL),
		WindowTitle,
		MainInstruction,
		Content,
		CommandButtons,
		icon,
		&pushedButton);
	if (r != S_OK) {
		return IDCANCEL;		// same as cancel
	}
	return pushedButton;
#endif
#if 1
	std::wstring text;
	text = MainInstruction;
    text += L"\n";
    text += Content;
	int r = message_box(
		wc_to_mb(text).c_str(),
        wc_to_mb(std::wstring(WindowTitle)).c_str(),
		MB_OK, helpid);
    return r;
#endif
}

#endif	// HAVE_TaskDialogIndirect

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
