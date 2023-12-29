/*
  M5Stack Core2 for AWSのATECC608Aセキュアエレメントを使って署名する
  Arduino ECCX08モジュールを使用する
  2023-12-29
 */
#include <M5Unified.h>
#include <SD.h>
#include <WiFi.h>
#include <ArduinoECCX08.h>
#include <utility/ECCX08JWS.h>
#include <WebServer.h>
#include <base64.hpp>

#define SERIAL_SPEED 115200
#define WIFIINFO_FILENAME "/m5sign-wifi.txt"
#define SLOT 0

void readWiFiInfo(String *wifissid, String *wifipass);
bool sign(String hexDigest, byte *signature);
void handleSerial();
void startHttpd();

WebServer httpd(80);

bool wifiStatus = false;
bool isHttpdStarted = false;

void setup() {
  // 起動時セットアップ
  // Core2 For AWSのATECC608Aの設定
  // ArduinoECCX08の設定とWireおよびアドレスが異なるために再設定
  // ECCX08.cpp:894付近 ECCX08Class ECCX08(Wire, 0x60);
  Wire1.begin();
  // [E][Wire.cpp:513] requestFrom(): i2cRead returned Error 263 への対応
  Wire1.setBufferSize(256);
  ECCX08 = ECCX08Class(Wire1, 0x35);

  Serial.begin(SERIAL_SPEED);
  while(!Serial);

  M5.begin();
  M5.Display.setTextColor(TFT_GREEN);
  M5.Display.println("Start M5Sign...");
  M5.Display.setTextColor(TFT_WHITE);

  // ATECC608Aの初期化
  M5.Display.print("* Start ATECC608A...");
  if(ECCX08.begin()){
    M5.Display.println("Success!!");
  }else{
    M5.Display.println("Failed to communicate with ECC508/ECC608!");
    while(1);
  }

  String wifissid, wifipass;
  readWiFiInfo(&wifissid, &wifipass);
  if(wifissid == ""){
    M5.Display.println("* WiFi info not found. Start without WiFi.");
  }else{
    M5.Display.println("* WiFi info found. Connecting to WiFi.");
    M5.Display.print("  Device MAC Addr: ");
    M5.Display.println(WiFi.macAddress());
    WiFi.begin(wifissid, wifipass);
  }
}

void loop() {
  // メインループ
  if(!wifiStatus && WiFi.status() == WL_CONNECTED){
    wifiStatus = true;
    M5.Display.setTextColor(TFT_CYAN);
    M5.Display.print("* WiFi CONNECTED: ");
    M5.Display.println(WiFi.localIP());
    M5.Display.setTextColor(TFT_WHITE);
    startHttpd();
    isHttpdStarted = true;
  }
  handleSerial();
  if(isHttpdStarted){ httpd.handleClient(); }
  delay(100);
}

void readWiFiInfo(String *wifissid, String *wifipass) {
  // SDカードからWiFi接続情報を得る
  // 1行目：SSID, 2行目：パスフレーズ
  *wifissid = *wifipass = "";
  if(!SD.begin(GPIO_NUM_4)){ return; }
  if(!SD.exists(WIFIINFO_FILENAME)){ return; }
  File fh = SD.open(WIFIINFO_FILENAME, FILE_READ);
  while(fh.available()){
    *wifissid = fh.readStringUntil('\n');
    *wifipass = fh.readString();
  }
  fh.close();
  wifissid->trim();
  wifipass->trim();
}

bool sign(String hexDigest, byte *signature) {
  // 署名する
  if(hexDigest.length() != 64){ return false; } // 64文字のみ

  // hexDigestをバイト列にする
  byte digest[33] = {};
  for(int i = 0; i < 32; i++){ sscanf(&hexDigest[i*2], "%02x", &digest[i]); }

  // 署名を返す
  byte tmpSignature[64] = {};
  ECCX08.ecSign(SLOT, digest, tmpSignature);
  memcpy(signature, tmpSignature, 64);
  return true;
}

void handleSerial() {
  // シリアル通信の処理
  if(Serial.available()){
    String hexDigest = Serial.readStringUntil('\r');
    if(hexDigest == "PUBKEY"){
      // 公開鍵(PEM)を返す
      String publicKeyPem = ECCX08JWS.publicKey(SLOT, false);
      Serial.print(publicKeyPem);
    }else{
      // 署名をBase64エンコードして返す
      byte signature[64] = {};
      if(sign(hexDigest, signature)){
        // 署名に成功
        unsigned char b64signature[encode_base64_length(64)];
        encode_base64(signature, sizeof(signature), b64signature);
        Serial.write(b64signature, sizeof(b64signature));
      }
    }
    Serial.write('\r');
  }
}

void httpHandleRoot() {
  // 署名用URLの処理
  if(httpd.method() != HTTP_POST){
    httpd.send(200, "text/plain", "OK\n");
    return;
  }

  String hexDigest = httpd.arg("digest");
  byte signature[64] = {};
  if(sign(hexDigest, signature)){
    unsigned char b64signature[encode_base64_length(64)];
    encode_base64(signature, sizeof(signature), b64signature);
    httpd.send(200, "text/plain", (char*)b64signature);
  }else{
    httpd.send(400, "text/plain", "ERR\n");
  }
}

void httpHandlePublicKey() {
  // 公開鍵(PEM)を返す
  httpd.send(200, "text/plain", ECCX08JWS.publicKey(SLOT, false));
}

void startHttpd() {
  // WebServerを開始する
  M5.Display.print("* Start WebServer...");
  httpd.on("/", httpHandleRoot);
  httpd.on("/publickey", httpHandlePublicKey);
  httpd.begin();
  M5.Display.println("Success");
}
