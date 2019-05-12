/*
The MIT License (MIT)

Copyright © 2018 Médéric NETTO

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <Arduino.h>
#include <ArduinoJson.h>

#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <SPI.h>

#include <App.hpp>
#include <TimeLib.h>
#include <PrintUtils.h>
#include <Bitmaps.h>
#include <BoardConfig.hpp>
#include <OtaHandler.hpp>

Adafruit_ILI9341 tft =
    Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST, TFT_MISO);

const char host[] = "api.coinmarketcap.com";

// Colors
int ILI9341_COLOR;
#define CUSTOM_DARK 0x4228 // Background color

// Bitmap_WiFi
extern uint8_t wifi_1[];
extern uint8_t wifi_2[];
extern uint8_t wifi_3[];

unsigned long previousMillis = 0;
long interval = 0;

time_t progressTimestamp = 0;

int coin = 0;
String crypto[] = {"bitcoin", "ethereum", "ripple", "litecoin", "monero"};
String oldPrice[5];

bool show_eur = true;

void setup() {

  Serial.begin(115200);

  pinMode(TFT_BL, OUTPUT);
  pinMode(BUTTON_L, INPUT_PULLUP);
  pinMode(BUTTON_M, INPUT_PULLUP);
  pinMode(BUTTON_R, INPUT_PULLUP);

  tft.begin();
  tft.setRotation(0);
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextWrap(true);
  tft.setCursor(0, 170);
  tft.setTextSize(2);

  tft.println("Connecting to: ");
  tft.println(" ");
  tft.println(WIFI_SSID);
  Serial.println("start");
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  digitalWrite(TFT_BL, true);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(200);
    tft.drawBitmap(70, 50, wifi_1, 100, 100, ILI9341_WHITE);
    delay(200);
    tft.drawBitmap(70, 50, wifi_2, 100, 100, ILI9341_WHITE);
    delay(200);
    tft.drawBitmap(70, 50, wifi_3, 100, 100, ILI9341_WHITE);
    delay(200);
    tft.fillRect(70, 50, 100, 100, ILI9341_BLACK);
  }
  Serial.println("\nConnected");
  Serial.println(WiFi.localIP());
  tft.println(" ");
  tft.println("WiFi connected");
  tft.println("IP address: ");
  tft.println(WiFi.localIP());

  InitializeOTA();

  delay(1500);
  tft.fillScreen(ILI9341_BLACK); // Clear Screen
  tft.setTextColor(0x0BBF);
  tft.setCursor(0, 150);
  tft.setTextSize(2);
  tft.println("Crypto Money Ticker");
  tft.drawLine(0, 130, 240, 130, ILI9341_WHITE);
  tft.drawLine(0, 185, 240, 185, ILI9341_WHITE);

  tft.setTextSize(1);
  tft.setCursor(5, 307);
  tft.setTextColor(ILI9341_WHITE);
  tft.println("Data from: CoinMarketCap.com");
}

void loop() {

  ArduinoOTA.handle();

  bool refresh = false;

  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(1, 10);

  if (digitalRead(BUTTON_L) == LOW) {
    Serial.println("BUTTON_L pressed");
    refresh = true;
    coin -= 2;
    if (coin < 0)
      coin = 4;
  }

  if (digitalRead(BUTTON_M) == LOW) {
    Serial.println("BUTTON_M pressed");
    show_eur = true ^ show_eur;
    refresh = true;
    coin -= 1;
    if (coin < 0)
      coin = 0;
    oldPrice[coin] = "0";
  }

  if (digitalRead(BUTTON_R) == LOW) {
    Serial.println("BUTTON_R pressed");
    refresh = true;
  }

  if (refresh == true) {
    printTransition();
  }

  unsigned long currentMillis = millis();

  if( currentMillis - progressTimestamp >= 250 )
  {
    progressTimestamp = currentMillis;
    int progress = (int)(currentMillis - previousMillis);
    progress /= 250.0 ;
    tft.drawPixel( progress, 319,  ILI9341_WHITE );
  }
  
  if (currentMillis - previousMillis >= interval || refresh == true) {

    previousMillis = currentMillis;
    interval = 60000; //                              <<<--------------------

    if (coin == 5) {
      coin = 0;
    }

    tft.drawFastHLine( 0, 319, 240, ILI9341_GREEN );

    Serial.print(">>> Connecting to ");
    Serial.println(host);

    WiFiClientSecure client;

    const int httpsPort = 443;
    if (!client.connect(host, httpsPort)) {
      tft.fillScreen(ILI9341_BLACK);
      tft.println(">>> Connection failed");
      return;
    }

    Serial.print("Requesting URL: ");
    Serial.println("Connected to server!");

    client.println("GET /v1/ticker/" + crypto[coin] + "/?convert=EUR HTTP/1.1");
    client.println("Host: api.coinmarketcap.com");
    client.println("Connection: close");
    client.println();

    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        tft.fillScreen(CUSTOM_DARK);
        tft.println(">>> Client Timeout!");
        client.stop();
        return;
      }
    }

    String data;
    while (client.available()) {
      data = client.readStringUntil('\r');
      Serial.println(data);
    }

    data.replace('[', ' ');
    data.replace(']', ' ');

    char buffer[data.length() + 1];
    data.toCharArray(buffer, sizeof(buffer));
    buffer[data.length() + 1] = '\0';

    const size_t bufferSize = JSON_OBJECT_SIZE(15) + 110;
    DynamicJsonBuffer jsonBuffer(bufferSize);

    JsonObject &root = jsonBuffer.parseObject(buffer);

    if (!root.success()) {
      tft.println("parseObject() failed");
      return;
    }

    String name = root["name"];                           // "Bitcoin"
    String symbol = root["symbol"];                       // "BTC"
    String price_usd = root["price_usd"];                 // "573.137"
    String price_eur = root["price_eur"];                 // "573.137"
    String percent_change_1h = root["percent_change_1h"]; // "0.04"
    String last_updated =
        root["last_updated"];     // "1472762067" <-- Unix Time Stamp
    String error = root["error"]; // id not found

    printTransition();
    String price;

    if (show_eur) {
      price = price_eur.substring(0, 8);
    } else {
      price = price_usd.substring(0, 8);
    }

    switch (coin) {

    case 0: // Bitcoin
      tft.drawBitmap(5, 5, bitcoin, 45, 45, ILI9341_ORANGE);
      printName(name, symbol);
      printPrice(price, show_eur);
      printChange(percent_change_1h);
      printTime(last_updated);
      printPagination();
      printError(error);
      tft.fillCircle(98, 300, 4, ILI9341_WHITE);
      break;

    case 1: // Ethereum
      tft.drawBitmap(5, 5, ethereum, 45, 45, ILI9341_WHITE);
      printName(name, symbol);
      printPrice(price, show_eur);
      printChange(percent_change_1h);
      printTime(last_updated);
      printPagination();
      printError(error);
      tft.fillCircle(108, 300, 4, ILI9341_WHITE);
      break;

    case 2: // Ripple
      tft.drawBitmap(5, 5, ripple, 45, 45, ILI9341_NAVY);
      printName(name, symbol);
      printPrice(price, show_eur);
      printChange(percent_change_1h);
      printTime(last_updated);
      printPagination();
      printError(error);
      tft.fillCircle(118, 300, 4, ILI9341_WHITE);
      break;

    case 3: // Litecoin
      tft.drawBitmap(5, 5, litecoin, 45, 45, ILI9341_LIGHTGREY);
      printName(name, symbol);
      printPrice(price, show_eur);
      printChange(percent_change_1h);
      printTime(last_updated);
      printPagination();
      printError(error);
      tft.fillCircle(128, 300, 4, ILI9341_WHITE);
      break;

    case 4: // Monero
      tft.drawBitmap(5, 5, monero, 45, 45, ILI9341_ORANGE);
      printName(name, symbol);
      printPrice(price, show_eur);
      printChange(percent_change_1h);
      printTime(last_updated);
      printPagination();
      printError(error);
      tft.fillCircle(138, 300, 4, ILI9341_WHITE);
      break;
    }

    oldPrice[coin] = price;
    coin++;
    previousMillis = millis();
  }
}

