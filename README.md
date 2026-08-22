# ESP32 Matrix Platform

A local-first firmware platform for programmable LED matrices built around ESP32 controllers.

The project starts from the working `64x64_matrix_01.ino` firmware for an Adafruit MatrixPortal S3 and a 64×64 HUB75 RGB panel. The immediate goal is to turn that single sketch into a maintainable platform while preserving its existing animations, Wi-Fi setup flow, web interface, patches, LFO controls, and playback behavior.

## Planned hardware targets

| Profile | Controller | Display | Initial sizes |
| --- | --- | --- | --- |
| `matrixportal_s3_hub75` | Adafruit MatrixPortal S3 | HUB75 RGB via Adafruit Protomatter | 64×64 |
| `esp32_wroom_ws2812` | ESP32-WROOM-32 or compatible | WS2812 / WS2812B | 16×16, 22×22 |
| `esp32_wroom_max7219` | ESP32-WROOM-32 or compatible | MAX7219 monochrome matrix | 32×32 |

These targets share source code, pattern metadata, the web experience, and the downloadable pattern catalog. They do not use one universal firmware binary: display drivers, flash sizes, pin mappings, capabilities, and OTA partition tables are selected per build profile.

## Current baseline

The current sketch provides:

- Nine built-in RGB animations
- Per-pattern parameters and LFO modulation
- Three persistent patch slots per pattern
- Auto-cycle and adjustable frame rate
- Multiple saved Wi-Fi networks
- Setup access point and captive portal fallback
- mDNS and a responsive local web interface
- Preferences-backed device settings
- Adafruit Protomatter output for the MatrixPortal S3

The baseline sketch remains at [`64x64_matrix_01.ino`](64x64_matrix_01.ino) while the new architecture is introduced in small, testable steps.

## Direction

The intended platform combines:

- C++ firmware and performance-sensitive drawing primitives
- Display backends for HUB75, WS2812(B), and MAX7219
- Sandboxed Lua patterns that can be installed without recompiling firmware
- LittleFS storage for installed patterns, playlists, and metadata
- A static GitHub-hosted pattern catalog
- Hardware-specific OTA firmware releases
- A mobile-first local web UI with Simple and Advanced modes

Normal playback and control remain entirely local. Internet access is only needed to check for or install pattern and firmware updates.

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), [`docs/HARDWARE_PROFILES.md`](docs/HARDWARE_PROFILES.md), and [`docs/ROADMAP.md`](docs/ROADMAP.md) for the initial design.

## Project status

Architecture and baseline stabilization. The existing sketch is the reference implementation and should remain usable at every migration step.

