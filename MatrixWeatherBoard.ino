#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiManager.h>    // <-- new

#include <ArduinoJson.h>

// ===== DISPLAY: NeoMatrix (32x8) =====
#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>

#include <time.h>

// ----------------------
// NTP / Timezone config
// ----------------------
const char* ntpServer = "pool.ntp.org";
// PST8PDT with DST rules: US: second Sunday in March, first Sunday in November
const char* tzInfo   = "PST8PDT,M3.2.0,M11.1.0";

// ----------------------
// Matrix configuration
// ----------------------
#define PIN 12  // Data pin for your matrix

Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(
    32, 8, PIN,
    NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
    NEO_GRB + NEO_KHZ800
);

// ----------------------
// Brightness config
// ----------------------
// Day/night brightness (0–255)
const uint8_t MATRIX_BRIGHTNESS_DAY   = 5;  // adjust to taste
const uint8_t MATRIX_BRIGHTNESS_NIGHT = 1;   // dimmer at night

// Time ranges for brightness (local time)
const uint8_t DAY_START_HOUR   = 7;   // 07:00
const uint8_t NIGHT_START_HOUR = 21;  // 21:00

uint8_t g_currentBrightness = MATRIX_BRIGHTNESS_DAY;
unsigned long g_lastBrightnessUpdateMs = 0;
const unsigned long BRIGHTNESS_UPDATE_INTERVAL_MS = 60000; // update every 60s

// ----------------------
// Scrolling state
// ----------------------
String    g_currentMsg;
bool      g_hasMsg         = false;
int16_t   g_scrollX        = 0;
int16_t   g_textWidth      = 0;      // cache of message width in pixels
uint16_t  g_currentColor   = 0;
uint8_t   g_refreshIndex   = 0;
unsigned long g_lastScrollMs = 0;
const unsigned long SCROLL_INTERVAL_MS = 60;  // ms between scroll steps

// ----------------------
// WiFi / App configuration
// ----------------------

const char* GREETING_MESSAGE = "Hello from Advi!";
const char* ZIPCODE          = "95391";  // change this to your ZIP

const unsigned long REFRESH_INTERVAL_MS = 600000;  // 10 minutes
unsigned long lastUpdate = 0;

// ----------------------
// WiFi helpers
// ----------------------
void connectWiFi() {
  WiFiManager wm;

  // Optional: reset settings for testing - this deletes stored SSID + password
  // wm.resetSettings();

  // Custom AP name when no WiFi is configured or connection fails
  const char* apName = "Advi-Matrix-Setup";  // SSID of config portal

  // autoConnect() tries saved WiFi first.
  // If it fails, it starts a config portal AP with name apName.
  Serial.println("Connecting using WiFiManager...");
  if (!wm.autoConnect(apName)) {
    Serial.println("Failed to connect and hit timeout. Rebooting...");
    delay(3000);
    ESP.restart();
    return;
  }

  // If we get here, we successfully connected to WiFi
  Serial.println("WiFi connected via WiFiManager!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}


// ----------------------
// Time initialization
// ----------------------
void initTime() {
  Serial.println("Configuring NTP time...");
  configTime(0, 0, ntpServer);   // get UTC from NTP
  setenv("TZ", tzInfo, 1);       // set timezone to PST/PDT
  tzset();

  struct tm timeinfo;
  int retries = 0;
  while (!getLocalTime(&timeinfo) && retries < 20) {
    Serial.println("Waiting for NTP time sync...");
    delay(500);
    retries++;
  }

  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain NTP time");
  } else {
    Serial.println("NTP time synchronized");
  }
}

// ----------------------
// Brightness helper
// ----------------------
void updateBrightnessFromTime() {
  unsigned long nowMs = millis();
  if (nowMs - g_lastBrightnessUpdateMs < BRIGHTNESS_UPDATE_INTERVAL_MS &&
      g_lastBrightnessUpdateMs != 0) {
    return;  // don't spam getLocalTime(); update once per minute
  }
  g_lastBrightnessUpdateMs = nowMs;

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return;  // if time not available, keep current brightness
  }

  uint8_t hour = timeinfo.tm_hour;

  uint8_t targetBrightness;
  if (hour >= DAY_START_HOUR && hour < NIGHT_START_HOUR) {
    targetBrightness = MATRIX_BRIGHTNESS_DAY;
  } else {
    targetBrightness = MATRIX_BRIGHTNESS_NIGHT;
  }

  if (targetBrightness != g_currentBrightness) {
    g_currentBrightness = targetBrightness;
    matrix.setBrightness(g_currentBrightness);
    matrix.show();
    Serial.print("Brightness updated to: ");
    Serial.println(g_currentBrightness);
  }
}

