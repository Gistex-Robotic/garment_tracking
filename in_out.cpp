#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "esp_wifi.h"
#include "esp_bt.h"

#if __has_include(<esp_arduino_version.h>)
#include <esp_arduino_version.h>
#endif

// =====================================================
// WIFI
// =====================================================
const char* ssid = "Robot_Resource (Lokal)";
const char* password = "robot@9876";

// =====================================================
// MQTT
// =====================================================
const char* mqtt_server = "10.5.0.106";
const int mqtt_port = 1883;

// =====================================================
// SET IDENTITAS ALAT DI SINI
// Pilih salah satu:
// String scanType = "in";   // alat IN
// String scanType = "out";  // alat OUT
// =====================================================
String scanType = "out";

// =====================================================
// RFID UART
// =====================================================
#define RFID_RX_PIN 16
#define RFID_TX_PIN 17

// =====================================================
// PASSIVE BUZZER
// =====================================================
#define BUZZER_PIN 25
#define BUZZER_CHANNEL 0
#define BUZZER_FREQ 2000
#define BUZZER_RESOLUTION 8

// =====================================================
// BUTTON OPTIONAL
// GPIO15 -> BUTTON -> GND
// Jangan ditekan saat ESP booting.
// =====================================================
#define BUTTON_PIN 15

// =====================================================
// LCD
// =====================================================
LiquidCrystal_I2C lcd(0x27, 16, 2);

// =====================================================
// OBJECT
// =====================================================
WiFiClient espClient;
PubSubClient client(espClient);
HardwareSerial RFID(2);

// =====================================================
// DEVICE & TOPIC
// =====================================================
String deviceId;

String topicTag;
String topicStatus;
String topicReply;

// =====================================================
// DATA TAMPILAN READY
// Row 1: OUTPUT | NAMA
// Row 2: COUNT BUNDLE | LINE.NO | IN/OUT
// Contoh:
// 150 | RANGGA
// C12 | L3 | OUT
// =====================================================
String operatorName = "NO LOGIN";
int actualOutput = 0;
int countBundle = 0;
int lineNo = 0;

// =====================================================
// RFID DEBOUNCE
// =====================================================
String lastTag = "";
unsigned long lastReadTime = 0;
const unsigned long debounceTime = 1500;

bool rfidProcessing = false;
unsigned long rfidProcessingStart = 0;
const unsigned long rfidProcessingTimeout = 7000;

// =====================================================
// RESTORE SESSION AFTER RESTART
// =====================================================
bool restoreRequested = false;

// =====================================================
// RETURN SCREEN
// =====================================================
bool returnToReady = false;
unsigned long returnReadyTime = 0;

// =====================================================
// HEARTBEAT
// =====================================================
unsigned long lastHeartbeat = 0;
const unsigned long heartbeatInterval = 5000;

// =====================================================
// BUTTON
// =====================================================
bool lastButtonReading = HIGH;
bool buttonState = HIGH;
unsigned long lastButtonDebounceTime = 0;
const unsigned long buttonDebounceDelay = 50;

bool buttonMessageActive = false;
unsigned long buttonMessageStart = 0;
const unsigned long buttonMessageDuration = 3000;

// =====================================================
// DEBUG
// =====================================================
bool debugMode = true;

void debugPrint(String msg)
{
  if (debugMode)
  {
    Serial.println(msg);
  }
}

// =====================================================
// WIRELESS OPTIMIZATION
// =====================================================
void optimizeWireless()
{
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  esp_wifi_set_ps(WIFI_PS_NONE);

  // Bluetooth tidak dipakai, matikan agar radio lebih fokus ke WiFi.
  btStop();
}

// =====================================================
// BUZZER COMPATIBLE ARDUINO ESP32 CORE 2.x / 3.x
// =====================================================
void buzzerBegin()
{
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(BUZZER_PIN, BUZZER_FREQ, BUZZER_RESOLUTION);
  ledcWriteTone(BUZZER_PIN, 0);
#else
  ledcSetup(BUZZER_CHANNEL, BUZZER_FREQ, BUZZER_RESOLUTION);
  ledcAttachPin(BUZZER_PIN, BUZZER_CHANNEL);
  ledcWriteTone(BUZZER_CHANNEL, 0);
#endif
}

