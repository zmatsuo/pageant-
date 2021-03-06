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
    not_cmake/version.cpp \
    misc.cpp \
    misc_cpp.cpp \
    winpgnt.cpp \
    winhelp.cpp \
    winmisc.cpp \
    winutils.c \
    winutils_qt.cpp \
    passphrasedlg.cpp \
    aboutdlg.cpp \
    confirmacceptdlg.cpp \
    sock_server.c \
    ud_socket.c \
    ssh-agent_emu.cpp \
    pageant.cpp \
    sshsh256.c \
    sshecc.c \
    cert/cert_capi.c \
    cert/cert_common.c \
    cert/cert_pkcs.c \
    settingdlg.cpp \
    setting.cpp \
    ssh-agent_ms.cpp \
    cert/pkcs_helper.c \
    winutils_cpp.cpp \
    keyviewdlg.cpp \
    btselectdlg.cpp \
    bt_agent_proxy.cpp \
    bt_agent_proxy_main.cpp \
    debug.cpp \
    ckey.cpp \
    codeconvert.cpp \
    obfuscate.cpp \
    passphrases.cpp \
    keystore.cpp \
    rdpkeydlg.cpp \
    rdp_ssh_relay.cpp \
    sshbn_cpp.cpp \
    keyfile.cpp \
    encode.cpp \
    smartcard.cpp \
    import.c \
    sshbcrypt.c \
    sshblowf.c \
    
HEADERS  += mainwindow.h \
    passphrasedlg.h \
    aboutdlg.h \
    confirmacceptdlg.h \
    cert/cert_capi.h \
    cert/cert_common.h \
    cert/cert_pkcs.h \
    settingdlg.h \
    setting.h \
    keyviewdlg.h \
    btselectdlg.h \
    bt_agent_proxy.h \
    bt_agent_proxy_main.h \
    ckey.h \
    codeconvert.h \
    obfuscate.h \
    passphrases.h \
    keystore.h \
    rdpkeydlg.h \
    rdp_ssh_relay.h \
    smartcard.h \
    sshblowf.h \
    sshbn.h
    
FORMS    += mainwindow.ui \
    passphrasedlg.ui \
    aboutdlg.ui \
    confirmacceptdlg.ui \
    settingdlg.ui \
    keyviewdlg.ui \
    btselectdlg.ui \
    rdpkeydlg.ui

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

RC_FILE = pageant+.rc
    

