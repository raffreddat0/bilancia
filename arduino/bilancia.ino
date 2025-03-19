#include <WiFiS3.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include "led.h"
#include "wifi.h"

String ssid;
String password;
int status = WL_IDLE_STATUS;

StaticJsonDocument<512> doc;
char ip[] = "91.227.114.136";
float tara0 = 0, tara = 5;

SoftwareSerial mySerial(2, 3); // RX, TX

void setup() {
  Serial.begin(9600);
  mySerial.begin(9600);
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
    if (status == WL_CONNECTED)
      Serial.println("connected");
    else
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
        if (status == WL_CONNECTED)
          Serial.println("connected");
        else
          Serial.println("connection error");
      }
    }
  }

  float k = tara0 - tara;
  if (status == WL_CONNECTED && (k <= -5 || k >= 5)) {
    tara0 = tara;
    String food = request("GET", ip, "/data");

    String query;
    query.reserve(64);
    query += "SELECT * FROM foods WHERE nome = ";
    query += "'pane'";

    String result = request("POST", ip, "/db", query);
    deserializeJson(doc, result);

    JsonArray array = doc.as<JsonArray>();
    float calorie = array[0]["calorie"];
    float carboidrati = array[0]["carboidrati"];
    float proteine = array[0]["proteine"];
    float grassi = array[0]["grassi"];
    String valutazione = array[0]["valutazione"];

    Serial.println(tara * (calorie / 100));
  }

  animation();
}