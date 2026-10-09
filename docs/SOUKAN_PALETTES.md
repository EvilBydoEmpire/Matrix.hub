# Soukan palettes

Ten four-color palettes based on the five user-supplied photographs, in upload
order, left page before right page. `SoukanPalettes.h` is the source of truth.
The RGB values are visually adapted sRGB/LED approximations, not authoritative
measurements of printed ink. Sampling avoided labels, paper and specular glare;
the warm photographed paper cast was reduced while retaining muted pigments.

| Palette | Printed scheme | Character |
|---|---|---|
| Soukan 1 | 255 | Calamine blue, black, pyrite yellow, raw sienna |
| Soukan 2 | 256 | Orange, cinnamon, viridian, black |
| Soukan 3 | 249 | Medici blue, ecru, olive ochre, russet |
| Soukan 4 | 250 | Nile blue, sea green, pyrite yellow, peach red |
| Soukan 5 | 247 | Benzol green, deep Lyons blue, apricot yellow, sienna |
| Soukan 6 | 248 | Lavender, eosine pink, slate purple, khaki |
| Soukan 7 | 243 | Slate, olive, ivory buff, sienna |
| Soukan 8 | 244 | Antwerp blue, Antwerp green, light brown drab, tawny |
| Soukan 9 | 241 | Medici blue, pale lemon, red orange, Isabella |
| Soukan 10 | 242 | Diamine green, eosine pink, burnt sienna, black |

## Color interpretation

Only Soukan palettes use pigment plateaus and smooth, shorter transitions.
Existing palettes keep their original interpolation. Palette expansion is baked
into the 256-entry hue LUT and 4096-entry RGB LUT, not added to per-pixel render
loops. Narrow-band effects have a curated domain expansion; motion, spatial
parameters and user brightness are not changed. Full black stays black.

Photographs use a luminance-ordered version of the four colors, preserving tonal
order instead of mapping skin and shadows through arbitrary hue. Their original
source pixels are unchanged; select **Original** to restore original colors.
Grey source graphics also use a tonal ramp. Snake uses separate body/head/apple
palette roles to avoid black holes in its one-pixel body.
Dark graphic canvases are capped at their original intensity, so a near-black
background cannot become a full-strength pigment. Solar Flare starts in the
yellow portion of Soukan 5; thin Whitney filaments use Soukan 6. Ripples use one
shared RGB limiting factor at overlaps instead of independently saturating each
channel, preserving the ring's hue instead of progressively whitening it.

The palette editor still exposes four editable stops, hue, span and saturation.
All 59 compiled gallery entries (including archived entries) receive a curated
Soukan default; existing IDs are not reordered. Device calibration, bit depth,
clock phase and output orientation are untouched.

## Default migration and persistence

A color-only compiled default overlay avoids rewriting 59 large preset records
in the small NVS partition. Motion/style/LFO settings are loaded from the user's
existing default slot unchanged. Existing stored colors are not erased during
the upgrade, and non-default patch slots remain unchanged.

Saving a default after this upgrade, or explicitly choosing another default
slot, marks that stable pattern ID in the 10-byte `soukan_ud` preference. The
user's saved palette (including a legacy palette) then takes precedence over
the overlay on every subsequent boot. Custom stops use the existing sparse
palette formats; extending from 11 to 21 banks does not change stored-record
layout or move old palette IDs. Bookmark/rating/tag/archive storage is untouched.

## Verification

`test_soukan.cpp` checks assignments, exact pigment anchors, periodic sampling,
tonal ordering and black shadows. `review_soukan.py` snapshots the old defaults,
checks all non-color parameters after migration, and records native RGB565
hardware snapshots for visual review. Review images normalize brightness using
one equal RGB gain; they are not a physical measurement of the panel's gamut.
The final on-device color appearance still depends on ambient light and LEDs.

`/preview?format=rgb565-native` exports the existing frame buffer as little-endian
RGB565 without a large JSON allocation. The JSON RGB888 form remains available.
These snapshots are requested only for review; they add no background capture
task or steady-state rendering overhead.

`preview_soukan.py` records 24 real frames per gallery entry at a requested
67 ms cadence. Actual capture intervals are reported, not assumed. A single
equal-channel brightness normalization is used for thumbnails; there is no
per-channel gamma transform, RGB332 conversion, or added dither. The editable
source atlas remains lossless RGB. Firmware embeds an adaptive-indexed PNG:
28px thumbnail tiles on HUB75 (462 KB) and 16px tiles on the flash-constrained
WROOM targets (178 KB). Both retain 24 frames at about 15 preview FPS and 256
adaptive colors. Mean RGB quantization error is 2.54/255 for HUB75 and 2.68/255
for WROOM. `pack_soukan_preview.py` regenerates both profiles from one capture.
This avoids extra rendering work on the ESP32 while browsing the gallery.

### Final device checks (October 4)

All 59 defaults passed the post-reboot non-color-preset/metadata audit.
Editable Soukan and legacy Original defaults both survived an OTA reboot;
temporary test colors were restored and read back. Capture median was
70.48 ms/frame for 24-frame loops. The deployed atlas CRC matches the generated
HUB75 asset. Dashboard checks verified all ten labels and both tile sizes.

All four builds passed: HUB75 DMA / Protomatter and WS2812 16×16 / 22×22.
DMA uses 1,720,781 / 2,097,152 flash bytes; WROOM uses about 1,497,500 /
1,572,864 bytes. No new library dependency, output dithering or calibration
change was introduced. Host UB/bounds tests passed all 63 internal renderer
IDs at all three matrix sizes, plus the additive-ripple color regression test.

Native on-device spot checks (target 240 for benchmarking, subsequently restored
to 120) measured Checkerboard 99 FPS, Chromatic Vortex 110, Chromatic Bloom 124,
Ripples 120, photo 142, and the three Acid variants 71 / 70 / 82 FPS. These are
rendering measurements, not guaranteed rates for every parameter combination.
See `soukan-benchmark.json`, `soukan-review.json`, `soukan-persistence.json`,
`soukan-preview-capture.json` and `soukan-preview-encoding.json`.
