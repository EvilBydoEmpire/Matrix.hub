# Implementation status

## Authored colors and ambient strobe studies — October 10, 2026

Source now has 62 gallery entries. Restored source RGB and authored palettes as
defaults, with bespoke restrained palettes for early generic/native effects.
Soukan stays available, but no longer overrides every default. Color-only
migration uses a separate persistent user-override mask; non-color presets are
untouched. Three full-resolution ambient temporal studies are manual-only,
warn on selection, and have non-flashing thumbnails. See
ART_DIRECTION_AND_STROBES.md. All three hardware targets compile; host render
tests pass at 16/22/64. Live Matrix.64 verification/upload is pending while its
Wi-Fi connection is intermittent. Native preview recapture is pending too.

## Personal-photo removal — October 9, 2026

Removed both personal-photo patterns, the embedded image header, photo renderer,
import/registration scripts and their source/derived preview rows. The gallery
now has 59 entries. Variant IDs 56/57 remain inert, hidden tombstones; later IDs
are not renumbered. Persisted bookmark bits retain their original slots via
bookmarkStorageIndex, avoiding a destructive settings migration. Calibration
comparisons now use procedural grayscale/RGB charts rather than photographs.
No Wi-Fi credentials are removed or modified. The owner explicitly wants
existing saved networks retained when gifting the device. Historical notes
below describe prior versions, not the current photo-free project.

## Matrix.hub identity and public handoff — October 9, 2026

Project branding is Matrix.hub by Televator. Device display names are Matrix.16,
Matrix.22 and Matrix.64; DNS-safe hostnames are matrix-16/22/64.local and setup
hotspots are Matrix.16/22/64-Setup. Existing hardware-profile IDs and saved NVS
keys are unchanged. Old firmware keeps the old names until flashed.

`gift/INSTALL_WITH_AI.txt` is the unified discovery, setup, coding, repair and
fork/PR handoff. `tools/matrix_hub.py` verifies profile/dimensions/OTA readiness
before wireless uploads and refuses an implicit Protomatter-to-DMA migration.
The legacy upload wrapper now requires an explicit size and verified host;
it no longer selects the first USB device or silently installs 3-bit firmware.

Public source snapshots are generated with `tools/export_public.py`: private
photo textures become zero-filled slots, photo thumbnail rows are blackened,
and both target-specific PNGs and the embedded header are rebuilt. Full-flash
backups, private photo originals, recordings and local graph/cache state are
not exported. Local photo assets are retained. GitHub repository renamed to
Televatorxyz/Matrix.hub with history preserved.

WS2812 photo color diagnosis: the installed NeoMatrix library expands RGB565
through gamma5/gamma6, while the shared photo path can additionally apply
photoGamma100=180. The checked WROOM reported that extra correction enabled.
This double transfer is a concrete suspect, not a measured optical calibration.
No color correction was changed by the naming/publication work. P3-to-sRGB is
already present in the photo importer. Gallery loops are prerecorded and do
not reproduce LED output, saved settings or all current calibration controls.

The photo-free host renderer passes all registered IDs at 16/22/64 dimensions
under UBSan, and dashboard, organizer, palette and updater identity tests pass.

## WROOM installation and Classic 08 — October 9, 2026

Imported user-provided LED_Matrix_08 Ripples/Plasma as stable variants 63/64,
gallery entries 59/60 (61 total). Existing patterns and IDs remain unchanged.
Both retain the source algorithms, expose eight meaningful controls, share
editable palettes, and use a 16-pixel logical canvas at all supported sizes.
Simulation is fixed at 30 Hz, independent of output FPS. Gallery previews use
the real C++ renderer. See `patterns/imports/led_matrix_08/README.md`.

Quality persistence now supports a second 64-bit block; the first block's key
and bits are untouched. Library metadata grows to 96 RAM entries while sparse
LM2/LM3 and old 80-entry fixed storage remain readable. Soukan override masks
read the original 10 bytes and expand to 12 bytes when saved.

