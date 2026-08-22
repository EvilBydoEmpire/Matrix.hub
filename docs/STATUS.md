# Implementation status

## MatrixPortal S3 Lua vertical slice

Status: compiled, flashed, and activated on the 64×64 HUB75 reference device.

The first vertical slice contains:

- A pinned PlatformIO `matrixportal_s3_hub75` environment
- The original nine C++ animations unchanged as normal/fallback playback
- A hardware-independent `DisplayBackend` interface
- A concrete Adafruit Protomatter backend
- Vendored official Lua 5.4.8 configured for 32-bit ESP32 numbers
- A 96 KB per-runtime allocation limit
- Per-callback instruction and wall-time budgets
- A restricted standard library without filesystem, OS, package, debug, dynamic
  loading, printing, or garbage-collector controls
- C++ implementations of `gfx.clear`, `gfx.pixel`, `gfx.line`, `gfx.rect`,
  `gfx.fillRect`, and `gfx.circle`
- C++ implementations of `color.rgb` and `color.hsv`
- A compiled-in Hello Lua pattern and dashboard start/stop control

## Measured build footprint

| Build | Static RAM | Firmware flash |
| --- | ---: | ---: |
| Original sketch in pinned environment | 85,008 bytes | 867,665 bytes |
| Display backend + Lua vertical slice | 85,248 bytes | 978,009 bytes |
| Difference | +240 bytes | +110,344 bytes |

PlatformIO reports these against 327,680 bytes of static RAM and the current
2,097,152-byte application partition. Dynamic allocations such as the Lua heap
and Protomatter framebuffer are not included in the static-RAM figure.

On the reference device, the runtime reported approximately 12.2 KB after
loading and 12.6 KB while the Hello Lua pattern was active, below the configured
96 KB limit.

## Hardware verification

- Firmware upload completed and its flash hashes were verified by esptool.
- The device reconnected to its saved Wi-Fi network.
- `/control` reported the Lua runtime available with no load error.
- `runLuaHello` activated the pattern.
- A later `/control` request confirmed that it remained active without a
  runtime-budget or allocation error.

Visual appearance still requires human confirmation at the physical panel.

## Deliberately not included yet

- LittleFS pattern loading
- Pattern manifest parsing on the device
- Local upload or GitHub catalog installation
- Dynamic Lua parameters in the web UI
- Custom OTA/LittleFS partition table
- Firmware OTA
- WS2812(B) and MAX7219 backends

