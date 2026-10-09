# Pattern library design

## Acid studies and reversible archive — October 4, 2026

The compiled gallery now contains **59 patterns**, with stable variant IDs
60–62 appended: **Acid Cathedral** (square-perspective throat), **Phase Slippage**
(sliding moiré lattices) and **Liquid Circuit** (domain-warped saddle grid).
All render directly at 64×64, use the shared editable palette, and expose the
usual eight controls: brightness, motion speed, grid density, distortion,
aliasing, zoom, rotation and symmetry. Aliasing is intentional spatial detail
inside these patterns, not global temporal dithering. Preview generation uses
the exact C++ renderer; hardware captures replace the three bootstrap rows.

Each gallery card has **Archive**. The **Library / Archive / restore** selector
opens the archive, where **Restore** brings a pattern back. This is reversible
soft deletion: presets, bookmarks, ratings, folders and tags stay intact;
compiled code remains in flash. Archived patterns are excluded from normal
gallery results and auto-cycle. Archiving the playing pattern advances to the
next non-archived entry; if none remain the panel goes black and the dashboard
remains available. Restoring a pattern resumes playback when the library was
empty. Startup also skips an archived default.

API: `setPatternArchived`, value `{id: <stable metadata ID>, archived: true|false}`.
Sparse `/library` rows append a sixth boolean field. LM3 packs this flag into
bit 7 of the serialized rating byte without increasing per-record size. LM2
and the original fixed-layout metadata remain readable. The verified two-key
NVS journal and its 1,400-byte budget are unchanged. Organization edits preserve
the archive bit. No existing pattern IDs or bookmark positions were reordered.

Tests: `test_library_metadata.cpp` covers LM2 migration, archive round trips,
illegal flags, sparse storage and bounded cycling including empty/all-archived;
`test_library_organizer.cjs` covers active/archive filtering and restoration;
`test_archive_device.py` stages a reversible archive test across OTA reboot.

On-device native-render measurements (`docs/acid-benchmark.json`, 4-bit DMA,
20 MHz, no preview capture): Cathedral **70.9 FPS**, Phase Slippage **69.4 FPS**,
Liquid Circuit **82.3 FPS**, scan **~888 Hz**. Active HTTP frame capture lowers
observed animation FPS; that separate workload is recorded in
`docs/acid-preview-capture.json`. These are default-parameter measurements, not
a guarantee for all controls/network loads. The final 24-frame hardware atlas
is ~213 KiB and fits both WROOM OTA app partitions without lowering preview FPS.

## Local organizer — October 4, 2026

The current gallery has **56 active patterns**. Ratings/organization are local
device data, not a cloud service. In the editor select 1–5 stars (Clear = unrated),
optionally enter Folder, Variant family and comma-separated Tags, then choose
**Save rating / organization**. This does not overwrite any animation preset.
Blank folder means Unfiled; blank family restores the compiled family grouping.
Changing a folder/family name moves that pattern; unused names are reclaimed.

Search also matches folders, families and tags. Filters combine bookmarks,
folder, tag and minimum rating/unrated. Sorting supports library order, rating
and name; bookmarks retain priority. “Keep variants together” groups related
patterns, including both Resonant Orbits versions. Groups containing a bookmark
sort first; other variants remain next to that bookmark within the group.

`/library` (also `USBGET,LIBRARY`) reads metadata; `setLibraryMetadata` writes
one complete record. IDs are independent of gallery ordering: native engine ID,
or 16 + collection variant ID. A **2,232-byte RAM structure** holds 80
record slots and shared dictionaries: **12 folders, 16 custom families, 24 tags**,
up to four tags per pattern. Automatic families need no dictionary entries.
Limits use UTF-8 bytes: folder 24, family 48, each tag 20. Server-side validation
and flash-write errors are reported, not silently discarded. A corrupt or newer
metadata format is not overwritten. Flash stores only occupied records/names:
4 bytes empty, 8 bytes per rated/organized pattern plus shared name bytes.
The 1,400-byte serialized budget leaves room for two copies in the existing
small NVS partition; shortened/shared names conserve this budget.
All 80 slots with ratings alone need 644 bytes. Initial fixed-size LM1 storage
is read for migration, but never written again: replacing its 2,232-byte blob
exhausted update headroom even though NVS reported free entries. That failed
replacement left an unreadable legacy key in the device test. Only agent-created
test metadata was affected; the pre-test user metadata was empty and was restored.
The final journal uses separate `libmeta_a`/`libmeta_b` keys with generation
numbers and a payload checksum (10-byte envelope). It erases only the inactive
backup, writes its replacement, and reads/compares every byte before making it
current. The active copy is never overwritten during that operation. Startup
chooses the newest valid copy. Legacy data is removed only after a verified
journal copy exists. The scoped `resetUnreadableLibrary` recovery command
requires the literal `RESET UNREADABLE LIBRARY` and only works when no readable
metadata exists; it affects no presets, bookmarks or network settings.
The UI keeps unsaved drafts across pattern
selection; Save commits to the matrix, Revert discards only that draft.

