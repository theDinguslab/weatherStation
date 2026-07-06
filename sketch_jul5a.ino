#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Arduino_GFX_Library.h>

// ---- FILL THESE IN ----
const char* WIFI_SSID = "<YOUR_SSID>>";
const char* WIFI_PASSWORD = "<YOUR_PASSWORD>";

// I am from London Canada — change if you want a different location
const float LATITUDE = 43.0000;
const float LONGITUDE = -81.2500;
// ------------------------

#define TFT_BL 21
Arduino_DataBus *bus = new Arduino_ESP32SPI(2, 15, 14, 13, 12);
Arduino_GFX *gfx = new Arduino_ILI9341(bus, -1, 1, false);

unsigned long lastFetch = 0;
const unsigned long FETCH_INTERVAL_MS = 10UL * 60UL * 1000UL; // refresh every 10 minutes

String weatherCodeToText(int code) {
  if (code == 0) return "Clear";
  if (code >= 1 && code <= 3) return "Cloudy";
  if (code == 45 || code == 48) return "Fog";
  if (code >= 51 && code <= 57) return "Drizzle";
  if (code >= 61 && code <= 67) return "Rain";
  if (code >= 71 && code <= 77) return "Snow";
  if (code >= 80 && code <= 82) return "Showers";
  if (code >= 95) return "Storm";
  return "Unknown";
}

void showMessage(const char* line1, const char* line2 = "") {
  gfx->fillScreen(0x0000);
  gfx->setCursor(10, 10);
  gfx->setTextSize(2);
  gfx->println(line1);
  if (strlen(line2) > 0) gfx->println(line2);
}

void connectWiFi() {
  showMessage("Connecting WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(500);
  }

  if (WiFi.status() != WL_CONNECTED) {
    showMessage("WiFi failed!", "Check credentials");
  }
}

void fetchAndDisplay() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    if (WiFi.status() != WL_CONNECTED) return; // give up this cycle
  }

  HTTPClient http;
  String url = "http://api.open-meteo.com/v1/forecast?latitude=" + String(LATITUDE, 4) +
               "&longitude=" + String(LONGITUDE, 4) +
               "&current_weather=true&daily=temperature_2m_max,temperature_2m_min,weathercode&timezone=auto";

  http.begin(url);
  int httpCode = http.GET();

  if (httpCode != 200) {
    showMessage("Weather fetch failed!");
    gfx->print("HTTP code: ");
    gfx->println(httpCode);
    http.end();
    return;
  }

  String payload = http.getString();
  http.end();

  DynamicJsonDocument doc(4096);
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    showMessage("JSON parse failed!");
    return;
  }

  float currentTemp = doc["current_weather"]["temperature"];
  int currentCode = doc["current_weather"]["weathercode"];

  gfx->fillScreen(0x0000);
  gfx->setCursor(10, 10);
  gfx->setTextSize(2);
  gfx->println("HOMELAB WEATHER");
  gfx->println("--------------------");
  gfx->print("Now: ");
  gfx->print(currentTemp, 1);
  gfx->println(" C");
  gfx->println(weatherCodeToText(currentCode));
  gfx->println("");
  gfx->println("Forecast:");

  JsonArray days = doc["daily"]["time"];
  JsonArray highs = doc["daily"]["temperature_2m_max"];
  JsonArray lows = doc["daily"]["temperature_2m_min"];
  JsonArray codes = doc["daily"]["weathercode"];

  gfx->setTextSize(1);
  int numDays = min((int)days.size(), 4);
  for (int i = 0; i < numDays; i++) {
    const char* dateStr = days[i].as<const char*>();
    gfx->print(dateStr + 5); // skip "YYYY-" prefix, show "MM-DD"
    gfx->print(": ");
    gfx->print((float)lows[i], 0);
    gfx->print("-");
    gfx->print((float)highs[i], 0);
    gfx->print("C  ");
    gfx->println(weatherCodeToText(codes[i]));
  }
}

void setup() {
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  gfx->begin();
  gfx->fillScreen(0x0000);
  gfx->setTextColor(0xFFFF);

  connectWiFi();
  fetchAndDisplay();
  lastFetch = millis();
}

void loop() {
  if (millis() - lastFetch >= FETCH_INTERVAL_MS) {
    fetchAndDisplay();
    lastFetch = millis();
  }
}