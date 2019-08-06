#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <SHT3x.h>
#include <WiFi.h>

#define uS_TO_S_FACTOR 1000000 // Conversion factor for micro seconds to seconds
#define TIME_TO_SLEEP 300      // Time ESP32 will go to sleep (in seconds)
#define ERROR_SLEEP 5

const int batPin = A13;

const char *ssid = "SSID";
const char *password = "PASSWORD";
const String API_LINK = "APILINK";
SHT3x thSensor;

uint8_t temprature_sens_read();

float voltage(0.0);
bool err(false), onOff(false);
unsigned long lastMillis(0), lastMillis2(0);
int errcounter(0);

void sendMessage() {
  thSensor.UpdateData();
  int analogValue = analogRead(batPin);
  voltage = analogValue * 2 * (3.3 / 4095.0) * 1.082;
  int batt = constrain(map((int)(voltage * 100), 320, 420, 0, 100), 0, 100);

  if (WiFi.status() == WL_CONNECTED) { // Check WiFi connection status
    StaticJsonDocument<500> doc;
    doc["sensor"] = "SHT3X";
    doc["type"] = "TH";
    doc["temp"] = thSensor.GetTemperature();
    doc["hum"] = thSensor.GetRelHumidity();
    doc["batt"] = batt;
    doc["voltage"] = voltage;

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

void setup() {
  Serial.begin(115200);
  thSensor.Begin();

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

  delay(4000); // Delay needed before calling the WiFi.begin
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) { // Check for the connection
    delay(250);
    Serial.print(".");
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
  } else if (millis() - lastMillis >= 250) {
    lastMillis = millis();
    digitalWrite(13, onOff);
    onOff = !onOff;
    errcounter++;
  }

  if (errcounter > 100) {
    Serial.print("error counter exceeded, will try again in ");
    Serial.print(ERROR_SLEEP);
    Serial.println("s");
    esp_sleep_enable_timer_wakeup(ERROR_SLEEP * uS_TO_S_FACTOR);
    esp_deep_sleep_start();
  }
}