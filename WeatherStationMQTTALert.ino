/**The MIT License (MIT)

  Copyright (c) 2018 by Daniel Eichhorn - ThingPulse

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

  See more at https://thingpulse.com
*/

#include <ESPWiFi.h>
#include <ESPHTTPClient.h>
#if defined(ESP8266)
#include <Ticker.h>
#endif
#include <JsonListener.h>
#include <ArduinoOTA.h>
#if defined(ESP8266)
#include <ESP8266mDNS.h>
#else
#include <ESPmDNS.h>
#endif

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

#include "SSD1306Wire.h"
#include "OLEDDisplayUi.h"
#include "Wire.h"
#include "WundergroundClient.h"
#include "WeatherStationFonts.h"
#include "WeatherStationImages.h"
#include "TimeClient.h"
#include "ThingspeakClient.h"

// MQTT
#include <PubSubClient.h>


/***************************
   Begin Settings
 **************************/
// Please read http://blog.squix.org/weatherstation-getting-code-adapting-it
// for setup instructions.
// In addition to the "normal" libraries you also need to install the WifiManager fro tzapu

#define HOSTNAME "ESP8266-OTA-"

// Setup
const int UPDATE_INTERVAL_SECS = 10 * 60; // Update every 10 minutes

// Display Settings
const int I2C_DISPLAY_ADDRESS = 0x3c;
#if defined(ESP8266)
const int SDA_PIN = D1;
const int SDC_PIN = D2;
#else
const int SDA_PIN = 5; //D3;
const int SDC_PIN = 4; //D4;
#endif

// TimeClient settings
const float UTC_OFFSET = 2;

// Wunderground Settings
const boolean IS_METRIC = true;
const String WUNDERGRROUND_API_KEY = "5db20118cac4a4a9";
const String WUNDERGRROUND_LANGUAGE = "DL";
const String WUNDERGROUND_COUNTRY = "DL";
const String WUNDERGROUND_CITY = "Frechen";

// Initialize the oled display for address 0x3c
// sda-pin=14 and sdc-pin=12
SSD1306Wire     display(I2C_DISPLAY_ADDRESS, SDA_PIN, SDC_PIN);
OLEDDisplayUi   ui( &display );

// MQTT Settings
// Update these with values suitable for your network.
const char* mqtt_server = "192.168.2.112";

WiFiClient espClient;
PubSubClient client(espClient);

// MQTT Alert Settings
boolean doMQTTAlert = false;
long lastMQTTAlert = 0;
int MQTTAlertCounter = 0;
int maxMQTTAlertCounter = 50;
#define BLINKDELAY 500
String lastMQTTAlertMessageText = "Letzte Nachricht ";
String lastMQTTAlertText = "(Keine)";
String lastMQTTAlertTime = "-";
String lastMQTTAlertDate = "-";

/***************************
   End Settings
 **************************/

TimeClient timeClient(UTC_OFFSET);

// Set to false, if you prefere imperial/inches, Fahrenheit
WundergroundClient wunderground(IS_METRIC);

// flag changed in the ticker function every 10 minutes
bool readyForWeatherUpdate = false;

String lastUpdate = "--";

#if defined(ESP8266)
Ticker ticker;
#else
long timeSinceLastWUpdate = 0;
#endif

//declaring prototypes
void configModeCallback (WiFiManager *myWiFiManager);
void drawProgress(OLEDDisplay *display, int percentage, String label);
void drawOtaProgress(unsigned int, unsigned int);
void updateData(OLEDDisplay *display);
void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawForecast(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex);
void drawLastMQTTMessage(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state);
void setReadyForWeatherUpdate();
int8_t getWifiQuality();


// Add frames
// this array keeps function pointers to all frames
// frames are the single views that slide from right to left
FrameCallback frames[] = { drawDateTime, drawCurrentWeather, drawForecast, drawLastMQTTMessage };
int numberOfFrames = 4;

OverlayCallback overlays[] = { drawHeaderOverlay };
int numberOfOverlays = 1;

