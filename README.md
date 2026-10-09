# Matrix.hub — by Televator

One shared firmware project for **Matrix.16** (16×16 WS2812), **Matrix.22**
(22×22 WS2812), and **Matrix.64** (64×64 HUB75). Hardware-specific binaries,
shared pattern code and dashboard. The historical source filename remains
`64x64_matrix_01.ino`; select hardware using the PlatformIO build environment.

Start here: [single-document setup and AI guide](gift/INSTALL_WITH_AI.txt).
The renamed firmware uses `matrix-16.local`, `matrix-22.local`, and
`matrix-64.local`. Devices running older firmware retain their old hostnames
until updated. Initial provisioning/recovery uses USB; normal updates use OTA.

The active local gallery currently contains **61 patterns**. The older feature
notes below are historical, not a promise that every pattern has received an
individual small-panel aesthetic pass. Lua package downloads remain planned.

Public snapshots exclude personal photos and use empty photo slots with black
preview rows. `tools/export_public.py` creates this sanitized source tree without
altering the owner's local photo assets; never publish a private checkout's
generated image headers or full-flash backups wholesale.

For sharing new patterns today: fork this repository, develop/test on a branch,
and submit a pull request. Each recipient installs firmware built for their own
hardware. See the single-document guide for concrete steps and current limits.

## Historical v2 feature notes

Version 2.0 removes audio modulation and all audio receivers from the firmware
and dashboard. Click the star on a pattern card to bookmark it: bookmarked
patterns appear first, and bookmarks are saved on the device across restarts
and shared between browsers. The current gallery has **54 patterns**.
Newest additions: **Luminous Loom**, **Ferrofluid**, **Accretion**, **Origami
Flight**, **Signal Orchard**, **Whitney — Resonant Orbits**, **Whitney — Phase
Architecture**, and two personal photo patterns (**Foto — IMG_4786**, **Foto —
IMG_4704**). Each has eight controls and its own saved palette/defaults.
The personal-photo textures and generated previews are for local/device use;
they have **not** been cleared for publishing to this public repository.

The preceding additions are **Bioluminescent Topo Vortex — Abyssal**, **Warped Checkerboard
— Interference**, and **Ripples**, all rendered at native resolution by default.
**Whitney — Motion Graphics 1968** now moves continuously (line width replaces
pause duration); **Whitney — Chromatic Bloom** uses single-pixel particles.
The seven patterns bookmarked at the start of this revision were removed;
their source, previews and saved defaults are recoverable in
`patterns/archive/pre-advanced-2026-10-03/`. Other originals remain unchanged.
New bookmarks persist normally; revision 3 clears the old stars once only.
The earlier 58-pattern source is archived separately under
`patterns/archive/pre-curation-2026-10-03/`.

Palettes, patches, LFOs, scheduling, USB dashboard access, and OTA remain.
Each collection pattern now has independent saved settings and an Original
palette selected by default. Ember, Ice, and Reef are three new editable presets.
Preview loops contain 24 real-device frames and play at approximately 15 FPS.
The last edited numeric control stays selected: arrows adjust one step,
Shift+arrows adjust ten steps, even after focus leaves the control.

Build/upload with `tools/upload_matrix.command`. Historical audio companion
sources and analysis notes remain in `tools/` and `docs/` for reference; they
are no longer connected to the v2.0 firmware.

Imported patterns now default to native panel resolution. The per-pattern
**Rendering detail** selector offers **Native pixels** or **Smooth fast**;
the latter interpolates a smaller field instead of repeating square blocks.
Earlier all-pattern 60 FPS benchmarks used reduced internal resolutions;
native rendering has different
per-pattern frame rates. Diagnostics report the internal render dimensions.

During OTA, HUB75 briefly shows a dim notice and then turns off scanning/output
until reboot, avoiding bright frozen rows while flash is being written.

The compact instrument dashboard has three browser-saved color themes: Field,
Graphite, and Signal. Desktop groups library, eight rotary parameter controls,
and playback side by side. Drag a dial vertically (Shift for fine adjustment),
use its slider, or enter a number; changes are sent only on release. Palette,
LFO, and patch memory stay alongside the pattern; schedule, diagnostics,
utilities, and Wi-Fi fold away. Bookmarks can also be shown on their own.
Theme changes are browser-local and do not write to ESP32 settings storage.

Run `node tools/test_dashboard.cjs` for control-handler tests. For a local
non-mutating preview, run
`.venv/bin/python tools/preview_dashboard.py --device DEVICE_IP`;
its POST requests are mocked and never reach the device.

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

- Sixteen core RGB animations plus a forty-two-variant native shader collection,
  including ordered/error-diffusion dithering,
  autonomous Snake, a colored square tunnel, Perspective Flow, and the
  projected Flow Sphere and planar 2D Flow Field
- Exactly eight visible native controls per pattern, plus per-pattern LFO
  modulation and spatial styling
- A searchable animated-thumbnail gallery for all 58 current pattern entries;
  its six-frame loops are sampled from the actual firmware renderer over USB or Wi-Fi
- Seven cached firmware-side palettes whose four gradient stops are all
  editable, plus palette phase/span, global hue shift, and global saturation
