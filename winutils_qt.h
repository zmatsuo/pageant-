
#pragma once

#pragma warning(push)
#pragma warning(disable:4127)
#pragma warning(disable:4251)
#include <qwidget>
#pragma warning(pop)

void showAndBringFront(QWidget *win);
void PopDisplayedWindow();
void PushDisplayedWindow(QWidget *widget);
QWidget *getDispalyedWindow();
int message_box(QWidget *widget, const wchar_t *text, const wchar_t *caption, uint32_t style, uint32_t helpctxid = 0);
