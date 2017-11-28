cd /d %~dp0
bash make_version.sh

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
endlocal

c:\cygwin64\bin\bash make_zip.sh
pause