The append-only Orbits v2 (variant 59) adds no catalog migration. Existing
bookmarks, saved parameters and palettes are preserved. Chromatic Bloom (45) is
redesigned in place: seven coordinated single-pixel ribbons, square-spanning
petals and differential phase motion. Its four controls now mean Bloom speed,
Particle count, Petal folding and Counterpoint. Existing saved control values
are kept, with these new meanings. All eight visible controls remain useful.

The two changed previews are rendered from the actual firmware and sampled
from the ESP32 into the existing 24-frame atlas; unrelated rows are preserved.
`test_library_organizer.cjs`, `test_library_metadata.cpp`, and
`test_organizer_device.py` cover sorting/filtering, binary bounds/reclamation,
API validation and persistence across a firmware-update reboot.

Final device checks: all **20/20** diagnostic requests succeeded, electrical
scan ~**888 Hz**, 4 bits/channel at exact 20 MHz. Five alternating journal writes
and readbacks took 73–94 ms per HTTP round trip; the test record used **69 bytes**
including its envelope. It survived a subsequent firmware-update reboot and was
then restored to the prior empty metadata state. 232 free NVS entries remained.
Independent render-capacity samples (no preview traffic): Bloom ~**125 FPS**,
original Orbits ~**114 FPS**, square Orbits v2 ~**111 FPS**, all native 64×64.
Reports: `whitney-v2-benchmark.json`, `library-organizer-verification.json`,
`display-timing-library-organizer.json`. DMA 4-bit, Protomatter 3-bit, WROOM16
and WROOM22 builds all pass. Final showcase selects Orbits v2 without saving
or overwriting an animation preset.

The supplemental Graphify index covers only `src/platform`. Its AST extraction
has 11 dangling dependency edges and two collapsed multi-relations, so it is
not a complete call graph; source inspection and executable tests are authoritative.

### Original Acid concepts (now implemented above)

1. **Acid Cathedral:** checkerboard vaults bend into a breathing four-way tunnel;
   sharp black cutouts, lime/violet highlights, phase-locked waves of compression.
2. **Phase Slippage:** two counter-drifting lattices periodically lock into broad
   geometric shapes, then slip into traveling moiré bands and diagonal tears.
3. **Liquid Circuit:** checker contours flow like mercury around moving saddles;
   nested wave distortions create apparent feedback, without framebuffer history.

All three keep Interference's crisp one-pixel detail and strong negative
space, use shared palettes and four effective algorithm controls, and avoid
artificial dithering or silently reduced resolution.

## Storage decision

GitHub is the source of truth for the first public library. It already provides
version history, review, releases, attribution, and a natural workflow for a
public hobby/open-source project. The generated `catalog/catalog.json` can be
served through GitHub Pages or another static CDN, and devices can download a
small manifest plus the selected package rather than cloning the repository.

Supabase is not needed for the first library. Introduce it later only for data
that does not fit Git well: accounts, private drafts, favorites, ratings,
community uploads, moderation state, and analytics. In that phase, GitHub can
remain the canonical home for approved public source while Supabase stores the
interactive product data.

## Proposed package layout

```text
patterns/<pattern-id>/
  manifest.json
  main.lua
  preview.json
  LICENSE or license metadata
```

Every manifest should eventually include:

- stable ID, name, semantic version, and engine compatibility;
- display and color requirements;
- parameter definitions and defaults;
- author, original source URL, license, and conversion notes;
- file sizes and checksums;
- preview seed, duration, and representative saved parameter values.