// ----------------------
// Time formatting
// ----------------------
String getLocalDateTimeString() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "DateTime N/A";
  }
  char buf[20];
  // Format: "YYYY-MM-DD HH:MM"
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &timeinfo);
  return String(buf);
}

// ----------------------
// ZIP -> lat/lon
// ----------------------
bool getLatLonFromZip(const char* zipcode, float& latOut, float& lonOut) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected in getLatLonFromZip()");
    return false;
  }

  WiFiClient client;
  HTTPClient http;

  String url = "http://api.zippopotam.us/us/";
  url += zipcode;

  Serial.print("GET ");
  Serial.println(url);

  if (!http.begin(client, url)) {
    Serial.println("HTTP begin failed (zip)");
    return false;
  }

  int httpCode = http.GET();
  if (httpCode <= 0) {
    Serial.print("HTTP GET failed (zip): ");
    Serial.println(http.errorToString(httpCode));
    http.end();
    return false;
  }

  Serial.print("HTTP status (zip): ");
  Serial.println(httpCode);

  if (httpCode != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  DynamicJsonDocument doc(4096);
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.print("JSON parse error (zip): ");
    Serial.println(err.c_str());
    return false;
  }

  if (!doc.containsKey("places")) {
    Serial.println("No 'places' in ZIP JSON");
    return false;
  }

  JsonArray places = doc["places"].as<JsonArray>();
  if (places.size() == 0) {
    Serial.println("ZIP 'places' array empty");
    return false;
  }

  JsonObject place0 = places[0];
  const char* latStr = place0["latitude"];
  const char* lonStr = place0["longitude"];

  if (!latStr || !lonStr) {
    Serial.println("Missing lat/lon fields in ZIP JSON");
    return false;
  }

  latOut = atof(latStr);
  lonOut = atof(lonStr);

  return true;
}

