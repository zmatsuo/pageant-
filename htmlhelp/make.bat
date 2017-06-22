set PATH=C:\MinGW\bin
dlltool -m i386 -k -d htmlhelp.def -l libhtmlhelp.a
rem dlltool -m i386:x86-64 -k -d htmlhelp.def -l libhtmlhelp64.a
dlltool -m i386 -k -d ncrypt.def -l libncrypt.a
pause