Every imported pattern must expose exactly eight visible, meaningful pattern
controls in the matrix dashboard. A converter may combine source constants or
add useful spatial controls such as zoom, rotation, or symmetry, but it must not
pad the list with inert parameters. Palette editing, LFOs, audio routes, and
patch actions remain separate control groups and do not count toward the eight.

The catalog is generated from manifests. Pattern source is never duplicated in
the catalog.

## Browser preview and variants

The browser preview should expose the same small `gfx`, `color`, `ctx`, and
parameter contract as the ESP32 runtime. Lua runs inside a sandboxed browser Lua
runtime. A 64×64 canvas can render square pixels or simulated round LEDs without
changing the pattern. Fixed seeds and fixed time steps make previews repeatable.

A variant starts as a copy with a new ID and attribution link. The user edits
Lua or manifest parameters, previews it, runs validation, and installs it. The
first version intentionally has no integrated AI API. Copying the pattern and
runtime contract into an external coding assistant, then pasting the result back,
is cheaper, simpler, and avoids API-key handling and generated-code moderation.

## PatternFlow import preparation

The approximately 30 PatternFlow JavaScript files should enter through a staging
queue rather than directly into firmware. For each file:

1. preserve the untouched source and record its origin and license;
2. inspect dependencies and map its drawing operations to the Lua API;
3. map the animation into exactly eight useful controls, then create a manifest
   and deterministic preview case;
4. compare the browser simulation with the source visually;
5. benchmark on MatrixPortal S3 and later on ESP32-WROOM-32;
6. publish only patterns whose license permits redistribution and adaptation.

This makes building the package schema and preview runner worthwhile before the
bulk conversion begins.

The first five staged JavaScript imports are now converted: Blooming
Bioluminescence, Chladni Acoustic Waves, Snowfall, Pulsating Solar Flare, and
Murmuration 2x. Their untouched sources and `IMPORT.json` records are preserved
under `patterns/imports/patternflow/`; native ports live in
`src/patterns/PatternflowPack.cpp` and each exposes exactly eight visible
dashboard controls.

The October 3 curated firmware gallery contains 46 entries: eleven retained
core animations and 35 collection variants (17 retained, 18 new). Blooming's
supplied duplicate is byte-identical to the retained original, so it is not
listed twice. The original 58-entry source and bookmark snapshot are archived
under `patterns/archive/pre-curation-2026-10-03/` for recovery. Its preview atlas is generated by
`tools/profile_pattern_library.py`, which selects every pattern on the physical
MatrixPortal, records 24 28×28 snapshots of the firmware's final RGB output,
and writes both the PNG asset and its generated flash header. The same pass
records per-pattern render diagnostics in `docs/pattern-benchmark-curated-final.json`.
The gallery plays the frames at 67 ms intervals (~15 FPS). Recording requests
add network overhead; the separate benchmark-only reports exclude that load.

Collection patch banks are per variant rather than shared across the engine.
Empty slots use pointers, allocated only when saved; palette persistence stores
only edits relative to compiled defaults. Choosing an auto-cycle slot does not
overwrite its contents. Original restores source RGB for RGB-based ports; editing
its gradient or selecting a preset enables recoloring. Three additional gradients
are Ember, Ice, and Reef. No audio receivers or global dithering were reintroduced.

The native imports are adaptations, not pixel-exact JavaScript emulation.
Vega omits an occluded layer and maps its palette knob to color drift; TV cards
are simplified; Mondrian uses bounded guillotine reflow without the desktop's
full scene hierarchy; Midnight generates facades instead of six large world maps;
Bus Window retains bokeh/refraction but simplifies bead merging and fog trails;
Collatz uses immutable topology with bounded growth (smaller on WROOM);
Aizawa keeps 800 attractor particles. Pareidolia uses integer lattice hashing.
Soliton's field, Poolside's refraction, Entropy's sand, and the attractor remain
stateful. Several heavy native 64×64 patterns still run below 60 FPS; the explicit
Smooth / faster motion setting remains available rather than silently reducing
native resolution. Every imported pattern exposes eight controls: brightness,
four algorithm controls, spatial zoom, rotation, and symmetry.