void buzzerTone(int frequency)
{
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWriteTone(BUZZER_PIN, frequency);
#else
  ledcWriteTone(BUZZER_CHANNEL, frequency);
#endif
}

void buzzerOff()
{
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWriteTone(BUZZER_PIN, 0);
#else
  ledcWriteTone(BUZZER_CHANNEL, 0);
#endif
}

void beep(int duration = 150, int frequency = BUZZER_FREQ)
{
  if (duration <= 0)
    return;

  buzzerTone(frequency);
  delay(duration);
  buzzerOff();
}

void beepCode(int code)
{
  if (code <= 0)
    return;

  if (code == 1)
  {
    beep(120, 2200);
  }
  else if (code == 2)
  {
    beep(120, 2200);
    delay(80);
    beep(120, 2200);
  }
  else
  {
    beep(450, 900);
  }
}

// =====================================================
// LCD HELPER
// =====================================================
void lcdPrint16(int col, int row, String text)
{
  lcd.setCursor(col, row);

  if (text.length() > 16)
  {
    lcd.print(text.substring(0, 16));
  }
  else
  {
    lcd.print(text);

    for (int i = text.length(); i < 16; i++)
    {
      lcd.print(" ");
    }
  }
}

void showMessage(String row1, String row2, int beepValue = 0)
{
  lcd.clear();
  lcdPrint16(0, 0, row1);
  lcdPrint16(0, 1, row2);
  beepCode(beepValue);
}

void scheduleReady(unsigned long delayMs)
{
  returnToReady = true;
  returnReadyTime = millis() + delayMs;
}

bool validScanType()
{
  return scanType == "in" || scanType == "out";
}

String scanTypeUpper()
{
  String s = scanType;
  s.toUpperCase();
  return s;
}

String shortOperatorName(String name, int maxLen = 9)
{
  name.trim();
  name.toUpperCase();

  if (name.length() == 0)
  {
    return "NO LOGIN";
  }

  if (name.length() > maxLen)
  {
    return name.substring(0, maxLen);
  }

  return name;
}

// =====================================================
// READY SCREEN
// Row 1: OUTPUT | NAMA
// Row 2: COUNT BUNDLE | LINE.NO | IN/OUT
// =====================================================
void showReady()
{
  if (buttonMessageActive)
    return;

  lcd.clear();

  String row1 = "";
  String row2 = "";

  if (validScanType())
  {
    row1 =
      String(actualOutput) +
      " | " +
      shortOperatorName(operatorName, 9);

    row2 =
      "C" +
      String(countBundle) +
      " | L" +
      String(lineNo) +
      " | " +
      scanTypeUpper();
  }
  else
  {
    row1 = "SCAN TYPE ERR";
    row2 = "SET IN/OUT";
  }

  lcdPrint16(0, 0, row1);
  lcdPrint16(0, 1, row2);
}

// =====================================================
// SIMPLE JSON PARSER
// Tidak perlu ArduinoJson.
// =====================================================
String jsonGetString(String json, String key)
{
  String pattern = "\"" + key + "\"";

  int p = json.indexOf(pattern);
  if (p < 0)
    return "";

  p = json.indexOf(':', p + pattern.length());
  if (p < 0)
    return "";

  p++;

  while (p < (int)json.length() && (json[p] == ' ' || json[p] == '\t'))
  {
    p++;
  }

  if (p >= (int)json.length())
    return "";

  if (json[p] == '"')
  {
    p++;

    String value = "";
    bool escape = false;

    while (p < (int)json.length())
    {
      char c = json[p++];

      if (escape)
      {
        value += c;
        escape = false;
      }
      else if (c == '\\')
      {
        escape = true;
      }
      else if (c == '"')
      {
        break;
      }
      else
      {
        value += c;
      }
    }

    return value;
  }

  int end = p;

  while (
    end < (int)json.length() &&
    json[end] != ',' &&
    json[end] != '}')
  {
    end++;
  }

  String value = json.substring(p, end);
  value.trim();
  value.replace("\"", "");

  return value;
}

