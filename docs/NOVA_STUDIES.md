# Nova studies and personal photos

Nine append-only additions bring the library from 45 to 54 entries. Stable
variant IDs 49–57; gallery indices 45–53. No library-revision migration and no
bookmark reset. Existing defaults, palettes and quality choices remain intact.

| Pattern | Four algorithm/image controls | Visual approach |
| --- | --- | --- |
| Luminous Loom | Weave speed, ribbon count, undulation, ribbon width | Over/under satin ribbons, independent traveling warp and weft |
| Ferrofluid | Flow speed, droplets, cohesion, metallic reflection | Moving implicit liquid surface, metallic rim and reflected color bands |
| Accretion | Orbital speed, disc tilt, event horizon, turbulence | Tilted orbital filaments, central occlusion, lensed back rim and photon ring |
| Origami Flight | Motion speed, facet count, folding depth, precession | Hinged planes, depth-sorted perspective projection, tumbling grid |
| Signal Orchard | Pulse speed, trees, branching depth, surge | Growing/retracting circuit branches and traveling light packets |
| Whitney — Resonant Orbits | Canon speed, particle count, rate divergence, echo length | Differential angular velocities, counter-rotating partners, periodic visual resonances |
| Whitney — Phase Architecture | Motion speed, strand count, torsion, precession | Twisted ruled wire surface in continuous spatial rotation |
| Foto — IMG_4786 | Exposure, contrast, black level, color temperature | User's first photograph, static at neutral settings |
| Foto — IMG_4704 | Exposure, contrast, black level, color temperature | User's second photograph, static at neutral settings |

Brightness, zoom, rotation and symmetry complete eight visible controls for
each entry. Palettes and defaults are saved independently for every pattern.
Original mode preserves photograph colors; other palettes recolor through the
shared RGB lookup. The photographs are not animated unless an existing spatial
control/LFO is explicitly applied. No automatic pan, face crop or distortion.

## Rendering and resources

`NovaStudies.inc` renders at native resolution by default, using the existing
2-byte-per-pixel transient canvas (8 KiB at 64×64). Motion is integrated from
elapsed time; trails are analytic, not extra full-frame history. Lookups and
small per-row/per-column tables avoid expensive transcendental pixel loops.
No ordered dithering or error diffusion is introduced. All geometry scales to
the 16×16 and 22×22 profiles; native hardware benchmarking uses the MatrixPortal.

The photographs were decoded/oriented by macOS ImageIO (`sips`), resampled to
64×64, converted from their embedded Display P3 profiles to sRGB, and encoded
as two RGB444 textures in `PersonalPhotos.h`: **16 KiB total
flash**, no runtime HEIC/JPEG decoder and no full-resolution source copies.
Their 4-bit source channels feed the shared brightness/palette RGB565 lookup;
the physical panel still uses its configured 3-bit electrical profile. Small
WROOM panels use area averaging; the 64×64 fast path reads exact texels.

**Privacy:** the textures, photo-containing preview PNG/header and compiled
firmware contain personal photos. They are local/device artifacts, not approved
for public GitHub distribution. No GitHub publication was performed.

## Validation

`tools/test_nova.cpp` checks all four pattern controls affect output, consistent
30/60/120-FPS timing, changing animation vs static photos, palette routing, and
zero-brightness blackout. `test_pattern_render.cpp` exercises all registered
IDs at 16, 22 and 64 pixels under UBSan/bounds, including extreme controls.
`test_library_append.py` verifies the existing 45 patterns' saved defaults,
quality, bookmarks and playback target against a pre-install device snapshot.
All nine gallery previews use the exact renderer and are finally sampled from
the ESP32. Preview recordings are not used as capacity measurements.

## Measured MatrixPortal performance

Native 64×64, current 3-bit HUB75 profile, default controls, requested 240 FPS
to measure capacity without preview-recording traffic (`nova-benchmark.json`):