## Remix revision 2 — October 3, 2026

The current catalog supersedes the earlier batch counts above: **46 active
patterns = 10 native originals + 28 collection originals + 8 remixes**.
The bookmarked snapshot selected Floyd Flow, Velvet Steps, TV Setup, Pareidolia,
Midnight Metropolis, Mercury Rain, Entropy Glass, and Bus Window Rain for removal.
Their renderer bodies were removed from the active build, not merely hidden
in the browser. Native persistence slots and collection ID tombstones remain
inert for backward compatibility. Source, gallery atlas, bookmarks, and all
38 surviving default settings were snapshotted in
`patterns/archive/pre-remix-2026-10-03/` before deployment.

| New independent copy | Refinement | Native 64×64 FPS, default controls |
| --- | --- | ---: |
| Whitney Harmonics II | Frame-rate-independent satin trails and harmonic pearls | 64 |
| Chladni Resonance | Smoothly morphing resonant modes, luminous nodal filaments | 97 |
| Vega444 C — Orbital | Convex rotating dot lens, breathing cores/rims | 63 |
| Warped Checkerboard — Silk | Continuous warped fabric coordinates and soft pixel coverage | 64 |
| Bioluminescent Topo Vortex II | Counter-rotating contours and traveling sub-pixel beads | 63 |
| Ripple Cell Grid 2 — Liquid | Coherent expanding waves through a breathing dot lattice | 65 |
| Twisted Tunnel — Hyperspace | Continuous portal travel, corkscrew and curved flight path | 79 |
| Squares — Infinite | Perspective acceleration and layered, antialiased neon outlines | 98 |

Measurements: MatrixPortal S3, 3-bit HUB75 at ~195.8 Hz electrical refresh,
target 240 FPS for benchmarking, no preview-recording traffic. See
`remix-benchmark.json` for exact values and draw/show timings. These are sampled
default-setting results, not guarantees for every control combination or heavy
Wi-Fi load. The user's target 120 FPS was restored after each benchmark.

All eight originals remain independently selectable. Every remix has eight
visible controls: brightness, four algorithm controls, zoom, rotation, symmetry;
editable Original colors and LFO settings remain independent per pattern.
Search `remix` in the gallery to show this batch. No audio or dithering added.
Antialiasing is continuous pixel coverage, not spatial/temporal dithering.

The remixes share a single transient RGB565 surface (8 KB at 64×64, 512 bytes
at 16×16), freed when leaving them. Chladni precomputes separable modes by row
and column. Vega shades only the visible dot/rim/background branch. Tunnels
use forward polygon rasterization instead of nested per-pixel searches.
Variable-width Wu line rasterization replaced repeated bilinear splats:
Squares improved from ~33 to ~98 FPS in the hardware pass without lowering
resolution. Integrated clocks and analytic trails preserve animation pace
when frame rate changes. Control extrema and 16/22/64-pixel targets pass host
undefined-behavior/bounds tests; each algorithm control is tested independently.

Collection identities 0–34 are stable; new variants are 35–42. A sparse active
ID array drives gallery selection, direct API validation and auto-cycle.
VP3 patch storage records its slot count; the reader explicitly recognizes
VP2's 35-slot header so saved defaults cannot be misaligned when IDs are added.
Library revision 2 clears the removed bookmarks exactly once, not at every boot.

The final eight thumbnail rows are recorded from the actual ESP32 using
`profile_pattern_library.py --only 38 ... --only 45`; retained rows are remapped
by engine/variant identity. The new HTTP `preview?format=rgb888` recording
endpoint area-integrates the actual RGB565 framebuffer into 28×28 RGB888:
unlike nearest-neighbor sampling it cannot skip a thin line entirely. Recording
applies a square-root display-transfer curve before RGB332 thumbnail encoding
so low-coverage LED filaments remain visible on the browser canvas. This is
preview-only, does not add LED dithering or alter brightness, and incurs no
per-frame cost when a preview is not being recorded. The legacy preview format
and USB snapshot protocol remain compatible. `render_remix_preview.cpp` also supports offline
visual QA with the exact same C++ algorithms. The one-shot curation script is
guarded against replay; do not rerun it as a build step.

