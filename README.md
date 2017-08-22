# このプログラムについて

ssh ageant for windows

Windows用のssh-agentです。
次の通信を行うことができます。
- pageant (putty ssh-agent)
- ssh-agent cygwin
- ssh-agent Microsoft (0.0.17.0は未対応)

まだまだ気になるところはありますが、
概ね動作します。

## わかっている不具合/修正したい動作

- pkcs#11のコントロールコードが今ひとつ
	- putty-cac初期(putty-sc)と現在のコードが入り混じっている
- カードリーダーの挿抜、スマートカードの挿抜
	- スマートカードが利用できる状態で起動しないといけない
	- pagent+が動作中に使用不能(カードを取り出すなど)になってはいけない
- カードリーダー、スマートカードの選択UIの追加

## スマートカード対応について

putty-CACを元にしたCAPI(Cryptographic API)経由、pkcs#11経由で対応しています。
手元に使えるハードウェアが限られているのでほとんどテストできていません。

マイナンバーカード対応版OpenSCがインストールしてあれば
マイナンバーカードが使えます。

動作したリーダー+カード
- SCR3310-NTTCom + マイナンバーカード
- ACS ACR122 + マイナンバーカード
- yubiko yubikey neo pivモード

Windowsでスマートカードでsshできるようになります。

# 通信できるプログラム

次のプログラムと通信できます。

## terminal
- putty
- teraterm

## OpenSSH
- Win32 port of OpenSSH
	- v0.0.17.0は未対応

## cygwin/msys ssh familys
- cygwin/msys ssh
- mobaxterm (you need reset `SSH_AUTH_SOCK`)
- TortoiseSVN/TortoiseGit(modified plink)
- SourceTree
- Git for Windows

## client
- Bivise ssh client

## scp
- winscp
- filezilla

# 動作チェック

次のようにコマンドを実行して同一のfingerprintが表示されればokです。
表示されているバージョンはチェックした時点のものです。

## git for windows
```
$ ssh -V
OpenSSH_7.3p1, OpenSSL 1.0.2k  26 Jan 2017
$ ssh-add -l -E md5
```

## msys2
```
$ pacman -S openssh
$ LANG=C pacman -Sl | grep openssh
msys openssh 7.5p1-1 [installed]
$ ssh-add -l -E md5
```

## cygwin
```
$ cygcheck -c | grep openssh
openssh                                 7.5p1-1                      OK
$ ssh-add -l -E md5
```

## source tree
- open sourcetree gui
- push terminal button
```
$ ssh -V
OpenSSH_7.1p2, OpenSSL 1.0.2g  1 Mar 2016
$ ssh-add -l -E md5
```

## openssh ported by microsoft

- https://github.com/PowerShell/Win32-OpenSSH/releases/tag/v0.0.16.0
- v0.0.17.0からソケットの仕組みが変わったらしく、通信できません

```
PS C:\Program Files\OpenSSH-Win64> .\ssh -V
OpenSSH_7.5p1, OpenSSL 1.0.2d 9 Jul 2015
.\ssh-add -l -E md5
```

# 参照したプロジェクトなど

- PuTTY  
	<https://www.chiark.greenend.org.uk/~sgtatham/putty/latest.html>
- PuTTY-CAC  
	<https://risacher.org/putty-cac/>

reference.txt を参照してください。

# プログラムの起動

次のQTのdllがexeと同じフォルダにあれば起動します(Debug版の場合は各々のDebug版)
- QtCore.dll
- Qt5Gui.dll
- Qt5Widgets.dll

exeだけのときはパスを通しておくとokです(Qtをstatic linkにしてexe1つだけで起動するようにしたい :-)。
次のようなバッチファイルで起動させる方法があります。

```bat
set QT_BIN=C:\Qt\5.9\msvc2015_64\bin
set exe=debug\pageant+.exe
path %QT_BIN%;%PATH%
start "" %exe%
```

# ビルド

- 準備
	- <https://www.qt.io/> から Qtをダウンロードしてインストールする
	- Qt 5.9
	- Visual Studo 2015を使用する場合
		- msvc2015 64-bit ,32-bit をインストール
	- MinGWを使用する場合
		- MinGW 5.3.0 32 bit (Desktop Qt 5.9.0 MinGW) を使用する

- Visual Studio 2015を使用する場合
	- `pageant+.sln`をVisual Studio 2015で開く

- mingwを使用する場合
	- `pageant+.pro`からQtCreatorを開く
	- Configure Projectでmingwを選ぶ

# X11 Forwarding (Cygwin/X + ssh)

Cygwin/Xを使っていて、cygwin系の`ssh -Y`を使用するとX11 Forwardingがう
まくいくのに、puttyやMS sshなどではうまくいかないのは、Xサーバーがtcp
をlistenしていないためです。

`startxwin -- -listen tcp`などでtcp経由で接続できるようになります。
- http://x.cygwin.com/docs/faq/cygwin-x-faq.html#q-xserver-nolisten-tcp-default

# license

- おおもとのPuttyがMIT
- このプロジェクトはそれにならって基本MIT
- Putty-CACの元になったPutty SCがGNUのようなので、それに由来するソースはGNU
- バイナリで配布するときはGNU
