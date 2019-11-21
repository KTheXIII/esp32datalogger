#include <Adafruit_INA219.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <SHT3x.h>
#include <WiFi.h>
#include <Wire.h>

#define uS_TO_S_FACTOR 1000000 // Conversion factor for micro seconds to seconds
#define TIME_TO_SLEEP 300      // Time ESP32 will go to sleep (in seconds)
#define ERROR_SLEEP 5

const char *ssid = "SSID";
const char *password = "PASSWORD";
const String API_LINK = "APILINK";

SHT3x thSensor;
Adafruit_INA219 ina219;

bool err(false), onOff(false);
unsigned long lastMillis(0), lastMillis2(0);
int errcounter(0);

float totalCurrent(0.0);
int measuredCQ(0);

void sendMessage();
void errors();

void setup() {
  Serial.begin(115200);

  thSensor.Begin();
  ina219.begin();

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

  delay(4000); // Delay needed before calling the WiFi.begin
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) { // Check for the connection
    delay(250);
    Serial.print(".");
    errcounter++;
    errors();
  }
  Serial.println();
  Serial.println("Connected to the WiFi network");

  sendMessage();

  if (!err)
    esp_deep_sleep_start();
  pinMode(13, OUTPUT);
}

void loop() {
  if (err && millis() - lastMillis2 >= 2500) {
    lastMillis2 = millis();
    sendMessage();

    if (!err)
      esp_deep_sleep_start();
  } else if (millis() - lastMillis >= 500) {
    lastMillis = millis();
    digitalWrite(13, onOff);
    onOff = !onOff;
    errcounter++;
  }

  errors();
}

void sendMessage() {
  float shuntvoltage = 0;
  float busvoltage = 0;
  float currentmA = 0;
  float loadvoltage = 0;
  float power_mW = 0;

  shuntvoltage = ina219.getShuntVoltage_mV();
  busvoltage = ina219.getBusVoltage_V();
  currentmA = ina219.getCurrent_mA();
  power_mW = ina219.getPower_mW();
  loadvoltage = busvoltage + (shuntvoltage / 1000);

  int batt = map((long)(busvoltage * 100), 320, 420, 0, 100);
  thSensor.UpdateData();

  if (WiFi.status() == WL_CONNECTED) { // Check WiFi connection status
    StaticJsonDocument<500> doc;
    doc["sensor"] = "SHT3X";
    doc["type"] = "TH";
    doc["temp"] = thSensor.GetTemperature();
    doc["hum"] = thSensor.GetRelHumidity();

    doc["batt"] = batt;
    doc["voltage"] = busvoltage;
    doc["current_mA"] = currentmA;
    doc["power_mW"] = power_mW;
    doc["loadvoltage"] = loadvoltage;

    HTTPClient http;
    http.begin(API_LINK);

    http.addHeader("Content-Type", "application/json");

    String output;
    serializeJson(doc, output);
    int httpResponseCode = http.POST(output);

    if (httpResponseCode > 0) {
      String response = http.getString(); // Get the response to the request
      Serial.println("status: " + String(httpResponseCode));
      Serial.println(response);
      err = false;
    } else {
      Serial.print("Error on sending POST: ");
      Serial.println("status: " + String(httpResponseCode));
      err = true;
    }

    http.end(); // Free resources
  } else {
    Serial.println("Error in WiFi connection");

    esp_sleep_enable_timer_wakeup(ERROR_SLEEP * uS_TO_S_FACTOR);
    esp_deep_sleep_start();
  }
}

void errors() {
  if (errcounter > 100) {
    Serial.print("error counter exceeded, will try again in ");
    Serial.print(ERROR_SLEEP);
    Serial.println("s");
    esp_sleep_enable_timer_wakeup(ERROR_SLEEP * uS_TO_S_FACTOR);
    esp_deep_sleep_start();
  }
}
