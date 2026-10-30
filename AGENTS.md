# Matrix.hub

Shared ESP32 firmware, LED patterns and the browser app for Matrix.16, Matrix.22 and Matrix.64. `src/main.cpp` holds the firmware and HTTP handlers for every size, and the build environment selects the panel. [README.md](README.md) maps each device to its environment and host and holds the build, update and test commands; the guides in `docs/` cover architecture, the app, hardware and the pattern library, and open work lives in `docs/UPDATE.md`.

## Hardware

- Confirm the device and the intended change before any hardware write. `.venv/bin/python tools/matrix_hub.py inspect` checks profile, size, backend and OTA readiness. Matrices of the same size share a hostname, and a router or Bonjour entry can outlive a device's connection, so trust an address only when `/control` and `/diagnostics` answer.
- Preserve OTA reception, OTA-capable partitions, NVS and saved networks. OTA replaces only the application; partition changes need USB and a flash backup. Raising `PatternStore::Version` resets every device's settings to defaults on its next boot, keeping Wi-Fi networks and the white balance, so raise it only when the stored layout changes.
- Opening, closing or resetting the WROOM's USB serial port with other tools can leave it in download mode, dark until a power cycle; `tools/open_usb_app.py` restarts it into normal boot and closes cleanly. Restart boards through an OTA update and use serial only to recover a board that Wi-Fi cannot reach.
- Physical panel orientation is set where frames leave for the panel, never per pattern.

## Stable identities

Stored settings key on pattern and palette IDs, so never renumber them, and keep retired IDs reserved. [docs/PATTERN_LIBRARY.md](docs/PATTERN_LIBRARY.md) lists every place a pattern is registered.

## Editing

- `web/` is the app's source. Every PlatformIO build runs `tools/build_web.py`, which regenerates `web/palettes.js`, `web/gallery.js` and `src/generated/WebAssets.h`; commit them with the source. The build also stamps the firmware with `git describe`, so a device reports the commit it runs.
- After changing a pattern, a palette or the gallery, run `tools/render_previews.py` and commit the regenerated thumbnails; the host tests fail while they are older than their sources.
- Code a pattern calls for every pixel inlines or runs from IRAM, and pixel loops keep divisions and libm calls outside; [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) explains the flash cache behind this.
- Keep secrets, flash and NVS backups and personal images out of commits; backups contain Wi-Fi passwords. Preserve imported licences and attribution in `docs/THIRD_PARTY_NOTICES.md`.
- Temporary output belongs in `.local/`; addresses, ports and verification notes in the ignored `LOCAL.md`.
- Branch names are dash-case; commits are brief, sentence-case and start with a verb. Timestamps use Europe/Berlin.
- Keep the pinned platform and library versions unless the task changes dependencies.

## Validation

`tools/run_host_tests.sh` runs every host test and stops at the first failure.

Firmware changes need sequential builds of all three environments. App changes also need a browser pass at phone and desktop widths in light and dark; [docs/APP.md](docs/APP.md) describes the preview server. Report host checks, builds and device verification separately.
