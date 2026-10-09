# Hardware profiles

Product names: **Matrix.16**, **Matrix.22**, **Matrix.64**, under **Matrix.hub
by Televator**. Their new local hostnames are `matrix-16.local`,
`matrix-22.local`, `matrix-64.local`. Stable internal profile IDs remain
unchanged. Historical measurements below retain the names used at the time.

## Standing requirement: wireless firmware updates

Every ESP32 installed for this project must support firmware updates over local
Wi-Fi without a USB connection. This applies to existing and future hardware
profiles, including WROOM WS2812 and MatrixPortal S3 HUB75 devices.

- Keep `MATRIX_ENABLE_OTA=1` (the shared firmware default) in deployed builds,
  initialize the receiver after Wi-Fi connects, and service it during playback.
- Install an OTA-capable partition table with two adequately sized application
  slots during initial USB provisioning. Preserve Wi-Fi credentials and user
  settings; back up flash before a partition migration.
- Verify the exact device/profile, network reachability and `otaReady` after
  provisioning. When practical, test a wireless upload and successful reboot;
  distinguish receiver readiness from an actually tested OTA transfer.
- Use the matching firmware profile and the device's verified hostname/IP for
  updates. USB is optional for ordinary updates, but remains the recovery path
  for broken firmware, unavailable Wi-Fi or partition-table changes.
- Never deploy the `matrixportal_s3_hub75_3bit_no_ota_measure` diagnostic build
  as a normal installation. Any future diagnostic disabling OTA must be
  temporary and followed by restoration of an OTA-enabled build.

Devices still need power and a reachable local Wi-Fi connection; OTA does not
provide remote Internet access. Do not expose the update receiver publicly.

### October 4: DMA edge correction

On this 64×64 panel, `clkphase=false` fixes the displaced physical shift column
that appeared as the top row after the 90° clockwise output rotation. The user
confirmed “Jetzt korrekt” in the full firmware. Retained: exact 20 MHz, 4-bit
depth, latch blanking 2, 90° output rotation and OE duty 80/255. No pixels are
clipped, shifted in software or hidden. The pinned upstream driver's README
documents this clock-phase choice for one-pixel displacement/ghosting.

Library metadata writes may erase NVS flash sectors. The experimental DMA
profile briefly blanks/stops DMA around that blob write and resumes on success
or failure, following the existing OTA flash-operation strategy. An initial
write succeeded but its HTTP response timed out during a longer communication
pause; without serial logs this is not evidence of a confirmed DMA crash.

The platform uses one source tree with separate build artifacts. Partition tables and display drivers are selected at build time, so devices do not all receive the same binary.

## `matrixportal_s3_hub75`

- Controller: Adafruit MatrixPortal S3
- Display: 64×64 HUB75 RGB
- Backend: Adafruit Protomatter
- Flash: 8 MB on the current reference board
- PSRAM: available on the current reference board
- Status: working reference sketch

The existing MatrixPortal S3 HUB75 pins and double-buffered Protomatter configuration remain the baseline; the current hardware-verified depth is three bits per channel.

The measured default 8 MB partition table allocates 20 KB NVS, 8 KB OTA
metadata, two 2,048 KB OTA application slots, 256 KB TinyUF2, and 3,776 KB FAT
filesystem space. A future LittleFS table can retain the same filesystem size.

The current hardware-verified profile uses three PWM bits per channel for a
calmer HUB75 electrical refresh. Global spatial low-light dithering is disabled:
only the explicitly named **Bayer Flow** and **Floyd Flow** patterns use
dithering as an aesthetic algorithm. The `matrixportal_s3_hub75_4bit_test`
profile remains available for comparing the color-depth/refresh tradeoff.

### Isolated refresh experiments (2026-10-03)

The production three-bit baseline measured 194.83 Hz median electrical refresh
over 40 diagnostic requests, with 40 successful responses and no request errors.
Median HTTP response time was 92 ms (p95 227 ms). This is a short connectivity
check, not a long-term Wi-Fi stability result. Raw results are recorded in
`display-timing-3bit-baseline.json`.

`matrixportal_s3_hub75_4bit_20mhz_usb` is a compiled, isolated four-bit test.
It increases only the Protomatter LCD data clock from approximately 17.8 MHz to
20 MHz, retaining the existing timing safety margins. It does not overclock the
CPU. `tools/protomatter_timing.py` only patches this environment's pinned
Protomatter 1.7.1 dependency (and its corresponding OTA test environment), not
the production dependency. A 12.5% data-clock increase does not imply the same
increase in electrical refresh because other scan overhead remains.

`tools/hub75-dma-probe/` is a separate, compiled USB test project, not the normal
dashboard firmware. It pins ESP32-HUB75-MatrixPanel-DMA 3.0.14, uses four PWM
bits with no automatic depth reduction, and provides 8/16/20 MHz tests. It boots
with output disabled and stops display output automatically after two minutes.
After a stopped test, reboot before starting another. Serial commands include
`START20`, `START16`, `START8`, `STOP`, `CHART`, `PHOTO0`, `PHOTO1`, `PLASMA`,
`STATS`, and `REBOOT`. Wi-Fi credentials are read from NVS without changing them.
The probe exposes both the driver's calculated scan rate and a hardware PCNT
counter on row-address E; agreement must be checked on hardware before treating
the latter as validated. Photo assets remain private.