int jsonGetInt(String json, String key, int defaultValue = 0)
{
  String value = jsonGetString(json, key);
  value.trim();

  if (value.length() == 0)
    return defaultValue;

  return value.toInt();
}

// =====================================================
// UPDATE DATA READY DARI JSON REPLY
// =====================================================
void updateReadyDataFromJson(String json)
{
  String action = jsonGetString(json, "action");

  String jsonOperatorName = jsonGetString(json, "operator_name");
  String jsonNama = jsonGetString(json, "nama");

  if (jsonOperatorName.length() > 0)
  {
    operatorName = jsonOperatorName;
  }
  else if (jsonNama.length() > 0)
  {
    operatorName = jsonNama;
  }

  // COUNT BUNDLE
  int jsonCountBundle = jsonGetInt(json, "count_bundle", -1);
  int jsonBundleCount = jsonGetInt(json, "bundle_count", -1);
  int jsonTotalBundle = jsonGetInt(json, "total_bundle", -1);

  if (jsonCountBundle >= 0)
  {
    countBundle = jsonCountBundle;
  }
  else if (jsonBundleCount >= 0)
  {
    countBundle = jsonBundleCount;
  }
  else if (jsonTotalBundle >= 0)
  {
    countBundle = jsonTotalBundle;
  }

  // LINE
  int jsonLineNo = jsonGetInt(json, "line_no", -1);
  int jsonLine = jsonGetInt(json, "line", -1);

  if (jsonLineNo >= 0)
  {
    lineNo = jsonLineNo;
  }
  else if (jsonLine >= 0)
  {
    lineNo = jsonLine;
  }

  // Actual output.
  int totalOutputQty = jsonGetInt(json, "total_output_qty", -1);
  int operatorOutput = jsonGetInt(json, "operator_output", -1);
  int actualOutputJson = jsonGetInt(json, "actual_output", -1);
  int totalOutput = jsonGetInt(json, "total_output", -1);
  int qtyJson = jsonGetInt(json, "qty", -1);

  if (totalOutputQty >= 0)
  {
    actualOutput = totalOutputQty;
  }
  else if (operatorOutput >= 0)
  {
    actualOutput = operatorOutput;
  }
  else if (actualOutputJson >= 0)
  {
    actualOutput = actualOutputJson;
  }
  else if (totalOutput >= 0)
  {
    actualOutput = totalOutput;
  }
  else if (qtyJson >= 0)
  {
    actualOutput = qtyJson;
  }

  // Identitas alat tetap mengikuti program ESP.
  // scanType TIDAK ditimpa dari JSON.

  if (action == "logout" || action == "restore_empty")
  {
    operatorName = "NO LOGIN";
    actualOutput = 0;
    countBundle = 0;
    lineNo = 0;
  }
}

// =====================================================
// REQUEST RESTORE SESSION
// =====================================================
void requestRestoreSession()
{
  if (!validScanType())
  {
    showMessage("SCAN TYPE ERR", "SET IN/OUT", 3);
    return;
  }

  if (!client.connected())
  {
    return;
  }

  if (restoreRequested)
  {
    return;
  }

  restoreRequested = true;

  rfidProcessing = true;
  rfidProcessingStart = millis();

  String payload =
    "__RESTORE__" +
    String("/") +
    deviceId +
    String("/") +
    scanType;

  debugPrint("Restore session request: " + payload);

  client.publish(
    topicTag.c_str(),
    payload.c_str()
  );

  showMessage("SYNC SESSION", "PLEASE WAIT", 0);
}

