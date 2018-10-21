# このプログラムについて

Windows用のssh agentです。putty pageantをベースにしています。

次のことを目指しています。
- 秘密鍵を利用する様々なssh関連のプログラムから利用できる
- 秘密鍵を手元(ローカル)に置いておいてなるべくコピーせずに済むようにする

次の経路でssh関連のプログラムと通信を行うことができます。
- pageant (putty ssh-agent compatible)
- ssh-agent unix domain socket (cygwin emulation)
- ssh-agent unix domain socket (Windows Native) *1
- Microsoft ssh *1 (Named Pipe,Microsoft ssh-agant compatible)
- ssh-agent TCP接続 (socat(WSL)からの接続用)

  *1 Windows10 version 1803(Redstone 4)から利用できます
 
まだまだ気になるところはありますが、概ね動作します。

# できること

- rsa-sha2-256/512 で署名ができます  
  (OpenSSH 7.7から従来の(ssh-rsa(SHA1))署名を行うと警告が出ます)
- BT pageant+を利用できます
- RDP(リモートデスクトップ)のVirtual Channelを利用
    - 接続した先(サーバ側)のpageant+から、ローカル(クライアント側)のpageant+を利用できます
- Windows Native unix domain socket対応
    - Windows 10 version 1803 Redstone 4(RS4)以降で利用可能です
	- ([build17061から](https://blogs.msdn.microsoft.com/commandline/2017/12/19/af_unix-comes-to-windows/)利用可能だと思われます)
	- [socat + TCP](https://github.com/zmatsuo/pageant-/wiki/WSL%E3%81%8B%E3%82%89-pageant---%E3%82%92%E5%88%A9%E7%94%A8%E3%81%99%E3%82%8B)を使用せずにWSLから直接利用できます
	- Windows側からは `c:/path/.ssh-ageant` とした場合、WSL側からは `export SSH_AUTH_SOCK=/mnt/c/path/.ssh-agent`などと設定します
- Windows 10 version 1803(Redstone 4)より正式版となったMicorsoftによる移植版OpenSSHのageantとなることができます
- sshの秘密鍵のパスフレーズを記憶,適当なタイミングで忘れることができます
- スマートキー(マイナンバーカード)を使うことができます
- よく使われる秘密鍵ファイルフォーマットをサポートしています(Putty形式,OpenSSH形式,ssh.com形式)
- 読み込んだ秘密鍵からsshサーバー用公開鍵を抽出できます

# 使い方

## インストール

- インストーラを使用する場合  
  インストーラ(pageant+-xxx.exe)を実行してください
- アーカイブファイルを使用する場合  
  ファイルを解いて適当なフォルダに置いてください

## 起動

- 他のsshエージェントを止める
	- pageant(putty)
	- ssh-pageant(cygwinなど)
	- Microsoft OpenSSH ageant
- 環境変数`SSH_AUTH_SOCK`を調整する
	- Microsoft版OpenSSH を使用するときは設定しない(又は`\\.\pipe\openssh-ssh-agent`)
- pageant+を起動する
- 必要に応じて設定から自動起動するようにする
- `ssh-add -l`などで接続できればok

## 鍵ファイル

- 'Add Key file'ボタンを押してファイルを追加
- `ssh-add -l`で鍵を表示できればok

## Micorsoft版OpenSSH

- 環境変数`SSH_AUTH_SOCK`を設定しない場合の初期値は`\\.\pipe\openssh-ssh-agent`

## リモートデスクトップ経由(RDP relay)

- 接続した先(サーバ側)
	- pageant+を起動、設定の`SSH Agent relay on RDP Server`をチェックする
- ローカル(クライアント側)
	- pageant+を起動、設定の`SSH Agent relay on RDP Client`をチェックする  
      (チェックを入れた後のmstscの実行から有効になります)
	- `pageant compatible`のチェックも入れる
- リモートデスクトップ(mstsc.exe)で接続する
- `Add Rdp Key`からダイアログを出す
- `View client key`を押してクライアント側の鍵一覧を表示する
- `Import Key`を押して鍵を取り込む

## マイナンバーカード(スマートカード)

- マイナンバーカードに対応したOpenSCをインストールしておく  
  opensc 0.17.0 からマイナンバーカード対応のようです  
  [0.18.0](https://github.com/OpenSC/OpenSC/releases/tag/0.18.0)で動作確認しました
- 'Add PKCS Cert'ボタンを押す
- `C:\Program Files\OpenSC Project\OpenSC\pkcs11\opensc-pkcs11.dll`を選ぶ
- 証明書選択で次のものを選ぶ
	- カード名が数字とアルファベット
	- 発行者が Japan Agency for Local
- pinは、利用者証明用電子証明書(マイナポータルへのログインなどに使用)に設定したもの

## BT pageant+

- `Add BT`ボタンを押す
- BT pageant+が動作しているデバイスを選択
- `connect`ボタンを押す
- デバイスを選択
- 鍵を選択
- `鍵取込`ボタンを押す

## 公開鍵の取得

- key listの鍵一覧から、公開鍵を取得することができます
- key listを表示して、鍵を1つ選択する
- `公開鍵`ボタンを押す
- クリップボードにコピーされた文字列を、サーバーの `~/.ssh/authorized_keys` に追加する

# 制限

## 鍵ファイル

- 楕円曲線暗号系のテストはあまり行っていません

## 動作確認できたスマートカード

putty-CACを元にしたCAPI(Cryptographic API)経由、pkcs#11経由で対応しています。
手元に使えるハードウェアが限られているのでほとんどテストできていません。

動作したリーダー+カード
- SCR3310-NTTCom + マイナンバーカード
- ACS ACR122 + マイナンバーカード
- yubiko yubikey neo pivモード

## うまく自動起動しなくなったとき

pageant+のスタートタップ登録をもう一度やり直してください

# 通信できるプログラム

次のプログラムと通信できます。

## terminal
- putty
- teraterm

## WSL(Windows Subsystem for Linux)
- OpenSSH
	- Windows Native unix domain socket経由
	- socatを経由したソケット接続

## OpenSSH
- OpenSSH
	- Windows 10 version 1803(Redstone 4)より正式版となったMicorsoftによる移植版

## cygwin/msys ssh family
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

# 設定

設定はpageant+が動作しているPCのレジストリに保存します。

iniファイルに保存することもできます。

## 1. レジストリでiniファイルを指定
- `HKEY_CURRENT_USER\Software\pageant+\IniFile`

このレジストリが設定してあると、指定iniファイルを使用します。
REG_EXPAND_SZの場合は環境変数参照(%HOME%など)があると展開されます。
(`%HOME%\.pageant+.ini`など)。

設定がない場合、iniファイルを探します。

## 2. 自動で探す
次の順でファイルを探します。見つかったらそのファイルを使用します。
- `%HOME%\.pageant+_[host].ini`
- `%HOME%\.pageant+.ini`
- `%HOMEDRIVE%%HOMEPATH%\.pageant+_[host].ini`
- `%HOMEDRIVE%%HOMEPATH%\.pageant+.ini`
- `%USERPROFILE%\.pageant+_[host].ini`
- `%USERPROFILE%\.pageant+.ini`
- `%APPDATA%\pageant+\pageant+_[host].ini`
- `%APPDATA%\pageant+\pagent+.ini`
- `[pageant+.exe(又はdll)と同じフォルダ]\pageant+_[host].ini`
- `[pageant+.exe(又はdll)と同じフォルダ]\pageant+.ini`
- `[pageant+.exe(又はdll)のカレントフォルダ]\pageant+_[host].ini`
- `[pageant+.exe(又はdll)のカレントフォルダ]\pageant+.ini`

## 3. 上記すべて見つからなかった場合
レジストリに保存する
- `HKEY_CURRENT_USER\Software\pageant+`

# 参照したプロジェクトなど

- PuTTY  
	<https://www.chiark.greenend.org.uk/~sgtatham/putty/latest.html>
- PuTTY-CAC  
	<https://risacher.org/putty-cac/>

reference.txt を参照してください。

# ビルド準備

- Visual Studo 2017をインストール  
  (MinGW 32bitでのコンパイルはテストしていません)
- `libs/runonce.bat`を実行

# リリース用ビルド

ソースフォルダでコマンドプロンプトで次の通りに実行する
```
mkdir build
cd build
cmake .. -G "Visual Studio 15 2017 Win64"
cmake --build . --config release
make_installer.bat
```

# 開発用ビルド

- 準備
    - <https://www.qt.io/> から Qtをダウンロードしてインストールする
    - `not_cmake/make_version.bat`を実行して  
      `not_cmake/version.cpp`を生成しておく

- Visual Studio 2017を使用する場合
    - Vsiual Studio 2017に`Qt Visual Studio Tools`をインストールする
    - `pageant+.sln`をVisual Studio 2017で開く

- Qt Creatorを使用する場合
    - `pageant+.pro`をQt Creatorで開く

# X11 Forwarding (Cygwin/X + ssh)

Cygwin/Xを使っていて、cygwin系の`ssh -Y`を使用するとX11 Forwardingがう
まくいくのに、puttyやMS sshなどではうまくいかないのは、Xサーバーがtcp
をlistenしていないためです。

`startxwin -- -listen tcp`などでtcp経由で接続できるようになります。
- http://x.cygwin.com/docs/faq/cygwin-x-faq.html#q-xserver-nolisten-tcp-default

# license

- PuttyはMIT
- このプロジェクトはそれにならって基本MIT
- Putty-CACの元になったPutty SCがGNUのようなので、それに由来するソースはGNU
- バイナリで配布するときはGNU
