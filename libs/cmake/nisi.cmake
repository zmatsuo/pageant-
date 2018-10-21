
# https://sourceforge.net/projects/nsis/

cmake_minimum_required(VERSION 3.8)

set(BASE nsis-3.03)
set(HASH_SHA1 ea69aa8d538916c9e8630dfd0106b063f7bb5d46)

set(CHECK_FILE "${BASE}/NSIS.exe")

if(NOT EXISTS ${CHECK_FILE})

  set(ARC ${CMAKE_SOURCE_DIR}/download/nsis/${BASE}.zip)
  
  file(
	DOWNLOAD
	https://sourceforge.net/projects/nsis/files/NSIS%203/3.03/nsis-3.03.zip/download
    ${ARC}
    EXPECTED_HASH SHA1=${HASH_SHA1}
    SHOW_PROGRESS
    )

  execute_process(
    COMMAND ${CMAKE_COMMAND} -E tar "xvf"
    ${ARC}
    )

endif()
