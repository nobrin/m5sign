# M5Sign用Pythonクライアント

2023-12-29

M5Signを起動したモジュールにシリアル通信およびHTTP通信で署名をリクエストし、署名内容を検証するための簡単なクライアントです。最低限の動作をするだけのスクリプトです。

動作検証はWindows10 22H2 + PlatformIO 6.1.11 + Python 3.11.6で行いました。

WiFiによる接続のため、環境によってはWebサーバーへの接続が不安定な場合があるようです。


## 使い方

    $ python3 m5sign_client.py
    Serial OK
    HTTP OK

スクリプト中の以下の定義を環境に合わせて修正して下さい。

- SERIAL_PORT = "COM5" -- Core2を接続したシリアルポート
- URLBASE     = "http://192.168.11.127" -- Core2のIPアドレス


## スクリプトの内容

- M5SignClient -- ベースクラスです。verify()のみ定義してあります
- M5SignSerialClient -- シリアル通信用クライアント
- M5SignHttpClient -- HTTP通信用クライアント


## 参考

署名は(r[32], s[32])で返される。ASN.1 DERに変換するときはpyasn1モジュールを使って変換することもできます。
クライアントスクリプト中ではpyasn1を使わずに、値の先頭ビットが1になっている場合は0x00を付与することで変換しています。

```
def to_der_with_pyasn1(raw):
    # pyasn1を使う場合
    # https://github.com/mpdavis/python-jose/issues/47
    import binascii
    from pyasn1.type import univ, namedtype
    from pyasn1.codec.der.encoder import encode

    r, s = raw[:32], raw[32:]
    ds = univ.Sequence(componentType=namedtype.NamedTypes()).setComponents(
        univ.Integer(int(binascii.hexlify(r), 16)),
        univ.Integer(int(binascii.hexlify(s), 16)),
    )
    return encode(ds)
```

m5sign_client.pyでの実装はpyasn1を使わずに処理しています。

```
def to_der(raw):
    # 署名(R, S)をDERフォーマットで返す
    r, s = raw[:32], raw[32:]

    # 先頭ビットが1の場合は0x00を追加する
    if(r[0] & 0b10000000): r = b"\x00" + r
    if(s[0] & 0b10000000): s = b"\x00" + s

    # ASN.1 DERフォーマットで返す
    return b"\x30" \
        + (len(r) + len(s) + 4).to_bytes(1, "little") \
        + b"\x02" + len(r).to_bytes(1, "little") + r \
        + b"\x02" + len(s).to_bytes(1, "little") + s
```