WROOM 16×16 GPIO22 installed by USB on ESP32-D0WD-V3 (4 MB). Full pre-install
flash backup is in ignored `.pio/backups/`, not committed or published.
The existing Wi-Fi namespace survived and connected automatically. OTA is ready.
Both imports measure about 60 FPS with roughly 146 KB free heap
(~143 KiB); measurements are in `classic08-wroom-verification.json`.
Firmware occupies 1,511,705 / 1,835,008 bytes (82.4%); static RAM 112,784 bytes.
16×16, 22×22 and MatrixPortal 4-bit DMA builds pass. The 64×64 hardware has
NOT been reflashed for this import.

The WROOM partition table uses two 1.75 MiB application slots and 448 KiB
LittleFS (initialized by USB, mounted, 8,192 / 458,752 bytes used). Previous
filesystem contents are recoverable from the flash backup. USB dashboard supports an explicit MATRIX_SERIAL_PORT or unambiguous
single USB modem/serial device. It holds the port open to prevent repeated UART
reset pulses, and reports the correct dimensions for its locally served atlas.

Tests: Classic 08 timing at 30/60/120 FPS, all four algorithm controls, 16/22/64
bounds, immediate blackout, metadata migration, all registered renderer IDs,
dashboard/organizer/Wi-Fi regressions pass. AddressSanitizer deadlocked in the
host macOS runtime before main; UndefinedBehaviorSanitizer plus explicit
framebuffer bounds checks completed instead.

## Explicit saved Wi-Fi selection — October 9, 2026

Wi-Fi / recovery now lists saved SSIDs (never passwords), marking the connected
and preferred networks. Save network keeps the current connection and displays
persistent inline acknowledgement/error feedback; storage writes are read back
before success is reported. Connect selected queues a restart after acknowledgement
and persists the preferred network index. Startup tries that network first, then
the remaining saved networks, with the existing setup-hotspot fallback unchanged.
The USB dashboard uses the same save/control commands without its former forced
restart or comma-delimited SSID restriction. Saving is not a password/connectivity
test. Changes to animation parameters and patterns are outside this update.

Validation: dashboard regressions and Wi-Fi selector tests pass; MatrixPortal
4-bit DMA and both WROOM sizes compile. An OTA transfer completed, but the
matrix did not return at its previous address (192.168.178.115), so on-device
verification is pending. A subsequent upload attempt received no invitation
response. No saved network was deliberately switched or removed during testing.

## Soukan color collection — October 4, 2026

Ten editable four-stop palettes adapted from the user's photographed print
schemes are curated across all 59 active/archived gallery entries. A color-only
default overlay preserves motion, brightness, style, LFO and library metadata;
subsequent user-saved defaults take precedence. Legacy palette IDs and Original
remain available. See `SOUKAN_PALETTES.md` for mapping and persistence details.

Native device-frame review corrected grey-background amplification, isolated
Snake body/head/apple roles, expanded narrow hue domains and preserved additive
Ripple pigments at overlaps. Real 24-frame gallery loops now use adaptive
256-color PNGs without RGB332 or dithering. HUB75 uses 28px thumbnails and
WROOM uses 16px thumbnails so both retain OTA flash space. Hardware output
calibration, scan timing, bit depth and orientation are unchanged.


## Native-detail rendering and calm OTA — October 3, 2026

Patternflow ports now default to the actual panel dimensions (64×64 on the
MatrixPortal). Optional Smooth fast quality is saved independently for each
variant and uses fixed-point bilinear interpolation. Glass Torus remains an
adaptive cached SDF renderer. The older all-pattern 60 FPS results below were
measured with reduced internal resolutions and do not describe native-detail
performance. Updated results are in `pattern-benchmark-native64-final.json`.

On-device spot checks at target 240 FPS measured Chromatic Vortex at roughly
56–59 FPS in native 64×64 and 94–95 FPS in smooth 32×32. Vorticity measured
24 FPS native versus 65 FPS smooth 24×24; Blooming measured 23 versus 65;
Silk Drift measured 32 versus 65. These are rendering rates, not electrical
scan refresh, and change with parameters, transforms, and control traffic.
The final comparison is in `pattern-benchmark-quality-comparison.json`.

