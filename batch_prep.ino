#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "esp_wifi.h"

// ================= WIFI =================
const char* ssid = "Robot_Resource (Lokal)";
const char* password = "robot@9876";

// ================= MQTT =================
const char* mqtt_server = "10.5.0.106";
const int mqtt_port = 1883;

// ================= RFID =================
#define RFID_RX_PIN 16
#define RFID_TX_PIN 17

// ================= BUTTON =================
#define BUTTON_PIN 15

// ================= PASSIVE BUZZER =================
#define BUZZER_PIN 25
#define BUZZER_CHANNEL 0
#define BUZZER_FREQ 2000
#define BUZZER_RESOLUTION 8

// ================= LCD =================
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ================= OBJECT =================
WiFiClient espClient;
PubSubClient client(espClient);
HardwareSerial RFID(2);

// ================= DEVICE =================
String deviceId;

String topicTag;
String topicStatus;
String topicNama;
String topicLine;
String topicAction;
String topicMessage;
String topicProses;
String topicStyle;
String topicOutput;
String topicTarget;

// ================= USER DATA =================
String nama = "";
String line = "";
String proses = "";
String style = "";
String operatorOutput = "0";
String targetOutput = "0";

String lastAction = "";
String lastMessage = "";

// ================= STATE =================
bool waitingLoginData = false;
bool syncingRetained = false;
bool showingProcessStyle = false;

bool returnToMainScreen = false;
unsigned long returnScreenTime = 0;

// ================= RFID DEBOUNCE =================
String lastTag = "";
unsigned long lastReadTime = 0;

const unsigned long debounceTime = 1500;

bool rfidProcessing = false;
unsigned long rfidProcessingStart = 0;
const unsigned long rfidProcessingTimeout = 5500;

// ================= BUTTON DEBOUNCE =================
bool lastButtonReading = HIGH;
bool buttonState = HIGH;

unsigned long lastButtonDebounceTime = 0;
const unsigned long buttonDebounceDelay = 50;

bool buttonMessageActive = false;
unsigned long buttonMessageStart = 0;
const unsigned long buttonMessageDuration = 3000;

// ================= HEARTBEAT =================
unsigned long lastHeartbeat = 0;
const unsigned long heartbeatInterval = 5000;

// ================= DEBUG =================
bool debugMode = true;

void debugPrint(String msg)
{
  if (debugMode)
  {
    Serial.println(msg);
  }
}

// ================= PASSIVE BUZZER =================
void beep(int duration = 200, int frequency = BUZZER_FREQ)
{
  if (duration <= 0)
    return;

  ledcWriteTone(BUZZER_CHANNEL, frequency);
  delay(duration);
  ledcWriteTone(BUZZER_CHANNEL, 0);
}

// ================= LCD HELPER =================
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

// ================= CLEAN TEXT =================
String cleanLineNumber(String lineText)
{
  lineText.replace("LINE", "");
  lineText.replace("Line", "");
  lineText.replace("line", "");
  lineText.trim();

  if (lineText.length() == 0)
  {
    return "-";
  }

  return lineText;
}

String cleanNumberText(String value)
{
  value.trim();

  if (value.endsWith(".00"))
  {
    value.remove(value.length() - 3);
  }
  else if (value.endsWith(".0"))
  {
    value.remove(value.length() - 2);
  }

  if (value.length() == 0)
  {
    value = "0";
  }

  return value;
}

// ================= SCHEDULE RETURN SCREEN =================
void scheduleReturnScreen(unsigned long delayMs)
{
  returnToMainScreen = true;
  returnScreenTime = millis() + delayMs;
}

// ================= LCD READY =================
void showTapRFID()
{
  if (buttonMessageActive)
    return;

  showingProcessStyle = false;

  lcd.clear();
  lcdPrint16(0, 0, "RFID READY");
  lcdPrint16(0, 1, "TAP KARTU");

  beep(100);
}

// ================= LCD USER IDLE =================
void showUser()
{
  if (buttonMessageActive)
    return;

  showingProcessStyle = false;

  if (nama.length() == 0)
  {
    showTapRFID();
    return;
  }

  lcd.clear();

  lcdPrint16(0, 0, nama);

  String row2 =
    cleanNumberText(operatorOutput) +
    "/" +
    cleanNumberText(targetOutput);

  lcdPrint16(0, 1, row2);

  beep(250);
}

// ================= LCD PROCESS + STYLE =================
void showProcessStyle()
{
  if (buttonMessageActive)
    return;

  if (nama.length() == 0)
  {
    showTapRFID();
    return;
  }

  showingProcessStyle = true;

  lcd.clear();

  String row1 = "P:";
  row1 += proses;

  String row2 = "S:";
  row2 += style;
  row2 += " / ";
  row2 += cleanLineNumber(line);

  lcdPrint16(0, 0, row1);
  lcdPrint16(0, 1, row2);

  beep(180);

  scheduleReturnScreen(3000);
}

