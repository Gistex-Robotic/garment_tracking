#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include "esp_bt.h"
#include "esp_wifi.h"

#if __has_include(<esp_arduino_version.h>)
#include <esp_arduino_version.h>
#endif

// =====================================================
// WIFI
// =====================================================
const char *ssid = "Tracked Robotic";
const char *password = "Factory@RFID";

// =====================================================
// MQTT
// =====================================================
const char *mqttServer = "10.5.0.106";
const uint16_t mqttPort = 1883;

// =====================================================
// IDENTITAS ALAT
// Isi salah satu: "in", "out", atau "assy".
// Upload firmware yang sama, hanya ubah nilai ini per alat.
// =====================================================
String scanType = "out";

// =====================================================
// RFID UART
// =====================================================
constexpr uint8_t RFID_RX_PIN = 16;
constexpr uint8_t RFID_TX_PIN = 17;
constexpr uint32_t RFID_BAUD = 9600;

// =====================================================
// PASSIVE BUZZER
// =====================================================
constexpr uint8_t BUZZER_PIN = 25;
constexpr uint8_t BUZZER_CHANNEL = 0;
constexpr uint16_t BUZZER_FREQ = 2000;
constexpr uint8_t BUZZER_RESOLUTION = 8;

// =====================================================
// BUTTON OPTIONAL: GPIO15 -> BUTTON -> GND
// Jangan ditekan saat ESP32 booting.
// =====================================================
constexpr uint8_t BUTTON_PIN = 15;

// =====================================================
// INTERVAL
// =====================================================
constexpr unsigned long WIFI_RETRY_INTERVAL = 10000;
constexpr unsigned long MQTT_RETRY_INTERVAL = 5000;
constexpr unsigned long HEARTBEAT_INTERVAL = 5000;
constexpr unsigned long RFID_DEBOUNCE_TIME = 1500;
constexpr unsigned long RFID_PROCESSING_TIMEOUT = 8000;
constexpr unsigned long BUTTON_DEBOUNCE_DELAY = 50;
constexpr unsigned long BUTTON_MESSAGE_DURATION = 3000;

// =====================================================
// OBJECT
// =====================================================
LiquidCrystal_I2C lcd(0x27, 16, 2);
WiFiClient espClient;
PubSubClient client(espClient);
HardwareSerial RFID(2);

// =====================================================
// DEVICE DAN MQTT TOPIC
// =====================================================
String deviceId;
String topicTag;
String topicStatus;
String topicReply;

// =====================================================
// DATA READY SCREEN
// =====================================================
String operatorName = "NO LOGIN";
int actualOutput = 0;
int countBundle = 0;
int lineNo = 0;

// =====================================================
// STATE RFID
// =====================================================
String lastTag;
unsigned long lastReadTime = 0;
bool rfidProcessing = false;
unsigned long rfidProcessingStart = 0;

// =====================================================
// STATE KONEKSI
// =====================================================
unsigned long lastWifiAttempt = 0;
unsigned long lastMqttAttempt = 0;
unsigned long lastHeartbeat = 0;
bool wifiPreviouslyConnected = false;
bool mqttPreviouslyConnected = false;
bool restoreRequested = false;

// =====================================================
// RETURN SCREEN
// =====================================================
bool returnToReady = false;
unsigned long returnReadyStart = 0;
unsigned long returnReadyDelay = 0;

// =====================================================
// BUTTON
// =====================================================
bool lastButtonReading = HIGH;
bool buttonState = HIGH;
unsigned long lastButtonDebounceTime = 0;
bool buttonMessageActive = false;
unsigned long buttonMessageStart = 0;

// =====================================================
// DEBUG
// =====================================================
bool debugMode = true;

void debugPrint(const String &message)
{
  if (debugMode)
  {
    Serial.println(message);
  }
}

// =====================================================
// SCAN TYPE
// =====================================================
bool validScanType()
{
  return scanType == "in" ||
         scanType == "out" ||
         scanType == "assy";
}