Optimizations cache polar coordinates/static noise, replace per-pixel powers
with interpolated lookup curves, keep shader arithmetic single-precision,
hoist per-frame vortex/ribbon geometry, use coordinate reciprocals, and copy
untransformed HUB75 frames directly into the canvas. Geometry cache capacity
matches the target panel so smaller WROOM builds do not reserve 64×64 caches.
Smooth upscaling interpolates packed RGB565 channels with fixed-point weights
and uses the same direct-canvas path when transforms/fades are inactive.
Blooming and Chladni coordinate normalization was corrected and their actual
rendered thumbnail loops recaptured.

OTA renders a dim status notice for 250 ms before flash writes, then calls
Protomatter stop to disable OE and the refresh timer until reboot. An error
resumes scanning and restores the previous auto-cycle setting. WS2812 holds
a dim update icon without needing continuous row scanning.

Quality and bookmark flags are stored as compact 64-bit values in the existing
settings namespace, with write/readback verification. Startup removes only
the obsolete `matrix-cfg/audio_p` blob from the removed audio-routing feature;
that blob had exhausted usable NVS space and prevented new settings saves.
Pattern patches, palettes, and Wi-Fi entries are preserved. Diagnostics expose
NVS used/free entry counts.

## Version 2.0 — October 3, 2026

### Curated library and controls

The device bookmark snapshot retained 28 of the previous 58 gallery entries.
Thirty unbookmarked algorithms were removed from the active renderers, with
pre-curation sources archived locally. Bookmarks are cleared once on migration,
not on later reboots. Eighteen new native Patternflow adaptations bring the
gallery to 46; the nineteenth supplied file, Blooming, was an identical duplicate.

Original is now the preselected palette; Ember, Ice and Reef are added. Collection
variants have independent defaults/patches, including gradients, spatial controls
and LFOs, stored sparsely. Empty collection slots no longer reserve full banks in
static RAM, keeping the ordinary WROOM builds viable. Last-edited controls remain
selected for arrow-key editing (Shift: ten steps). Real-rendered previews have
24 frames at 67 ms (~15 FPS), without additional ESP32 rendering while browsing.

Native kernels use float arithmetic, cached geometry/transcendentals, integer
noise hashing, shared simulation workspace and bounded flash-backed Collatz
topology. Defaults/control extremes passed host undefined-behavior/bounds checks
at 16×16, 22×22 and 64×64. Dashboard keyboard/release tests passed. Benchmark
results are in `docs/pattern-benchmark-curated-*.json`; native heavy simulations
remain below 60 FPS, and explicit smooth rendering is available.

Audio reception, routing, modulation, diagnostics, and dashboard controls have
been removed. USB dashboard access and Wi-Fi provisioning remain available.
Pattern bookmarks are saved on the device and place starred cards first.
All three supported firmware profiles compile successfully, and bookmark
sorting/toggling passed a dashboard JavaScript check. Historical audio sections
below describe earlier versions; companion sources remain for reference.

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
- Live one-second performance telemetry for actual FPS, render cost, frame-budget
  overruns, CPU speed, and heap health
- A lightweight `/diagnostics` JSON endpoint, matching dashboard diagnostics,
  plus a concise serial `PERF` line every five seconds

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

## Initial performance sample

Measured on the 240 MHz MatrixPortal S3 at a 45 FPS target and five-bit
Protomatter color depth. Values include framebuffer presentation time.

| Pattern | Actual FPS | Average render | Budget used |
| --- | ---: | ---: | ---: |
| Starfield | 44.0 | 10.37 ms | 47% |
| Warped Checkerboard | 33.6 | 28.80 ms | 130% |
| Moiré Rings | 33.5 | 28.80 ms | 130% |
| Moiré Lines | 33.6 | 28.69 ms | 129% |
| Warping Checkers | 33.6 | 28.79 ms | 130% |
| Shockwave | 13.7 | 72.03 ms | 324% |
| Warp Corridors | 11.1 | 89.14 ms | 401% |
| Pixel Avalanche | 43.0 | 11.39 ms | 51% |
| Kaleidoscope | 43.0 | 10.02 ms | 45% |
| Hello Lua | 43.4 | 10.79 ms | 49% |

The sparse Lua pattern is in the same performance class as the sparse C++
patterns. The full-screen C++ patterns are render-bound, primarily due to
per-pixel floating-point trigonometry, distance calculations, and HSV-to-RGB
conversion. Free heap remained around 190 KB; memory pressure was not the
frame-rate limit in this sample.

## Render optimization result

