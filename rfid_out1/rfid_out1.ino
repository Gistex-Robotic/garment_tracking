#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define BUZZER_PIN D5

const char* ssid = "Robot_Resource (Lokal)";
const char* pass = "robot@9876";
const char* broker = "10.5.0.106";

LiquidCrystal_I2C lcd(0x27, 16, 2);
WiFiClient EspClient;
PubSubClient client(EspClient);

String login, out = "";
String pout = "0";

unsigned long beepStart = 0;
bool isBeeping = false;
static bool loginMessageShown = false;

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
  String serialPart = tagHex.substring(4, 10);

  uint64_t tagDec = strtoull(serialPart.c_str(), NULL, 16);

  char buffer[20];
  snprintf(buffer, sizeof(buffer), "%010llu", (unsigned long long)tagDec);

  return String(buffer);
}

void callback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (int i = 0; i < length; i++) msg += (char)payload[i];

  if (strcmp(topic, "rfid/gt1/login") == 0) {
    login = (msg == "1") ? "Ready" : "Not";
  }
  else if (strcmp(topic, "rfid/gt1/output") == 0) {
    out = msg;
  }
}

unsigned long lastReconnectAttempt = 0;

void reconnect() {
  if (millis() - lastReconnectAttempt < 1000) return;
  lastReconnectAttempt = millis();

  String clientId = "EspClient";
  clientId += String(random(0xffff));

  beepNonBlocking(); 

  if (client.connect(clientId.c_str())) {
    client.subscribe("rfid/gt1/login");
    client.subscribe("rfid/gt1/output");
    client.publish("rfid/gt1/mac", WiFi.macAddress().c_str());

    lcd.clear();
    lcd.setCursor(2, 0);
    lcd.print("MQTT TERHUBUNG");
  } else {
    lcd.clear();
    lcd.setCursor(2, 0);
    lcd.print("GAGAL MENGHUBUNGKAN");
    lcd.setCursor(4, 1);
    lcd.print("KE MQTT");
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("MENGHUBUNGKAN");
  lcd.setCursor(0, 1);
  lcd.print("KE WIFI");

  WiFi.begin(ssid, pass);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long lastBeepWifi = 0;
    if (millis() - lastBeepWifi > 1000) {
      beepNonBlocking();
      lastBeepWifi = millis();
    }
    updateBeep();
    return;
  }
  static bool mqttInit = false;
  if (!mqttInit) {
    client.setServer(broker, 1883);
    client.setCallback(callback);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(" RFID READY ");
    mqttInit = true;
  }
  if (!client.connected()) {
    reconnect();
  } else {
    client.loop();
  }
  updateBeep();

  if (login != "Ready") {
    loginMessageShown= false;
  
    lcd.setCursor(0, 0);
    lcd.print("SILAHKAN LOGIN  ");
    lcd.setCursor(0, 1);
    lcd.print("TAP RFID        ");
  } else {
    if (!loginMessageShown){
    lcd.setCursor(0, 0);
    lcd.print("BERHASIL LOGIN! ");
    lcd.setCursor(0, 1);
    lcd.print("TAP RFID OUTPUT ");
    loginMessageShown = true;
    }
    if (out != pout) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("OUTPUT =");
      lcd.setCursor(0, 1);
      lcd.print(out);
      pout = out;
    }
  }

  String tag = readTag(Serial);
  if (tag.length() > 0) {
    beepNonBlocking();
    client.publish("rfid/gt1/tag", tag.c_str());
  }
}