// ----------------------
// Weather fetch
// ----------------------
bool getWeatherTodayAndTomorrow(float lat, float lon,
                                float& tmaxToday, float& tminToday,
                                float& windNow, float& windMaxToday,
                                float& tmaxTomorrow, float& tminTomorrow,
                                float& windMaxTomorrow,
                                String& currentTimeIso) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected in getWeatherTodayAndTomorrow()");
    return false;
  }

  WiFiClient client;
  HTTPClient http;

  String url = "http://api.open-meteo.com/v1/forecast?";
  url += "latitude=" + String(lat, 4);
  url += "&longitude=" + String(lon, 4);
  url += "&daily=temperature_2m_max,temperature_2m_min,wind_speed_10m_max";
  url += "&current=wind_speed_10m";
  url += "&temperature_unit=fahrenheit";
  url += "&wind_speed_unit=mph";
  url += "&timezone=America%2FLos_Angeles";
  url += "&forecast_days=2";

  Serial.print("GET ");
  Serial.println(url);

  if (!http.begin(client, url)) {
    Serial.println("HTTP begin failed (weather)");
    return false;
  }

  int httpCode = http.GET();
  if (httpCode <= 0) {
    Serial.print("HTTP GET failed (weather): ");
    Serial.println(http.errorToString(httpCode));
    http.end();
    return false;
  }

  Serial.print("HTTP status (weather): ");
  Serial.println(httpCode);

  if (httpCode != HTTP_CODE_OK) {
    Serial.println("Non-200 status from weather API");
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  DynamicJsonDocument doc(8192);
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.print("JSON parse error (weather): ");
    Serial.println(err.c_str());
    return false;
  }

  if (!doc.containsKey("daily")) {
    Serial.println("No 'daily' in weather JSON");
    return false;
  }

  JsonObject daily = doc["daily"];

  JsonArray tmaxArr = daily["temperature_2m_max"].as<JsonArray>();
  JsonArray tminArr = daily["temperature_2m_min"].as<JsonArray>();
  JsonArray wmaxArr = daily["wind_speed_10m_max"].as<JsonArray>();

  if (tmaxArr.size() < 2 || tminArr.size() < 2 || wmaxArr.size() < 2) {
    Serial.println("Not enough daily entries in weather JSON");
    return false;
  }

  tmaxToday    = tmaxArr[0].as<float>();
  tminToday    = tminArr[0].as<float>();
  windMaxToday = wmaxArr[0].as<float>();

  tmaxTomorrow    = tmaxArr[1].as<float>();
  tminTomorrow    = tminArr[1].as<float>();
  windMaxTomorrow = wmaxArr[1].as<float>();

  if (!doc.containsKey("current")) {
    Serial.println("No 'current' in weather JSON");
    return false;
  }

  JsonObject current = doc["current"];

  if (!current.containsKey("wind_speed_10m")) {
    Serial.println("No 'wind_speed_10m' in current JSON");
    return false;
  }
  windNow = current["wind_speed_10m"].as<float>();

  if (current.containsKey("time")) {
    currentTimeIso = current["time"].as<String>();  // e.g. "2025-11-17T23:05"
  } else {
    currentTimeIso = "";
  }

  return true;
}

// ----------------------
// DISPLAY HELPERS
// ----------------------
uint16_t getNextColor() {
  static const uint8_t palette[][3] = {
    {255,   0,   0},  // red
    {  0, 255,   0},  // green
    {  0,   0, 255},  // blue
    {255, 255,   0},  // yellow
    {255,   0, 255},  // magenta
    {  0, 255, 255},  // cyan
    {255, 255, 255},  // white
    {255, 128,   0}   // orange
  };
  const uint8_t paletteSize = sizeof(palette) / sizeof(palette[0]);

  uint8_t idx = g_refreshIndex % paletteSize;
  g_refreshIndex++;

  uint8_t r = palette[idx][0];
  uint8_t g = palette[idx][1];
  uint8_t b = palette[idx][2];

  return matrix.Color(r, g, b);
}

String buildDisplayMessage(const String& dateTimeStr,
                           float tmaxToday, float tminToday,
                           float windNow, float windMaxToday,
                           float tmaxTomorrow, float tminTomorrow,
                           float windMaxTomorrow) {
  String msg;
  msg.reserve(128);  // avoid String reallocation spikes

  msg  = GREETING_MESSAGE;
  msg += " ";
  msg += dateTimeStr;

  msg += " | Today ";
  msg += String((int)tmaxToday);
  msg += "/";
  msg += String((int)tminToday);
  msg += "F WNow";
  msg += String((int)windNow);
  msg += " Max";
  msg += String((int)windMaxToday);

  msg += " | Tomorrow ";
  msg += String((int)tmaxTomorrow);
  msg += "/";
  msg += String((int)tminTomorrow);
  msg += "F W";
  msg += String((int)windMaxTomorrow);

  return msg;
}

void updateMatrixMessage(const String& dateTimeStr,
                         float tmaxToday, float tminToday,
                         float windNow, float windMaxToday,
                         float tmaxTomorrow, float tminTomorrow,
                         float windMaxTomorrow) {
  g_currentMsg = buildDisplayMessage(dateTimeStr,
                                     tmaxToday, tminToday,
                                     windNow, windMaxToday,
                                     tmaxTomorrow, tminTomorrow,
                                     windMaxTomorrow);
  g_currentColor = getNextColor();
  g_scrollX      = matrix.width();
  g_textWidth    = g_currentMsg.length() * 6; // 5px font + 1px spacing
  g_hasMsg       = true;
}

