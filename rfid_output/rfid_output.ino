#include <HardwareSerial.h>
#include <SoftwareSerial.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "esp_wifi.h"
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

// ================= WIFI & MQTT =================
const char* ssid = "Robot_Resource (Lokal)";
const char* pass = "robot@9876";
const char* broker = "10.5.0.107";

// ================= RFID =================
HardwareSerial rfidA(1);
HardwareSerial rfidB(2);
SoftwareSerial rfidC(4, 5);

// ======== LCD ==========
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ================= MQTT =================
WiFiClient EspClient;
PubSubClient client(EspClient);

// ================= BUZZER =================
const int buzzerPin = 25;

// ===== BUZZER STATE MACHINE =====
unsigned long buzzerMillis = 0;
unsigned long buzzerInterval = 100;
int buzzerStep = 0;
int buzzerRepeat = 0;
bool buzzerRunning = false;
bool buzzerState = false;

void buzzerBeep(int repeat, unsigned long onTime, unsigned long offTime) {
  buzzerRepeat = repeat * 2;
  buzzerInterval = onTime;
  buzzerMillis = millis();
  buzzerStep = 0;
  buzzerRunning = true;
  buzzerState = true;

  digitalWrite(buzzerPin, HIGH);
}

void buzzerUpdate() {
  if (!buzzerRunning) return;

  if (millis() - buzzerMillis >= buzzerInterval) {

    buzzerMillis = millis();
    buzzerState = !buzzerState;

    digitalWrite(buzzerPin, buzzerState ? HIGH : LOW);
    buzzerStep++;

    if (buzzerStep >= buzzerRepeat) {
      buzzerRunning = false;
      digitalWrite(buzzerPin, LOW);
      return;
    }

    buzzerInterval = buzzerState ? 100 : 100;
  }
}

// ================= Flag =================
bool debugMode = false;
bool mainscreen = false;
bool response = false;
bool sukses = false;
bool loginMessage = true;

// ================= TAG LOCK =================
bool lockA = false;
bool lockB = false;
bool lockC = false;

// ================= DATA =================
String login = "";
String resp = "";
String line = "";
String nama = "";
unsigned long mainscreentime = 0;

// ================= DEBUG =================
void debugPrint(const String& msg) {
  if (debugMode) Serial.println(msg);
}

// ================= MQTT CALLBACK =================
void callback(char* topic, byte* payload, unsigned int length) {

  String msg = "";
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  if (strcmp(topic, "rfid/qc1/login") == 0) {
    login = (msg == "1") ? "Ready" : "Not";
  }

  if (strcmp(topic, "rfid/qc1/response") == 0) {
    resp = msg;
    response = true;
    debugPrint(resp);
  }

  if (strcmp(topic, "rfid/qc1/line") == 0) {
    line = msg;
  }

  if (strcmp(topic, "rfid/qc1/nama") == 0) {
    nama = msg;
  }
}

// ================= MQTT RECONNECT =================
void reconnectMQTT() {

  while (!client.connected()) {

    String clientId = "EspClient-" + String(random(0xffff), HEX);

    buzzerBeep(1, 100, 100);

    if (client.connect(clientId.c_str())) {
      client.subscribe("rfid/qc1/line");
      client.subscribe("rfid/qc1/nama");
      client.subscribe("rfid/qc1/login");
      client.subscribe("rfid/qc1/response");
    } else {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("MQTT GAGAL");
      lcd.setCursor(0, 1);
      lcd.print("COBA LAGI...");
    }
  }
}

// ================= WIFI RECONNECT =================
void reconnectWiFi() {

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect();
    WiFi.begin(ssid, pass);

    buzzerBeep(1, 100, 100);
  }
}

