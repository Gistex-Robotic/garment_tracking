#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "esp_wifi.h"
// ================= PIN =================
#define BUZZER_PIN 33

// ================= WIFI & MQTT =================
const char* ssid = "Robot_Resource (Lokal)";
const char* pass = "robot@9876";
const char* broker = "10.5.2.222";

// IPAddress local_IP(10, 5, 2, 87);
// IPAddress subnet(10, 5, 2, 1);
// IPAddress gateway(255, 255, 0, 0);

// ================= OBJECT =================
LiquidCrystal_I2C lcd(0x27, 16, 2);
WiFiClient espClient;
PubSubClient client(espClient);

// ================= UART2 RDM6300 =================
HardwareSerial RFID(2);  // RX=16, TX=17

// ================= VARIABLES =================
String login = "";
String out = "";
String pout = "";
String ip;
String resp = "";
String nama = "";
String line = "";

unsigned long beepStart = 0;
bool isBeeping = false;
bool response = false;
unsigned long lastHeartbeat = 0;
const unsigned long heartbeatInterval = 5000;
bool taglock = false;
static bool loginMessageShown = false;
unsigned long lastReconnectAttempt = 0;
bool mainscreen = false;
unsigned long mainscreentime = 0;
bool sukses = false;
bool publish = false;

// ================= BUZZER =================
void beepNonBlocking(unsigned long dur = 500) {
  digitalWrite(BUZZER_PIN, HIGH);
  beepStart = millis();
  isBeeping = true;
}

void updateBeep() {
  if (isBeeping && millis() - beepStart >= 500) {
    digitalWrite(BUZZER_PIN, LOW);
    isBeeping = false;
  }
}

// ================= RDM6300 READER =================
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
  String serialHex = tagHex.substring(2, 10);

  uint64_t tagDec = strtoull(serialHex.c_str(), NULL, 16);

  char buffer[20];
  snprintf(buffer, sizeof(buffer), "%010llu", (unsigned long long)tagDec);
  return String(buffer);
}

// ================= MQTT CALLBACK =================
void callback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  if (strcmp(topic, "rfid/gt12/login") == 0) {
    login = (msg == "1") ? "Ready" : "Not";
  } else if (strcmp(topic, "rfid/gt12/output") == 0) {
    out = msg;
  } else if (strcmp(topic, "rfid/gt12/response") == 0) {
    resp = msg;
    response = true;
  } else if (strcmp(topic, "rfid/gt12/line") == 0) {
    line = msg;
  } else if (strcmp(topic, "rfid/gt12/nama") == 0) {
    nama = msg;
  }
}

// ================= MQTT RECONNECT =================
void reconnect() {
  if (millis() - lastReconnectAttempt < 1000) return;
  lastReconnectAttempt = millis();

  String clientId = "ESP32_";
  clientId += String(random(0xffff), HEX);

  beepNonBlocking();

  if (client.connect(clientId.c_str(),
                     "rfid/gt12/status", 0, true, "0")) {

    ip = WiFi.localIP().toString();

    client.subscribe("rfid/gt12/login");
    client.subscribe("rfid/gt12/output");
    client.subscribe("rfid/gt12/response");
    client.subscribe("rfid/gt12/line");
    client.subscribe("rfid/gt12/nama");
    client.publish("rfid/gt12/status", "1", true);
    client.publish("rfid/gt12/ip", ip.c_str(), true);
    pout = out;
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("MQTT GAGAL");
    lcd.setCursor(0, 1);
    lcd.print("COBA LAGI...");
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  RFID.begin(9600, SERIAL_8N1, 16, 17);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("MENGHUBUNGKAN KE");
  lcd.setCursor(0, 1);
  lcd.print(ssid);
  // WiFi.config(local_IP, subnet, gateway);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  WiFi.setSleep(false);
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B);
  esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
}
// ================= LOOP =================
void loop() {

  // ===== WIFI CHECK =====
  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long lastBeepWifi = 0;
    if (millis() - lastBeepWifi > 1000) {
      beepNonBlocking();
      lastBeepWifi = millis();
    }
    updateBeep();
    return;
  }

  // ===== MQTT INIT =====
  static bool mqttInit = false;
  if (!mqttInit) {
    client.setServer(broker, 1883);
    client.setCallback(callback);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("RFID READY");

    mqttInit = true;
  }

  // ===== MQTT CONNECTION =====
  if (!client.connected()) {
    reconnect();
  } else {
    client.loop();
  }

  updateBeep();

  // ===== HEARTBEAT =====
  if (client.connected() && millis() - lastHeartbeat >= heartbeatInterval) {

    lastHeartbeat = millis();
    client.publish("rfid/gt12/status", "1", true);
  }

  // ===== READ RFID =====
  String tag = readTag(RFID);

  if (tag.length() > 0 && !taglock) {
    taglock = true;
    client.publish("rfid/gt12/tag", tag.c_str());
    unsigned long startWait = millis();
    response = false;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("MEMBACA...");

    while (!response && millis() - startWait < 5000) {
      client.loop();
      updateBeep();
    }

    lcd.clear();

    if (response && !sukses) {
      lcd.setCursor(0, 0);
      lcd.print(resp.substring(0, 16));
      lcd.setCursor(0, 1);
      lcd.print(resp.substring(16));
    }
    if (!response) {
      lcd.setCursor(0, 0);
      lcd.print("TIMEOUT !");
    }
    mainscreen = true;
    mainscreentime = millis();
  }

  if (!RFID.available()) {
    taglock = false;
  }

  // ===== DISPLAY LOGIN =====
  if (login != "Ready") {
    loginMessageShown = false;
    lcd.setCursor(0, 0);
    lcd.print("SILAHKAN LOGIN ");
    lcd.setCursor(0, 1);
    lcd.print("TAP RFID      ");
  } else {
    if (!loginMessageShown) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("LOGIN BERHASIL");
      lcd.setCursor(0, 1);
      lcd.print("TAP RFID OUT ");
      loginMessageShown = true;
      beepNonBlocking();
      mainscreen = true;
      mainscreentime = millis();
    }
    if (out != pout) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("SUKSES");
      pout = out;
      beepNonBlocking();
      mainscreen = true;
      mainscreentime = millis();
      sukses = true;
    }
    if (mainscreen && millis() - mainscreentime >= 2000) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("LINE ");
      lcd.print(line);
      lcd.print("|");
      lcd.print(nama);
      lcd.setCursor(0, 1);
      lcd.print("OUTPUT:");
      lcd.print(out);
      mainscreen = false;
      sukses = false;
    }
  }
}
