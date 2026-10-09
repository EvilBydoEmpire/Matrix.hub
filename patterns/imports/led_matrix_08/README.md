# Classic 08 imports

Source: the user's local `LED_Matrix_08/LED_Matrix_08.ino`, supplied October 9,
2026. Only Plasma and Ripples were imported. No Wi-Fi credentials or unrelated
source settings were copied. The source remains unchanged at its original path.
These are user-provided Arduino algorithms, not Patternflow community imports;
no third-party license is inferred.

Stable variants: 63 **Ripples — Classic 08**, 64 **Plasma — Classic 08**.
Implementation: `src/patterns/Classic08.inc`, shared by HUB75 and WS2812.

- Four simultaneous randomly placed hard-edged expanding rings, additive
  trails, 255-to-zero lifetime in steps of two. Controls retain frequency,
  fade, thickness and expansion speed from the supplied sketch.
- Plasma retains the sum of three radial sine fields: center, bottom-left,
  top-right; phase-rate ratios 1, 1/0.7, 1/1.3; spatial ratios 1, 0.8, 1.2.
  Its previously unused third control now controls hue drift (50 = original).
- Source dimensions are declared 22×22, but the user identifies these as
  16×16-tuned. This port uses a 16-pixel logical canvas and preserves that
  framing at 16, 22 and 64 pixels. It is not a cropped 64-pixel framebuffer.
- Fixed 30 Hz simulation replaces the source's frame-dependent `delay(25)`
  plus WS2812 transfer. Timelines match at 30/60/120 render FPS. At low drop
  frequency, periods without visible rings are intentional, as in the source.
- Shared editable palettes replace FastLED HSV conversion. Original provides
  blue-violet rings and spectrum plasma; Soukan 5/6 are the platform defaults.
  A shared RGB gain bounds additive accumulation by the chosen palette's
  brightness. This avoids channel clipping whitening every overlap.
- Both have eight controls: brightness, four algorithm controls, zoom,
  rotation and symmetry. No audio mapping is reintroduced.

Previews are sampled from the exact native renderer. The old 59 atlas rows
remain in place; appended rows are 59 and 60. No existing IDs are renumbered.
Host tests: `tools/test_classic08.cpp`, `tools/test_pattern_render.cpp`.