String scanTypeUpper()
{
  String result = scanType;
  result.toUpperCase();
  return result;
}

// =====================================================
// MQTT TOPIC
// IN/OUT memakai flow lama.
// ASSY memakai flow RFID PREP ASSY.
// =====================================================
void buildTopics()
{
  if (scanType == "assy")
  {
    topicTag = "rfid/prep/assy/" + deviceId + "/tag";
    topicStatus = "rfid/prep/assy/" + deviceId + "/status";
    topicReply = "rfid/prep/assy/reply/" + deviceId;
  }
  else
  {
    topicTag = "rfid/batch/" + deviceId + "/tag";
    topicStatus = "rfid/batch/" + deviceId + "/status";
    topicReply = "rfid/batch/reply/" + deviceId;
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
  btStop();
}

// =====================================================
// BUZZER, COMPATIBLE ESP32 CORE 2.x DAN 3.x
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
  {
    return;
  }

  buzzerTone(frequency);
  delay(duration);
  buzzerOff();
}

void beepCode(int code)
{
  if (code <= 0)
  {
    return;
  }

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
// LCD
// =====================================================
void lcdPrint16(int row, String text)
{
  lcd.setCursor(0, row);

  if (text.length() > 16)
  {
    text = text.substring(0, 16);
  }

  lcd.print(text);
  for (int i = text.length(); i < 16; i++)
  {
    lcd.print(' ');
  }
}

void showMessage(const String &row1, const String &row2, int beepValue = 0)
{
  lcdPrint16(0, row1);
  lcdPrint16(1, row2);
  beepCode(beepValue);
}

void scheduleReady(unsigned long delayMs)
{
  returnToReady = true;
  returnReadyStart = millis();
  returnReadyDelay = delayMs;
}

String shortOperatorName(String name, int maxLength = 9)
{
  name.trim();
  name.toUpperCase();

  if (name.length() == 0)
  {
    return "NO LOGIN";
  }

  if (name.length() > maxLength)
  {
    return name.substring(0, maxLength);
  }

  return name;
}

void showReady()
{
  if (buttonMessageActive || rfidProcessing)
  {
    return;
  }

  if (!validScanType())
  {
    showMessage("SCAN TYPE ERR", "IN/OUT/ASSY", 0);
    return;
  }

  String row1 = String(actualOutput) + " | " +
                shortOperatorName(operatorName, 9);
  String row2 = "C" + String(countBundle) +
                " | L" + String(lineNo) +
                " | " + scanTypeUpper();

  lcdPrint16(0, row1);
  lcdPrint16(1, row2);
}

// =====================================================
// SIMPLE JSON PARSER
// =====================================================
String jsonGetString(const String &json, const String &key)
{
  const String pattern = "\"" + key + "\"";
  int position = json.indexOf(pattern);

  if (position < 0)
  {
    return "";
  }

  position = json.indexOf(':', position + pattern.length());
  if (position < 0)
  {
    return "";
  }

  position++;
  while (position < static_cast<int>(json.length()) &&
         (json[position] == ' ' || json[position] == '\t'))
  {
    position++;
  }

  if (position >= static_cast<int>(json.length()))
  {
    return "";
  }

  if (json[position] == '"')
  {
    position++;
    String value;
    bool escape = false;

    while (position < static_cast<int>(json.length()))
    {
      const char current = json[position++];

      if (escape)
      {
        value += current;
        escape = false;
      }
      else if (current == '\\')
      {
        escape = true;
      }
      else if (current == '"')
      {
        break;
      }
      else
      {
        value += current;
      }
    }

    return value;
  }

  int end = position;
  while (end < static_cast<int>(json.length()) &&
         json[end] != ',' &&
         json[end] != '}')
  {
    end++;
  }

  String value = json.substring(position, end);
  value.trim();
  value.replace("\"", "");
  return value;
}

int jsonGetInt(const String &json, const String &key, int defaultValue = 0)
{
  String value = jsonGetString(json, key);
  value.trim();

  if (value.length() == 0)
  {
    return defaultValue;
  }

  return value.toInt();
}

void updateReadyDataFromJson(const String &json)
{
  const String action = jsonGetString(json, "action");
  const String jsonOperatorName = jsonGetString(json, "operator_name");
  const String jsonName = jsonGetString(json, "nama");

  if (jsonOperatorName.length() > 0)
  {
    operatorName = jsonOperatorName;
  }
  else if (jsonName.length() > 0)
  {
    operatorName = jsonName;
  }

  const int jsonCountBundle = jsonGetInt(json, "count_bundle", -1);
  const int jsonBundleCount = jsonGetInt(json, "bundle_count", -1);
  const int jsonTotalBundle = jsonGetInt(json, "total_bundle", -1);

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

  const int jsonLineNo = jsonGetInt(json, "line_no", -1);
  const int jsonLine = jsonGetInt(json, "line", -1);

  if (jsonLineNo >= 0)
  {
    lineNo = jsonLineNo;
  }
  else if (jsonLine >= 0)
  {
    lineNo = jsonLine;
  }

  const int totalOutputQty = jsonGetInt(json, "total_output_qty", -1);
  const int operatorOutput = jsonGetInt(json, "operator_output", -1);
  const int actualOutputJson = jsonGetInt(json, "actual_output", -1);
  const int totalOutput = jsonGetInt(json, "total_output", -1);

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

  if (action == "logout" || action == "restore_empty")
  {
    operatorName = "NO LOGIN";
    actualOutput = 0;
    countBundle = 0;
    lineNo = 0;
  }
}

// =====================================================
// RFID PROCESSING
// =====================================================
void releaseRFIDProcessing()
{
  rfidProcessing = false;
}

bool allowRFID(const String &tag)
{
  const unsigned long now = millis();

  if (rfidProcessing)
  {
    debugPrint("RFID ignored, waiting response: " + tag);
    return false;
  }

  if (tag == lastTag && now - lastReadTime < RFID_DEBOUNCE_TIME)
  {
    debugPrint("RFID duplicate ignored: " + tag);
    return false;
  }

  lastTag = tag;
  lastReadTime = now;
  return true;
}

bool publishPayload(const String &payload)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    showMessage("WIFI LOST", "TUNGGU KONEKSI", 3);
    scheduleReady(2000);
    return false;
  }

  if (!client.connected())
  {
    showMessage("SERVER LOST", "TUNGGU KONEKSI", 3);
    scheduleReady(2000);
    return false;
  }

  if (!client.publish(topicTag.c_str(), payload.c_str(), false))
  {
    showMessage("KIRIM GAGAL", "SCAN ULANG", 3);
    scheduleReady(2000);
    return false;
  }

  rfidProcessing = true;
  rfidProcessingStart = millis();
  return true;
}

