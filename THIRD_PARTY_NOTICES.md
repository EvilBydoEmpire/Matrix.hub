# Third-party notices

## Optional HUB75 DMA backend

The experimental `matrixportal_s3_hub75_dma_4bit_usb` build uses
[ESP32-HUB75-MatrixPanel-DMA](https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA),
release 3.0.14 (MIT license) pinned at `f17fb7fe9d487e9643f919eb5aeedea8d9d1f8d7`.
Its original license and component notices are retained in the dependency;
see its `LICENSE.txt` and source headers. The adapter is local project code;
the library source is not vendored or modified by this profile.

## Monochromatic Perspective Flow

The firmware pattern **Perspective Flow** is an adaptation of
“Monochromatic Perspective Flow” by **@martinnlopez**, originally published for
[PatternFlow](https://patternflow.work/community) on 2026-08-07.

The original and this adaptation are provided under the
[Creative Commons Attribution-ShareAlike 4.0 International license](https://creativecommons.org/licenses/by-sa/4.0/).
The port preserves the original perspective-scaled harmonic-line algorithm,
adapts its 64×128 canvas to 64×64, and replaces JavaScript drawing and sine
calls with the project's native C++ matrix and lookup-table rendering paths.

## Patternflow Collection

The native **Patternflow Collection** adapts the twenty animations from the
user's `LED_Matrix_22x22_11_patternflow.ino`, itself derived from
[Patternflow by Seunghun LEE / engmung](https://github.com/engmung/Patternflow).
The collection retains the original pattern names and four-control model, uses
resolution-independent coordinates, and routes color through this project's
editable cached palettes. These adaptations are provided under
[CC-BY-SA-4.0](https://creativecommons.org/licenses/by-sa/4.0/).

## Additional Patternflow Community imports

The following native, resolution-independent adaptations were imported from
Patternflow Community JavaScript sources supplied by the user. Each original
source and this adaptation are provided under
[CC-BY-SA-4.0](https://creativecommons.org/licenses/by-sa/4.0/):

- **Blooming Bioluminescence**, by **@WollyMeister**, 2026-08-18. The port
  preserves the moving Lissajous/Voronoi organisms, breathing membranes, and
  domain-warped internal plasma while routing color through editable palettes.
- **ChladniAcousticWaves**, by **@engmung**, 2026-07-31. The port preserves the
  modulated two-mode Chladni nodal equation and adjustable edge sharpness.
- **SnowFall**, by **@engmung**, 2026-07-29. The port preserves perspective
  depth, aerodynamic flutter, pearl highlights, and radial snow trails using a
  deterministic particle field suitable for ESP32 memory limits.
- **Pulsating Solar Flare**, by **@martinnlopez**, 2026-08-03. The port
  preserves the golden-angle particle distribution, radial pulse, and bloom.
- **Murmuration2x**, by **@engmung**, 2026-08-07. The port preserves flock
  cohesion/alignment/separation and falcon avoidance. Neighbor sampling and a
  180-bird ceiling replace the source's O(n²), 1,000-bird desktop loop so it
  remains responsive on ESP32 hardware.

Original publication venue: [Patternflow Community](https://patternflow.work/community).

## October 3, 2026 Community batch

The following user-supplied originals and native adaptations are CC-BY-SA-4.0.
Untouched sources are preserved as `patterns/imports/patternflow/patternflow_*.js`;
the native ports live in `src/patterns/PatternflowImports.inc`. These are
resolution-adapted, bounded-memory C++ ports, not pixel-exact JS emulation.
See `docs/PATTERN_LIBRARY.md` for algorithm changes and performance limitations.

- Velvet Steps — **@mikdnom**.
- Vega444 C — **@WollyMeister**.
- Twisted Tunnel, Mondrian — **@kpow**.
- TV Setup — **@petrusha**.
- Soliton Lattice Echo, Ripple Cell Grid 2, Poolside, Entropy Glass,
  Collatz Kelp, Bus Window Rain, Aizawa Viscous Ink Plume — **@engmung**.
- Pareidolia, Op Art Squircle Spacetime Warp, Midnight Metropolis,
  Bioluminescent Topo Vortex — **@martinnlopez**.
- Mercury Rain — **@Besoiobiy**.
- Hypno Circles — **@SimonePDA**.

The supplied Blooming Bioluminescence copy is byte-identical to the existing
@WollyMeister original and was deduplicated. The pre-curation archive includes
previously imported patterns removed from the active gallery; their notices
above remain applicable to those archived adaptations.

## October 3, 2026 independent remixes

`src/patterns/PatternRemixes.inc` adds separately named derivatives, retaining
the originals and the supplied source files:

- **Chladni Resonance** and **Ripple Cell Grid 2 — Liquid** derive from
  **@engmung**: separable morphing resonant modes and a breathing wave-displaced
  lattice, respectively.
- **Vega444 C — Orbital** derives from **@WollyMeister**: a rotating convex
  dot lens with breathing colored cores and rings.
- **Bioluminescent Topo Vortex II** derives from **@martinnlopez**: smooth
  sub-pixel, counter-rotating contour strands and traveling luminous beads.
- **Twisted Tunnel — Hyperspace** derives from **@kpow**: painter-ordered
  nested portals, continuous perspective travel, twisting, and a curved flight path.
- **Whitney Harmonics II** refines the imported Arduino Whitney Harmonics
  adaptation with analytic trails and sub-pixel harmonic motion. Attribution
  for the source remains as recorded in the existing imported-sketch archive.
- **Warped Checkerboard — Silk** and **Squares — Infinite** remix this
  project's native patterns with continuous fabric coordinates and antialiased
  perspective lines.

Patternflow-derived remixes remain **CC-BY-SA-4.0**. Algorithm changes,
optimization notes, and measured hardware performance are in
`docs/PATTERN_LIBRARY.md`. Retired adaptations in the recovery archives retain
their original notices; removal from the active catalog does not remove attribution.

## User-supplied John Whitney studies — October 3, 2026

The three sketches supplied in chat are preserved, with formatting normalized,
under `patterns/imports/whitney/`. Their native adaptations are in
`src/patterns/WhitneyStudies.inc`:

- **Whitney — Motion Graphics 1968**: supplied as **Kenny Kim**'s recreation of
  **John Whitney, Experiments in Motion Graphics (1968)**, with the user's
  reference [film link](https://www.youtube.com/watch?v=jIv-EcX9tUs&t=264s).
  Retains the two 31-line families, endpoint equations, cream/pink colors,
  teal-black background and 4:3 composition. The revision-3 adaptation removes
  the periodic pose hold and adds adjustable line width for continuous motion.
- **Whitney — Differential Columns**: user-supplied Whitney-inspired sketch;
  archived/removed from the active catalog in revision 3 at the user's request;
  recreation author not supplied. Retains 360 dots in each of three columns,
  the 5:3:4 differential rates and nonlinear slow passages. The native port
  corrects copied upper thresholds to match each band's own cycle length.
- **Whitney — Chromatic Bloom**: user-supplied Whitney-inspired sketch;
  recreation author not supplied. Retains the seven provided colors, radial
  sine motion, 200-particle default and 0–100-degree phase range. Revision 3
  replaces the square footprints with exactly one pixel each. A deterministic
  seed makes device/preview motion reproducible; a centered 2× crop improves
  small-panel readability (spatial zoom 0.5 approximates the full source framing).

No license statement was supplied for these three recreations. They are
recorded as user-supplied adaptations, **not** as verified CC-BY-SA works.
Confirm the recreation authors' redistribution terms before publishing these
specific source files as a licensed public pattern package. John Whitney is
credited for the artistic inspiration, not as the author of these JavaScript
recreations or the new firmware.

## Advanced studies and Ripples — revision 3

`src/patterns/AdvancedStudies.inc` contains:

- **Bioluminescent Topo Vortex — Abyssal**, a further adaptation of
  @martinnlopez's Patternflow Bioluminescent Topo Vortex; retains its two
  centers, dashed contours and original green/cyan/blue gradient, with new
  counterflow, harmonic braiding and traveling light packets. This derivative
  retains the source's CC-BY-SA-4.0 attribution/share-alike requirements.
- **Warped Checkerboard — Interference**, a development of the project's
  native fine-pixel checker renderer, using the saved source settings as its
  baseline and adding interacting waves and smooth coverage transitions.
- **Ripples**, adapted from the five-ring Arduino algorithm supplied in chat.
  Maintains soft additive expanding rings and trails, with resolution scaling,
  fixed-time simulation, bounded brightness and palette integration. No source
  author/license was specified; do not assume an unrelated third-party license.

## Nova studies and personal photos — October 3, 2026

`src/patterns/NovaStudies.inc` contains seven new project-authored renderers:
Luminous Loom, Ferrofluid, Accretion, Origami Flight, Signal Orchard, Whitney —
Resonant Orbits and Whitney — Phase Architecture. The two Whitney studies are
new interpretations of differential motion and visual counterpoint; no new
third-party source code was imported. This does not imply authorship or
endorsement by John Whitney.

`src/patterns/PhotoStudies.inc` displays the two user-supplied HEIC photographs,
IMG_4786 and IMG_4704, encoded locally as 64×64 RGB444 textures. Source files
remain unchanged. No copyright/license grant for public redistribution is
assumed. **PersonalPhotos.h, photo-containing preview atlases/headers, and
firmware binaries embed personal images: do not publish them without the
user's approval.** No photographs or firmware were published to GitHub for
this addition.

## Pong Wars — October 3, 2026

`src/patterns/PongWars.inc` adapts **Pong Wars** by Koen van Gilst (2024),
distributed under the MIT License: <https://github.com/vnglst/pong-wars>.
The full notice is retained in `patterns/imports/pong-wars/LICENSE`.
The user supplied the visual reference at
<https://x.com/denicmarko/status/2093698283000787295>.
The opposing balls, tile conversion, and pale mint/navy appearance follow
the reference. Collision handling, fixed-step timing, compact board storage,
palette integration and adjustable controls are new ESP32 adaptations.

## Clifford Glass Torus / HG_SDF

The native **Clifford Glass Torus** pattern is an ESP32 adaptation of shader
code supplied by the user. That code cites
[HG_SDF](https://www.shadertoy.com/view/Xs3GRB), the
[main SDF construction](https://www.shadertoy.com/view/wsfGDS), and Inigo
Quilez's [spectrum palette](https://www.shadertoy.com/view/ll2GD3).

The port retains the inverse stereographic projection, rotating Clifford
torus, rounded sphere intersection, depth spectrum, and accumulated fog-glass
glow. Its 82-step desktop raymarch is replaced by an adjustable 6–14-step
low-resolution SDF field, analytical bounding-sphere entry, alternating cached
updates, and bilinear reconstruction for ESP32 frame rates.
Original-source licensing remains governed by the linked Shadertoy pages.

## PsiOp 3D scenes

The native **PsiOp Wire Cube**, **PsiOp Death Star**, and **PsiOp Color Cube**
patterns adapt the three distinct demonstration scenes from
[PsiOp by aap](https://github.com/aap/psiop). PsiOp is distributed under the
[MIT License](https://github.com/aap/psiop/blob/master/LICENSE).

The ports preserve the rotating wireframe cube and axes, point-cloud sphere
with equator and recessed dish, and painter-sorted filled cube. They use this
project's resolution-independent pixel writer and editable palettes instead
of the original ST7735 framebuffer and software rasterizer.
