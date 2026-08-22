# Architecture

## Design principles

1. Preserve the known-working MatrixPortal S3 firmware while migrating incrementally.
2. Keep normal device operation local and account-free.
3. Share the pattern model and user experience across supported ESP32 targets.
4. Keep hardware-specific display, pin, memory, and partition details in build profiles.
5. Use Lua for animation logic and composition; keep expensive pixel loops in C++.
6. Retain built-in C++ fallback patterns so a filesystem or script failure cannot leave the device unusable.
7. Pause normal rendering and display a dedicated progress frame during filesystem writes and firmware updates.

## Proposed component boundaries

```text
Application
├── DeviceProfile
├── DisplayBackend
│   ├── ProtomatterBackend
│   ├── Ws2812Backend
│   └── Max7219Backend
├── Canvas / Graphics API
├── PatternEngine
│   ├── BuiltInPattern
│   └── LuaPattern
├── ParameterStore
├── PlaylistService
├── PatternStorage
├── PatternCatalogClient
├── FirmwareUpdateService
├── WifiManager
└── WebApplication
```

The pattern engine draws through `Canvas`, not directly through Protomatter, FastLED/NeoPixel, or MAX7219 libraries.

## Display capabilities

A backend reports capabilities in addition to width and height:

```cpp
struct DisplayCapabilities {
  uint16_t width;
  uint16_t height;
  bool rgb;
  uint8_t colorDepth;
  float aspect;
};
```

Pattern manifests can require RGB output, minimum dimensions, or an aspect-ratio range. A backend may quantize RGB output for monochrome displays, but the catalog should not claim that every visual is equally suitable for every panel.

## Coordinate systems

Patterns receive:

- Pixel coordinates for exact drawing
- Relative coordinates `u` and `v` in the range 0…1
- Centered isotropic coordinates scaled by half of the shorter display dimension

The isotropic mapping preserves circles on rectangular displays. A 64×16 display is therefore approximately `x = -4…4`, `y = -1…1`.

## Pattern packages

Installed patterns use a package directory rather than embedding discovery metadata in executable Lua:

```text
/patterns/aurora-rings/
├── manifest.json
└── main.lua
```

`manifest.json` describes identity, version, engine compatibility, display requirements, and parameters. `main.lua` provides lifecycle callbacks such as initialization and frame rendering.

Downloads are written to a staging path. The device validates size, digest, manifest, compatibility, and Lua loading before replacing an installed version. Installed patterns continue to work offline.

## Lua runtime rules

- A memory-limited allocator is mandatory.
- Each callback has an instruction and execution-time budget.
- Calls are protected and errors are reported without crashing the firmware.
- Repeatedly failing patterns are quarantined and replaced by a fallback pattern.
- Filesystem, operating-system, package-loading, debug, and network libraries are unavailable to scripts.
- Direct pixel drawing is supported for sparse effects.
- Full-frame effects and expensive primitives run in C++ through `gfx` and `fx` APIs.

## Storage responsibilities

- Preferences/NVS: Wi-Fi networks, device identity, global settings, active profile
- LittleFS: installed pattern packages, pattern parameter values, catalog cache, playlists
- Firmware image: built-in fallback patterns and a fallback web interface

Settings are keyed by stable pattern and parameter IDs rather than raw C++ struct layouts.

## Update behavior

Pattern and firmware installation is an explicit maintenance state:

1. Stop normal pattern execution.
2. Render an `Installing…` or `Updating…` status frame.
3. Perform download, validation, and flash/filesystem writes.
4. Resume playback or reboot when complete.
5. Preserve the previous working pattern or firmware on failure.