// ================= RFID PARSER =================
String readTag(Stream& serial) {

  if (!serial.available()) return "";
  if (serial.read() != 0x02) return "";

  String hexData = "";
  unsigned long start = millis();

  while (millis() - start < 100) {
    if (serial.available()) {
      char c = serial.read();
      if (c == 0x03) break;
      hexData += c;
    }
  }

  if (hexData.length() < 12) return "";

  String tagHex = hexData.substring(0, 10);
  String serialPart = tagHex.substring(2, 10);

  uint64_t tagDec = strtoull(serialPart.c_str(), NULL, 16);

  char buffer[20];
  snprintf(buffer, sizeof(buffer), "%010llu", (unsigned long long)tagDec);

  return String(buffer);
}

// ================= WAIT RESPONSE =================
void waitResponse() {

  unsigned long wait = millis();
  response = false;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("MEMBACA...");

  while (!response && millis() - wait < 5000) {
    client.loop();
    buzzerUpdate();
  }

  lcd.clear();

  if (response) {
    lcd.setCursor(0, 0);
    lcd.print(resp.substring(0, 16));
    lcd.setCursor(0, 1);
    lcd.print(resp.substring(16));

    buzzerBeep(2, 100, 100); 
  } else {
    lcd.setCursor(0, 0);
    lcd.print("TIMEOUT !");

    buzzerBeep(1, 500, 200); 
  }

  mainscreen = true;
  mainscreentime = millis();
}

// ================= SETUP =================
void setup() {

  if (debugMode) Serial.begin(115200);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("BOOTING QC1");
  delay(1000);

  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  WiFi.setSleep(false);

  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B);
  esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("MENGHUBUNGKAN KE");
  lcd.setCursor(0, 1);
  lcd.print(ssid);

  while (WiFi.status() != WL_CONNECTED) {
    buzzerBeep(1, 100, 100);
    delay(300);
  }

  client.setServer(broker, 1883);
  client.setCallback(callback);

  rfidA.begin(9600, SERIAL_8N1, 16, 17);
  rfidB.begin(9600, SERIAL_8N1, 18, 19);
  rfidC.begin(9600);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("TERHUBUNG KE");
  lcd.setCursor(0, 1);
  lcd.print(ssid);
}

// ================= LOOP =================
void loop() {

  reconnectWiFi();

  if (!client.connected()) {
    reconnectMQTT();
  }

  client.loop();
  buzzerUpdate();

  bool tagDetected = false;

  // ===== RFID A =====
  if (rfidA.available()) {
    String tag = readTag(rfidA);

    if (tag.length() > 0 && tag != "0000000000") {
      tagDetected = true;

      if (!lockA) {
        client.publish("rfid/qc1/rework", tag.c_str());
        lockA = true;
        waitResponse();
      }
    }
  } else lockA = false;

  // ===== RFID B =====
  if (rfidB.available()) {
    String tag = readTag(rfidB);

    if (tag.length() > 0 && tag != "0000000000") {
      tagDetected = true;

      if (!lockB) {
        client.publish("rfid/qc1/reject", tag.c_str());
        lockB = true;
        waitResponse();
      }
    }
  } else lockB = false;

  // ===== RFID C =====
  if (rfidC.available()) {
    String tag = readTag(rfidC);

    if (tag.length() > 0 && tag != "0000000000") {
      tagDetected = true;

      if (!lockC) {
        client.publish("rfid/qc1/good", tag.c_str());
        lockC = true;
        waitResponse();
      }
    }
  } else lockC = false;

  // ===== UI =====
  if (login == "Ready") {

    if (!loginMessage) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("LOGIN BERHASIL");
      lcd.setCursor(0, 1);
      lcd.print("TAP RFID OUT");
      loginMessage = true;
      mainscreen = true;
      mainscreentime = millis();
    }

  } else if (login != "Ready"){

    loginMessage = false;
    lcd.setCursor(0, 0);
    lcd.print("SILAHKAN LOGIN ");
    lcd.setCursor(0, 1);
    lcd.print("TAP RFID       ");

    if (tagDetected) {
      buzzerBeep(1, 50, 50); 
    }
  }

  if (mainscreen && millis() - mainscreentime >= 2000) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("QC LINE ");
    lcd.print(line);
    lcd.setCursor(0, 1);
    lcd.print(nama);
    mainscreen = false;
  }

  delay(10);
}