cmake_minimum_required(VERSION 2.8)

find_package (Threads)

if(MSVC OR MINGW)
add_executable(echo_server echo_server.cpp ../ud_socket.c ../sock_server.c)
else()
add_executable(echo_server echo_server.cpp ../sock_server.c)
endif()
include_directories(..)
set(CMAKE_CXX_FLAGS "-Wall -std=c++11")

if(MINGW)
    # for detecting Windows compilers
	target_link_libraries(echo_server ws2_32.lib shlwapi.lib)
endif()

target_link_libraries (echo_server ${CMAKE_THREAD_LIBS_INIT})
