# Hardware profiles

The platform uses one source tree with separate build artifacts. Partition tables and display drivers are selected at build time, so devices do not all receive the same binary.

## `matrixportal_s3_hub75`

- Controller: Adafruit MatrixPortal S3
- Display: 64×64 HUB75 RGB
- Backend: Adafruit Protomatter
- Flash: 8 MB on the current reference board
- PSRAM: available on the current reference board
- Status: working reference sketch

The existing MatrixPortal S3 HUB75 pins and five-bit, double-buffered Protomatter configuration remain the baseline.

## `esp32_wroom_ws2812`

- Controller: ESP32-WROOM-32 or compatible
- Displays: 16×16 WS2812 and 22×22 WS2812B
- Backend: to be selected after reviewing the existing sketches and wiring
- Expected baseline: commonly 4 MB flash without PSRAM, but the exact module must be detected or declared
- Status: planned

The profile must define data pin, pixel order, matrix wiring order, origin, serpentine/progressive layout, rotation, and power-safe default brightness.

## `esp32_wroom_max7219`

- Controller: ESP32-WROOM-32 or compatible
- Display: 32×32 MAX7219 dot matrix
- Output: monochrome
- Backend: to be selected after reviewing the existing sketch and module topology
- Expected baseline: commonly 4 MB flash without PSRAM, but the exact module must be detected or declared
- Status: planned

The profile must define SPI pins, controller count, module arrangement, rotation, and module addressing order.

## Partition strategy

The final partition layouts will be selected only after measuring compiled firmware sizes with the Lua runtime and web assets included.

Every OTA-capable profile needs:

- NVS storage
- OTA metadata
- Two equal application slots
- A LittleFS data partition

The 8 MB MatrixPortal S3 and a 4 MB ESP32-WROOM-32 will therefore have different partition CSV files. Firmware manifests must include an exact hardware-profile identifier so a device never installs an image built for another controller or display backend.

The partition layout should be finalized before devices are distributed because ordinary application OTA updates should not be relied upon to migrate partition boundaries.

## Information still needed

Before implementing each WROOM backend, capture:

- Exact ESP32 board/module marking
- Reported flash size and whether PSRAM is present
- Existing sketch
- Display library currently used
- Pin mapping
- Matrix wiring/module arrangement
- Known-good brightness and power configuration

