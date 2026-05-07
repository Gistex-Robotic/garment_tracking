#include <HardwareSerial.h>
#include <SoftwareSerial.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include <PubSubClient.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

const char* ssid   ="Tracked";
const char* pass   ="Factory@RFID";
const char* broker ="10.5.2.222";

IPAddress local_IP(10,5,2,81);
IPAddress gateway(10,5,2,1);
IPAddress subnet(255,255,0,0);

HardwareSerial rfidA(1);
HardwareSerial rfidB(2);
SoftwareSerial rfidC(4, 5);  

LiquidCrystal_I2C lcd(0x27, 16,2);
WiFiClient EspClient;
PubSubClient client(EspClient);

bool debugMode = true;
unsigned long buzzerOnTime = 0;
const int buzzerPin = 25;


void debugPrint(const String& msg) {
  if (debugMode) Serial.println(msg);
}

void reconnectMQTT() {
  while (!client.connected()) {
    String clientId = "EspClient-" + String(random(0xffff), HEX);

    digitalWrite(buzzerPin, HIGH);
    delay(500);
    digitalWrite(buzzerPin, LOW);
    delay(500);

    if (client.connect(clientId.c_str())) {
      debugPrint("MQTT terhubung");
    } else {
      delay(2000);
    }
  }
}

void reconnectWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect();
    WiFi.begin(ssid, pass);
    digitalWrite(buzzerPin, HIGH);
    delay(300);
    digitalWrite(buzzerPin, LOW);
    delay(300);
  }
}

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

void setup() {
  if (debugMode) {
    Serial.begin(115200);
    debugPrint("Mode debug aktif");
  }

  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("MENGHUBUNGKAN");
  lcd.setCursor(0,1);
  lcd.print("KE WIFI...");

  WiFi.config(local_IP,gateway,subnet);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  WiFi.setSleep(false);
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B);
  esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(buzzerPin, HIGH);
    delay(300);
    digitalWrite(buzzerPin, LOW);
    delay(300);
  }

  client.setServer(broker, 1883);

  rfidA.begin(9600, SERIAL_8N1, 16, 17);
  rfidB.begin(9600, SERIAL_8N1, 18, 19);
  rfidC.begin(9600);
  lcd.setCursor(0,0);
  lcd.print("RFID READY");
}

void loop() {
  reconnectWiFi();
  if (!client.connected()) reconnectMQTT();
  client.loop();

  bool tagDetected = false;

  if (rfidA.available()) {
    String tag = readTag(rfidA);
    if (tag.length()) {
      client.publish("rfid/qc/rework", tag.c_str());
      debugPrint("[RDM1] " + tag);
      tagDetected = true;
    }
  }

  if (rfidB.available()) {
    String tag = readTag(rfidB);
    if (tag.length()) {
      client.publish("rfid/qc/reject", tag.c_str());
      debugPrint("[RDM2] " + tag);
      tagDetected = true;
    }
  }

  if (rfidC.available()) {
    String tag = readTag(rfidC);
    if (tag.length()) {
      client.publish("rfid/qc/good", tag.c_str());
      debugPrint("[RDM3] " + tag);
      tagDetected = true;
    }
  }

  if (tagDetected) {
    digitalWrite(buzzerPin, HIGH);
    buzzerOnTime = millis();
  }

  if (millis() - buzzerOnTime > 500) {
    digitalWrite(buzzerPin, LOW);
  }

  delay(10);
}