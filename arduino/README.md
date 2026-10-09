# Arduino IDE launch sketches

These thin sketches compile the same shared firmware as PlatformIO. Keep their
folders in this repository so the relative includes continue to resolve.

## First USB installation

1. Install **ESP32 by Espressif Systems 3.3.10** (or newer 3.x) in Boards
   Manager. Both sketches are verified with 3.3.10.
2. Install these Library Manager dependencies:
   - Adafruit NeoMatrix 1.3.3
   - Adafruit NeoPixel 1.13.0
   - Adafruit GFX Library 1.12.6
   - ArduinoJson 6.21.5
3. Install the vendored Lua runtime once: copy the repository's `lib/lua`
   folder into the Arduino sketchbook `libraries` folder and name the copy
   `Lua_Matrix_Runtime`. Its `library.properties` file makes it an Arduino
   library; no network download is needed.
4. Open the `.ino` for the physical matrix size.
5. Select **ESP32 Dev Module**, **4 MB flash**, and **Custom** under
   **Partition Scheme**, then select the board's USB serial port. The local
   `partitions.csv` supplies two 1.5 MB OTA application slots and a 960 KB
   LittleFS pattern partition.
6. Upload over USB. On first boot the device creates either
   `WS2812-16x16-Setup` or `WS2812-22x22-Setup`; join it and enter Wi-Fi details.

## Wireless updates after the first USB installation

The application includes ArduinoOTA. Once it is on Wi-Fi, upload subsequent
builds using `matrix-ws2812-16x16.local` or
`matrix-ws2812-22x22.local` as the network target. PlatformIO already provides
matching `_ota` environments.

OTA is intentionally in the application firmware, not the ESP32 ROM
bootloader. The immutable ROM loader only supports serial flashing; a first USB
flash is required to install this partition table and the OTA-capable app.