Both experiments require an identified MatrixPortal USB recovery connection
before uploading, particularly because the alternate library documents
MatrixPortal S3 Wi-Fi compatibility problems. Restore using the verified serial port:

```sh
.venv/bin/pio run -e matrixportal_s3_hub75_3bit_test -t upload --upload-port EXACT_MATRIX_PORT
```

Do not erase flash/NVS. Do not use unrelated serial ports. Measure each test
with `tools/benchmark_display_timing.py` and check the physical panel for
flicker/ghosting as well as Wi-Fi responsiveness before integrating a driver.

Hardware trials on 2026-10-03, after USB became available:

- Four-bit Protomatter at 20 MHz: median 116.65 Hz (111.67–119.33 Hz),
  40/40 HTTP responses, median 175 ms / p95 574 ms. Both the baseline and this
  trial ran Starfield. This candidate does not improve the refresh tradeoff.
- Isolated four-bit DMA at 20 MHz: driver calculated 887 Hz; **not independently
  verified** because the PCNT E counter remained zero. 0/30 HTTP responses
  during output. USB remained functional; HTTP recovered after USB STOP.
- Isolated four-bit DMA at 16 MHz: driver calculated 710 Hz; counter still
  unvalidated. 0/10 HTTP responses during output; USB STOP worked.

Reports: `display-timing-4bit-20mhz.json`,
`display-timing-dma-4bit-20mhz.json`, `display-timing-dma-4bit-16mhz.json`.
Neither experimental candidate is suitable for production as tested. The
DMA clock calculations must not be presented as measured refresh rates or
proof of flicker-free output. Optical feedback is still pending. Further work
needs to isolate pin/counter setup and MatrixPortal-specific Wi-Fi interference
before integrating the alternate driver. No persistent pattern settings or
Wi-Fi credentials were changed by the probe.

After the trials, production three-bit firmware was restored successfully over
USB. The final check returned 10/10 HTTP responses and approximately 190 Hz
median electrical refresh (`display-timing-3bit-restored.json`). The normal
55-pattern dashboard, gamma 1.8 photo correction, neutral RGB balance and
120 FPS target were present. This was the state before the follow-up below.

### Follow-up: corrected DMA clock and working full-firmware candidate

Two errors in the first probe were found and corrected:

1. On the pinned library's S3 backend, nominal `HZ_20M` actually selects
   divider **7**, i.e. 160/7 ≈ **22.86 MHz**, not 20 MHz. `S3_LCD_DIV_NUM=8`
   forces the actual 20 MHz clock. The old report's clock label was nominal.
2. The probe called `gpio_set_direction(INPUT_OUTPUT)` on address E after
   attaching DMA. ESP-IDF 4.4's output-enable implementation reattaches the pin
   to ordinary GPIO output, disconnecting LCD data. This caused the zero PCNT
   reading and compromised the earlier optical test. `gpio_ll_input_enable`
   now enables only the input gate, leaving LCD output routing intact.

