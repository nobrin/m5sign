# M5Sign

2023-12-29

M5Stack Core2 for AWSのセキュアエレメント(ATECC608A)を使って署名をします。Core2 for AWSのATECC608Aは出荷時に秘密鍵が設定されており、設定などもあらかじめロックされているようです。また、AWSによるデバイス証明書も登録されているようです。そのため、秘密鍵の生成などを行わずに使用することができます。

最低限の実装のため、例外処理は実装していませんのでご注意下さい。


## 動作

ArduinoECCX08ライブラリを使用して、Core2側で署名を行い、署名を取得します。
HSM(Hardware Security Module)っぽく(?)使えるように、シリアル通信およびHTTP通信でSHA256ダイジェストの16進数文字列を送るとBase64エンコードされた署名データ(R[32], S[32])が返ってきます。

また、PEM形式の公開鍵を取得することもできます。


## 使い方
### 起動

ファームウェアをビルドして、書き込むだけです。クライアントの実装例は `client/m5sign_client.py` を参考にして下さい。詳細は `client/README.md` を参照して下さい。

### シリアル通信

Core2へのシリアル通信ポートに書き込むことで公開鍵の取得、署名を行うことができます。

- SHA256ダイジェストの16進数文字列+"\r"を渡すとBase64エンコードされた署名が返ります
- `PUBKEY\r` を渡すとPEM形式の公開鍵(改行は\n)が返ります

### Webサーバー

HTTP通信によりシリアル通信と同様のことができます。

- ルート -- digest={SHA256ダイジェストの16進数文字列}をPOSTするとBase64エンコードされた署名が返ります
- /publickey -- PEM形式の公開鍵が返ります

#### WiFi設定

WiFi設定でネットワークに接続することでWebサーバーが有効になります。

`m5sign-wifi.txt` というファイルの1行目にSSID、2行目にパスフレーズを記述し、SDカードのルートに配置することで、WiFi接続を行います。SDカードを挿さない、または、ファイルがない場合はWiFi接続を行わず、Webサーバーは起動しません。

WiFiの環境によってはサーバーへの通信が不安定なことがあるようです。


## ビルド環境

- VSCode 1.85.1
- PlatformIO 6.1.11
  - M5Unified 0.1.11
  - ArduinoECCX08 1.3.5
  - densaugeo/base64 1.4.0


## 注意点

- Core2ではArduinoECCX08を初期化するときにWire1およびアドレス0x35を使用します。`Wire1.setBufferSize(256)` でバッファサイズを大きくしないと `[E][Wire.cpp:513] requestFrom(): i2cRead returned Error 263` エラーが発生するようです。
- M5StackのUNIT IDのページにはSparkFun ATECCX08A Arduino Libraryへのリンクがありますが、このライブラリだとread_output()中のsendCommand()の後のdelay(1)が短いらしく、EEPROMアクセスのコマンド(EEPROM Slot 10の読み出しで起こりました)によっては `[E][Wire.cpp:513] requestFrom(): i2cRead returned Error 263` および `Error -1` というエラーが出ます( `delay(5)` にすると解消)。ArduinoECCX08モジュールだとこの問題は発生しません。
- これらの対策をすることでSpackFunのライブラリでも同じようなことはできますが、ArduinoECCX08だと、PEM形式での出力などが付いているため便利でした。署名のみならSpackFunでも使用できます(上記の修正は必要)。


## 参考サイト

- M5Stack UNIT ID -- 外部ユニットのページ
  - https://docs.m5stack.com/en/unit/id
- ArduinoECCX08
  - https://github.com/arduino-libraries/ArduinoECCX08
- SparkFun ATECCX08a Arduino Library
  - https://github.com/sparkfun/SparkFun_ATECCX08a_Arduino_Library
- M5Stack Core2 for AWS のセキュリティチップ(ATECC608A)から真性乱数を得る
  - https://qiita.com/furufuru/items/aefdfc894fcce411c6cc
- I2Cの接続デバイス
  - https://www.koki.muhen.jp/archives/2714
- M5Stack Devices I2C Address
  - https://docs.m5stack.com/en/product_i2c_addr
- ATECC608A Data Sheet
  - 英語: http://ww1.microchip.com/downloads/en/DeviceDoc/ATECC608A-TFLXTLS-CryptoAuthentication-Data-Sheet-DS40002138A.pdf
  - 日本語: http://ww1.microchip.com/downloads/jp/DeviceDoc/40002138A_JP.pdf
- [E][Wire.cpp:513] requestFrom(): i2cRead returned Error 263 を解決する必要がある
  - https://forum.microchip.com/s/topic/a5C3l000000BqESEA0/t391496
- Wire.hについて
  - https://lang-ship.com/blog/work/m5unified-1/
