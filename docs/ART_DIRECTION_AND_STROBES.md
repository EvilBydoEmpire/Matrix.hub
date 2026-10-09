# Authored colors and ambient temporal studies

The previous Soukan rollout applied a color-only default override to every
pattern. That was intentional at the time, but erased source color identities
and made many different works share the same four-pigment look.

## New default policy

- Palette ID 7 is now labelled **Authored / curated**, without changing any IDs.
- Imported RGB artwork uses its original RGB output, not a hue-to-Soukan remap.
  This restores the original Whitney 1968 cream/pink, Vega primaries, red/yellow
  Ripple Cell Grid, green/cyan/blue Topo Vortex, gray Soliton and classic plasma.
- Existing authored palettes for the remixes and newer native studies remain.
- Early adaptations with generic placeholder spectrum tables receive individual
  palettes: water tones for fluid fields, ivory for Folded Cross/Death Star,
  copper/steel for pillars and ribbons, and muted stained-glass pigments.
- Native effects use `ArtDirection::Native`: mostly one or two hues, an accent
  only where the geometry benefits. Black comes from the animation's geometry,
  not a black palette stop that can arbitrarily erase part of a shape.
- All ten Soukan choices remain selectable and editable.

Only the selected default slot's color controls are overlaid. Brightness,
geometry, speed, LFO, ratings, bookmarks, folders, tags and Wi-Fi are untouched.
Other patch slots are not recolored. The new `art_ud1` mask distinguishes new
explicitly saved color choices from old Soukan saves; saving a new default wins
over the compiled overlay on every subsequent boot. No full preset-bank rewrite
or NVS erase is needed.

## Three new patterns (stable variants 65–67)

| Pattern | Temporal idea | Color treatment |
| --- | --- | --- |
| Strobe — Chrono Iris | Moving petals mixed with sampled, phase-locked poses | Blue silver / warm ivory |
| Strobe — Rolling Silk | Traveling exposure bands across a liquid curtain | Petrol / pale jade / parchment |
| Strobe — Phase Garden | Spatially staggered lantern pulses | Amber / gold |

Controls: Pulse rate (0–100 maps to 0.25–8 Hz), density, exposure width
(8–80% of cycle), and Flash depth (0–100%). The last control blends a continuously
lit image with the pulse, so depth zero has no brightness strobe. Existing
brightness and spatial controls complete the usual eight visible controls.
LFO/time scaling can change the effective pulse rate.

Defaults are below 1 Hz and 16–20% flash depth, with internally bounded light
output. These are comfort-oriented defaults, **not a photosensitivity safety
certification**. Flashing can cause discomfort or seizures in susceptible people.
The dashboard warns before selection; these three patterns are skipped by
automatic cycling, even if a strong preset has been saved. Their preview atlas
rows are still images, avoiding flashing thumbnails. Strong settings should only
be selected deliberately, not around people who have not agreed to them.

## Validation and limitations

The same C++ renderers generate the collection preview atlas and contact sheet.
Colors were reviewed across the whole collection; a browser image is not proof
of the physical LEDs' color accuracy. Native-engine previews require a capture
on the matrix using `tools/preview_native_art.py` (no preset saves).

`test_ambient_studies.cpp` verifies full-frame writes, deterministic reset and
each control at its extrema at 16/22/64 pixels, plus auto-cycle exclusion.
The full renderer suite runs with undefined-behavior sanitization. Firmware
builds for all three targets. Device FPS and physical appearance must be verified
separately; host render timings are not ESP32 FPS measurements.

The gallery has 62 entries; the two retired photo bookmark positions make all
64 bookmark bits occupied. Any further addition requires a storage migration.