Sources: the pinned library's
`src/platforms/esp32s3/gdma_lcd_parallel16.cpp`, and
[Espressif GPIO driver](https://github.com/espressif/esp-idf/blob/v4.4.7/components/driver/gpio.c).
The upstream [MatrixPortal/Wi-Fi discussion](https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA/discussions/258)
describes similar problems but does not prove the exact RF mechanism here.

Successful candidate: exact 20 MHz, original GPIO drive strength 3, Wi-Fi
modem sleep disabled, four bits, double buffering, internal SRAM, latch blanking
2, default clock phase, no driver CIE transform (photo gamma remains 1.8).
Lower GPIO drive strengths were worse in this setup and were rejected.
The independent E counter now measures approximately 888 full scans/sec,
consistent with the driver's calculated 887 Hz. The user reported the corrected
probe looked good. This is evidence for this board/network, not all environments.

- `dma-wifi-exact20-isolation.json`: short controlled comparisons.
- `dma-wifi-exact20-confirm.json`: 70/70 HTTP responses with plasma/photo.
- `dma-wifi-exact20-extended.json`: 280/280 responses, median 31 ms.
- `display-timing-dma-full-firmware.json`: full dashboard, 100/100 responses,
  median 25 ms / p95 44 ms; approximately 888 Hz scan, Starfield median 120 FPS,
  roughly 80 KB free heap.
- `dma-full-integration.json`: native pattern, photo, RGB channel calibration,
  Lua and test chart passed; 55-pattern catalog, bookmarks, current parameters
  and playback settings verified after restoring the prior selection.

`matrixportal_s3_hub75_dma_4bit_usb` is the separate full-firmware candidate.
`src/platform/DmaMatrix.h` preserves the RGB565 canvas interface (including
direct pixel access) and converts it into DMA bitplanes on show. Existing
pattern code and the production three-bit default are unchanged. Both the
three-bit profile and WS2812 16×16 still compile.

**Brightness caveat:** this initial experimental profile uses OE duty 80/255,
matching the validated dim probe, in addition to the existing pattern brightness.
It is not a full-power calibration and does not overwrite saved brightness values.
The automatic upload helper still selects the conservative three-bit profile;
explicitly select the DMA environment to upload the new candidate.

The probe now fixes its clock per build and rejects obsolete START16/START8
requests. Test scripts always stop output afterwards. The full firmware has no
two-minute cutoff. Long-duration testing and other Wi-Fi channels remain to be
validated before making this profile the default.

The full DMA candidate was uploaded over USB, then the identical application
was successfully installed over Wi-Fi OTA (exit status 0). After reboot,
`display-timing-dma-after-ota.json` recorded 20/20 HTTP responses, median
28 ms latency, 888 Hz median scan, and 120 FPS for Starfield. The complete
four-bit candidate is now installed, not the standalone probe. The prior
three-bit fallback binary/profile remains available. Dashboard interaction
tests (`tools/test_dashboard.cjs`) also pass. OTA success was tested; the
OTA-error resume path is implemented but has not been fault-injection tested.

### Panel mounting orientation

HUB75 output is now rotated **90 degrees clockwise** at the final output
boundary (`PanelOrientation.h`, `MATRIX_OUTPUT_ROTATION=1`). The DMA converter
maps coordinates directly; the Protomatter fallback uses a temporary rotated
copy and restores its logical canvas. Direct-buffer patterns, Lua, photographs,
calibration charts and update notices therefore share one physical orientation.
Pattern rotation remains a separate artistic parameter; WS2812 is unchanged.
`outputRotationDegrees` is reported by both control and diagnostics APIs.

All 55 active patterns / 165 preset slots were audited and backed up in
`pattern-rotation-before.json`. Only four slot-0 rotations were changed:
Pendulum Waves and both personal photos: 90 -> 0 degrees; Collatz Kelp:
100 -> 0 degrees, explicitly requested by the user. Other preset values,
bookmarks and default-slot selections were preserved. Corrections were saved
through the existing checked persistence API and reloaded for verification;
see `pattern-rotation-corrections.json`. There is no repeating boot migration.
The mapping test verifies all four rotations, all 4,096 coordinates, inverse
mapping and clockwise corner positions. Both HUB75 profiles compile. After
USB upload the device reported 90 degrees, and the 20-request check returned
20/20 successes with approximately 888 Hz electrical refresh.

## `esp32_wroom_ws2812`

- Controller: ESP32-WROOM-32 or compatible
- Displays: 16×16 WS2812 and 22×22 WS2812B
- Backend: Adafruit NeoMatrix/NeoPixel through the shared `DisplayBackend`
- Expected baseline: commonly 4 MB flash without PSRAM, but the exact module must be detected or declared
- Status: 16×16 installed and tested on ESP32-D0WD-V3 / 4 MB flash on October 9, 2026; 22×22 compiles, physical test pending

The known-working 22×22 reference establishes GPIO 22, 800 kHz WS2812 timing,
GRB channel order, top-left origin, horizontal rows, and alternating/serpentine
row direction. Both current profiles use those defaults. Width and height are
compile-time values, so other rectangular WS2812 matrices can be added without
changing the renderer. The NeoPixel buffer stays at full precision; rendered
pattern brightness remains explicitly controlled by the normal 0–15 scale.

The 22×22 panel contains 484 pixels and therefore spends approximately 14.5 ms
on each WS2812 wire transfer, giving a practical physical ceiling near 68 FPS.
The 16×16 panel's 256-pixel transfer is approximately 7.7 ms. Unlike HUB75,
WS2812 pixels hold their last value and have no separate electrical scan rate.

The 4 MB WROOM partition layout now contains two 1.75 MiB OTA slots and a
448 KiB LittleFS data partition. NVS remains at 0x9000 / 0x5000. The firmware
mounts it without automatic formatting. First installation or migration from
the earlier two-1.5-MiB / 960-KiB layout requires USB and a backup; do not migrate
partition boundaries with an ordinary application OTA update. Individual
pattern-package installation is still pending.

Thin Arduino IDE launch sketches for both sizes live under `arduino/`. Select
the ESP32 Dev Module's **Custom** partition scheme for the first USB upload so
the local partition CSV is installed. ArduinoOTA is an application feature,
not a replacement ROM bootloader: after that first USB installation, normal
firmware updates can be sent to `matrix-ws2812-16x16.local` or
`matrix-ws2812-22x22.local` without reconnecting USB.

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

Before implementing the MAX7219 backend, capture:

- Exact ESP32 board/module marking
- Reported flash size and whether PSRAM is present
- Existing sketch and display library
- SPI pin mapping and module arrangement/addressing order
