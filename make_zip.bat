setlocal 
cd /d %~dp0
if not defined QTDIR (
   set QTDIR=C:\Qt\5.10.1\msvc2017_64
)
set PATH=%QTDIR%\bin;%PATH%
rem set SH=bash.exe
set SH=c:\cygwin64\bin\bash.exe
rem set SH="C:\Program Files\Git\usr\bin\bash.exe"

%SH% make_version.sh

setlocal
set VCVARSALL="C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat"
if not exist %VCVARSALL% (
   set VCVARSALL="C:\Program Files (x86)\Microsoft Visual Studio\2017\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
)
if not exist %VCVARSALL% (
   echo VS2017?
   pause
   exit
)
pushd .
call %VCVARSALL% amd64
popd
msbuild pageant+.sln /t:rebuild /p:Configuration=Release /p:Platform="x64" /m
rem msbuild pageant+.sln /p:Configuration=Release /p:Platform="x64" /m
endlocal

%SH% make_zip.sh
endlocal
pause