// =====================================================
// RFID CONTROL
// =====================================================
bool allowRFID(String tag)
{
  unsigned long now = millis();

  if (
    rfidProcessing &&
    now - rfidProcessingStart < rfidProcessingTimeout)
  {
    debugPrint("RFID ignored, waiting response: " + tag);
    return false;
  }

  if (
    tag == lastTag &&
    now - lastReadTime < debounceTime)
  {
    lastReadTime = now;
    debugPrint("RFID duplicate ignored: " + tag);
    return false;
  }

  lastTag = tag;
  lastReadTime = now;

  rfidProcessing = true;
  rfidProcessingStart = now;

  return true;
}

void releaseRFIDProcessing()
{
  rfidProcessing = false;
}

// =====================================================
// BUTTON
// =====================================================
void handleButton()
{
  bool reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonReading)
  {
    lastButtonDebounceTime = millis();
  }

  if ((millis() - lastButtonDebounceTime) > buttonDebounceDelay)
  {
    if (reading != buttonState)
    {
      buttonState = reading;

      if (buttonState == LOW)
      {
        buttonMessageActive = true;
        buttonMessageStart = millis();

        showMessage("BELUM DIPROGRAM", "COMING SOON", 2);
      }
    }
  }

  lastButtonReading = reading;

  if (
    buttonMessageActive &&
    millis() - buttonMessageStart >= buttonMessageDuration)
  {
    buttonMessageActive = false;
    showReady();
  }
}

// =====================================================
// MQTT CALLBACK
// =====================================================
void callback(char* topic, byte* payload, unsigned int length)
{
  String msg = "";

  for (unsigned int i = 0; i < length; i++)
  {
    msg += (char)payload[i];
  }

  msg.trim();

  String topicStr = String(topic);

  debugPrint("RX : " + topicStr + " = " + msg);

  if (topicStr == topicReply)
  {
    if (!rfidProcessing)
    {
      debugPrint("Reply ignored, not waiting response: " + msg);
      return;
    }

    releaseRFIDProcessing();
    restoreRequested = false;

    updateReadyDataFromJson(msg);

    String lcd1 = jsonGetString(msg, "lcd1");
    String lcd2 = jsonGetString(msg, "lcd2");
    int beepValue = jsonGetInt(msg, "beep", 1);

    if (lcd1.length() == 0)
    {
      lcd1 = "RESP ERROR";
      lcd2 = "CHECK API";
      beepValue = 3;
    }

    showMessage(lcd1, lcd2, beepValue);
    scheduleReady(2500);

    return;
  }
}

// =====================================================
// MQTT RECONNECT
// =====================================================
void reconnectMQTT()
{
  while (!client.connected())
  {
    debugPrint("Connecting MQTT...");

    String clientId = "ESP32-" + deviceId;

    lcd.clear();
    lcdPrint16(0, 0, "MQTT CONNECT");
    lcdPrint16(0, 1, mqtt_server);

    if (
      client.connect(
        clientId.c_str(),
        topicStatus.c_str(),
        0,
        true,
        "offline"))
    {
      debugPrint("MQTT Connected");

      client.subscribe(topicReply.c_str());

      client.publish(
        topicStatus.c_str(),
        "online",
        true);

      lcd.clear();
      lcdPrint16(0, 0, "MQTT SUCCESS");
      lcdPrint16(0, 1, deviceId);

      delay(600);

      unsigned long flushStart = millis();

      while (millis() - flushStart < 800)
      {
        client.loop();
        delay(10);
      }

      showReady();

      requestRestoreSession();
    }
    else
    {
      debugPrint("MQTT Failed");

      lcd.clear();
      lcdPrint16(0, 0, "MQTT FAILED");
      lcdPrint16(0, 1, "RETRY...");

      delay(2000);
    }
  }
}