void requestRestoreSession()
{
  if (!validScanType() || restoreRequested || !client.connected())
  {
    return;
  }

  const String payload =
      "__RESTORE__/" + deviceId + "/" + scanType;

  if (publishPayload(payload))
  {
    restoreRequested = true;
    debugPrint("Restore session request: " + payload);
    showMessage("SYNC SESSION", "PLEASE WAIT", 0);
  }
}

// =====================================================
// MQTT CALLBACK
// =====================================================
void callback(char *topic, byte *payload, unsigned int length)
{
  String message;
  message.reserve(length + 1);

  for (unsigned int i = 0; i < length; i++)
  {
    message += static_cast<char>(payload[i]);
  }
  message.trim();

  const String topicString = String(topic);
  debugPrint("RX : " + topicString + " = " + message);

  if (topicString != topicReply)
  {
    return;
  }

  if (!rfidProcessing)
  {
    debugPrint("Reply ignored, no active request");
    return;
  }

  releaseRFIDProcessing();
  restoreRequested = false;
  updateReadyDataFromJson(message);

  String lcd1 = jsonGetString(message, "lcd1");
  String lcd2 = jsonGetString(message, "lcd2");
  int beepValue = jsonGetInt(message, "beep", 1);

  if (lcd1.length() == 0)
  {
    lcd1 = "RESP ERROR";
    lcd2 = "CHECK API";
    beepValue = 3;
  }

  showMessage(lcd1, lcd2, beepValue);
  scheduleReady(2500);
}