The full-screen render paths now use a 1,024-entry sine lookup table, cached
RGB565 hue palettes, an inexpensive radial-distance approximation, and band
tests that avoid per-pixel `hypotf`/`fmodf`. Frame scheduling now retains a
stable deadline so small web/Wi-Fi delays do not accumulate as timing drift.

At the original 45 FPS setting, all built-in patterns now sustain the target
with no render-budget overruns in the sampled windows:

| Pattern | Before | After | Optimized average render |
| --- | ---: | ---: | ---: |
| Starfield | 44.0 FPS | 45.2 FPS | 10.95 ms |
| Warped Checkerboard | 33.6 FPS | 45.0 FPS | 14.75 ms |
| Moiré Rings | 33.5 FPS | 45.0 FPS | 13.78 ms |
| Moiré Lines | 33.6 FPS | 45.0 FPS | 13.11 ms |
| Warping Checkers | 33.6 FPS | 44.7 FPS | 12.57 ms |
| Shockwave | 13.7 FPS | 45.0 FPS | 13.71 ms |
| Warp Corridors | 11.1 FPS | 44.9 FPS | 13.40 ms |
| Pixel Avalanche | 43.0 FPS | 45.0 FPS | 10.93 ms |
| Kaleidoscope | 43.0 FPS | 45.1 FPS | 9.65 ms |

A second hardware run at a 60 FPS target sustained approximately 60 FPS for
all nine C++ patterns. Hello Lua sustained 60.5 FPS. Optimized average render
costs ranged from 9.7 to 13.2 ms at that target. Occasional worst-frame budget
crossings coincided with live HTTP diagnostics, but did not lower the measured
one-second FPS.

The lookup tables add about 4.5 KB of static RAM. The optimized build uses
89,920 of 327,680 static bytes (27.4%) and 983,197 bytes of the current
2,097,152-byte application partition (46.9%). Five-bit Protomatter output is
retained because the firmware already reaches 60 FPS without reducing color
precision.

## Dithering, palettes, and high-FPS experiment

Two additional built-in patterns are available:

- **Bayer Flow** uses an animated 8×8 ordered-threshold matrix.
- **Floyd Flow** uses serpentine Floyd–Steinberg error diffusion with two to
  six quantization levels.

Every built-in pattern now has a palette selector, base hue, and hue-spread
control. Six firmware-side palette families are available: Spectrum,
Analogous, Split complement, Sunset, Ocean, and Neon. A 256-entry RGB565 cache
is rebuilt only when the palette controls or brightness change, so pixels use
a constant-time lookup during normal rendering. Patch persistence was migrated
from version 7 to version 8 without discarding the existing nine-pattern patch
data.

The dashboard accepts requested frame rates from 1 to 240 FPS and reports draw
time separately from framebuffer-swap wait. Hardware tests establish these
limits on the 64×64 MatrixPortal S3 reference device:

- Five-bit PWM: continuous swaps plateau around 65–67 FPS, whether the request
  is 120 or 240 FPS.
- Four-bit PWM: sparse Starfield sustains 118.8 FPS at a 120 FPS request and
  reaches 124.8 FPS at a 240 FPS request.
- Four-bit full-screen samples at a 120 FPS request ranged from about 63 FPS
  (Warped Checkerboard, Bayer Flow, Floyd Flow) to 95.5 FPS (Warp Corridors).

The split telemetry explains the difference. At four bits, Starfield draws in
about 0.31 ms and waits about 5.62 ms for presentation. Warped Checkerboard
draws in about 6.08 ms and then waits about 8.39 ms because it misses the next
scan boundary. Floyd Flow draws in about 8.55 ms and waits about 6.02 ms.

Therefore 120 FPS is a valid per-pattern capability, not a universal setting.
240 FPS is above the measured panel-swap ceiling for this 64×64/four-bit
configuration. The normal build remains five-bit; a
`matrixportal_s3_hub75_4bit_test` profile exists for controlled visual testing.

The current build uses 90,760 of 327,680 static RAM bytes (27.7%) and 991,109
of the 2,097,152-byte application slot (47.3%).

## Native pattern and dashboard update pass

