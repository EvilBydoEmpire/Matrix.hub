# Third-Party Notices

The app shows no credits; this file carries them. Where a pattern's name in the app differs from its source, the original title follows in parentheses.

## Licences

This project's own code, app and documentation are licensed under the MIT licence in [LICENSE.txt](../LICENSE.txt). Work adapted from others keeps the terms its authors gave it, as the sections below record; the MIT licence does not extend to it:

- Patternflow adaptations and remixes are CC-BY-SA-4.0, together with their sources in `patterns/imports/patternflow/`, `src/generated/CollatzTopology.h`, the `tools/generate_collatz.py` that builds it, and their thumbnails.
- Pong Wars and Death Star keep the MIT licences and copyright notices of their originals.
- Motion Graphics 1968, Chromatic Bloom, Ripples, Raindrops and Plasma adapt privately supplied sketches that came without a licence, so they remain under their authors' terms.
- The libraries the build fetches keep their own licences, listed under [Build dependencies](#build-dependencies).

## Patternflow

Patternflow works are licensed under [CC-BY-SA-4.0](https://creativecommons.org/licenses/by-sa/4.0/), and so are their adaptations and remixes here. Originals were published in the [Patternflow Community](https://patternflow.work/community). The ports are resolution-independent, bounded-memory C++ that routes colour through this project's palettes, rather than pixel-exact emulations of the JavaScript. Unchanged sources live in `patterns/imports/patternflow/`.

- Vector Field Flow, Chromatic Vortex, Liquid Ripple, Kaleidoscope, Isometric Pillars, Colour Ribbons, Folded Cross, Bitwise Shards, Pendulum Waves, Nachtschwärmer, Cathedral Glass, Tidal Topography and Whitney Harmonics adapt animations of the private Arduino sketch `LED_Matrix_22x22_11_patternflow.ino`, itself derived from [Patternflow by Seunghun LEE / engmung](https://github.com/engmung/Patternflow), and keep its four-control model.
- Blooming Bioluminescence by @WollyMeister (2026-08-18) keeps its moving Lissajous and Voronoi organisms, breathing membranes and domain-warped plasma.
- Chladni Acoustic Waves by @engmung (2026-07-31) keeps the modulated two-mode Chladni nodal equation and adjustable edge sharpness.
- Pulsating Solar Flare by @martinnlopez (2026-08-03) keeps the golden-angle particles, radial pulse and bloom.
- In `src/patterns/PatternflowImports.inc`: Vega Lens (Vega444_C) by @WollyMeister; Twisted Tunnel by @kpow; Soliton Lattice Echo, Ripple Cell Grid (RippleCellGrid2), Poolside and Collatz Kelp by @engmung; Squircle Spacetime Warp (Op-Art Squircle Spacetime Warp) and Topo Vortex (Bioluminescent Topo Vortex) by @martinnlopez; Hypno Circles by @SimonePDA. `src/generated/CollatzTopology.h` derives from Collatz Kelp.

## Remixes and further studies

`src/patterns/PatternRemixes.inc` and `src/patterns/AdvancedStudies.inc` hold separately named derivatives. Harmonic Canon, Chladni Resonance and Abyssal Vortex derive from Patternflow works and carry the CC-BY-SA-4.0 terms above.

- Chladni Resonance, with separable morphing resonant modes, derives from @engmung.
- Abyssal Vortex keeps Topo Vortex's two centres, dashed contours and green, cyan and blue gradient and adds counterflow, braiding and light packets; it derives from @martinnlopez.
- Harmonic Canon refines the Whitney Harmonics adaptation from the private Arduino sketch with analytic trails and sub-pixel harmonic motion.
- Satin, Interference and Infinite Squares develop this project's native renderers.
- Ripples adapts a five-ring Arduino algorithm, and Raindrops and Plasma adapt the Ripples and Plasma effects of the Arduino sketch `LED_Matrix_08.ino`. Both were supplied privately without a licence, so all three remain under their authors' terms.

## John Whitney studies

`src/patterns/WhitneyStudies.inc` adapts two privately supplied JavaScript sketches, kept with normalised formatting in `patterns/imports/whitney/`.

- Motion Graphics 1968 is Kenny Kim's recreation of John Whitney's *Experiments in Motion Graphics* (1968), with this [film reference](https://www.youtube.com/watch?v=jIv-EcX9tUs&t=264s). It keeps the two line families of 31 lines by default, their endpoint equations, cream and pink lines on teal black and the 4:3 composition, and moves continuously instead of pausing.
- Chromatic Bloom keeps the seven colours on their dark ground and the radial sine motion. Its particles form seven ribbons of 14–90 single pixels, mapped from polar coordinates onto the square, with petal folding and counterpoint between the ribbons.

Neither sketch came with a licence, and the author of the second is unknown; both remain under their authors' terms. John Whitney is credited for the artistic inspiration, not for these recreations or the firmware.

## Original studies

`src/patterns/NovaStudies.inc` holds this project's own renderers: Luminous Loom, Ferrofluid, Accretion, Origami Flight, Signal Orchard, Resonant Orbits, Square Orbits and Phase Architecture. The last three are new interpretations of John Whitney's differential motion and counterpoint and imply no authorship or endorsement by him. Acid Cathedral, Phase Slippage and Liquid Circuit in `AcidStudies.inc` and Chrono Iris, Rolling Silk and Lantern Garden in `FlashingStudies.inc` are this project's own as well.

## Pong Wars

`src/patterns/PongWars.inc` adapts [Pong Wars](https://github.com/vnglst/pong-wars) by Koen van Gilst (2024) under the MIT licence, kept in full in `patterns/imports/pong-wars/LICENSE`. The opposing balls, tile conversion and pale mint and navy look follow [this visual reference](https://x.com/denicmarko/status/2093698283000787295); collisions, fixed-step timing, compact board storage, palettes and controls are new.

## PsiOp

Death Star adapts the point-cloud sphere with equator and recessed dish from the demonstration scenes of [PsiOp by aap](https://github.com/aap/psiop) under the MIT licence, kept in full in `patterns/imports/psiop/LICENSE`. It draws through this project's pixel writer and palettes instead of the ST7735 framebuffer and software rasteriser.

## Build dependencies

PlatformIO fetches these libraries at the versions `platformio.ini` pins. They are not part of this repository, but every firmware image links them. Whoever distributes a compiled image passes on their notices and licence texts; for the LGPL libraries that includes their source and this repository, from which the image can be rebuilt with modified libraries.

| Library | Version | Licence | Builds |
| --- | --- | --- | --- |
| [Arduino core for ESP32](https://github.com/espressif/arduino-esp32) on ESP-IDF 4.4.7 | 2.0.17 | LGPL-2.1-or-later; ESP-IDF is Apache-2.0, and its components keep their own licences | All |
| [Adafruit GFX](https://github.com/adafruit/Adafruit-GFX-Library), including the built-in font of the update notice | 1.12.6 | BSD-2-Clause | All |
| [Adafruit BusIO](https://github.com/adafruit/Adafruit_BusIO), required by GFX | 1.17.4 | MIT | All |
| [ArduinoJson](https://github.com/bblanchon/ArduinoJson) | 6.21.5 | MIT | All |
| [Adafruit NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel) | 1.13.0 | LGPL-3.0-or-later | WS2812B |
| [Adafruit NeoMatrix](https://github.com/adafruit/Adafruit_NeoMatrix) | 1.3.3 | LGPL-3.0-or-later | WS2812B |
| [ESP32-HUB75-MatrixPanel-DMA](https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA) at `f17fb7fe9d487e9643f919eb5aeedea8d9d1f8d7` | 3.0.14 | MIT | HUB75 |
