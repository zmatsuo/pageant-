
#if defined(QT_STATIC)

#include <QtCore/QtPlugin>
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)

#endif

#pragma comment(linker,"\"/manifestdependency:type='win32'		\
name='Microsoft.Windows.Common-Controls' version='6.0.0.0'		\
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