The core native engine contains sixteen patterns. **Pixel Avalanche** alternates
between white grains filling black and black grains filling white without a
blank reset frame. **Snake** autonomously plays a Nokia-era grid game with a red
apple, palette-driven body, collision resets, one-pixel cells, and controls for speed,
maximum length, and apple-seeking behavior. **Squares** renders independently
paced colored outlines expanding from the center as a perspective tunnel.

Bayer Flow now bilinearly interpolates adjacent ordered-threshold cells during
fractional drift. This removes the previous whole-pixel threshold jumps while
keeping the per-frame inner pixel loop table-based. Each pattern also has a
one-click command that overwrites its selected auto-cycle patch with the current
parameters, making that state the permanent default.

A monotonic state revision is included in the one-second diagnostics response.
Every successful local API change and every automatic pattern transition bumps
the revision; open dashboards fetch fresh controls whenever it changes. The
same telemetry now samples Protomatter's electrical frame counter separately
from animation presentation FPS. On the four-bit reference build at a 60 FPS
animation target, live measurements were approximately 126–127 Hz electrical
refresh. Snake and Squares both sustained 60 FPS at about 38–39% of the render
budget; smoothed Bayer Flow sustained 59.6 FPS at about 69%.

Persistence format version 10 retains all version 9 patches, forces Snake to
one-pixel cells, and appends **Perspective Flow**, a native 64×64 C++ adaptation
of @martinnlopez's CC-BY-SA-4.0 PatternFlow animation. Its harmonic inner loop
uses the firmware sine table and a cached 256-level grayscale palette. On the
four-bit hardware profile it sustained 59.9 FPS at the 60 FPS target, with an
average 14.0 ms render cost including the framebuffer swap. The updated build
uses 92,832 of 327,680 static RAM bytes (28.3%) and 1,001,065 of the
2,097,152-byte application slot (47.7%).

## MatrixPortal pattern storage capacity

The board's current 8 MB partition table contains two 2,048 KB OTA application
slots, a 256 KB TinyUF2 application, and a 3,776 KB filesystem partition. The
filesystem is currently typed as FAT; the planned LittleFS profile can retain
approximately the same 3.69 MiB capacity. Pattern packages are expected to be
only a few kilobytes each, leaving practical room for hundreds of Lua patterns
even after reserving staging space, manifests, playlists, and catalog metadata.

## Per-pattern controls and editable palettes

All sixteen native patterns now expose exactly eight controls in the Pattern
Parameters section. Renderer-specific controls are supplemented by shared
spatial zoom, rotation, offset, and symmetry controls where necessary. The
spatial transformation is applied through the common pixel output path, so it
works consistently across every native renderer while preserving a fast path
for the default transform.

The palette section remains separate from those eight controls. Every one of
the seven palette choices owns four editable gradient stops per pattern and
patch; changing the selector reveals that palette's own colors. Palette phase,
span, global hue shift, and global saturation are applied through the same
cached 256-color lookup table. Pixel Avalanche defaults to an all-white Custom
gradient to preserve its black/white behavior; Perspective Flow defaults to a
grayscale Custom gradient. Both can still select and edit any other palette.

Pattern parameters, LFO settings, palette selection, all 28 gradient stops,
palette phase/span, global hue/saturation tuning, and spatial style are saved
together in each of the three patch slots. The one-click permanent-default
command overwrites the currently selected auto-cycle patch with that complete
state. Playback FPS, auto-cycle, cycle timing, legacy re-sync, and device
networking remain device-global.

Dashboard range inputs update their local readout during a drag but send a
firmware command only on the final change/release event. This avoids flooding
the ESP32 HTTP server while retaining immediate visual feedback in the browser.

## ESP32-WROOM WS2812 profiles and Patternflow collection

The shared source now compiles for 16×16 and 22×22 ESP32-WROOM WS2812 panels.
Both profiles reproduce the known-working reference wiring: GPIO 22, 800 kHz
GRB pixels, top-left origin, horizontal serpentine rows, and 50/255 conservative
startup brightness. They use two 1.5 MB OTA slots and a 960 KB LittleFS data
partition. LittleFS mounts without automatic formatting and its capacity is
reported in diagnostics.