// =====================================================
// WIFI NON-BLOCKING
// =====================================================
void startWiFi()
{
  debugPrint("Connecting WiFi...");
  showMessage("CONNECT WIFI", ssid, 0);
  WiFi.disconnect(false);
  delay(50);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  lastWifiAttempt = millis();
}

void maintainWiFi()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    if (!wifiPreviouslyConnected)
    {
      wifiPreviouslyConnected = true;
      mqttPreviouslyConnected = false;
      restoreRequested = false;
      lastMqttAttempt = 0;

      debugPrint("WiFi connected: " + WiFi.localIP().toString());
      showMessage("WIFI CONNECTED", WiFi.localIP().toString(), 1);
      scheduleReady(1200);
    }
    return;
  }

  if (wifiPreviouslyConnected)
  {
    wifiPreviouslyConnected = false;
    mqttPreviouslyConnected = false;
    client.disconnect();
    releaseRFIDProcessing();
    restoreRequested = false;

    debugPrint("WiFi lost");
    showMessage("WIFI LOST", "RECONNECT...", 3);
  }

  if (millis() - lastWifiAttempt >= WIFI_RETRY_INTERVAL)
  {
    startWiFi();
  }
}

// =====================================================
// MQTT NON-BLOCKING
// =====================================================
void maintainMQTT()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    return;
  }

  if (client.connected())
  {
    if (!mqttPreviouslyConnected)
    {
      mqttPreviouslyConnected = true;
      debugPrint("MQTT connection active");
    }
    return;
  }

  if (mqttPreviouslyConnected)
  {
    mqttPreviouslyConnected = false;
    releaseRFIDProcessing();
    restoreRequested = false;
    debugPrint("MQTT disconnected");
    showMessage("SERVER LOST", "RECONNECT...", 3);
  }

  if (millis() - lastMqttAttempt < MQTT_RETRY_INTERVAL)
  {
    return;
  }
  lastMqttAttempt = millis();

  debugPrint("Connecting MQTT...");
  const String clientId =
      "ESP32-" + scanTypeUpper() + "-" + deviceId;

  const bool connected = client.connect(
      clientId.c_str(),
      topicStatus.c_str(),
      0,
      true,
      "offline");

  if (!connected)
  {
    debugPrint("MQTT failed, state=" + String(client.state()));
    showMessage("MQTT FAILED", "RETRY...", 0);
    return;
  }

  mqttPreviouslyConnected = true;
  client.subscribe(topicReply.c_str(), 0);
  client.publish(topicStatus.c_str(), "online", true);

  debugPrint("MQTT connected");
  debugPrint("Subscribed: " + topicReply);
  showMessage("MQTT SUCCESS", scanTypeUpper(), 1);

  restoreRequested = false;
  requestRestoreSession();
}

// =====================================================
// RFID READER STX/ETX
// =====================================================
String readTag(Stream &serial)
{
  if (!serial.available())
  {
    return "";
  }

  if (serial.read() != 0x02)
  {
    return "";
  }

  String hexData;
  const unsigned long start = millis();

  while (millis() - start < 100)
  {
    if (!serial.available())
    {
      delay(1);
      continue;
    }

    const char current = static_cast<char>(serial.read());
    if (current == 0x03)
    {
      break;
    }

    hexData += current;
  }

  if (hexData.length() < 12)
  {
    return "";
  }

  const String tagHex = hexData.substring(0, 10);
  const String serialHex = tagHex.substring(2, 10);
  const uint64_t tagDecimal =
      strtoull(serialHex.c_str(), nullptr, 16);

  char buffer[20];
  snprintf(
      buffer,
      sizeof(buffer),
      "%010llu",
      static_cast<unsigned long long>(tagDecimal));

  return String(buffer);
}

