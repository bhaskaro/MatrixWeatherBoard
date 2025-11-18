# MatrixWeatherBoard (ESP8266 NeoMatrix Weather Display)

MatrixWeatherBoard is an ESP8266-powered 32×8 NeoPixel LED matrix that displays:

- Local date & time (PST/PDT via NTP)
- Today’s and tomorrow’s weather (Open-Meteo)
- Real-time wind conditions
- A custom greeting message
- Auto day/night brightness
- WiFi setup via WiFiManager (no credentials in code)

This project runs on a Wemos D1 mini or any ESP8266-based development board.

File: `MatrixWeatherBoard.ino`


## 1. Features

- Scrolling text on 32×8 NeoMatrix LED panel  
- Automatic Wi-Fi provisioning using WiFiManager  
- NTP time sync with daylight savings support  
- Weather fetched from Open-Meteo using ZIP → lat/lon lookup  
- Auto-adjusting brightness:
  - Daytime brightness
  - Night-time dim mode  
- Clean configurable constants for fast customization  
- Very low memory footprint (optimized for ESP8266)


## 2. Hardware Requirements

- **ESP8266 board** (Wemos D1 mini recommended)
- **32×8 NeoPixel (WS2812) RGB LED matrix**
- USB cable for first-time flashing
- 5V power supply for matrix (recommended external supply)

### Wiring

| Matrix | ESP8266 |
|--------|---------|
| DIN    | GPIO 12 (D6) |
| 5V     | 5V |
| GND    | GND (must match ESP GND) |

⚠ **Important:** Always connect a common ground when using an external 5V supply for the LED matrix.


## 3. Software Requirements

Install:

- **Arduino IDE** (1.8.x or 2.x)
- **ESP8266 Board Package**


[http://arduino.esp8266.com/stable/package_esp8266com_index.json](http://arduino.esp8266.com/stable/package_esp8266com_index.json)

### Required Libraries

Install via **Tools → Manage Libraries…**:

- `Adafruit NeoPixel`
- `Adafruit GFX Library`
- `Adafruit NeoMatrix`
- `WiFiManager` (tzapu & tablatronix)
- `ArduinoJson` (v6.x)

All of these are used by `MatrixWeatherBoard.ino`.


## 4. Getting the Code

Clone from GitHub:

```bash
git clone https://github.com/bhaskaro/MatrixWeatherBoard.git
cd MatrixWeatherBoard
```

Ensure your folder structure is:

```
MatrixWeatherBoard/
  MatrixWeatherBoard.ino
```

(Arduino requires the folder name and `.ino` name to match.)

## 5. Opening & Compiling in Arduino IDE

1. Open **Arduino IDE**
2. `File → Open…` → select `MatrixWeatherBoard.ino`
3. Select board:

   ```
   Tools → Board → ESP8266 → LOLIN (Wemos) D1 R2 & mini
   ```
4. Select the USB port:

   ```
   Tools → Port → (your COM/ttyUSB port)
   ```
5. Click **Verify**, then **Upload**

## 6. First-Time Wi-Fi Setup (WiFiManager)

On first boot, or if credentials are missing:

1. The ESP8266 starts its own WiFi Access Point:

   ```
   MatrixWeatherBoard-Setup
   ```
2. Connect using your phone/laptop.
3. A configuration portal opens. If not, visit:

   ```
   http://192.168.4.1
   ```
4. Choose your home Wi-Fi and enter the password.
5. The ESP reboots and connects to your network.

WiFi credentials are saved in flash and reused automatically.

### Resetting WiFi Settings

To force the WiFi setup portal again:

* Erase flash (Arduino IDE → Tools → Erase Flash → All Flash Contents)
  **or**
* Temporarily insert:

```cpp
wm.resetSettings();
```

in the sketch and upload.

## 7. Custom Parameters & Configuration

Modify values near the top of `MatrixWeatherBoard.ino`:

### Greeting Message

```cpp
const char* GREETING_MESSAGE = "Hello from Advi!";
```

### Weather Location (ZIP)

```cpp
const char* ZIPCODE = "90028";
```

### Brightness Control

```cpp
const uint8_t MATRIX_BRIGHTNESS_DAY   = 25;
const uint8_t MATRIX_BRIGHTNESS_NIGHT = 4;

const uint8_t DAY_START_HOUR   = 7;
const uint8_t NIGHT_START_HOUR = 21;
```

### Refresh Intervals

Weather refresh interval:

```cpp
const unsigned long REFRESH_INTERVAL_MS = 600000; // 10 minutes
```

Scroll speed:

```cpp
const unsigned long SCROLL_INTERVAL_MS = 60; // ms per step
```

## 8. How Day/Night Brightness Works

Time is synced via NTP using PST/PDT timezone rules:

```cpp
const char* tzInfo = "PST8PDT,M3.2.0,M11.1.0";
```

Brightness auto-adjusts:

* Between `DAY_START_HOUR` and `NIGHT_START_HOUR` → **day brightness**
* Otherwise → **night brightness**

No manual adjustment required.

## 9. How Weather Fetching Works

1. ZIP code → lat/lon via:

   ```
   http://api.zippopotam.us/us/<ZIPCODE>
   ```
2. Weather from:

   ```
   https://api.open-meteo.com/
   ```
3. Data extracted:

   * Today temperature high/low
   * Tomorrow temperature high/low
   * Current wind speed
   * Max wind today / tomorrow
4. Everything is built into the scrolling Matrix message.

## 10. Troubleshooting

### ❌ Matrix stays off

* Check DIN is on **GPIO12 (D6)**
* Make sure the power supply is strong enough
* Confirm matrix type: GRB + 800 KHz

### ❌ No WiFi setup portal

* Device already has valid credentials stored
* Erase flash to force portal

### ❌ No weather data

* Check internet connectivity
* Open Serial Monitor (115200 baud) to view HTTP responses

## 11. License

This project is licensed under the **MIT License**.

You are free to use, modify, and distribute this software with proper attribution.
See the [LICENSE](LICENSE) file for full details.

**Note:**  
This project uses third-party libraries (Adafruit NeoMatrix, Adafruit GFX, WiFiManager, ArduinoJson, etc.) which are each released under permissive open-source licenses.  
These licenses remain applicable to their respective components.

The MatrixWeatherBoard project does not claim ownership of or responsibility for
these external APIs. All data returned from these endpoints is subject to the
terms of the respective providers.


## 12. Data Sources

This project uses public, free APIs for retrieving weather and location data:

- **ZIP → Latitude/Longitude**  
  `http://api.zippopotam.us/us/<ZIP>`  
  Provided by the open Zippopotam.us API (no API key required).

- **Weather Forecast (Today/Tomorrow)**  
  `https://api.open-meteo.com/v1/forecast`  
  Provided by Open-Meteo, a free and open weather API.

These services are operated by their respective providers and may be subject to rate limits or availability constraints. This project does not host or control these services.

---

Enjoy your new **MatrixWeatherBoard**!
