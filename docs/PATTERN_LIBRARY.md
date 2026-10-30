# Pattern Library

The gallery holds the native engines and the collection variants. `src/patterns/PatternCatalog.h` lists them in gallery order, which the thumbnails and the app's library follow. A pattern's stable ID is its native engine ID, or 16 + its collection variant. Retired IDs stay reserved, so a new pattern always takes a new ID.

## Controls and configuration

Each pattern has a few controls of its own, up to four in the collection, and each one visibly changes it. A control that only shifts colour, repeats another or vanishes at panel size stays at its default: a collection control without a label in its metadata is hidden, and the firmware refuses it. Colour adds the palette and Saturation, and brightness is a single device setting. Patterns animate themselves: no control is modulated automatically, and there are no composition transforms, render-quality or frame-rate settings. A pattern keeps one configuration, saved automatically without versions and loaded whenever it plays, by hand or by cycling, in the record [ARCHITECTURE.md](ARCHITECTURE.md) describes. Reset restores its compiled defaults and default palette.

## Palettes

`src/patterns/Palettes.h` defines 29 named palettes plus Original: ten warm and earthy ones, twelve that widen the range and seven single tones. Every palette orders its four stops shadow, body, highlight and accent. The colour palettes share one OKLCH lightness ladder: shadow (L .24–.36), body (.46–.68), highlight (.80–.96) and accent (.61–.77). A single tone, from Red to Magenta, keeps one hue: its highlight is the pure tone, and the other stops step it down in linear light to .45, .72 and .86 of its lightness. The app lists the colour palettes around the colour wheel, then the single tones. Cyclic sampling wraps from accent back to shadow without a jump, and two-role patterns such as Pong Wars set shadow against highlight. Saturation runs from grey through as designed to boosted. Hue comes from the palette alone, which `tools/test_collection.cpp` checks by rendering each pattern under a one-colour palette.

Original keeps a pattern's authored colours: `ArtDirection::Native` for native engines and `originalColors` in the collection metadata. Collection patterns that draw source RGB show it unchanged under Original and remap it by hue under every other palette.

Design colours as sRGB hex values by lightness role. At brightness 2–5 shadow stops nearly vanish, so a pattern's features belong in body and highlight. Judge palettes on the 16×16 panel behind its diffuser as well as on 64×64: a screen shows composition, and only the panel shows gamut and white balance.

## Flashing patterns

Chrono Iris, Rolling Silk and Lantern Garden, variants 65–67, flash. Pulse rate spans 0.25–8 Hz, Exposure width spans 8–80 % of a cycle, and Flash depth blends continuous light with pulsing. The defaults stay below 1 Hz at 16–20 % depth, and flashing light can still cause discomfort or seizures, so these patterns play only when picked, without a warning in the app: cycling skips them, a restart starts the gallery instead, and their thumbnails are single stills. `tools/test_collection.cpp` checks that cycling skips them.

## Thumbnails

`.venv/bin/python tools/render_previews.py` plays every pattern from its compiled defaults and palette with the firmware's own code, built with each firmware's panel geometry, and embeds one atlas per panel size in `src/generated/PatternPreviewAtlas.h`. Matrix.16 and Matrix.22 get the panel's own 16- and 22-pixel frames, Matrix.64 its 64-pixel frames averaged down to 28. A row holds 24 frames from the fifth second on, flashing patterns a single still, and each atlas is quantised until it fits its flash budget. The header records a digest of its sources, and `tools/test_preview_tools.py` fails until the atlas is rendered again after a pattern, palette or gallery change.

## Adding a pattern

1. Write the renderer in a `src/patterns/*.inc` module: up to four controls that each visibly change it, coordinates from width and height, elapsed time, bounded memory, and the shared palette and pixel writer. Leave colour to the palette, and integrate each rate into a phase, so moving a control never jumps the motion. Pixel x is centred at x, so the panel centre is ((w−1)/2, (h−1)/2) and positions round to the nearest pixel. Counts and spacings tuned at 64 pixels thin out on smaller panels, so features stay at least two pixels apart; scale a control's range with the panel rather than capping it, so it still changes the pattern at 16. Tune its defaults at 16, 22 and 64 pixels. For every pixel, call only the `FastMath.h` and `Rgb565.h` helpers, lambdas marked `PIXEL_INLINE` and functions marked `IRAM_ATTR`, and keep divisions and libm calls outside the loop; [ARCHITECTURE.md](ARCHITECTURE.md) explains why.
2. Append a new variant ID and register it: metadata with control labels and `originalColors`, `reset` and `render` dispatch in `PatternflowPack.cpp`, the count in `PatternflowPack.h`, `Palettes::Variants`, `src/patterns/PatternCatalog.h` and `collection` in `web/patterns.js`.
3. Run `tools/render_previews.py` and the host tests, where `tools/test_collection.cpp` renders every pattern at each panel size and checks its timing, controls and colours, and build every environment. Measure its frame rate on the ESP32 itself, without network capture running: host render times and preview streaming both distort it.
4. Credit an adapted pattern in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md), and keep the source or the licence that came with it in `patterns/imports/`.
