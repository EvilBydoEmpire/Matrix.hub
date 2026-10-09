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
- Finalize a versioned package schema before bulk-importing PatternFlow sources
- Add an import staging area that records original author, source URL, license,
  conversion status, and visual verification for every incoming pattern

## Phase 4 — Library and playlists

- Add installed/available/update library views
- Keep public pattern source, manifests, attribution, versions, and the generated
  static catalog in GitHub as the initial source of truth
- Publish the catalog and packages through GitHub Pages or a CDN-friendly static
  endpoint; add Supabase only when accounts, private drafts, favorites, ratings,
  community uploads, moderation, or analytics justify a database
- Add a browser preview that runs the same Lua pattern contract in a sandboxed
  browser runtime and renders a simulated LED matrix on a nearest-neighbor canvas
- Let users duplicate a pattern into a local variant, edit its Lua and manifest,
  preview it, validate it, and install it on their device
- Document an external-AI workflow: copy the Lua pattern plus the runtime API
  contract into the user’s preferred coding assistant, then paste the result into
  the editor. An integrated paid AI feature is explicitly deferred until this
  simpler workflow proves insufficient.
- Add a static catalog and cached offline metadata
- Add install, update, delete, and update-all operations
- Add playlists, durations, favorites, next, and play/pause
- Migrate auto-cycle to playlists

## Phase 5 — Firmware OTA

- Lightweight local ArduinoOTA receiver and USB/OTA upload chooser implemented
- Publish hardware-specific firmware artifacts
- Add version and compatibility manifests
- Add progress and failure recovery
- Verify that NVS and LittleFS survive updates
- Add rollback validation

## Phase 6 — Additional hardware

- Hardware-test the implemented WS2812(B) backend on 16×16 and 22×22 panels
- Implement and test the MAX7219 backend on the 32×32 panel
- Tune Lua limits and graphics operations for 4 MB/no-PSRAM WROOM targets
- Document wiring and build/upload instructions for every supported profile

## Phase 7 — Musical timing intelligence

- Port the earlier visualizer's confidence-gated detector auto-tuning together
  with its kick-only BPM estimator and beat grid. After 16 confident beats in
  a normal section, its sensitivity servo compares accepted hit rate with the
  expected kick/snare/hat pattern and its centroid servo slowly re-centers each
  detector band on grid-aligned hits. Do not add centroid learning without the
  beat-confidence gate; false triggers could otherwise teach the detector to
  reinforce the wrong frequency band.
