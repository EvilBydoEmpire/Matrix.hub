// ===== Patternflow pattern =====
// Title:    Blooming Bioluminescence
// Author:   @WollyMeister
// Date:     2026-08-18
// SPDX-License-Identifier: CC-BY-SA-4.0
// ===============================

// @knobs Cells=2..10, Speed=0.1..3, Plasma=0..3, Hue=0..1
// @matrix 128x64

//
// Knob 1 (Cells):  Organism count — sparse cosmos to crowded swarm
// Knob 2 (Speed):  Drift and animation speed
// Knob 3 (Plasma): Internal plasma wave intensity
// Knob 4 (Hue):    Base palette hue — sweeps through the full spectrum

const MAX_CELLS = 10;

// Each organism has its own Lissajous orbit parameters so paths
// never repeat cleanly — complex, slow-drift trajectories.
const ORBIT_A  = [1.30, 0.71, 1.13, 0.92, 1.47, 0.63, 1.21, 0.84, 1.38, 1.05];
const ORBIT_B  = [0.94, 1.23, 0.81, 1.41, 0.73, 1.32, 1.02, 1.51, 0.62, 1.17];
const ORBIT_PH = [0.00, 0.63, 1.26, 1.89, 2.51, 3.14, 3.77, 4.40, 5.03, 5.65];
const ORBIT_R  = [0.52, 0.47, 0.60, 0.41, 0.56, 0.50, 0.44, 0.58, 0.49, 0.55];

function clamp(v, lo, hi) { return v < lo ? lo : v > hi ? hi : v; }
function fract(v) { return v - Math.floor(v); }
function mix(a, b, t) { return a + (b - a) * t; }

// Íñigo Quílez cosine palette — gives smooth, vivid spectral transitions.
// Slightly unequal frequency per channel makes colors richer than plain HSV.
function cospalette(t, out) {
  const TAU = 6.28318530718;
  out[0] = 0.5 + 0.5 * Math.cos(TAU * (t        + 0.00));
  out[1] = 0.5 + 0.5 * Math.cos(TAU * (t * 1.10 + 0.33));
  out[2] = 0.5 + 0.5 * Math.cos(TAU * (t * 0.90 + 0.67));
}

export function setup(params) {
  params.timeAcc  = 0.0;
  params.numCells = 5;
  params.speed    = 1.2;
  params.plasma   = 1.5;
  params.hue      = 0.0;
  params.rgb      = [0, 0, 0];
  params.col      = [0, 0, 0];
}

export function update(dt, input, params) {
  if (input && input.knobValues) {
    params.numCells = Math.max(2, Math.min(MAX_CELLS, Math.round(input.knobValues[0])));
    params.speed    = input.knobValues[1];
    params.plasma   = input.knobValues[2];
    params.hue      = input.knobValues[3];
  }
  params.timeAcc += dt * params.speed;
}

export function draw(display, params, time) {
  const w        = display.width;
  const h        = display.height;
  const t        = params.timeAcc;
  const n        = params.numCells;
  const plasmaStr = params.plasma;
  const baseHue  = params.hue;
  const rgb      = params.rgb;
  const col      = params.col;
  const aspect   = w / Math.max(1, h);

  // --- Precompute organism positions and breathing scale for this frame ---
  const cx    = [];
  const cy    = [];
  const pulse = [];
  for (let i = 0; i < n; i++) {
    const ph = ORBIT_PH[i];
    cx[i]    = Math.sin(t * ORBIT_A[i] + ph)       * ORBIT_R[i] * aspect;
    cy[i]    = Math.cos(t * ORBIT_B[i] + ph + 1.0) * ORBIT_R[i];
    // Slow breathing — each organism expands/contracts independently
    pulse[i] = 0.88 + 0.12 * Math.sin(t * 2.1 + ph * 3.7);
  }

  for (let y = 0; y < h; y++) {
    const ny = (y / h - 0.5) * 2.0;

    for (let x = 0; x < w; x++) {
      const nx = (x / w - 0.5) * 2.0 * aspect;

      // ── Voronoi: F1 (nearest cell) and F2 (second-nearest) ──────────────
      let f1 = 1e9, f2 = 1e9, f1i = 0;
      for (let i = 0; i < n; i++) {
        const dx = nx - cx[i];
        const dy = ny - cy[i];
        // Breathing pulse modulates the effective distance, making cells
        // appear to swell and shrink organically.
        const d = Math.sqrt(dx * dx + dy * dy) * pulse[i];
        if (d < f1) { f2 = f1; f1 = d; f1i = i; }
        else if (d < f2) { f2 = d; }
      }

      // border = 0 at cell centre, peaks sharply at the membrane boundary
      const border = f2 - f1;

      // ── Plasma texture ───────────────────────────────────────────────────
      // Four overlapping sine waves at different angles give the classic
      // demoscene plasma feel; domain-warping them produces organic texture.
      let plasma = 0;
      if (plasmaStr > 0.01) {
        const r = Math.sqrt(nx * nx + ny * ny);
        plasma =  Math.sin(nx * 7.0 + t * 1.3)
               +  Math.sin(ny * 6.0 - t * 1.0)
               +  Math.sin((nx + ny) * 5.0 + t * 0.8)
               +  Math.sin(r * 9.0 - t * 1.7);
        plasma *= 0.04 * plasmaStr;
      }

      // Warp the cell distance by plasma for irregular, alive-looking edges
      const dc = f1 + plasma;

      // ── Color mapping ────────────────────────────────────────────────────
      // Each cell gets its own band of the palette; borders add iridescent
      // shimmer; slow global hue drift keeps the overall palette moving.
      const cellHue = fract(baseHue + (f1i / Math.max(1, n)) + border * 0.3 + plasma * 0.12 + t * 0.03);
      cospalette(cellHue, col);

      // Three lighting layers:
      //   glow  — wide, soft exponential body light
      //   core  — tight hotspot at the organism's centre (goes white)
      //   rim   — narrow membrane shimmer at the Voronoi boundary
      const glow = Math.exp(-dc    *  4.0);
      const core = Math.exp(-dc    * 14.0);
      const rim  = Math.exp(-border *  9.0) * 0.7;

      // Faint animated starfield background so the void isn't pure black
      const bg = (Math.sin(nx * 12 + t * 0.2) * Math.cos(ny * 11 - t * 0.15) * 0.5 + 0.5) * 0.03;

      const brightness  = clamp(glow * 1.1 + rim, 0, 1);
      // Core bleaches toward white; saturated colour lives in the body glow
      const saturation  = clamp(1.0 - core * 0.85, 0.15, 1.0);

      const r_out = mix(col[0], 1.0, 1.0 - saturation) * brightness + bg;
      const g_out = mix(col[1], 1.0, 1.0 - saturation) * brightness + bg;
      const b_out = mix(col[2], 1.0, 1.0 - saturation) * brightness + bg;

      rgb[0] = Math.floor(clamp(r_out, 0, 1) * 255);
      rgb[1] = Math.floor(clamp(g_out, 0, 1) * 255);
      rgb[2] = Math.floor(clamp(b_out, 0, 1) * 255);
      display.setPixel(x, y, rgb[0], rgb[1], rgb[2]);
    }
  }
}

// ── Made with Patternflow Community · https://patternflow.work/community ──
// Shared under CC-BY-SA-4.0. Attribution is part of this licence —
// please keep this notice and the author credit above when you reuse,
// remix, or redistribute this pattern. Do not delete it.