String ESPChipID(void) {
#if defined(ESP8266)
  return String(ESP.getChipId(), HEX);
#else
  uint64_t EspChipID = ESP.getEfuseMac();
  char chipID[14];
  sprintf(chipID, "%04X%08X", (uint16_t)(EspChipID >> 32), (uint32_t)EspChipID);
  return chipID;

#endif
}
void setup() {
  // Turn On VCC
  //pinMode(D4, OUTPUT);
  //digitalWrite(D4, HIGH);
  Serial.begin(115200);

  // initialize dispaly
  display.init();
  display.clear();
  display.display();

  //display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setContrast(255);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  // Uncomment for testing wifi manager
  //wifiManager.resetSettings();
  wifiManager.setAPCallback(configModeCallback);

  //or use this for auto generated name ESP + ChipID
  wifiManager.autoConnect();

  //Manual Wifi
  //WiFi.begin(WIFI_SSID, WIFI_PWD);
  String hostname(HOSTNAME);
  hostname += ESPChipID();
#if defined(ESP8266)
  WiFi.hostname(hostname);
#else
  MDNS.begin((char *)&hostname);
#endif


  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    display.clear();
    display.drawString(64, 10, "Connecting to WiFi");
    display.drawXbm(46, 30, 8, 8, counter % 3 == 0 ? activeSymbol : inactiveSymbol);
    display.drawXbm(60, 30, 8, 8, counter % 3 == 1 ? activeSymbol : inactiveSymbol);
    display.drawXbm(74, 30, 8, 8, counter % 3 == 2 ? activeSymbol : inactiveSymbol);
    display.display();

    counter++;
  }

  ui.setTargetFPS(30);

  //Hack until disableIndicator works:
  //Set an empty symbol
  ui.setActiveSymbol(emptySymbol);
  ui.setInactiveSymbol(emptySymbol);

  ui.disableIndicator();

  // You can change the transition that is used
  // SLIDE_LEFT, SLIDE_RIGHT, SLIDE_TOP, SLIDE_DOWN
  ui.setFrameAnimation(SLIDE_LEFT);

  ui.setFrames(frames, numberOfFrames);

  ui.setOverlays(overlays, numberOfOverlays);

  // Inital UI takes care of initalising the display too.
  ui.init();

  // Setup OTA
  Serial.println("Hostname: " + hostname);
  ArduinoOTA.setHostname((const char *)hostname.c_str());
  ArduinoOTA.onProgress(drawOtaProgress);
  ArduinoOTA.begin();

  updateData(&display);

#if defined(ESP8266)
  ticker.attach(UPDATE_INTERVAL_SECS, setReadyForWeatherUpdate);
#endif

  // Stuff for MQTT setup
  setupMqtt(&display);


}

void setupMqtt(OLEDDisplay *display) {
  drawProgress(display, 10, "Initializing MQTT...");
  client.setServer(mqtt_server, 1883);
  // set mqtt callback routine
  client.setCallback(mqtt_callback);
  drawProgress(display, 50, "Setup stuff MQTT...");
  // MQTT Alert
  drawInitMQTTAlertText();
  pinMode(BUILTIN_LED, OUTPUT);    // Initialize the BUILTIN_LED pin as an output
  digitalWrite(BUILTIN_LED, HIGH); // set LED OFF
  drawProgress(display, 100, "Connecting MQTT...");
  Serial.println("MQTT Setup done");
}


void drawInitMQTTAlertText() {
  delay(500);
  //display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawStringMaxWidth(0, 40, 128, "MQTT Server conn.: " + String(mqtt_server) );
  display.display();
  delay(3000);
}

// MQTT Callback
// See https://github.com/knolleary/pubsubclient
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  String myPayload;
  String text;
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    //myPayload += (char)payload[i];
  }
  payload[length] = '\0';
  myPayload = String((char*)payload);

  Serial.println();

  // MQTT Topic LIGHTS
  // The payload LIGHT-1 can be extended with other numbers for other purposes

  if (strcmp(topic, "LIGHTS") == 0) {
    Serial.println("Topic LIGHTS found");
    Serial.println("Checking payload: " + myPayload);

    // LIGHT-1 ON?
    if (myPayload.startsWith("LIGHT-1:ON")) {
      digitalWrite(BUILTIN_LED, LOW);
      // make alert
      text = "LED AN";
      drawTextFlowMQTTAlert(text);
      // save the time and date
      saveMQTTAlertTimeDate(text);
      Serial.println(text);
    }

    // LIGHT-1 OFF?
    if (myPayload.startsWith("LIGHT-1:OFF")) {
      digitalWrite(BUILTIN_LED, HIGH);
      // make alert
      text = "LED AUS";
      drawTextFlowMQTTAlert(text);
      doMQTTAlert = false;
      // re-enable the frame transition after Alert
      ui.enableAutoTransition();
      // save the time and date
      saveMQTTAlertTimeDate(text);
      Serial.println(text);
    }

    // LIGHT-1 BLINK?
    if (myPayload.startsWith("LIGHT-1:BLINK")) {
      digitalWrite(BUILTIN_LED, LOW);
      // make alert
      text = "LED Blinken";
      drawTextFlowMQTTAlert(text);
      MQTTAlertCounter = 0;
      doMQTTAlert = true;
      // disable the frame transition during Alert
      ui.disableAutoTransition();
      // save the time and date
      saveMQTTAlertTimeDate(text);
      Serial.println(text);
    }
  }

  // checking for topic TEXT
  if (strcmp(topic, "TEXT") == 0) {
    Serial.println("Topic TEXT found");
    Serial.println("Checking payload: " + myPayload);
    if (myPayload.length() != 0) {
      Serial.println("Display Text: " + myPayload);
      // disable the frame transition during Alert
      ui.disableAutoTransition();
      // save the time and date
      saveMQTTAlertTimeDate(myPayload);
      // make alert
      drawTextFlowMQTTAlert(myPayload);
      doMQTTAlert = true;
    }
  }

  if (strcmp(topic, "blue") == 0) {
    // this one is blue...
  }

  if (strcmp(topic, "green") == 0) {
    // i forgot, is this orange?
  }
}

