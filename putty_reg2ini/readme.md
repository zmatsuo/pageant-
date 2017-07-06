
# puttyの設定をレジストリからiniファイルに変換

- オリジナル  
	[PuTTYごった煮版 レジストリ<->INIファイルコンバータ](http://yebisuya.dip.jp/yeblog/archives/a000456.html)

## 使い方

- reg2ini.batを実行
- `putty_export.ini`ができる

perl と reg(Windowsに入っているプログラム)を使用しています。

## 手を入れた点

exportしたレジストリのファイルはutf-16beなのでそれをそのまま読み込めるようにしました。
perl5.8頃からの機能を使っています。

iniファイルからの変換は少し怪しいです。

## 参照

同様の変更がすでにありました…。
- [PuTTYごった煮版 レジストリ<->INIファイルコンバータ の修正点](https://narazaka.net/o/konata/computing/puttyini.html)
