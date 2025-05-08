#include <WiFiS3.h>
#include <ArduinoJson.h>
#include <HX711_ADC.h>
#if defined(ESP8266)|| defined(ESP32) || defined(AVR)
#include <EEPROM.h>
#endif
#include <LiquidCrystal.h>
#include <WebSocketsClient.h>
#include "led.h"

String ssid;
String password;
int status = WL_IDLE_STATUS;

WebSocketsClient socket;
StaticJsonDocument<512> doc;

const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(7,6,5,4,3,2);

//pins:
const int HX711_dout = 8; //mcu > HX711 dout pin
const int HX711_sck = 9; //mcu > HX711 sck pin

//HX711 constructor:
HX711_ADC LoadCell(HX711_dout, HX711_sck);

const int calVal_eepromAdress = 0;
char ip[] = "91.227.114.136";
float tara0 = 0, tara = 0, calibration = 0;

void onEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
      case WStype_CONNECTED:
          Serial.println("Connected to WebSocket");
          break;
      case WStype_DISCONNECTED:
          //socket.setReconnectInterval(2 * 60  * 1000);
          Serial.println("Disconnected from WebSocket");
          break;
      case WStype_PING:
          socket.sendPing(NULL);
      case WStype_TEXT:
          Serial.println((char *)payload);
          deserializeJson(doc, (char *)payload);

          JsonArray array = doc.as<JsonArray>();
          String nome = array[0]["nome"];
          float calorie = array[0]["calorie"];
          float carboidrati = array[0]["carboidrati"];
          float proteine = array[0]["proteine"];
          float grassi = array[0]["grassi"];
          String valutazione = array[0]["valutazione"];

          Serial.print("Calorie: ");
          Serial.println(tara * 2.1 * (calorie / 100));
          break;
  }
}

void setup() {
  Serial.begin(9600); delay(10);
  Serial.println();
  Serial.println("Starting...");
  lcd.begin(16,2);
  lcd.print("bilancia");
  LoadCell.begin();
  //LoadCell.setReverseOutput(); //uncomment to turn a negative output value to positive
  float calibrationValue; // calibration value (see example file "Calibration.ino")
  calibrationValue = 696.0; // uncomment this if you want to set the calibration value in the sketch
#if defined(ESP8266)|| defined(ESP32)
  //EEPROM.begin(512); // uncomment this if you use ESP8266/ESP32 and want to fetch the calibration value from eeprom
#endif
  //EEPROM.get(calVal_eepromAdress, calibrationValue); // uncomment this if you want to fetch the calibration value from eeprom

  unsigned long stabilizingtime = 2000; // preciscion right after power-up can be improved by adding a few seconds of stabilizing time
  boolean _tare = true; //set this to false if you don't want tare to be performed in the next step
  LoadCell.start(stabilizingtime, _tare);
  if (LoadCell.getTareTimeoutFlag()) {
    Serial.println("Timeout, check MCU>HX711 wiring and pin designations");
    while (1);
  }
  else {
    LoadCell.setCalFactor(calibrationValue); // set calibration value (float)
    Serial.println("Startup is complete");
  }

  calibration = LoadCell.getData() * -1;
  loadLed();

  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
  }

  if (ssid.length() > 0 && password.length() > 0) {
    status = WiFi.begin(ssid.c_str(), password.c_str());
    if (status == WL_CONNECTED) {
      Serial.println("connected");
      socket.begin(ip, 7777);
      socket.onEvent(onEvent);
    } else
      Serial.println("connection error");
  }
}

void loop() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input.startsWith("wifi")) {
      String credentials = input.substring(5);
      int spaceIndex = credentials.indexOf(' ');

      if (spaceIndex != -1) {
        ssid = credentials.substring(0, spaceIndex);
        password = credentials.substring(spaceIndex + 1);

        status = WiFi.begin(ssid.c_str(), password.c_str());
        if (status == WL_CONNECTED) {
          Serial.println("connected");
          socket.begin(ip, 7777);
          socket.onEvent(onEvent);
        } else
          Serial.println("connection error");
      }
    }
  }

  static boolean newDataReady = 0;
  const int serialPrintInterval = 0; //increase value to slow down serial print activity

  // check for new data/start next conversion:
  if (LoadCell.update()) newDataReady = true;

  if (newDataReady) {

  float tara = LoadCell.getData() + calibration;
  float k = tara0 - tara;
  Serial.println(tara * 2.1);
  if (status == WL_CONNECTED && (k <= -5 || k >= 5)) {
    Serial.println(tara * 2.1);
    newDataReady = 0;
    tara0 = tara;

    socket.sendTXT("info");
  }
  }

  if (Serial.available() > 0) {
    char inByte = Serial.read();
    if (inByte == 't') LoadCell.tareNoDelay();
  }

  // check if last tare operation is complete:
  if (LoadCell.getTareStatus() == true) {
    Serial.println("Tare complete");
  }

  socket.loop();
  animation();
}