// MQTT Reconnect
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      //client.publish("outTopic", "hello world");
      // ... and resubscribe
      client.subscribe("LIGHTS");
      client.subscribe("TEXT");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

// Store current time and date
void saveMQTTAlertTimeDate(String text) {
  lastMQTTAlertText = text;
  lastMQTTAlertDate = wunderground.getDate();
  lastMQTTAlertTime = timeClient.getFormattedTime();
}

// An alert shall be visible and audible
// so far, visibile alert consists of altering the OLED in normal and invert display
// TODO: audible alert
void drawTextFlowMQTTAlert(String text) {
  // Remove all wheather stuff!
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawStringMaxWidth(0, 0, 128, text );
  display.display();
  // now invert the display 10 times
  for (int i = 0; i < 10; i++) {
    delay(50);
    if (i % 2 == 0) {
      display.invertDisplay();
    } else {
      display.normalDisplay();
    }
    display.display();
  }
}

// the alert will run within the standard loop()
// to trigger the alert, set doMQTTAlert to true.
// the first run of the alert will disable the ui frame transitions
// so that the alert text will not be overridden
// maxMQTTAlertCounter defines the amount of alert-blinkings
void runMQTTAlert() {
  long now = millis();
  // global: is the alert enabled now?
  if (doMQTTAlert) {
    // First run of MQTTAlert must disable the UI-transtion
    if (MQTTAlertCounter == 0) {
      ui.disableAutoTransition();
    }
    if (now - lastMQTTAlert > BLINKDELAY) {
      lastMQTTAlert = now;
      if (MQTTAlertCounter < maxMQTTAlertCounter) {
        MQTTAlertCounter++;
        if (MQTTAlertCounter % 2 == 0) {
          digitalWrite(BUILTIN_LED, LOW);
          display.invertDisplay();
          display.display();
        } else {
          digitalWrite(BUILTIN_LED, HIGH);
          display.normalDisplay();
          display.display();
        }
      } else {
        // stop blinking, re-enable UI-Transtion
        digitalWrite(BUILTIN_LED, HIGH);
        doMQTTAlert = false;
        MQTTAlertCounter = 0;
        display.normalDisplay();
        display.display();
        ui.enableAutoTransition();
      }
    }
  }
}

// standard loop
void loop() {

#if !defined(ESP8266)
  if (millis() - timeSinceLastWUpdate > (1000L * UPDATE_INTERVAL_SECS)) {
    setReadyForWeatherUpdate();
    timeSinceLastWUpdate = millis();
  }
#endif

  // run the updateData only, if MQTT ALERT not runnung
  if (doMQTTAlert == false && readyForWeatherUpdate && ui.getUiState()->frameState == FIXED) {
    updateData(&display);
  }

  // MQTT stuff
  if (!client.connected()) {
    reconnect();
  }
  // MQTT loop (the MQTT callback will be triggered)
  client.loop();

  if (doMQTTAlert == true) {
    runMQTTAlert();
  }

  // do standard UI stuff only when MQTT ALERT ist not running
  if (doMQTTAlert == false) {
    int remainingTimeBudget = ui.update();

    if (remainingTimeBudget > 0) {
      // You can do some work here
      // Don't do stuff if you are below your
      // time budget.
      ArduinoOTA.handle();
      delay(remainingTimeBudget);
    }
  }


}