All twenty visually distinct algorithms from the user's Patternflow reference
sketch are available inside **Patternflow Collection**. Its variant selector
loads the original four meaningful defaults/labels for each algorithm. The
renderer is resolution independent, supports the shared spatial transform,
LFO and audio routing, and maps its output through the same per-pattern editable
palette system as the core animations. The collection remains CC-BY-SA-4.0;
attribution is recorded in `THIRD_PARTY_NOTICES.md`.

Verified build footprints:

| Profile | Static RAM | Firmware flash |
| --- | ---: | ---: |
| ESP32-WROOM WS2812 16×16 | 84,612 bytes | 1,154,025 / 1,572,864 bytes |
| ESP32-WROOM WS2812 22×22 | 85,780 bytes | 1,154,177 / 1,572,864 bytes |
| MatrixPortal S3 HUB75 5-bit | 115,760 bytes | 1,087,573 / 2,097,152 bytes |
| MatrixPortal S3 HUB75 3-bit | 115,760 bytes | 1,087,933 / 2,097,152 bytes |

All four builds complete successfully. The two WS2812 profiles still require a
physical orientation/color-order test before being marked hardware verified.

## Audio v2, native macOS capture, and local OTA

Audio protocol v2 carries ten independently routable 8-bit signals: six energy
bands (sub, bass, low-mid, mid, presence, air), kick/snare/hi-hat spectral-flux
transients, and overall RMS loudness. It uses an 18-byte binary packet over the
browser WebSocket or native UDP port 82, plus a readable `AUD2` USB record.
Version-1 three-band packets remain accepted and are mapped into the expanded
state. Existing saved three-band patch routes migrate to bass, mid, and air.

The browser companion now analyzes and displays all ten signals. A native universal
macOS companion uses Core Audio process taps and UDP. It currently advances
on a 256-sample hop at 187.5 packets per second. Its Objective-C++ app bundle builds with
the Apple Command Line Tools, is ad-hoc signed, and requires neither an App
Store submission nor a developer account for personal use.

Native companion version 0.2 added an unsmoothed-by-default response path:
attack and decay are 0 ms for all six spectral bands and overall RMS. Each band
now has saved low/high frequency, gain, gate, attack, and decay controls. The
Monitor tab plots the 1,024 positive-frequency FFT bins as a logarithmic dBFS
spectrum.
Kick, snare, and hi-hat slots now carry configurable threshold-triggered
attack–hold–decay envelopes. Dashboard labels identify these as trigger sources;
their signed per-pattern routing remains compatible with every existing patch.

Native companion version 0.3 fixes the AppKit window hierarchy crash and adds
reliable reopen/activation behavior. It polls matrix diagnostics to distinguish
controller reachability from receiver-confirmed UDP, reporting RTT, packet
rate, payload throughput, and sequence lag. Trigger readouts are labeled as
boost envelopes rather than ambiguous percentages.

Native companion version 0.4 replaces the broad-band prototype with the
project's earlier musical detector architecture: 2,048-point SuperFlux at a
512-sample hop, adaptive per-drum thresholds, low/mid/high classification,
kick-leak rejection, refractory timing, velocity-aware AHD envelopes, and a
Unity-inspired buffered Flow energy signal. Only these four controls are shown;
the AUD2 packet remains 18 bytes for firmware and saved-route compatibility.
Version 0.4.1 separates event visualization from modulation: kick, snare, and
hi-hat now flash binary green indicators, while their velocity-aware AHD
envelopes continue unseen as the continuous values routed by the dashboard.
Detection and envelope controls are grouped per instrument on the main page.

Native companion version 0.5 moves kick onset to a dedicated 512-point
raised-Hann FFT while retaining the 2,048-point path for snare, hi-hat, Flow
energy, and the spectrum. Its one-shot trigger envelopes are now AHDSR with an
adjustable initial Impact peak, live curve visualization, sliders, and precise
number fields. The synthetic detector suite reports 20 kick, 12 snare, and 19
hi-hat events and detects an isolated kick 2.67 ms after onset under ideal
512-sample delivery.