- Three persistent patch slots per pattern
- One-click saving of the current controls as each pattern's permanent default
- Lightweight Arduino OTA reception with an automatic USB-or-Wi-Fi uploader
- Auto-cycle with fade-out/switch/fade-in transitions and adjustable frame rate
- A Berlin-time playback schedule with gradual startup and shutdown fades
- Multiple saved Wi-Fi networks
- Setup access point and captive portal fallback
- mDNS and a responsive local web interface
- Preferences-backed device settings
- Adafruit Protomatter output for the MatrixPortal S3 and NeoMatrix/NeoPixel
  output profiles for 16×16 and 22×22 ESP32-WROOM WS2812 matrices

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

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), [`docs/HARDWARE_PROFILES.md`](docs/HARDWARE_PROFILES.md), [`docs/AUDIO_COMPANION.md`](docs/AUDIO_COMPANION.md), [`docs/ROADMAP.md`](docs/ROADMAP.md), and [`docs/STATUS.md`](docs/STATUS.md) for the design and measured implementation status.

## Project status

The MatrixPortal S3 reference target now has a pinned PlatformIO build, a first
`DisplayBackend`, and a sandboxed embedded Lua 5.4.8 runtime. The dashboard's
**Run Hello Lua** button executes a Lua pattern whose drawing calls are handled
by the C++ Protomatter backend. Sixteen C++ animations remain available as
fallbacks, including Bayer and Floyd–Steinberg dither flows. Every built-in
pattern has eight visible native controls, editable cached palettes, and
per-patch LFO, color, and spatial-style settings. Dashboard sliders update
their displayed value while dragging and send only when released. The live
dashboard reports animation FPS and the separate HUB75 electrical refresh rate,
and open dashboards automatically follow pattern or parameter changes made by
another browser or API client.

The audio-reactive vertical slice now provides six energy bands, kick/snare/
hi-hat transient estimates, and overall loudness. The dependency-free browser
companion sends protocol v2 over WebSocket or USB. A native, ad-hoc-signed Mac
app uses an in-series Core Audio tap for one application or the whole system,
relays delayed audio to a selected output, and sends the same 18-byte state over
low-jitter UDP.
Firmware telemetry confirms all ten levels and the active transport, and each
signal can be routed with a signed amount to master time scale, brightness, or
a meaningful pattern parameter. Those routes are saved with each patch.

The native **Flow Sphere** pattern reinterprets the project's earlier Unity
AudioFlowfield/NoiseFlowfield design for 64×64 LEDs. It samples an evolving
three-axis field on a projected hemisphere; its default patch maps kick and
snare trigger envelopes to field speed.

The native **2D Flow Field** uses persistent particles in a planar curl field.
Its default patch maps the companion's Unity-inspired buffered Flow energy to
particle speed, so musical energy accelerates the motion while the field keeps
drifting at idle.

The native **Perspective Flow** pattern is adapted from “Monochromatic
Perspective Flow” by @martinnlopez for PatternFlow under CC-BY-SA-4.0. See
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).

## Build the MatrixPortal reference firmware

```sh
python3 -m venv .venv
.venv/bin/pip install -r requirements-dev.txt
.venv/bin/pio run -e matrixportal_s3_hub75
```

The generated firmware is written to
`.pio/build/matrixportal_s3_hub75/firmware.bin`.

For the reference panel's calmer 3-bit profile, run
`tools/upload_matrix.command`. It selects USB when a MatrixPortal serial port
is present and otherwise uses Wi-Fi OTA at `64x64-rgb-matrix.local`. The first
OTA-capable installation must be made once over USB. If local hostname
resolution is unavailable, pass the current IP as an argument, for example
`tools/upload_matrix.command 192.168.178.115`.

## Open the dashboard over USB

With the MatrixPortal connected by USB, double-click
`tools/open_usb_dashboard.command` or run it from Terminal. It opens
`http://127.0.0.1:8765/` and tunnels dashboard state, diagnostics, controls,
pattern selection, parameters, and Wi-Fi setup directly over USB serial. The
ESP32 and Mac do not need to share a Wi-Fi network for this mode.

If no saved network is reachable, the firmware also broadcasts the open
`64x64-Matrix-Setup` hotspot. Its full dashboard is available at
`http://192.168.4.1/`; the dedicated setup page remains at `/setup`.

WS2812/WROOM builds use GPIO 22, GRB order, and top-left serpentine rows. They
keep the pixel buffer at full precision; output level is controlled explicitly
through each pattern's brightness parameter and scheduled fades:

```sh
.venv/bin/pio run -e esp32_wroom_ws2812_16x16
.venv/bin/pio run -e esp32_wroom_ws2812_22x22
```

Arduino IDE launch sketches and first-install instructions are in
[`arduino/README.md`](arduino/README.md). The first installation is over USB;
subsequent application updates can use ArduinoOTA at each device's `.local`
hostname.

The MatrixPortal build target intentionally uses the board's existing default
partition table. A custom OTA/LittleFS layout will be selected only after the
Lua vertical slice has been exercised on hardware and its final firmware size
is known.