// WIFI Manager
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 10, "Wifi Manager");
  display.drawString(64, 20, "Please connect to AP");
  display.drawString(64, 30, myWiFiManager->getConfigPortalSSID());
  display.drawString(64, 40, "To setup Wifi Configuration");
  display.display();
}

void drawProgress(OLEDDisplay *display, int percentage, String label) {
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64, 10, label);
  display->drawProgressBar(2, 28, 124, 10, percentage);
  display->display();
}

void drawOtaProgress(unsigned int progress, unsigned int total) {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 10, "OTA Update");
  display.drawProgressBar(2, 28, 124, 10, progress / (total / 100));
  display.display();
}

void updateData(OLEDDisplay *display) {
  drawProgress(display, 10, "Updating time...");
  timeClient.updateTime();
  drawProgress(display, 30, "Updating conditions...");
  wunderground.updateConditions(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
  drawProgress(display, 50, "Updating forecasts...");
  wunderground.updateForecast(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
  lastUpdate = timeClient.getFormattedTime();
  readyForWeatherUpdate = false;
  drawProgress(display, 100, "Done...");
  delay(1000);
}

void drawLastMQTTMessage(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(x, 0 + y, lastMQTTAlertMessageText);
  display->drawString(x, 11 + y, lastMQTTAlertDate);
  display->drawString(x, 21 + y, lastMQTTAlertTime);
  display->drawStringMaxWidth(x, 31, 128, lastMQTTAlertText );
}

void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  String date = wunderground.getDate();
  int textWidth = display->getStringWidth(date);
  display->drawString(64 + x, 5 + y, date);
  display->setFont(ArialMT_Plain_24);
  String time = timeClient.getFormattedTime();
  textWidth = display->getStringWidth(time);
  display->drawString(64 + x, 15 + y, time);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(60 + x, 5 + y, wunderground.getWeatherText());

  display->setFont(ArialMT_Plain_24);
  String temp = wunderground.getCurrentTemp() + "°C";
  display->drawString(60 + x, 15 + y, temp);
  int tempWidth = display->getStringWidth(temp);

  display->setFont(Meteocons_Plain_42);
  String weatherIcon = wunderground.getTodayIcon();
  int weatherIconWidth = display->getStringWidth(weatherIcon);
  display->drawString(32 + x - weatherIconWidth / 2, 05 + y, weatherIcon);
}


void drawForecast(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  drawForecastDetails(display, x, y, 0);
  drawForecastDetails(display, x + 44, y, 2);
  drawForecastDetails(display, x + 88, y, 4);
}

void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  String day = wunderground.getForecastTitle(dayIndex).substring(0, 3);
  day.toUpperCase();
  display->drawString(x + 20, y, day);

  display->setFont(Meteocons_Plain_21);
  display->drawString(x + 20, y + 12, wunderground.getForecastIcon(dayIndex));

  display->setFont(ArialMT_Plain_10);
  display->drawString(x + 20, y + 34, wunderground.getForecastLowTemp(dayIndex) + "|" + wunderground.getForecastHighTemp(dayIndex));
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {
  display->setColor(WHITE);
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(0, 54, String(state->currentFrame + 1) + "/" + String(numberOfFrames));

  String time = timeClient.getFormattedTime().substring(0, 5);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(38, 54, time);

  display->setTextAlignment(TEXT_ALIGN_CENTER);
  String temp = wunderground.getCurrentTemp() + "°C";
  display->drawString(90, 54, temp);

  int8_t quality = getWifiQuality();
  for (int8_t i = 0; i < 4; i++) {
    for (int8_t j = 0; j < 2 * (i + 1); j++) {
      if (quality > i * 25 || j == 0) {
        display->setPixel(120 + 2 * i, 63 - j);
      }
    }
  }


  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(Meteocons_Plain_10);
  String weatherIcon = wunderground.getTodayIcon();
  int weatherIconWidth = display->getStringWidth(weatherIcon);
  display->drawString(64, 55, weatherIcon);

  display->drawHorizontalLine(0, 52, 128);

}

// converts the dBm to a range between 0 and 100%
int8_t getWifiQuality() {
  int32_t dbm = WiFi.RSSI();
  if (dbm <= -100) {
    return 0;
  } else if (dbm >= -50) {
    return 100;
  } else {
    return 2 * (dbm + 100);
  }
}

void setReadyForWeatherUpdate() {
  Serial.println("Setting readyForUpdate to true");
  readyForWeatherUpdate = true;
}
