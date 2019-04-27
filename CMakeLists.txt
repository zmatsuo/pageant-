
project(pageant+)

cmake_minimum_required(VERSION 2.8.10.2)

option(USE_STATIC_QTLIB "Only one exe file" ON)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release")
cmake_policy(SET CMP0020 NEW)
cmake_policy(SET CMP0043 NEW)

set(VERSION_MAJOR "0")
set(VERSION_MINOR "9")
set(VERSION_PATCH "2")

find_package(Git)
if(GIT_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
    OUTPUT_VARIABLE GIT_HASH
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  message(STATUS "GIT hash: ${GIT_HASH}")
else()
  message(STATUS "GIT not found")
  set(GIT_HASH "unknown")
endif()

if(${MSVC_VERSION} GREATER_EQUAL 1910)
  # Visual Studio 2017以上の時
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP")
  if(USE_STATIC_QTLIB)
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")
  endif()
endif()

if(USE_STATIC_QTLIB)
#  set(Qt5_DIR "${CMAKE_SOURCE_DIR}/libs/qt-5.10.1-static-ltcg-msvc2017-x86_64")
  find_package(Qt5 COMPONENTS Core Gui Widgets REQUIRED
#	HINTS "${CMAKE_SOURCE_DIR}/libs/qt-5.10.1-static-ltcg-msvc2017-x86_64"
	HINTS "${CMAKE_SOURCE_DIR}/libs/qt_static_5.12.3"
	)

  string(REPLACE "/MD" "/MT" CMAKE_C_FLAGS_DEBUG ${CMAKE_C_FLAGS_DEBUG})
  string(REPLACE "/MD" "/MT" CMAKE_C_FLAGS_RELEASE ${CMAKE_C_FLAGS_RELEASE})
  string(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
  string(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE})
  add_definitions(-DQT_STATICPLUGIN)
  add_definitions(-DQT_STATIC)
  set( QT_STATIC TRUE )
else()
  #https://blog.kitware.com/cmake-finding-qt5-the-right-way/
  find_package(Qt5 COMPONENTS Core Gui Widgets REQUIRED
	HINTS "C:/Qt/5.12.3/msvc2017_64"
	)
endif()

list(APPEND CMAKE_PREFIX_PATH ${Qt5_DIR})

message(STATUS "Qt5")
message(STATUS " version ${Qt5Widgets_VERSION_STRING}")
message(STATUS " Qt5_DIR ${Qt5_DIR}")
message(STATUS " Qt5Core_DIR ${Qt5Core_DIR}")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

if(NOT USE_STATIC_QTLIB)
  # https://blog.quickmediasolutions.com/2017/11/24/using-windeployqt-with-cpack.html
  include("${CMAKE_SOURCE_DIR}/Windeployqt.cmake")
endif()

#include(InstallRequiredSystemLibraries)
#set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)

add_definitions(-DUNICODE)
add_definitions(-D_UNICODE)
add_definitions(-DPUTTY_CAC)

include_directories(
  ${CMAKE_SOURCE_DIR}/cert
  )

string(TIMESTAMP _time_stamp)
 
configure_file(
  version.cpp.in
  ${CMAKE_CURRENT_BINARY_DIR}/version.cpp
  @ONLY)

configure_file(
  version.rc.in
  ${CMAKE_CURRENT_BINARY_DIR}/version.rc
  @ONLY)

qt5_add_resources(
  QRC
  pageant+.qrc
  )

set(SRC
  aboutdlg.ui
  btselectdlg.ui
  confirmacceptdlg.ui
  keyviewdlg.ui
  mainwindow.ui
  passphrasedlg.ui
  rdpkeydlg.ui
  settingdlg.ui
  #
  pageant+.rc
  #
  main.cpp
  mainwindow.cpp
  misc.cpp
  misc_cpp.cpp
  winpgnt.cpp
  winhelp.cpp
  winmisc.cpp
  winutils.c
  winutils_qt.cpp
  passphrasedlg.cpp
  aboutdlg.cpp
  confirmacceptdlg.cpp
  sock_server.c
  ud_socket.c
  ssh-agent_emu.cpp
  pageant.cpp
  setting.cpp
  ssh-agent_ms.cpp
  winutils_cpp.cpp
  #
  sshaes.c
  sshbn.c
  sshdes.c
  sshdss.c
  sshmd5.c
  sshpubk.c
  sshrsa.c
  sshsh512.c
  sshsha.c
  sshsh256.c
  sshecc.c
  sshbn_cpp.cpp
  sshbcrypt.c
  sshblowf.c
  sshblowf.h
  sshbn.h
  #
  cert/cert_capi.c
  cert/cert_common.c
  cert/cert_pkcs.c
  cert/pkcs_helper.c
  cert/cert_capi.h
  cert/cert_common.h
  cert/cert_pkcs.h
  #
  settingdlg.cpp
  keyviewdlg.cpp
  btselectdlg.cpp
  bt_agent_proxy.cpp
  bt_agent_proxy_main.cpp
  debug.cpp
  ckey.cpp
  codeconvert.cpp
  obfuscate.cpp
  passphrases.cpp
  keystore.cpp
  rdpkeydlg.cpp
  rdp_ssh_relay.cpp
  keyfile.cpp
  encode.cpp
  smartcard.cpp
  import.c
  mainwindow.h
  passphrasedlg.h
  aboutdlg.h
  confirmacceptdlg.h
  settingdlg.h
  setting.h
  keyviewdlg.h
  btselectdlg.h
  bt_agent_proxy.h
  bt_agent_proxy_main.h
  ckey.h
  codeconvert.h
  obfuscate.h
  passphrases.h
  keystore.h
  rdpkeydlg.h
  rdp_ssh_relay.h
  smartcard.h
  #
  version.h
  ${CMAKE_CURRENT_BINARY_DIR}/version.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/version.rc
  #
  qtstatic.cpp
  )

add_executable(
  pageant+ WIN32
  ${SRC}
  ${QRC}
  )

if(USE_STATIC_QTLIB)
  set(QT5_ADDITIONAL_LIBS
    optimized ${Qt5_DIR}/../../Qt5FontDatabaseSupport.lib
    debug ${Qt5_DIR}/../../Qt5FontDatabaseSupportd.lib
    optimized ${Qt5_DIR}/../../Qt5AccessibilitySupport.lib
    debug ${Qt5_DIR}/../../Qt5AccessibilitySupportd.lib
    optimized ${Qt5_DIR}/../../Qt5ThemeSupport.lib
    debug ${Qt5_DIR}/../../Qt5ThemeSupportd.lib
    optimized ${Qt5_DIR}/../../Qt5EventDispatcherSupport.lib
    debug ${Qt5_DIR}/../../Qt5EventDispatcherSupportd.lib
    optimized ${Qt5_DIR}/../../Qt5WindowsUIAutomationSupport.lib
    debug ${Qt5_DIR}/../../Qt5WindowsUIAutomationSupport.lib

    optimized ${Qt5_DIR}/../../../plugins/platforms/qminimal.lib
    debug ${Qt5_DIR}/../../../plugins/platforms/qminimald.lib
    optimized ${Qt5_DIR}/../../../plugins/platforms/qwindows.lib
    debug ${Qt5_DIR}/../../../plugins/platforms/qwindowsd.lib
    optimized ${Qt5_DIR}/../../../plugins/imageformats/qico.lib
    debug ${Qt5_DIR}/../../../plugins/imageformats/qicod.lib

    optimized ${Qt5_DIR}/../../qtlibpng.lib
    debug ${Qt5_DIR}/../../qtlibpngd.lib
    optimized ${Qt5_DIR}/../../qtpcre2.lib
    debug ${Qt5_DIR}/../../qtpcre2d.lib
    optimized ${Qt5_DIR}/../../qtharfbuzz.lib
    debug ${Qt5_DIR}/../../qtharfbuzzd.lib
    optimized ${Qt5_DIR}/../../qtfreetype.lib
    debug ${Qt5_DIR}/../../qtfreetyped.lib

    optimized ${Qt5_DIR}/../../../plugins/iconengines/qsvgicon.lib
    debug ${Qt5_DIR}/../../../plugins/iconengines/qsvgicond.lib
    )
endif()


target_link_libraries(
  pageant+
  Qt5::WinMain
  Qt5::Core
  Qt5::Widgets
  Qt5::Gui
  ${QT5_ADDITIONAL_LIBS}
  #
  imm32.lib
  version.lib
  Winmm.lib
  Netapi32.lib
  Userenv.lib
  Comdlg32.lib
  shell32.lib
  gdi32.lib
  User32.lib
  Advapi32.lib
  Comctl32.lib
  Dwmapi.lib
  )

qt5_use_modules(
  pageant+
  Widgets
  )

if(NOT USE_STATIC_QTLIB)
  windeployqt(
    pageant+
    ${CMAKE_CURRENT_BINARY_DIR}
    )
endif()
  

###############

SET(RDP_SRC
  codeconvert.h
  debug.h
  misc.h
  misc_cpp.h
  pageant+.h
  puttymem.h
  rdp_ssh_relay_def.h
  setting.h
  winmisc.h
  winpageant_client.h
  winutils.h
  codeconvert.cpp
  debug.cpp
  misc.cpp
  misc_cpp.cpp
  setting.cpp
  version.cpp
  winmisc.cpp
  winpageant_client.cpp
  winutils.c
  pageant+_rdp_client.cpp
  #
  version.h
  ${CMAKE_CURRENT_BINARY_DIR}/version.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/version.rc
  )

add_library(
  pageant+_rdp_client SHARED
  ${RDP_SRC}
  )


##############################################################################
install(
  TARGETS pageant+
  RUNTIME
  DESTINATION bin
  COMPONENT applications)
install(
  TARGETS pageant+_rdp_client
  RUNTIME
  DESTINATION bin
  COMPONENT applications)

set(CPACK_GENERATOR "NSIS;ZIP;7Z")
set(CPACK_PACKAGE_NAME "pageant+")
set(CPACK_PACKAGE_VENDOR "https://github.com/zmatsuo")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "pageant+ based on pageant")
set(CPACK_PACKAGE_VERSION_MAJOR ${VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${VERSION_PATCH})
set(CPACK_PACKAGE_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}_${GIT_HASH}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "pageant+")
set(CPACK_PACKAGE_EXECUTABLES "pageant+" "pageant+")
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/license.txt)
set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/zmatsuo/pageant-")
set(CPACK_NSIS_HELP_LINK "https://github.com/zmatsuo/pageant-/issues")

