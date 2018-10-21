setlocal
rem set CMAKE=cmake.exe
set CMAKE="C:\Program Files\CMake\bin\cmake.exe"
rem set CMAKE="C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
%CMAKE% -P runonce.cmake
echo %CMAKE% %%1 %%2 %%3 %%4 %%5 %%6 %%7 %%8 > ..\cmake.bat
pause