## Three Whitney studies — historical append-only addition (before revision 3)

The next addition brings the active gallery to **49 entries** without deleting
or reordering any of the preceding 46. Stable collection IDs 43, 44 and 45 are
gallery indices 46, 47 and 48. No bookmark migration/reset is performed; VP3
defaults automatically expand their stored count when a new patch is saved.

| Pattern | Four algorithm controls | Preserved source identity |
| --- | --- | --- |
| Whitney — Motion Graphics 1968 | Motion speed, line count, phase spacing, pause duration | Two 31-line harmonic families; original cream, pink and teal; 3 s movement / 0.8 s pose hold at normal speed |
| Whitney — Differential Columns | Motion speed, orbit radius, phase divergence, dot size | 360 white dots per column, differential 5:3:4 winding, nonlinear slow passages |
| Whitney — Chromatic Bloom | Wave speed, tile count, radial travel, phase scatter | 200 squares, seven supplied colors, radial sinusoidal displacement, 0–100° initial phase spread |

Brightness, spatial zoom, rotation and symmetry complete each pattern's eight
visible controls. Original palettes are selected initially. RGB-based studies
retain their source colors through the existing RGB lookup table; edited
gradients/presets recolor them. Differential Columns uses three locations in
the editable gradient, so it can become three colored columns without changing
its default all-white look. Bloom's Original preserves all seven source colors;
the four palette-editor stops define its optional recoloring gradient.

The line study implements a non-blocking pose hold, never `delay(800)`. Set its
Pause duration control to zero for uninterrupted motion. The dot study uses
fixed 60 Hz virtual integration, bounded positional/envelope phases, symmetric
cycle-relative slow passages and antialiased dot coverage at the wrap boundary.
The bloom uses deterministic particle phases and source-over blending to avoid
turning overlapping colors into white. All three reuse the transient 8 KB
RGB565 drawing surface already used by the remixes; no new full-size buffer.

`tools/test_whitney.cpp` checks control impact, palette routing, timing at
different output frame rates, the held pose, and continued 30-second winding.
The general sanitizer tests cover defaults/extremes at 16, 22 and 64 pixels.
`tools/test_library_append.py` snapshots and checks all pre-existing defaults,
quality settings, bookmarks, target FPS and auto-cycle across firmware updates.
Initial previews use the actual C++ renderer; the final appended rows are
recorded from the ESP32 with area-filtered sampling. Existing preview rows are
preserved unchanged. See `whitney-benchmark.json` for hardware measurements.

MatrixPortal S3 / 3-bit HUB75 default-control benchmark (native 64×64, 240 FPS
requested to measure capacity, no preview-recording traffic): Motion Graphics
1968 **91.3 FPS**, Differential Columns **64.9 FPS**, Chromatic Bloom **78.4 FPS**.
These are sampled capacities, not guarantees at every parameter combination.
The user's 120 FPS target is restored after testing. Both WROOM 16×16 and
22×22 configurations also compile; their physical performance was not measured.

Attribution and the unspecified source licensing are recorded separately in
`THIRD_PARTY_NOTICES.md`; these studies are not assumed to inherit Patternflow's
license. This addition does not publish files or releases to GitHub.

## Revision 3 — advanced studies and bookmark removal

The current active library has **45 entries**. Exact archived live bookmarks:
Mondrian, Aizawa Viscous Ink Plume, Vega444 C — Orbital, Bioluminescent Topo
Vortex II, Ripple Cell Grid 2 — Liquid, Twisted Tunnel — Hyperspace, and
Whitney — Differential Columns. These seven renderer cases and gallery entries
were removed. Their source, previews and saved settings are recoverable from
`patterns/archive/pre-advanced-2026-10-03/`. IDs remain tombstones, so survivors'
defaults, palettes, quality settings and preset slots do not shift. Library
revision 3 clears old bookmarks once, not on every reboot.

Whitney — Motion Graphics 1968 now uses uninterrupted time. Its former pause
control becomes **Line width**, with the same default value retaining the old
line thickness. This also fixes saved presets that previously requested a hold.
Chromatic Bloom now draws one hard pixel per particle: no balls, enlarged
squares, halo or bilinear footprint; its seven colors and motion are retained.

