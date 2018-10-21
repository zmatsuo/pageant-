
# https://github.com/martinrotter/qt5-minimalistic-builds

cmake_minimum_required(VERSION 3.8)

set(BASE qt-5.10.1-static-ltcg-msvc2017-x86_64)
set(HASH_SHA1 e5eb86de78537781d548f824333788e70908f838)

set(CHECK_FILE "${BASE}/lib/Qt5Widgets.lib")

if(NOT EXISTS ${CHECK_FILE})

  set(ARC ${CMAKE_SOURCE_DIR}/download/qt_lib/${BASE}.7z)
  
  file(
	DOWNLOAD
	https://bitbucket.org/skunkos/qt5-minimalistic-builds/downloads/${BASE}.7z
    ${ARC}
    EXPECTED_HASH SHA1=${HASH_SHA1}
    SHOW_PROGRESS
    )

  execute_process(
    COMMAND ${CMAKE_COMMAND} -E tar "xvf"
    ${ARC}
    )

  execute_process(
    COMMAND ${BUILD_DIR}/qt-5.10.1-static-ltcg-msvc2017-x86_64/bin/qtbinpatcher.exe
    WORKING_DIRECTORY "${BUILD_DIR}"
    )

endif()
