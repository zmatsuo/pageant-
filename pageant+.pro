#-------------------------------------------------
#
# Project created by QtCreator 2017-03-25T13:29:38
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = pageant+
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += _WIN32_WINNT=_WIN32_WINNT_WIN7
msvc:DEFINES += PUTTY_CAC

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += cert
mingw:QMAKE_LFLAGS += -static-libgcc

SOURCES += main.cpp\
    mainwindow.cpp \
    sshaes.c \
    sshbn.c \
    sshdes.c \
    sshdss.c \
    sshmd5.c \
    sshpubk.c \
    sshrsa.c \
    sshsh512.c \
    sshsha.c \
    tree234.c \
    version.c \
    misc.c \
    misc_cpp.cpp \
    winpgnt.cpp \
    winpgntc.c \
    winhelp.cpp \
    winmisc.cpp \
    winutils.c \
    winutils_qt.cpp \
    passphrasedlg.cpp \
    aboutdlg.cpp \
    confirmacceptdlg.cpp \
    db.cpp \
    libunixdomain/sock_server.c \
    libunixdomain/ud_socket.c \
    ssh-agent_emu.cpp \
    pageant.c \
    pageant_cpp.cpp \
    sshsh256.c \
    sshecc.c \
    cert/cert_capi.c \
    cert/cert_common.c \
    cert/cert_pkcs.c \
    aqsync.c \
    winucs.c \
    miscucs.c \
    settingdlg.cpp \
    setting.cpp \
    ssh-agent_ms.cpp \
    cert/pkcs_helper.c

HEADERS  += mainwindow.h \
    passphrasedlg.h \
    aboutdlg.h \
    confirmacceptdlg.h \
    cert/cert_capi.h \
    cert/cert_common.h \
    cert/cert_pkcs.h \
    settingdlg.h \
    setting.h \

FORMS    += mainwindow.ui \
    passphrasedlg.ui \
    aboutdlg.ui \
    confirmacceptdlg.ui \
    settingdlg.ui \

RESOURCES += \
    pageant+.qrc

LIBS += -lgdi32
LIBS += -lUser32
LIBS += -lAdvapi32
LIBS += -lComdlg32
LIBS += -lShell32
LIBS += -lOle32
mingw:LIBS += -lws2_32
mingw:LIBS += -L$$PWD/htmlhelp -lhtmlhelp
mingw:LIBS += -lshlwapi
mingw:LIBS += -luuid
mingw:LIBS += -lWtsapi32
mingw:LIBS += -lComctl32