Three independent additions, stable collection IDs 46–48:

| Pattern | Four algorithm controls | Design |
| --- | --- | --- |
| Bioluminescent Topo Vortex — Abyssal | Ring count, flow speed, vortex twist, braiding | Two breathing counterflow cores, interwoven dashed contours, traveling light packets; original green/cyan/blue gradient |
| Warped Checkerboard — Interference | Motion speed, wavelength, distortion, wave interference | Original one-pixel lattice and saved purple/cyan look, multiple moving wave lenses, smooth coverage transfer |
| Ripples | Expansion speed, ring width, spawn density, decay | Five random soft rings, additive trails and extended decay, ported from the supplied Arduino sketch |

Brightness, zoom, rotation and symmetry complete **eight visible controls**.
The advanced copies start from the originals' archived settings/colors; the
original patterns remain unchanged. Checkerboard's default brightness remains
6, matching the saved source; the other additions default to 7. Parameters and
palette edits remain independently savable per pattern.

All render at native 64×64 by default and adapt to 16×16 / 22×22. No ordered or
error-diffusion dithering is introduced. Ripples simulates at a fixed virtual
60 Hz to preserve the source's probability/fade/life behavior at different
output frame rates; ring dimensions scale from a 22-pixel source. It uses a
24 KB RGB fractional-precision trail buffer at 64×64, allocated only while
active, plus the shared 8 KB drawing surface. Additive output is bounded by
the selected palette/brightness, including immediate blackout at brightness 0.
The two advanced variations use only the shared canvas and small lookup arrays.

Validation: `test_whitney.cpp` covers all five changed/new patterns' controls,
palette routing and frame-rate-independent timing, no former pose hold,
single-pixel Bloom footprints, and Ripples brightness. The general UBSan/bounds
test exercises all registered IDs at 16, 22 and 64 pixels and control extremes.
`test_advanced_library.py` checks the live removal targets and every survivor's
default settings. Native-renderer previews are replaced by real-device captures
using the existing area-sampled pipeline; surviving rows are remapped by stable
identity. Benchmark reports separate capacity tests from preview traffic.

Final sampled MatrixPortal S3 / 3-bit capacities at default controls, native
64×64 with a 240 FPS test target and no preview traffic: Motion Graphics 1968
**95.6 FPS**, Chromatic Bloom **112.2 FPS**, Abyssal **65.1 FPS**, Interference
**64.2 FPS**, Ripples **96.7 FPS**. See `advanced-benchmark.json`. These are not
guarantees for extreme parameter settings, transforms or simultaneous dashboard
traffic. Ripples' simulation runs at 60 Hz even when rendering faster. Testing
restores the user's original selected pattern, parameters and 120 FPS target.

### Pong Wars — append-only addition

Stable collection ID 58 / gallery index 54 adds Pong Wars, adapted from Koen
van Gilst's MIT reference. The library now has 55 active patterns. Existing
indices, bookmarks, presets and the library migration revision are unchanged.

Eight controls: brightness, game speed, tile size, ball size, bounce variation,
zoom, rotation and symmetry. Original palette selects pale mint and navy;
its four editable stops control the two territories and their opposing balls.
The renderer draws at native 64×64 and also supports 16×16 / 22×22.

Physics uses bounded 240 Hz fixed steps, circle/tile collisions and a 128-byte
bit-packed board (8–32 tiles per axis); it shares the transient RGB565 canvas.
No dithering or per-frame allocation is used. Tile size resets the board;
other controls preserve the current game. At render rates below 10 FPS the
simulation intentionally caps catch-up at 100 ms to avoid long stalls.

`tools/test_pong.cpp` checks identical trajectories at 30/60/120/240 FPS,
all four algorithm controls, territory conversion, reflection, palette output,
and 27 simulated minutes at extreme speed. Preview frames use the same native
renderer, then are replaced by captures from the device.

On the MatrixPortal S3 / 3-bit profile, default settings at native 64×64
measured **96.7 FPS** (8.92 ms average rendering) with a 240 FPS test target
and no preview traffic; see `pong-benchmark.json`. This is a capacity sample,
not a guarantee for transforms or simultaneous dashboard traffic. Both WROOM
16×16 and 22×22 firmware profiles also compile successfully.
