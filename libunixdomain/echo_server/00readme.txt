
# build

## cygwin
- cygwinŠÂ‹«‚Å
- mkdir build_cygwin
- cd build_cygwin
- cmake ..
- make

## visual studio
- windows‚ÌŠÂ‹«(cmd,powershell)‚Å
- mkdir build_vc
- cd build_vc
- cmake ..
- make

## msys2
- msys2ŠÂ‹«‚Å
- mkdir build_msys2
- cd build_msys2
- cmake ..
- make

## mingw
- mingwŠÂ‹«‚Å
  - path C:\Qt\Tools\mingw530_32\bin
  - path C:\Program Files\CMake\bin;%PATH%
- mkdir build_mingw
- cd build_mingw
- cmake -G"MinGW Makefiles" ..
- mingw32-make

# test
- echo_server‚ð‹N“®
- netcat‚ÅÚ‘±
    - unix domain socket‚Ì‚Æ‚« `nc -U socket_path`‚ÅÚ‘±
    - tcp socket ‚Ì‚Æ‚« `nc localhost [port]`‚ÅÚ‘±
- netcat‘¤‚©‚ç‘—M‚·‚é

# hard test
- ls -R > /tmp/ls.txt
- ls -R | nc -U /tmp/test.sock > /tmp/nc.txt
- ls -R | nc -U /tmp/test.sock | nc -U /tmp/test.sock | nc -U /tmp/test.sock | nc -U /tmp/test.sock | nc -U /tmp/test.sock | nc -U /tmp/test.sock | nc -U /tmp/test.sock | nc -U /tmp/test.sock| nc -U /tmp/test.sock| nc -U /tmp/test.sock| nc -U /tmp/test.sock> /tmp/nc2.txt
