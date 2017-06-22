#undef UNICODE

#include "winutils.h"
#include "winmisc.h"
#include "winhelp.h"
#include "winhelp_.h"

#include "gui_stuff.h"

#pragma comment(lib,"Comctl32.lib")

static VOID CALLBACK message_box_help_callback(LPHELPINFO lpHelpInfo)
{
	const DWORD_PTR id = lpHelpInfo->dwContextId;
	launch_help_id(NULL, (DWORD)id);
}

int message_box(LPCTSTR text, LPCTSTR caption, DWORD style, DWORD helpctxid)
{
	MSGBOXPARAMSA mbox = {
		sizeof(mbox)
	};
    mbox.hInstance = (HINSTANCE)::GetModuleHandle(NULL);
    mbox.hwndOwner = get_hwnd();
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
    return MessageBoxIndirectA(&mbox);
}

//////////////////////////////////////////////////////////////////////////////

struct tdst {
	int helpid;
};

static HRESULT CALLBACK tdcb(
	HWND     hwnd,
	UINT     uNotification,
	WPARAM   wParam,
	LPARAM   lParam,
	LONG_PTR dwRefData
)
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
	tdc.hwndParent = get_hwnd();
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


// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
