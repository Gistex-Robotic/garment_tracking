#include <HardwareSerial.h>
#include <SoftwareSerial.h>
#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid = "GGI_RFID";
const char* pass = "Factory@RFID";
const char* broker = "10.5.0.106";

IPAddress localIP(0,0,0,0);
IPAddress subnet(0,0,0,0);
IPAddress gateway(0,0,0,0);

HardwareSerial rfidA(1);
HardwareSerial rfidB(2);
SoftwareSerial rfidC(4, 5);
String login = "";
WiFiClient EspClient;
PubSubClient client(EspClient);

bool debugMode = false;
unsigned long buzzerOnTime = 0;
const int buzzerPin = 25;


void debugPrint(const String& msg) {
  if (debugMode) Serial.println(msg);
}
void callback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  if (msg == "1") {
    login = "Ready";
    digitalWrite(buzzerPin, HIGH);
    delay(2000);
  } else {
    login = "Not";
  }
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
      client.subscribe("rfid/qc3/login");
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
  String serialPart = tagHex.substring(4, 10);

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

  WiFi.config(local_IP,gateway,subnet);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(buzzerPin, HIGH);
    delay(300);
    digitalWrite(buzzerPin, LOW);
    delay(300);
  }

  client.setServer(broker, 1883);
  client.setCallback(callback);

  rfidA.begin(9600, SERIAL_8N1, 16, 17);
  rfidB.begin(9600, SERIAL_8N1, 18, 19);
  rfidC.begin(9600);
}

void loop() {
  reconnectWiFi();
  if (!client.connected()) reconnectMQTT();
  client.loop();

  bool tagDetected = false;

  if (rfidA.available()) {
    String tag = readTag(rfidA);
    if (tag.length()) {
      client.publish("rfid/qc3/rework", tag.c_str());
      debugPrint("[RDM1] " + tag);
      tagDetected = true;
    }
  }

  if (rfidB.available()) {
    String tag = readTag(rfidB);
    if (tag.length()) {
      client.publish("rfid/qc3/reject", tag.c_str());
      debugPrint("[RDM2] " + tag);
      tagDetected = true;
    }
  }

  if (rfidC.available()) {
    String tag = readTag(rfidC);
    if (tag.length()>0) {
      client.publish("rfid/qc3/good", tag.c_str());
      debugPrint("[RDM3] " + tag);
      tagDetected = true;
    }
  }
  if (login == "Ready") {
    if (tagDetected) {
      digitalWrite(buzzerPin, HIGH);
      buzzerOnTime = millis();
    }
  } else{
    if (tagDetected){
      digitalWrite(buzzerPin, HIGH);
      delay(300);
      digitalWrite(buzzerPin, LOW);
      buzzerOnTime = millis();
    }
  }
  if (millis() - buzzerOnTime > 300) {
    digitalWrite(buzzerPin, LOW);
  }

  delay(10);
}