| Pattern | Sampled FPS |
| --- | ---: |
| Luminous Loom | 70.8 |
| Ferrofluid | 64.9 |
| Accretion | 64.5 |
| Origami Flight | 96.4 |
| Signal Orchard | 70.6 |
| Whitney — Resonant Orbits | 95.5 |
| Whitney — Phase Architecture | 67.3 |
| Foto — IMG_4786 | 112.8 |
| Foto — IMG_4704 | 111.8 |

Ferrofluid initially measured 48.4 FPS. Bounded positive reciprocals and
calculating surface normals only inside the visible silhouette raised it to
64.9 FPS without dropping pixels, blob count or resolution. Rates are sampled
capacities, not guarantees under every control setting or concurrent network
load. Static photographs need no high animation rate; their figure measures
renderer throughput. The user's prior FPS target/selection are restored.

Both WROOM configurations compile within their OTA application partitions;
their physical performance was not benchmarked. The initial full-color host
preview bootstrap was too large for those partitions; device-sampled compact
previews bring each firmware back below the 1.5 MiB limit without reducing the
resolution of the actual patterns or photograph textures.

## Color calibration / comparison (October 3, 2026)

The initial photo output exposed two limitations: no LED transfer correction,
and channel-value dimming before the 3-bit driver truncates low bits. At
brightness 7/15 only levels 0–3 survive; the neutral first photo falls to 17
distinct output colors. Display P3-to-sRGB conversion alone does not solve this.
Procedural RGB patterns have no embedded P3 profile to convert, but their dim
gradients share the same output precision and panel white-balance constraints.

The dashboard's **Color calibration / comparison** fold adds:

- A neutral ramp, white/R/G/B blocks and channel ramps (default peak 96/255).
- Side-by-side full-photo RAW/FIX views; FIX uses adjustable photo-only gamma,
  initially 1.8, applied before UI brightness. This is a tunable LED transfer
  curve, not an ICC conversion or a measured panel calibration.
- A simulated 3-bit / native 4-bit photo view, available only on >=4-bit HUB75
  builds. The old comparison no longer misleadingly runs on a 3-bit driver.
  Simulation truncates to 3-bit values then maps those levels to the nearest
  available 4-bit duties; exact 3-bit temporal/flicker behavior is not simulated.
- Device-wide RGB attenuation (0–100% each), initially neutral. It covers all
  HUB75 renderers, including direct-framebuffer and Lua paths. White balance
  must be judged on the physical panel; no camera-derived gains are assumed.

RGB calibration is applied during Protomatter output with a source-buffer
restore afterwards, preventing cumulative dimming. Neutral gains bypass the
extra work. Non-neutral gains allocate one reusable 8 KiB scratch frame; no
allocation occurs in the render loop. Photo gamma uses a 256-byte LUT and
invalidates the cached palettes only when changed. Other patterns do not get
photo gamma. No dithering is introduced.

Calibration views pause auto-cycle without changing its saved setting, stop
on pattern selection, and time out after ten minutes. Changes are live but
only persisted by **Save device calibration**, stored separately as a small
versioned `matrix-cfg/colorcal` blob. Per-pattern defaults and bookmarks stay
unchanged. **Neutral RGB / original photo output** disables photo correction
and resets gains; save afterwards if this should survive reboot.

For the physical comparison, the temporary `matrixportal_s3_hub75_4bit_ota`
build measured **126.9 Hz electrical refresh** versus approximately 195 Hz on
the 3-bit profile. Thus extra gradations may reintroduce visible flicker.
`tools/upload_matrix.command` still deliberately selects the usual **3-bit**
profile; reverting retains the calibration controls but disables the 4-bit
comparison. Calibration view selection is not persisted. The 4-bit firmware
does not automatically revert its bit depth after the test timeout.

Validation: `test_color_calibration.cpp` exhaustively checks RGB565 neutral
identity, blackout, channel isolation and simulated depth, plus gamma
monotonicity/endpoints. `test_calibration_device.py` validates mode/color
controls and rejects invalid values, restoring prior playback parameters and
test gains before leaving the chart active. Visual white-balance and flicker
assessment still require the user's observation of the physical panel.