void processTag(const String &tag)
{
  if (!allowRFID(tag))
  {
    return;
  }

  if (!validScanType())
  {
    showMessage("SCAN TYPE ERR", "IN/OUT/ASSY", 3);
    scheduleReady(2500);
    return;
  }

  const String payload =
      tag + "/" + deviceId + "/" + scanType;

  debugPrint("Publish : " + payload);

  if (publishPayload(payload))
  {
    showMessage("PROCESSING " + scanTypeUpper(), tag, 1);
  }
}

// =====================================================
// BUTTON
// =====================================================
void handleButton()
{
  const bool reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonReading)
  {
    lastButtonDebounceTime = millis();
  }

  if (millis() - lastButtonDebounceTime > BUTTON_DEBOUNCE_DELAY &&
      reading != buttonState)
  {
    buttonState = reading;

    if (buttonState == LOW)
    {
      buttonMessageActive = true;
      buttonMessageStart = millis();
      showMessage("BELUM DIPROGRAM", " BANGG!!!", 2);
    }
  }

  lastButtonReading = reading;

  if (buttonMessageActive &&
      millis() - buttonMessageStart >= BUTTON_MESSAGE_DURATION)
  {
    buttonMessageActive = false;
    showReady();
  }
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
      RFID_BAUD,
      SERIAL_8N1,
      RFID_RX_PIN,
      RFID_TX_PIN);

  lcd.init();
  lcd.backlight();
  showMessage("SYSTEM BOOT", "PLEASE WAIT", 1);

  optimizeWireless();

  deviceId = WiFi.macAddress();
  deviceId.replace(":", "");
  deviceId.toUpperCase();
  buildTopics();

  debugPrint("Device ID   : " + deviceId);
  debugPrint("Scan Type   : " + scanType);
  debugPrint("topicTag    : " + topicTag);
  debugPrint("topicStatus : " + topicStatus);
  debugPrint("topicReply  : " + topicReply);

  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
  client.setBufferSize(1536);
  client.setKeepAlive(30);
  client.setSocketTimeout(5);

  if (!validScanType())
  {
    showMessage("SCAN TYPE ERR", "IN/OUT/ASSY", 3);
  }

  // Membuat percobaan pertama langsung dijalankan.
  lastWifiAttempt = millis() - WIFI_RETRY_INTERVAL;
  lastMqttAttempt = millis() - MQTT_RETRY_INTERVAL;
  maintainWiFi();
}

// =====================================================
// LOOP
// =====================================================
void loop()
{
  maintainWiFi();
  maintainMQTT();

  if (client.connected())
  {
    client.loop();
  }

  handleButton();

  if (rfidProcessing &&
      millis() - rfidProcessingStart >= RFID_PROCESSING_TIMEOUT)
  {
    releaseRFIDProcessing();
    restoreRequested = false;

    debugPrint("RFID processing timeout");
    showMessage("TIMEOUT", "SCAN ULANG", 3);
    scheduleReady(2000);
  }

  if (returnToReady &&
      millis() - returnReadyStart >= returnReadyDelay)
  {
    returnToReady = false;
    showReady();
  }

  if (client.connected() &&
      millis() - lastHeartbeat >= HEARTBEAT_INTERVAL)
  {
    lastHeartbeat = millis();
    client.publish(topicStatus.c_str(), "online", true);
  }

  if (!buttonMessageActive && !rfidProcessing)
  {
    const String tag = readTag(RFID);
    if (tag.length() > 0)
    {
      processTag(tag);
    }
  }

  delay(2);
}
