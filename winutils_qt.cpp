
#include "winutils_qt.h"

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

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
