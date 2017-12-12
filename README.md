# このプログラムについて

ssh agent for windows

Windows用のssh-agentです。
次の通信を行うことができます。
- pageant (putty ssh-agent)
- ssh-agent cygwin
- ssh-agent Microsoft (0.0.17.0以降は未対応)
- ssh-agent TCP接続 (socat(WSL)からの接続用)

まだまだ気になるところはありますが、概ね動作します。

# できること

- sshの秘密鍵のパスフレーズを記憶,適当なタイミングで忘れる
- ほとんどのsshクライアントと通信できる
- スマートカードを使うことができる
- WSLのssh-agentとして利用[ここを参照ください](https://github.com/zmatsuo/pageant-/wiki/WSL%E3%81%8B%E3%82%89-pageant---%E3%82%92%E5%88%A9%E7%94%A8%E3%81%99%E3%82%8B)

## 大雑把な使い方

### インストール

- zipファイルを解いて適当なフォルダに置く

### 起動

- 他のsshエージェントを止める
	- pageant(putty)
	- ssh-pageant(cygwinなど)
- 環境変数`SSH_AUTH_SOCK`を調整する
- pageant+を起動する
- 必要に応じて設定から自動起動するようにする
- `ssh-add -l`で接続できればok

### 鍵ファイル

- 'Add Key file'ボタンを押してファイルを追加
- `ssh-add -l -E md5`で鍵を表示できればok

### マイナンバーカード(スマートカード)

- 'Add PKCS Cert'ボタンを押す
- `C:\Program Files\OpenSC Project\OpenSC\pkcs11\opensc-pkcs11.dll`を選ぶ
- 証明書選択で次のものを選ぶ
	- カード名が数字とアルファベット
	- 発行者が Japan Agency for Local
- 追加したスマートカードを選択して、`公開鍵`ボタンを押す
- クリップボードにコピーされた文字列を、
  サーバーの `~/.ssh/authorized_keys` に追加する
- pinは、利用者証明用電子証明書(マイナポータルへのログインなどに使用)に設定したもの

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

## OpenSSH
- Win32 port of OpenSSH
	- v0.0.17.0以降未対応
- socatを経由したソケット接続

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

# 動作チェック

次のようにコマンドを実行して同一のfingerprintが表示されればokです。
表示されているバージョンはチェックした時点のものです。

## Ubuntu 16.04.3 LTS on WSL
socat経由で接続しています
`/usr/bin/socat UNIX-LISTEN:/unix/domain/path,fork TCP:127.0.0.1:8080`

```
$ ssh -V
OpenSSH_7.2p2 Ubuntu-4ubuntu2.2, OpenSSL 1.0.2g  1 Mar 2016
$ ssh-add -l -E md5
```

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

# 制限

- addボタンで使用できる鍵ファイル種類
	- putty形式の秘密鍵
	- RSA 2048bit,4096bitのみテスト
	- 他の形式も使えるかもしれない

# 参照したプロジェクトなど

- PuTTY  
	<https://www.chiark.greenend.org.uk/~sgtatham/putty/latest.html>
- PuTTY-CAC  
	<https://risacher.org/putty-cac/>

reference.txt を参照してください。

# ビルド

- 準備
	- <https://www.qt.io/> から Qtをダウンロードしてインストールする
	- Qt 5.9.3
	- Visual Studo 2017をインストール
	- MinGW 32bitではコンパイルできなくなっています

- Visual Studio 2017を使用する場合
	- `pageant+.sln`をVisual Studio 2017で開く
- Qt Creatorを使用する場合
	- `pageant+.pro`をQt Creatorで開く

# X11 Forwarding (Cygwin/X + ssh)

Cygwin/Xを使っていて、cygwin系の`ssh -Y`を使用するとX11 Forwardingがう
まくいくのに、puttyやMS sshなどではうまくいかないのは、Xサーバーがtcp
をlistenしていないためです。

`startxwin -- -listen tcp`などでtcp経由で接続できるようになります。
- http://x.cygwin.com/docs/faq/cygwin-x-faq.html#q-xserver-nolisten-tcp-default

# やりたかったこと、これからやりたいこと

- スマートカード+sshを使いたかった
- できるようになったが、カードを持ち歩くことに疑問を感じる
- 持ち歩いているスマホに秘密キーを入れておけばokでは?
- BT経由でスマホと通信、基礎実験はできた状態
- WSLを使ってみたところ良さそうだったので、socat経由で受け付ける口を追加

# license

- PuttyはMIT
- このプロジェクトはそれにならって基本MIT
- Putty-CACの元になったPutty SCがGNUのようなので、それに由来するソースはGNU
- バイナリで配布するときはGNU
