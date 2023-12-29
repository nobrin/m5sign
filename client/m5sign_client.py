#!/usr/bin/env python3
# M5Stack Core2 for AWSのセキュアエレメント(ATECC608A)を使った署名用クライアント
# シリアル通信およびHTTP通信に対応
# 2023-12-29
import sys, os
sys.path.insert(0, "lib")

import time
from serial import Serial
from urllib.request import urlopen
from urllib.parse import urljoin
from hashlib import sha256
from base64 import b64decode
from Crypto.Hash import SHA256
from Crypto.PublicKey import ECC
from Crypto.Signature import DSS

SERIAL_PORT = "COM5"
URLBASE     = "http://192.168.11.127"

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

class M5SignClient:
    # M5Signクライアントのベースクラス
    def verify(self, pubkey_pem, message_bytes, signature):
        # 署名を検証する
        # 署名は(R[32], S[32])のバイナリまたはDER
        key = ECC.import_key(pubkey_pem)
        msg_hash = SHA256.new(message_bytes)
        encoding = "binary" if len(signature) == 64 else "der"
        verifier = DSS.new(key, "fips-186-3", encoding)
        return verifier.verify(msg_hash, signature)

class M5SignSerialClient(M5SignClient):
    # シリアル通信用クライアント
    def __init__(self, port):
        self.serial = Serial(port, 115200, timeout=3)
        self.serial.reset_input_buffer()
        self.serial.reset_output_buffer()

    def _read_response(self):
        # 応答を読み込む
        while True:
            if self.serial.in_waiting:
                return self.serial.read_all().strip()
            time.sleep(0.1)

    def get_public_key(self):
        # 公開鍵(PEM)を得る
        self.serial.write(b"PUBKEY\r")
        return self._read_response().decode()

    def sign(self, message_bytes):
        # 署名を得る: (R[32], S[32])のバイナリ
        hex_digest_bytes = sha256(message_bytes).hexdigest().encode()
        self.serial.write(hex_digest_bytes + b"\r")
        return b64decode(self._read_response())

class M5SignHttpClient(M5SignClient):
    # HTTP通信用クライアント
    def __init__(self, urlbase):
        self.urlbase = urlbase

    def get_public_key(self):
        # 公開鍵(PEM)を得る
        ua = urlopen(urljoin(self.urlbase, "publickey"))
        return ua.read().decode().strip()

    def sign(self, message_bytes):
        # 署名を得る
        hex_digest_bytes = sha256(message_bytes).hexdigest()
        ua = urlopen(self.urlbase, f"digest={hex_digest_bytes}".encode())
        return b64decode(ua.read())

if __name__ == "__main__":
    # 署名を得る
    # - 署名をするファイル -- message.txt
    # - 署名(バイナリ)     -- signature.bin
    # - 署名(DER)          -- signature.der
    # - 公開鍵(PEM)        -- M5Sign.pub
    def sign(client, msg):
        signature = client.sign(msg)                            # 署名を得る
        open("signature.bin", "wb").write(signature)            # 署名を保存
        open("signature.der", "wb").write(to_der(signature))    # 署名を保存(DER)

    def verify(client, msg):
        # 署名を検証する
        if not os.path.isfile("M5Sign.pub"):
            # 公開鍵がなければ取得する
            open("M5Sign.pub", "w").write(client.get_public_key())
        pubkey = open("M5Sign.pub").read()
        client.verify(pubkey, msg, open("signature.bin", "rb").read())
        client.verify(pubkey, msg, open("signature.der", "rb").read())

    msg = open("message.txt", "rb").read()

    # シリアル通信による署名・検証
    client_serial = M5SignSerialClient(SERIAL_PORT)
    sign(client_serial, msg)
    verify(client_serial, msg)
    print("Serial OK")

    # HTTP通信による署名・検証
    os.unlink("M5Sign.pub")
    client_http = M5SignHttpClient(URLBASE)
    sign(client_http, msg)
    verify(client_http, msg)
    print("HTTP OK")