
#include "winutils_qt.h"

#include "gui_stuff.h"
#include <stack>
#include <windows.h>

void showAndBringFront(QWidget *win)
{
#if 1
	Qt::WindowFlags eFlags = win->windowFlags();
	eFlags |= Qt::WindowStaysOnTopHint;
	win->setWindowFlags(eFlags);
	win->show();
	eFlags &= ~Qt::WindowStaysOnTopHint;
	win->setWindowFlags(eFlags);
#endif
#if 0
	win->show();
	win->activateWindow();
	win->raise();
#endif
}

static std::stack<QWidget *> displayed_window;

#if 0
void PushDisplayedWindow(HWND hWnd)
{
	displayed_window.push(hWnd);
}
#endif

void PushDisplayedWindow(QWidget *widget)
{
	displayed_window.push(widget);
}

void PopDisplayedWindow()
{
	displayed_window.pop();
}

QWidget *getDispalyedWindow()
{
	if (displayed_window.size() == 0) {
		return nullptr;
	}
	return displayed_window.top();
}

static HWND GetHWND(QWidget *w)
{
	HWND hWnd;
	if (w == nullptr) {
		hWnd = GetDesktopWindow();
	} else {
		hWnd = reinterpret_cast<HWND>(w->winId());
	}
	return hWnd;
}

int message_box(QWidget *widget, const wchar_t *text, const wchar_t *caption, uint32_t style, uint32_t helpctxid)
{
	return message_box(GetHWND(widget), text, caption, style, helpctxid);
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
