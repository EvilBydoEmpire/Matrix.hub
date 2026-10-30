# Matrix.hub

Firmware, a browser app and a library of generative patterns for the Matrix.16, Matrix.22 and Matrix.64 LED panels by Televator.

Each matrix is an ESP32 driving an LED panel. The app runs on the matrix itself, so once it is on your network it needs no computer, account or cloud service, and firmware updates arrive over Wi-Fi.

## Features

- 62 patterns, from Patternflow community works and John Whitney studies to Pong Wars, each drawn at the panel's own resolution.
- A few controls per pattern, each of which visibly changes it, 22 colour palettes and seven single tones plus the pattern's original colours, and one saved configuration per pattern.
- Cycling, a daily schedule with fades, and one brightness for the whole device.
- Brightness, fades and white balance in linear light on WS2812B, dithered so every LED keeps its hue; a white balance and test chart on every panel.
- Thumbnails rendered from the firmware's own code at each panel's size.
- A captive Wi-Fi setup page and several saved networks.

## Hardware

| Device | Controller | Panel | Environment | Address |
| --- | --- | --- | --- | --- |
| Matrix.16 | ESP32-WROOM | WS2812B, 16×16 | `esp32_wroom_ws2812_16x16` | `matrix-16.local` |
| Matrix.22 | ESP32-WROOM | WS2812B, 22×22 | `esp32_wroom_ws2812_22x22` | `matrix-22.local` |
| Matrix.64 | Adafruit MatrixPortal S3 | HUB75, 64×64 | `matrixportal_s3_hub75_64x64` | `matrix-64.local` |

## Getting started

1. Plug in the matrix. It starts playing at once and joins a saved network in the background; when none answers, it opens its own network, `Matrix.16-Setup` (or `-22`, `-64`).
2. Join that network. On the page that opens, or at `http://192.168.4.1/`, choose your 2.4 GHz network, enter its password and press Join. The matrix restarts and joins it.
3. Switch back to your network and open `http://matrix-16.local/`. If the name does not resolve, use the address from your router's list of devices.

## Development

You need Git and Python 3; the host tests also need `clang++` and Node.js.

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements-dev.txt
mkdir -p .local/platformio
export PLATFORMIO_CORE_DIR="$PWD/.local/platformio"
.venv/bin/pio run -e esp32_wroom_ws2812_16x16
tools/run_host_tests.sh
```

Update a matrix over Wi-Fi with `tools/matrix_hub.py`. `inspect` confirms its profile and update readiness; `update` builds the matching image, uploads it and checks that the matrix restarted on the commit it built, which Device → About also shows:

```sh
.venv/bin/python tools/matrix_hub.py inspect --profile 16 --host matrix-16.local
.venv/bin/python tools/matrix_hub.py update --profile 16 --host matrix-16.local
```

Matrices of the same size share a hostname, so with several on one network, pass the IP address of the one you mean. On macOS, allow the firewall prompt for Python at once, because the matrix connects back within ten seconds. A first installation or a recovery goes over USB with `.venv/bin/pio run -e ENVIRONMENT -t upload --upload-port PORT`; hold BOOT while plugging in if the upload cannot connect.

## Documentation

- [Architecture](docs/ARCHITECTURE.md): rendering, settings storage, the HTTP interface and the network.
- [App](docs/APP.md): views, behaviour, the USB bridge and the preview server.
- [Pattern library](docs/PATTERN_LIBRARY.md): controls, palettes, thumbnails and adding a pattern.
- [Hardware profiles](docs/HARDWARE_PROFILES.md): wiring, pins and panel settings.
- [Third-party notices](docs/THIRD_PARTY_NOTICES.md): credits, and the licences of adapted patterns and libraries.
- [Open work](docs/UPDATE.md): what remains to build and test.
- [AGENTS.md](AGENTS.md): instructions for coding agents.
