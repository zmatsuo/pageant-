setlocal
set CURDIR=%~dp0%

rem jomをインストールしておく
if exist jom\jom.exe goto pass_jom
wget http://download.qt.io/official_releases/jom/jom.zip
mkdir jom
pushd jom
tar xvf ../jom.zip
popd
:pass_jom
set PATH=%CURDIR%\jom;%PATH%

rem qtソース取得
set QT_VERSION=5.12.3
set QT_SRC_BASE=qt-everywhere-src-%QT_VERSION%
set QT_SRC=%QT_SRC_BASE%.zip
if exist %QT_SRC% goto pass_qt_src_download
wget http://download.qt.io/official_releases/qt/5.12/5.12.3/single/%QT_SRC%
:pass_qt_src_download

rem qtソース展開
if exist %QT_SRC_BASE%\configure.bat goto pass_extract_src
tar xvf %QT_SRC%
:pass_extract_src

rem ビルド時のオプション指定を変更する
rem https://kazblog.hateblo.jp/entry/2017/09/22/005219
if exist %QT_SRC_BASE%\qtbase\mkspecs\common\msvc-desktop.conf.org goto pass_modify_config
pushd %QT_SRC_BASE%\qtbase\mkspecs\common
mv msvc-desktop.conf msvc-desktop.conf.org
sed -e 's/-MD/-MT/' msvc-desktop.conf.org > msvc-desktop.conf
popd
:pass_modify_config

rem インストール先
set PREFIX=%CURDIR%qt_%QT_VERSION%_static
echo %PREFIX%

rem build
pushd %QT_SRC_BASE%
mkdir build_x64
cd build_x64
call ..\Configure.bat -confirm-license -opensource -static -ltcg -nomake examples -nomake tests -no-opengl -no-angle -no-warnings-are-errors -skip qtwebchannel -skip qtwebengine -no-qdbus -prefix %PREFIX%
jom
jom install
popd

endlocal