include(CPack)

##############################################################################

find_program(
  NSIS NSIS.exe
  HINTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/nsis-3.03"
  HINTS "C:/Program Files (x86)/NSIS"
  )

message(STATUS "NSIS")
message(STATUS " dir ${NSIS}")

string(REGEX REPLACE [[^(.*)/.*$]] [[\1]] CMAKE_PATH ${CMAKE_COMMAND})
message(STATUS "cmake(cpack)")
message(STATUS " cmake path ${CMAKE_PATH}")

file(TO_NATIVE_PATH ${CMAKE_PATH} CMAKE_PATH_N)

file(
  WRITE "${CMAKE_CURRENT_BINARY_DIR}/make_installer.bat"
  "cd %~dp0\n"
  "set PATH=\"${CMAKE_PATH_N}\";%PATH%\n"
  "setlocal\n"
  )
file(APPEND
  "${CMAKE_CURRENT_BINARY_DIR}/make_installer.bat"
  "cpack -G ZIP\n"
  "cpack -G 7Z\n"
  )
if(NSIS)
  file(TO_NATIVE_PATH ${NSIS} NSIS_N)
  string(REGEX REPLACE [[^(.*)\\.*$]] [[\1]] NSIS_N_PATH ${NSIS_N})
  file(APPEND
    "${CMAKE_CURRENT_BINARY_DIR}/make_installer.bat"
    "set PATH=${NSIS_N_PATH};%PATH%\n"
    "cpack -G NSIS\n"
    )
endif(NSIS)
file(APPEND
  "${CMAKE_CURRENT_BINARY_DIR}/make_installer.bat"
  "endlocal\n"
  "pause\n"
  )
