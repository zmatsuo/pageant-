OpenSC野良ビルド
===============

- wikiのBuild instructions(Windows)にそってコンパイル
	- https://github.com/OpenSC/OpenSC/wiki/OpenSC-Windows-installer

## ざっくりした説明

### インストール
- Visual Studio 2015
- Cryptographic Provider Development Kit（以前のWindows CNG SDK）
	- https://www.microsoft.com/en-us/download/details.aspx?id=30688
	- インストール先 `C:\Program Files (x86)\Windows Kits\8.0\Cryptographic Provider Development Kit`
   - WiX (3.11)
	- http://wixtoolset.org/
- openssl
	- http://slproweb.com/products/Win32OpenSSL.html
	- OpenSSL 1.1.0f
- zlib
	- zlib-1.2.11
	- mkdir build
	- cd build
	- cmake .. -DCMAKE_INSTALL_PREFIX=./install -G "Visual Studio 14 2015 Win64"
    - cmake --build . --target install --config Release
- mingw
	- http://www.mingw.org/
- OpenSC
	- https://github.com/OpenSC/
	- 2017-07-17 SHA-1: 172f320c9a1b5664240be5fa3e143622941b2845

### 手順

- git clone https://github.com/OpenSC/OpenSC.git
- win32/Make.rules.mak をパッチ、次の値を調整
	- `ENABLE_OPENSSL`
	- `OPENSSL_DIR`
	- `OPENSSL_LIB`
	- `ENABLE_ZLIB_STATIC`
	- `ZLIB_INCL_DIR`
	- `CNGSDK_INCL_DIR = "/IC:\Program Files (x86)\Windows Kits\8.0\Cryptographic Provider Development Kit\Include"`
- dos窓などを使う
- `set PATH=C:\mingw\msys\1.0\bin;C:\mingw\bin;%PATH%`
- openscフォルダで
- `bash.exe -c ./bootstrap`
- `bash.exe -c ./configure`
- `"C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" amd64`
- `nmake /f Makefile.mak BUILD_ON=WIN64 BUILD_FOR=WIN64 PLATFORM=x64`
- `cd etc`
- `make`
- `cd ..\win32`
- `nmake /f Makefile.mak BUILD_ON=WIN64 BUILD_FOR=WIN64 PLATFORM=x64 OpenSC.msi`
- `win32/OpenSC.msi`ができる

---------------

メモ
===

## mingwでconfigureできない件

- http://blog.k-tai-douga.com/article/52368303.html

## CNG (Crypto Next Genaration)

Cryptographic Provider Development Kit（以前のWindows CNG SDK）
- https://www.microsoft.com/en-us/download/details.aspx?id=30688

インストール先
- `C:\Program Files (x86)\Windows Kits\8.0\Cryptographic Provider Development Kit`

## OpenSSL
- http://slproweb.com/products/Win32OpenSSL.html
- C:\OpenSSL-Win64が標準的なフォルダ?

# zlib
cmakeが無いときはこんな感じだった

- x86  
	`nmake /f win32\Makefile.msc LOC="-DASMV -DASMINF" OBJA="inffas32.obj match686.obj" zlib.lib`

- x64  
	`nmake /f win32\Makefile.msc AS=ml64 LOC="-DASMV -DASMINF -I." OBJA="inffasx64.obj gvmat64.obj inffas8664.obj" zlib.lib`

# マイナンバーカード
- https://www.osstech.co.jp/~hamano/posts/jpki-ssh/

これで公開キーが取得できればOpenSCのコンパイルはokと思われる
- `pkcs15-tool --read-ssh-key 1 > id_rsa.pub`