Native companion version 0.6 adds an alternative predecoded MP3 source for
latency calibration. The supplied 5:21 Dreamscapes file is copied into the app
bundle, decoded to mono 48 kHz PCM, and analyzed ahead of the original MP3's
audible playhead. Version 0.7 adds timestamped look-ahead buffering and saved
offsets for kick, snare, hi-hat, and Flow. Version 0.7.1 expands their range to
±1,000 ms, deduplicates file-timeline event flashes, narrows the envelope graph,
and moves kick analysis to a 1,024-point raised-Hann FFT with a 250 ms default
minimum interval and 750 ms cold-start learning period. Negative offsets emit
the complete envelope before its audible event; the default is -20 ms. Hardware
verification confirmed receiver-side UDP input while Flow Sphere sustained
approximately 59.6 FPS.

Native companion version 0.8 adds a true in-series live route on macOS 14.2+.
It can tap one running application or the whole system while excluding itself,
mute the source's direct hardware path only while actively relaying it, and play
through a selectable physical output after a saved 0–1,000 ms stereo delay.
The default is 120 ms. A lock-free FIFO keeps FFT work off Core Audio's
real-time callback, and non-48-kHz tap streams are resampled for the existing
48-kHz detector. Analysis remains immediate, giving the LED path real look-ahead
relative to the delayed audible signal.

Firmware persistence version 13 appends **Flow Sphere**, validates expanded
audio-route tables during migration, and retains the fourteen earlier visual
patches. Flow Sphere ports the core musical relationship from the user's Unity
`AudioPeer`/`AudioFlowfield`/`NoiseFlowfield` chain: an evolving multi-axis
field is projected onto a sphere, with kick and snare envelopes routed to flow
speed by default. On the 240 MHz MatrixPortal S3 it used 107,632 of 327,680 RAM
bytes (32.8%) and 1,067,485 of 2,097,152 application bytes (50.9%). A live
60-FPS test measured approximately 11.8 ms average total render time and no
frame-budget overruns.

Firmware persistence version 14 appends **2D Flow Field** without discarding
the fifteen version-13 pattern patches. The renderer advances persistent
particles through a planar curl field and maps the native companion's buffered
Flow energy to flow speed by default. The 3-bit build uses 114,736 of 327,680
static RAM bytes (35.0%) and 1,070,785 of 2,097,152 application bytes (51.1%).

The palette pipeline now treats editable gradient stops as the authoritative
color source. Legacy per-pattern saturation constants no longer multiply down
their chroma; Global saturation at 100% preserves the selected palette, while
phase/span and pattern field values choose where and how brightly it appears.

The firmware also includes a local-network ArduinoOTA receiver. During a write,
the matrix displays **INSTALLING UPDATE**, auto-cycle pauses, and successful OTA
reboots automatically. The measured incremental cost is 1,872 static RAM bytes
and 10,800 flash bytes. The complete 3-bit build uses 106,680 of 327,680 static
RAM bytes (32.6%) and 1,063,265 of the 2,097,152-byte application slot (50.7%).
The upload chooser prefers a detected MatrixPortal USB serial device and uses
the `.local` OTA endpoint only when USB is absent.

Hardware verification completed on the reference MatrixPortal S3: the first
OTA-capable image was installed over `/dev/cu.usbmodem114201`, all flash hashes
verified, and a subsequent 1,063,632-byte upload over Wi-Fi OTA returned
`Result: OK`. After reboot, diagnostics reported `otaReady: true`, 3-bit color,
approximately 195.5 Hz electrical refresh, and 60.2 animation FPS.

The four-bit MatrixPortal S3 build was compiled, flashed, and tested on the
reference panel. It uses 93,888 of 327,680 static RAM bytes (28.7%) and
1,005,577 of the 2,097,152-byte application slot (47.9%). The live dashboard
reported 60.1 FPS at a 60 FPS target and approximately 126.1 Hz HUB75 electrical
refresh during the Starfield verification sample.

## Three-bit output and transport measurements

The first three-band transport was hardware-tested before its version-2
expansion. A Wi-Fi packet containing `64,128,192` read back as
`0.251,0.502,0.753`, source `wifi`, and sequence 42 while Starfield sustained
60.0 FPS and about 194.9 Hz electrical refresh. Version 2 preserves that parser
as a compatibility path and exposes all ten current levels in `/diagnostics`.

