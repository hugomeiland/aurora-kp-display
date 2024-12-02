#include <SPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <TFT_eSPI.h> // Graphics and font library for ST7735 driver chip
#include "user_setup.h"

TFT_eSPI tft = TFT_eSPI();

// settings for Wifi, either local or from user_setup.h
// #define WIFI_SSID "my_wifi_network_ssid";  //  your network SSID (name)
// #define WIFI_PASSWORD "wifi_password";  // your network password

const char ssid[] = WIFI_SSID;  //  your network SSID (name)
const char pass[] = WIFI_PASSWORD;  // your network password

float Kp = 0;
int Index = 0;

void getKp() {
  WiFiClientSecure* client = new WiFiClientSecure;
  if (client) {
    client->setInsecure();
    HTTPClient https;
    Serial.print("[HTTPS] begin...\n");
    if (https.begin(*client, "https://services.swpc.noaa.gov/text/3-day-forecast.txt")) {  // HTTPS
      Serial.print("[HTTPS] GET...\n");
      int httpCode = https.GET();
      if (httpCode > 0) {
        Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String payload = https.getString();
          int payloadIndex = 0;
          int lineStart = 0;
          int lineEnd = 0;
          int payloadLength = payload.length();
          while (payloadIndex < payload.length()) {
            lineEnd = payload.indexOf("\n", payloadIndex);
            String line = payload.substring(lineStart, lineEnd);
            if (line.startsWith("The greatest expected 3 hr Kp for")) {
              Serial.println(line);
              int kpStart = line.indexOf(" is ");
              int kpEnd = line.indexOf(" (");
              String KpString = line.substring(kpStart + 4, kpEnd);
              Serial.println(KpString);
              Kp = KpString.toFloat();
            }
            //Serial.println(line);
            payloadIndex = lineEnd + 1;
            lineStart = lineEnd + 1;
          }
        }
      } else {
        Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
      }
      https.end();
    }
  } else { Serial.printf("[HTTPS] Unable to connect\n"); }
}

void getIndex() {
  WiFiClientSecure* client = new WiFiClientSecure;
  if (client) {
    client->setInsecure();
    HTTPClient https;
    Serial.print("[HTTPS] begin...\n");
    if (https.begin(*client, "https://services.swpc.noaa.gov/text/aurora-nowcast-hemi-power.txt")) {  // HTTPS
      Serial.print("[HTTPS] GET...\n");
      int httpCode = https.GET();
      if (httpCode > 0) {
        Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String payload = https.getString();
          String SouthIndexString = payload.substring(payload.length() - 4, payload.length() - 1);
          String NorthIndexString = payload.substring(payload.length() - 12, payload.length() - 9);
          Serial.println(NorthIndexString);
          Serial.println(SouthIndexString);
          Index = NorthIndexString.toInt();
          Serial.println(Index);
        }
      } else {
        Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
      }
      https.end();
    }
  } else {
    Serial.printf("[HTTPS] Unable to connect\n");
  }
}

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(2);
  tft.fillScreen(TFT_WHITE);
  Serial.print("Attempting to connect to WPA SSID: ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
  Serial.print("You're connected to the network");
}

void loop() {
  Kp = 0; Index = 0;
  for (int i = 0; i < 20; i++) {
    getKp();
    delay(5000);
    getIndex();
    delay(5000);
    if (Kp > 0 && Index > 0) break;
    }

  Serial.println(Kp);
  tft.fillScreen(TFT_WHITE);
  tft.setCursor(0, 0, 2);
  tft.setTextColor(TFT_WHITE,TFT_BLACK);
  tft.setTextSize(2);
  tft.println("3-day");
  tft.println("forecast:");
  tft.setTextSize(3);
  tft.print("Kp ");
  tft.println(Kp, 1);
  tft.setTextSize(2);
  tft.println("30 minute");
  tft.println("index:");
  tft.setTextSize(3);
  tft.print(Index);
  tft.println(" gw");

  Serial.println("Waiting 3 minute before the next round...");
  delay(180000);
}