// ----------------------
// Debug print + data fetch
// ----------------------
void printGreetingAndData() {
  Serial.println("============================================");
  Serial.println(GREETING_MESSAGE);
  Serial.println("============================================");

  float lat = 0.0f, lon = 0.0f;
  if (!getLatLonFromZip(ZIPCODE, lat, lon)) {
    Serial.println("Failed to get lat/lon from ZIP.");
    return;
  }
  Serial.print("ZIP ");
  Serial.print(ZIPCODE);
  Serial.print(" -> Lat: ");
  Serial.print(lat, 4);
  Serial.print(", Lon: ");
  Serial.println(lon, 4);

  float tmaxToday, tminToday, windNow, windMaxToday;
  float tmaxTomorrow, tminTomorrow, windMaxTomorrow;
  String apiTimeIso;

  if (!getWeatherTodayAndTomorrow(
        lat, lon,
        tmaxToday, tminToday,
        windNow, windMaxToday,
        tmaxTomorrow, tminTomorrow,
        windMaxTomorrow,
        apiTimeIso)) {
    Serial.println("Failed to get weather data.");
    return;
  }

  Serial.println();
  Serial.println("---- Weather Today ----");
  Serial.print("Max Temp (F): ");
  Serial.println(tmaxToday);
  Serial.print("Min Temp (F): ");
  Serial.println(tminToday);
  Serial.print("Wind NOW (mph): ");
  Serial.println(windNow);
  Serial.print("Max Wind (mph): ");
  Serial.println(windMaxToday);

  Serial.println();
  Serial.println("---- Weather Tomorrow ----");
  Serial.print("Max Temp (F): ");
  Serial.println(tmaxTomorrow);
  Serial.print("Min Temp (F): ");
  Serial.println(tminTomorrow);
  Serial.print("Max Wind (mph): ");
  Serial.println(windMaxTomorrow);

  String localDateTime = getLocalDateTimeString();
  Serial.print("Local date/time (NTP): ");
  Serial.println(localDateTime);

  Serial.println("============================================");

  updateMatrixMessage(localDateTime,
                      tmaxToday, tminToday,
                      windNow, windMaxToday,
                      tmaxTomorrow, tminTomorrow,
                      windMaxTomorrow);
}

// ----------------------
// Setup & Loop
// ----------------------
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println("Wemos D1 mini - Greeting & Weather (NeoMatrix 32x8)");

  matrix.begin();
  matrix.setBrightness(g_currentBrightness);
  matrix.fillScreen(0);
  matrix.show();

  WiFi.mode(WIFI_STA);
  connectWiFi();
  initTime();

  // Set correct brightness based on actual time at boot
  updateBrightnessFromTime();
}

void scrollStep() {
  if (!g_hasMsg || g_currentMsg.length() == 0) {
    return;
  }

  unsigned long nowMs = millis();
  if (nowMs - g_lastScrollMs < SCROLL_INTERVAL_MS) {
    return;
  }
  g_lastScrollMs = nowMs;

  int16_t xEnd = -g_textWidth;

  matrix.fillScreen(0);
  matrix.setTextWrap(false);
  matrix.setTextColor(g_currentColor);
  matrix.setCursor(g_scrollX, 0);
  matrix.print(g_currentMsg);
  matrix.show();

  g_scrollX--;

  if (g_scrollX < xEnd) {
    g_scrollX = matrix.width();  // restart scroll
  }
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, reconnecting...");
    connectWiFi();
  }

  unsigned long nowMs = millis();
  if (nowMs - lastUpdate > REFRESH_INTERVAL_MS || lastUpdate == 0) {
    lastUpdate = nowMs;
    printGreetingAndData();
  }

  // Auto brightness based on local time
  updateBrightnessFromTime();

  // Continuously scroll current message
  scrollStep();
}