A separate three-bit test profile compiles with a 4×4 ordered native-pattern output dither.
The dither retains apparent shadow gradations that plain three-bit truncation
would collapse, at the cost of a fine spatial texture. It was flashed to the
reference panel and measured approximately 195.7 Hz electrical refresh while
Starfield sustained 60.3 FPS. Visual preference between three-bit+dither and
four-bit remains a user-facing panel test.

The four-bit test build now includes a diagnostic screen with simulated
three-bit ramps on the left and native four-bit ramps on the right. It shows
full-range grayscale, low-light grayscale, and a moving RGB gradient. On
installation it measured about 125 Hz electrical refresh while presenting the
test at 60 FPS.

Audio routes include brightness, all animated renderer parameters, Squares line
thickness, and shared zoom/rotation/offset/symmetry controls. Gain spans -200%
through +200% and defaults to 0%. Dashboard diagnostics display all ten levels
as live bars.

With the four-bit firmware rendering at 60 FPS, 200 sequential ping round trips
measured 13.45 ms median / 28.06 ms p95 over local Wi-Fi WebSocket and 2.07 ms
median / 10.77 ms p95 over USB serial. These measure firmware control round
trips rather than one-way audio latency, but quantify the additional Wi-Fi jitter.

## Multi-device playback and gallery

The ESP32-WROOM WS2812 backend now builds for both 16×16 and 22×22 matrices with
GPIO 22, GRB pixels, and top-left horizontal zigzag mapping. A 4 MB partition
layout supplies two 1.5 MB OTA slots and 960 KB LittleFS. Arduino IDE entry
sketches share the exact firmware source and include ArduinoOTA; the initial
partition/app installation remains a USB operation.

The dashboard replaces the long selector with a searchable 58-card gallery.
Each card animates a six-frame, 28×28 sequence sampled from the physical
firmware renderer over USB. The compressed atlas is embedded in firmware and
served identically over Wi-Fi or the USB dashboard bridge. All 42 native
collection variants appear as separate cards, expose eight visible controls,
and are traversed as separate auto-cycle stops.

Auto-cycle transitions now fade to black for 900 ms, switch at black, and fade
the new animation in over 1,600 ms. A saved Berlin-time schedule adds independent
1–3,600 second startup/shutdown fades. Both paths use a backend output scale,
so they do not rewrite palette or pattern parameters.

WS2812 output remains explicit: pattern brightness and scheduled fades control
the transmitted RGB levels, without a software current predictor or automatic
limiter. The updated 16×16 PlatformIO build uses 101,680 RAM bytes and
1,255,841 bytes of its 1,572,864-byte OTA slot; the 22×22 WROOM build uses
103,296 RAM bytes and 1,255,969 bytes of the same-sized OTA slot. The
MatrixPortal-S3 3-bit build uses 140,312 RAM bytes and 1,189,273 bytes of its
2,097,152-byte slot. All three
compile successfully. Arduino core 3.3.10 also compiles both WROOM wrappers.

The native collection now also includes the GPU-derived **Clifford Glass
Torus**, using a quality-controlled low-resolution SDF raymarch with bilinear
reconstruction, plus the MIT-licensed PsiOp demonstration scenes **Wire Cube**,
**Death Star**, and **Color Cube**. Each port uses editable project palettes
and four pattern-specific controls.

Thirteen additional variants were ported from the supplied WS2812 reference
sketch. Native trigonometric lookup, fast vector math, selective low-resolution
reconstruction, face culling, and specialized shader caching bring every one
of the 58 gallery entries above 60 FPS on the MatrixPortal S3 at an uncapped
240 FPS benchmark target. The slowest full-pass result was Warped Checkerboard
at 62.8 FPS; the separately rechecked Clifford Glass Torus reaches 76.2 FPS.
Full measurements are in `docs/pattern-benchmark.json` and
`docs/pattern-benchmark-torus-final.json`.

The current three-bit HUB75 profile no longer applies global low-light spatial
dithering. Bayer Flow and Floyd Flow remain the only intentional dithered
animations; all other patterns retain their direct palette output.

## Deliberately not included yet

- LittleFS pattern loading
- Pattern manifest parsing on the device
- Local upload or GitHub catalog installation
- Dynamic Lua parameters in the web UI
- Per-route response curves (the companion already has per-band attack/decay)
- A MAX7219 backend
- Hardware verification on the physical 16×16 and 22×22 WS2812 panels
