cd /d %~dp0
bash make_version.sh

setlocal
call "%VS140COMNTOOLS%vsvars32.bat"
msbuild pageant+.sln /t:rebuild /p:Configuration=Release /p:Platform="x64" /m
endlocal

bash make_zip.sh
pause