// =====================================================
// RFID READER
// =====================================================
String readTag(Stream& serial)
{
  if (!serial.available())
    return "";

  if (serial.read() != 0x02)
    return "";

  String hexData = "";

  unsigned long start = millis();

  while (millis() - start < 100)
  {
    if (serial.available())
    {
      char c = serial.read();

      if (c == 0x03)
        break;

      hexData += c;
    }
  }

  if (hexData.length() < 12)
    return "";

  String tagHex = hexData.substring(0, 10);
  String serialHex = tagHex.substring(2, 10);

  uint64_t tagDec =
    strtoull(
      serialHex.c_str(),
      NULL,
      16);

  char buffer[20];

  snprintf(
    buffer,
    sizeof(buffer),
    "%010llu",
    (unsigned long long)tagDec);

  return String(buffer);
}

// =====================================================
// WIFI
// =====================================================
void connectWiFi()
{
  lcd.clear();
  lcdPrint16(0, 0, "CONNECT WIFI");
  lcdPrint16(0, 1, ssid);

  optimizeWireless();

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  lcd.clear();
  lcdPrint16(0, 0, "WIFI CONNECTED");
  lcdPrint16(0, 1, WiFi.localIP().toString());

  delay(1000);
}

// =====================================================
// SETUP
// =====================================================
void setup()
{
  Serial.begin(115200);

  scanType.toLowerCase();
  scanType.trim();

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  buzzerBegin();

  RFID.begin(
    9600,
    SERIAL_8N1,
    RFID_RX_PIN,
    RFID_TX_PIN);

  lcd.init();
  lcd.backlight();

  showMessage("SYSTEM BOOT", "PLEASE WAIT", 1);

  if (!validScanType())
  {
    showMessage("SCAN TYPE ERR", "SET IN/OUT", 3);
    delay(3000);
  }

  connectWiFi();

  deviceId = WiFi.macAddress();
  deviceId.replace(":", "");
  deviceId.toUpperCase();

  debugPrint("Device ID : " + deviceId);
  debugPrint("Scan Type : " + scanType);

  topicTag =
    "rfid/batch/" +
    deviceId +
    "/tag";

  topicStatus =
    "rfid/batch/" +
    deviceId +
    "/status";

  topicReply =
    "rfid/batch/reply/" +
    deviceId;

  debugPrint("topicTag    : " + topicTag);
  debugPrint("topicStatus : " + topicStatus);
  debugPrint("topicReply  : " + topicReply);

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  client.setBufferSize(1024);

  reconnectMQTT();
}

// =====================================================
// LOOP
// =====================================================
void loop()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    showMessage("WIFI LOST", "RECONNECT...", 3);

    WiFi.reconnect();

    delay(1000);
    return;
  }

  if (!client.connected())
  {
    rfidProcessing = false;
    restoreRequested = false;
    reconnectMQTT();
  }

  client.loop();

  handleButton();

  if (buttonMessageActive)
    return;

  if (
    rfidProcessing &&
    millis() - rfidProcessingStart >= rfidProcessingTimeout)
  {
    rfidProcessing = false;
    restoreRequested = false;

    debugPrint("RFID processing timeout released");
    showMessage("TIMEOUT", "SCAN ULANG", 3);
    scheduleReady(2000);
  }

  if (returnToReady && millis() >= returnReadyTime)
  {
    returnToReady = false;
    showReady();
  }

  if (millis() - lastHeartbeat >= heartbeatInterval)
  {
    lastHeartbeat = millis();

    client.publish(
      topicStatus.c_str(),
      "online",
      true);
  }

  String tag = readTag(RFID);

  if (tag.length() > 0)
  {
    if (!allowRFID(tag))
    {
      return;
    }

    if (!validScanType())
    {
      releaseRFIDProcessing();
      showMessage("SCAN TYPE ERR", "SET IN/OUT", 3);
      scheduleReady(2500);
      return;
    }

    String payload =
      tag +
      "/" +
      deviceId +
      "/" +
      scanType;

    debugPrint("Publish : " + payload);

    client.publish(
      topicTag.c_str(),
      payload.c_str());

    showMessage("PROCESSING", tag, 1);
  }
}
