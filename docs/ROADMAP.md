# Roadmap

Each phase must leave the current reference target compilable and testable.

## Phase 0 — Baseline

- Preserve and version the current MatrixPortal S3 sketch
- Establish a reproducible build
- Record library, board-package, partition, memory, and frame-time information
- Add basic diagnostics

## Phase 1 — Firmware structure

- Split the monolithic sketch into focused modules without changing behavior
- Introduce device-profile configuration
- Introduce `DisplayBackend` and `Canvas`
- Run one existing C++ animation through a common pattern interface
- Move the remaining built-in patterns incrementally

## Phase 2 — Storage and Lua feasibility

- Select stable per-target OTA/LittleFS partition layouts
- Mount LittleFS with a safe fallback path
- Embed a pinned Lua runtime
- Run a minimal compiled-in Lua pattern
- Add memory, instruction, and time limits
- Benchmark sparse and full-frame scripts on S3 and WROOM-32

## Phase 3 — Installable patterns

- Implement pattern package manifests
- Implement staged local installation
- Add C++ `gfx` and `color` APIs
- Add selected C++ `fx` bulk operations
- Port one representative pattern to Lua
- Persist parameter values by pattern and parameter ID
- Generate web controls from manifest metadata
- Add browser-based local pattern upload

## Phase 4 — Library and playlists

- Add installed/available/update library views
- Add a static catalog and cached offline metadata
- Add install, update, delete, and update-all operations
- Add playlists, durations, favorites, next, and play/pause
- Migrate auto-cycle to playlists

## Phase 5 — Firmware OTA

- Publish hardware-specific firmware artifacts
- Add version and compatibility manifests
- Add progress and failure recovery
- Verify that NVS and LittleFS survive updates
- Add rollback validation

## Phase 6 — Additional hardware

- Implement and test the WS2812(B) backend on 16×16 and 22×22 panels
- Implement and test the MAX7219 backend on the 32×32 panel
- Tune Lua limits and graphics operations for 4 MB/no-PSRAM WROOM targets
- Document wiring and build/upload instructions for every supported profile

