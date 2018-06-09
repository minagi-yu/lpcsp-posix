# LPCSP for POSIX

Flash Programmer for LPC8xx/1xxx/2xxx/4xxx


## 概要

LPCSPはChaNさんが作られたNXPのLPC8xx/1xxx/2xxx/4xxxファミリーMCU用のフラッシュ書き込みソフトウェアです。

オリジナルはWindowsで動くWin32のプログラムのため、Unix系OSで使えるようにPOSIXのプログラムに移植しました。


## 使い方

ChaNさんのウェブサイトをみてください : http://elm-chan.org/works/sp78k/report_e.html


## オリジナルとの変更点

### ポート番号の代わりにデバイス名を指定する

Unix系OSではCOMポート番号で指定しないため。

USB-シリアル変換器を使ったときの例：

macOS：

    /dev/tty.usbserial-ABCD1234

Linux：

    /dev/ttyUSB0

FreeBSD：

    /dev/cuaU0

### デフォルトのボーレートが9600になっている

オリジナルのデフォルトのボーレート115200は、POSIX外なので定義されていないシステムの可能性があるため。


## サポートしてるシステム
macOS High SiellaとUbuntu 18.04、FreeBSD 11.1 ReleaseでLPC1114マイコンへの書き込みの確認を行いました。

POSIX拡張のcfmakeraw()やcfsetspeed()を使っているため、一部環境ではコンパイルできない可能性があります。


## 作者

### オリジナル

- ChaN
    - WEB : http://elm-chan.org/
    - Twitter : https://twitter.com/Philips_NE555

### POSIX移植

- minagi
    - WEB : https://mngu.net/
    - Twitter : https://twitter.com/minagi_yu


## ライセンス

GPLv3