// ================= TRY SHOW PROCESS STYLE =================
void tryShowProcessStyle()
{
  if (
    nama.length() > 0 &&
    line.length() > 0 &&
    proses.length() > 0 &&
    style.length() > 0
  )
  {
    waitingLoginData = false;
    showProcessStyle();
  }
}

// ================= LCD MESSAGE =================
void showMessage(
  String row1,
  String row2,
  int beepDuration = 150,
  bool force = false
)
{
  if (buttonMessageActive && !force)
    return;

  showingProcessStyle = false;

  lcd.clear();
  lcdPrint16(0, 0, row1);
  lcdPrint16(0, 1, row2);

  if (beepDuration > 0)
  {
    beep(beepDuration);
  }
}

// ================= MESSAGE PARSER =================
void showMessageFromPayload(String msg, int beepDuration = 150)
{
  int sep = msg.indexOf('|');

  if (sep >= 0)
  {
    String row1 = msg.substring(0, sep);
    String row2 = msg.substring(sep + 1);

    showMessage(row1, row2, beepDuration);
  }
  else
  {
    showMessage(msg, "", beepDuration);
  }
}

// ================= SHOW PREVIOUS SCREEN =================
void showPreviousScreen()
{
  if (nama.length() > 0)
  {
    showUser();
  }
  else
  {
    showTapRFID();
  }
}

// ================= BUTTON FUNCTION =================
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

      // INPUT_PULLUP:
      // tombol ditekan = LOW
      if (buttonState == LOW)
      {
        buttonMessageActive = true;
        buttonMessageStart = millis();

        returnToMainScreen = false;

        debugPrint("BUTTON GPIO15 PRESSED");

        showMessage(
          "BELUM DIPROGRAM",
          " BANGG!!!",
          200,
          true
        );
      }
    }
  }

  lastButtonReading = reading;

  if (
    buttonMessageActive &&
    millis() - buttonMessageStart >= buttonMessageDuration
  )
  {
    buttonMessageActive = false;
    showPreviousScreen();
  }
}

// ================= RFID DEBOUNCE FUNCTION =================
bool allowRFID(String tag)
{
  unsigned long now = millis();

  if (
    rfidProcessing &&
    now - rfidProcessingStart < rfidProcessingTimeout
  )
  {
    debugPrint("RFID ignored, waiting response: " + tag);
    return false;
  }

  if (
    tag == lastTag &&
    now - lastReadTime < debounceTime
  )
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

// ================= MQTT CALLBACK =================
void callback(
  char* topic,
  byte* payload,
  unsigned int length
)
{
  String msg = "";

  for (unsigned int i = 0; i < length; i++)
  {
    msg += (char)payload[i];
  }

  msg.trim();

  String topicStr = String(topic);

  debugPrint("RX : " + topicStr + " = " + msg);

  // ================= NAMA =================
  if (topicStr == topicNama)
  {
    nama = msg;

    if (nama.length() == 0)
    {
      line = "";
      proses = "";
      style = "";
      operatorOutput = "0";
      targetOutput = "0";
      waitingLoginData = false;
      showTapRFID();
      return;
    }

    if (syncingRetained)
      return;

    if (waitingLoginData)
    {
      tryShowProcessStyle();
      return;
    }

    if (line.length() > 0 && !showingProcessStyle)
    {
      showUser();
    }

    return;
  }

  // ================= LINE =================
  if (topicStr == topicLine)
  {
    line = msg;

    if (syncingRetained)
      return;

    if (waitingLoginData)
    {
      tryShowProcessStyle();
      return;
    }

    if (nama.length() > 0 && !showingProcessStyle)
    {
      showUser();
    }

    return;
  }

  // ================= PROSES =================
  if (topicStr == topicProses)
  {
    proses = msg;

    if (syncingRetained)
      return;

    if (waitingLoginData)
    {
      tryShowProcessStyle();
    }

    return;
  }

  // ================= STYLE =================
  if (topicStr == topicStyle)
  {
    style = msg;

    if (syncingRetained)
      return;

    if (waitingLoginData)
    {
      tryShowProcessStyle();
    }

    return;
  }

  // ================= OUTPUT OPERATOR =================
  if (topicStr == topicOutput)
  {
    operatorOutput = cleanNumberText(msg);

    if (
      syncingRetained ||
      waitingLoginData ||
      showingProcessStyle
    )
    {
      return;
    }

    if (nama.length() > 0)
    {
      showUser();
    }

    return;
  }

  // ================= TARGET OUTPUT =================
  if (topicStr == topicTarget)
  {
    targetOutput = cleanNumberText(msg);

    if (
      syncingRetained ||
      waitingLoginData ||
      showingProcessStyle
    )
    {
      return;
    }

    if (nama.length() > 0)
    {
      showUser();
    }

    return;
  }

  // ================= MESSAGE =================
  if (topicStr == topicMessage)
  {
    releaseRFIDProcessing();

    lastMessage = msg;

    if (msg.length() == 0)
      return;

    if (
      lastAction == "login" ||
      lastAction == "switch_operator"
    )
    {
      return;
    }

    showMessageFromPayload(msg, 150);

    if (
      lastAction == "output" ||
      lastAction == "bundle_full"
    )
    {
      scheduleReturnScreen(1800);
    }

    if (
      lastAction == "not_login" ||
      lastAction == "error" ||
      lastAction == "logout"
    )
    {
      scheduleReturnScreen(2000);
    }

    return;
  }

  // ================= ACTION =================
  if (topicStr == topicAction)
  {
    releaseRFIDProcessing();

    lastAction = msg;

    if (msg == "login")
    {
      waitingLoginData = true;
      showMessage("LOGIN SUCCESS", "WAIT DATA", 120);
      tryShowProcessStyle();
      return;
    }

    if (msg == "switch_operator" || msg == "switch")
    {
      waitingLoginData = true;
      showMessage("SWITCH USER", "WAIT DATA", 120);
      tryShowProcessStyle();
      return;
    }

    if (msg == "logout")
    {
      nama = "";
      line = "";
      proses = "";
      style = "";
      operatorOutput = "0";
      targetOutput = "0";
      waitingLoginData = false;

      showMessage("LOGOUT", "SUCCESS", 500);

      scheduleReturnScreen(1200);
      return;
    }

    if (msg == "output")
    {
      if (lastMessage.length() > 0)
      {
        showMessageFromPayload(lastMessage, 120);
      }
      else
      {
        showMessage("OUTPUT OK", "", 120);
      }

      scheduleReturnScreen(1800);
      return;
    }

    if (msg == "bundle_full" || msg == "full")
    {
      if (lastMessage.length() > 0)
      {
        showMessageFromPayload(lastMessage, 700);
      }
      else
      {
        showMessage("BUNDLE FULL", "", 700);
      }

      scheduleReturnScreen(2000);
      return;
    }

    if (msg == "not_login" || msg == "login_required")
    {
      showMessage("LOGIN DULU", "TAP ID CARD", 700);
      scheduleReturnScreen(2000);
      return;
    }

    if (msg == "error")
    {
      if (lastMessage.length() > 0)
      {
        showMessageFromPayload(lastMessage, 700);
      }
      else
      {
        showMessage("ERROR", "CHECK DATA", 700);
      }

      scheduleReturnScreen(2500);
      return;
    }

    return;
  }
}

// ================= MQTT RECONNECT =================
void reconnectMQTT()
{
  while (!client.connected())
  {
    debugPrint("Connecting MQTT...");

    String clientId = "ESP32-" + deviceId;

    lcd.clear();
    lcdPrint16(0, 0, "MQTT CONNECT TO");
    lcdPrint16(0, 1, mqtt_server);

    if (
      client.connect(
        clientId.c_str(),
        topicStatus.c_str(),
        0,
        true,
        "offline"
      )
    )
    {
      debugPrint("MQTT Connected");

      client.subscribe(topicNama.c_str());
      client.subscribe(topicLine.c_str());
      client.subscribe(topicAction.c_str());
      client.subscribe(topicMessage.c_str());
      client.subscribe(topicProses.c_str());
      client.subscribe(topicStyle.c_str());
      client.subscribe(topicOutput.c_str());
      client.subscribe(topicTarget.c_str());

      client.publish(
        topicStatus.c_str(),
        "online",
        true
      );

      lcd.clear();
      lcdPrint16(0, 0, "MQTT SUCCESS");
      lcdPrint16(0, 1, mqtt_server);

      delay(700);

      syncingRetained = true;

      unsigned long waitStart = millis();

      while (millis() - waitStart < 1500)
      {
        client.loop();
        delay(10);
      }

      syncingRetained = false;

      if (
        nama.length() > 0 &&
        proses.length() > 0 &&
        style.length() > 0
      )
      {
        showProcessStyle();
      }
      else if (nama.length() > 0)
      {
        showUser();
      }
      else
      {
        showTapRFID();
      }
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

// ================= RFID READER =================
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
      16
    );

  char buffer[20];

  snprintf(
    buffer,
    sizeof(buffer),
    "%010llu",
    (unsigned long long)tagDec
  );

  return String(buffer);
}

// ================= CONNECT WIFI =================
void connectWiFi()
{
  lcd.clear();
  lcdPrint16(0, 0, "CONNECT WIFI");
  lcdPrint16(0, 1, ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  esp_wifi_set_protocol(
    WIFI_IF_STA,
    WIFI_PROTOCOL_11B
  );

  esp_wifi_set_bandwidth(
    WIFI_IF_STA,
    WIFI_BW_HT20
  );

  lcd.clear();
  lcdPrint16(0, 0, "WIFI CONNECTED");
  lcdPrint16(0, 1, WiFi.localIP().toString());

  delay(1200);
}

// ================= SETUP =================
void setup()
{
  Serial.begin(115200);

  // BUTTON GPIO15 TO GND
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // PASSIVE BUZZER INIT
  ledcSetup(
    BUZZER_CHANNEL,
    BUZZER_FREQ,
    BUZZER_RESOLUTION
  );

  ledcAttachPin(
    BUZZER_PIN,
    BUZZER_CHANNEL
  );

  ledcWriteTone(
    BUZZER_CHANNEL,
    0
  );

  RFID.begin(
    9600,
    SERIAL_8N1,
    RFID_RX_PIN,
    RFID_TX_PIN
  );

  lcd.init();
  lcd.backlight();

  connectWiFi();

  deviceId = WiFi.macAddress();
  deviceId.replace(":", "");
  deviceId.toUpperCase();

  debugPrint("Device ID : " + deviceId);

  // ================= TOPIC =================

  topicTag =
    "rfid/batch/" +
    deviceId +
    "/tag";

  topicStatus =
    "rfid/batch/" +
    deviceId +
    "/status";

  topicNama =
    "rfid/batch/" +
    deviceId +
    "/nama";

  topicLine =
    "rfid/batch/" +
    deviceId +
    "/line";

  topicAction =
    "rfid/batch/" +
    deviceId +
    "/action";

  topicMessage =
    "rfid/batch/" +
    deviceId +
    "/message";

  topicProses =
    "rfid/batch/" +
    deviceId +
    "/proses";

  topicStyle =
    "rfid/batch/" +
    deviceId +
    "/style";

  topicOutput =
    "rfid/batch/" +
    deviceId +
    "/output";

  topicTarget =
    "rfid/batch/" +
    deviceId +
    "/target";

  debugPrint(topicTag);
  debugPrint(topicStatus);
  debugPrint(topicNama);
  debugPrint(topicLine);
  debugPrint(topicAction);
  debugPrint(topicMessage);
  debugPrint(topicProses);
  debugPrint(topicStyle);
  debugPrint(topicOutput);
  debugPrint(topicTarget);

  showTapRFID();

  client.setServer(
    mqtt_server,
    mqtt_port
  );

  client.setCallback(callback);

  client.setBufferSize(512);

  reconnectMQTT();
}

// ================= LOOP =================
void loop()
{
  handleButton();

  if (buttonMessageActive)
  {
    return;
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    showMessage("WIFI LOST", "RECONNECT...", 500);

    WiFi.reconnect();

    delay(1000);
    return;
  }

  if (!client.connected())
  {
    reconnectMQTT();
  }

  client.loop();

  handleButton();

  if (buttonMessageActive)
  {
    return;
  }

  // ================= RFID PROCESSING TIMEOUT =================
  if (
    rfidProcessing &&
    millis() - rfidProcessingStart >= rfidProcessingTimeout
  )
  {
    rfidProcessing = false;
    debugPrint("RFID processing timeout released");
  }

  // ================= RETURN SCREEN =================
  if (returnToMainScreen && millis() >= returnScreenTime)
  {
    returnToMainScreen = false;
    showingProcessStyle = false;

    if (nama.length() > 0)
    {
      showUser();
    }
    else
    {
      showTapRFID();
    }
  }

  // ================= HEARTBEAT =================
  if (
    millis() - lastHeartbeat
    >= heartbeatInterval
  )
  {
    lastHeartbeat = millis();

    client.publish(
      topicStatus.c_str(),
      "online",
      true
    );
  }

  // ================= RFID READ =================
  String tag = readTag(RFID);

  if (tag.length() > 0)
  {
    if (!allowRFID(tag))
    {
      return;
    }

    lastAction = "";
    lastMessage = "";

    String payload =
      tag +
      "/" +
      deviceId;

    debugPrint("Publish : " + payload);

    client.publish(
      topicTag.c_str(),
      payload.c_str()
    );

    lcd.clear();
    lcdPrint16(0, 0, "PROCESSING");
    lcdPrint16(0, 1, tag);

    beep(150);
  }
}
