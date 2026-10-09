// ===== Patternflow pattern =====
// Title:    Vega444_C
// Author:   @WollyMeister
// Date:     2026-09-25
// SPDX-License-Identifier: CC-BY-SA-4.0
// ===============================

// @matrix 128x64
// @knobs Grid=4..16, Bulge=0..1.4, Depth=0..1.5, Palette=0..1
// Patternflow layer-stack composition — flattened by Pattern Lab.
// Layers (bottom → top):
//   1. Code 2 (code, normal, 100%)
//   2. Code 3 (code, normal, 100%)
const __W = 128, __H = 64;
const __stack = [];

function __b64(s, n) {
  const A = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  const out = new Uint8ClampedArray(n);
  let o = 0, buf = 0, bits = 0;
  for (let i = 0; i < s.length && o < n; i++) {
    const v = A.indexOf(s[i]);
    if (v < 0) continue;
    buf = (buf << 6) | v; bits += 6;
    if (bits >= 8) { bits -= 8; out[o++] = (buf >> bits) & 255; }
  }
  return out;
}

// Run-length coded RGBA: base64 of {count-1, r, g, b, a} tuples.
function __rle(s, n) {
  const t = __b64(s, (s.length * 3) >> 2), out = new Uint8ClampedArray(n);
  let o = 0;
  for (let i = 0; i + 4 < t.length && o < n; i += 5) {
    for (let k = t[i] + 1; k > 0 && o < n; k--) {
      out[o++] = t[i + 1]; out[o++] = t[i + 2]; out[o++] = t[i + 3]; out[o++] = t[i + 4];
    }
  }
  return out;
}

function __mkDisplay(L) {
  return {
    width: __W, height: __H,
    setPixel(x, y, r, g, b) {
      const xi = x | 0, yi = y | 0;
      if (xi < 0 || xi >= __W || yi < 0 || yi >= __H) return;
      const i = (yi * __W + xi) * 4, B = L.buf;
      if (Array.isArray(r)) { B[i] = r[0] || 0; B[i + 1] = r[1] || 0; B[i + 2] = r[2] || 0; }
      else { B[i] = r; B[i + 1] = g || 0; B[i + 2] = b || 0; }
      B[i + 3] = 255;
    },
    setValue(x, y, v) {
      const xi = x | 0, yi = y | 0;
      if (xi < 0 || xi >= __W || yi < 0 || yi >= __H) return;
      v = Number.isFinite(v) ? (v < 0 ? 0 : v > 1 ? 1 : v) : 0;
      const i = (yi * __W + xi) * 4, B = L.buf, T = L.lut;
      if (T) {
        const li = Math.round(v * 255) * 4;
        B[i] = T[li]; B[i + 1] = T[li + 1]; B[i + 2] = T[li + 2]; B[i + 3] = T[li + 3];
      } else {
        const byte = Math.round(v * 255);
        B[i] = byte; B[i + 1] = byte; B[i + 2] = byte; B[i + 3] = 255;
      }
    },
  };
}

// ── code layer: Code 2 ──
(function () {
  // Shadow the wrapper's own exports: a layer that defines no setup/update
  // must yield undefined here, not capture the OUTER setup (infinite loop).
  var setup, update, draw;
// New layer — draws a scrolling value field, colored by this layer's ramp.
// Replace with your own pattern. Only draw() is required.
function draw(display, params, time) {
  for (let y = 0; y < display.height; y++) {
    for (let x = 0; x < display.width; x++) {
      const v = (x / display.width + time * 0.12) % 1;
      display.setValue(x, y, v);
    }
  }
}

  __stack.push({
    kind: "code", opacity: 1, blend: "normal", recolor: false, role: "paint", maskInvert: false,
    lut: [0,0,0,255,1,1,1,255,2,2,2,255,3,3,3,255,4,4,4,255,5,5,5,255,6,6,6,255,7,7,7,255,8,8,8,255,9,9,9,255,10,10,10,255,11,11,11,255,12,12,12,255,13,13,13,255,14,14,14,255,15,15,15,255,16,16,16,255,17,17,17,255,18,18,18,255,19,19,19,255,20,20,20,255,21,21,21,255,22,22,22,255,23,23,23,255,24,24,24,255,25,25,25,255,26,26,26,255,27,27,27,255,28,28,28,255,29,29,29,255,30,30,30,255,31,31,31,255,32,32,32,255,33,33,33,255,34,34,34,255,35,35,35,255,36,36,36,255,37,37,37,255,38,38,38,255,39,39,39,255,40,40,40,255,41,41,41,255,42,42,42,255,43,43,43,255,44,44,44,255,45,45,45,255,46,46,46,255,47,47,47,255,48,48,48,255,49,49,49,255,50,50,50,255,51,51,51,255,52,52,52,255,53,53,53,255,54,54,54,255,55,55,55,255,56,56,56,255,57,57,57,255,58,58,58,255,59,59,59,255,60,60,60,255,61,61,61,255,62,62,62,255,63,63,63,255,64,64,64,255,65,65,65,255,66,66,66,255,67,67,67,255,68,68,68,255,69,69,69,255,70,70,70,255,71,71,71,255,72,72,72,255,73,73,73,255,74,74,74,255,75,75,75,255,76,76,76,255,77,77,77,255,78,78,78,255,79,79,79,255,80,80,80,255,81,81,81,255,82,82,82,255,83,83,83,255,84,84,84,255,85,85,85,255,86,86,86,255,87,87,87,255,88,88,88,255,89,89,89,255,90,90,90,255,91,91,91,255,92,92,92,255,93,93,93,255,94,94,94,255,95,95,95,255,96,96,96,255,97,97,97,255,98,98,98,255,99,99,99,255,100,100,100,255,101,101,101,255,102,102,102,255,103,103,103,255,104,104,104,255,105,105,105,255,106,106,106,255,107,107,107,255,108,108,108,255,109,109,109,255,110,110,110,255,111,111,111,255,112,112,112,255,113,113,113,255,114,114,114,255,115,115,115,255,116,116,116,255,117,117,117,255,118,118,118,255,119,119,119,255,120,120,120,255,121,121,121,255,122,122,122,255,123,123,123,255,124,124,124,255,125,125,125,255,126,126,126,255,127,127,127,255,128,128,128,255,129,129,129,255,130,130,130,255,131,131,131,255,132,132,132,255,133,133,133,255,134,134,134,255,135,135,135,255,136,136,136,255,137,137,137,255,138,138,138,255,139,139,139,255,140,140,140,255,141,141,141,255,142,142,142,255,143,143,143,255,144,144,144,255,145,145,145,255,146,146,146,255,147,147,147,255,148,148,148,255,149,149,149,255,150,150,150,255,151,151,151,255,152,152,152,255,153,153,153,255,154,154,154,255,155,155,155,255,156,156,156,255,157,157,157,255,158,158,158,255,159,159,159,255,160,160,160,255,161,161,161,255,162,162,162,255,163,163,163,255,164,164,164,255,165,165,165,255,166,166,166,255,167,167,167,255,168,168,168,255,169,169,169,255,170,170,170,255,171,171,171,255,172,172,172,255,173,173,173,255,174,174,174,255,175,175,175,255,176,176,176,255,177,177,177,255,178,178,178,255,179,179,179,255,180,180,180,255,181,181,181,255,182,182,182,255,183,183,183,255,184,184,184,255,185,185,185,255,186,186,186,255,187,187,187,255,188,188,188,255,189,189,189,255,190,190,190,255,191,191,191,255,192,192,192,255,193,193,193,255,194,194,194,255,195,195,195,255,196,196,196,255,197,197,197,255,198,198,198,255,199,199,199,255,200,200,200,255,201,201,201,255,202,202,202,255,203,203,203,255,204,204,204,255,205,205,205,255,206,206,206,255,207,207,207,255,208,208,208,255,209,209,209,255,210,210,210,255,211,211,211,255,212,212,212,255,213,213,213,255,214,214,214,255,215,215,215,255,216,216,216,255,217,217,217,255,218,218,218,255,219,219,219,255,220,220,220,255,221,221,221,255,222,222,222,255,223,223,223,255,224,224,224,255,225,225,225,255,226,226,226,255,227,227,227,255,228,228,228,255,229,229,229,255,230,230,230,255,231,231,231,255,232,232,232,255,233,233,233,255,234,234,234,255,235,235,235,255,236,236,236,255,237,237,237,255,238,238,238,255,239,239,239,255,240,240,240,255,241,241,241,255,242,242,242,255,243,243,243,255,244,244,244,255,245,245,245,255,246,246,246,255,247,247,247,255,248,248,248,255,249,249,249,255,250,250,250,255,251,251,251,255,252,252,252,255,253,253,253,255,254,254,254,255,255,255,255,255],
    buf: new Uint8ClampedArray(__W * __H * 4),
    params: {},
    fSetup: typeof setup === "function" ? setup : null,
    fUpdate: typeof update === "function" ? update : null,
    fDraw: typeof draw === "function" ? draw : null,
  });
})();

// ── code layer: Code 3 ──
(function () {
  // Shadow the wrapper's own exports: a layer that defines no setup/update
  // must yield undefined here, not capture the OUTER setup (infinite loop).
  var setup, update, draw;



//
// Knob 1 (Grid):    Dot count across the sphere — coarse to fine lattice
// Knob 2 (Bulge):   Spherical swell (0 = flat op-art grid, 1 = full dome)
// Knob 3 (Depth):   Motion — ripple strength, background glow and colour drift
//                   (0 = completely still)
// Knob 4 (Palette): Colour scheme — 10 palettes across the knob's range:
//   Vega, Ember, Ice, Orchid, Inverted, Neon Night, Citrus, Aurora, Toxic, Deep Sea

const R        = 0.92;   // sphere radius, in vertical (ny) units. Centered, fixed.
const RD_INNER = 0.29;   // core radius   (cell units, 0.5 = cell edge)
const RD_OUTER = 0.44;   // ring radius   (cell units)
const AA       = 0.055;  // edge softness for the LEDs
const NUM_PAL  = 10;

// LED output gamma. 2.0 (a square) makes darks dark and keeps colours
// saturated on the panel; 1.0 disables it.
const OUTPUT_GAMMA = 2.0;

// Smoothing
const SS         = 2;     // supersamples per axis per LED (1 = off, 2 = 4x, 3 = 9x)
const TILE_SOFT  = 0.35;  // 0 = crisp flat tiles inside the sphere, 1 = fully smooth shading
const RIM_START  = 0.65;  // where (as a fraction of R) the rim switches to smooth screen-space shading
const RIM_END    = 0.95;
const GRID_GLIDE = 4.0;   // how fast the lattice glides to a new Grid value (higher = snappier)

// Outward ripple (driven by the Depth knob).
const WAVE_FREQ   = 6.0;   // number of wave rings across the field
const WAVE_SPEED  = 3.2;   // how fast crests travel outward
const RIPPLE_SIZE = 0.22;  // how much the dots swell/shrink at full Depth
const RIPPLE_LUM  = 0.10;  // overall brightness pulse on dots and tiles

// Background response to the ripple.
const BG_GLOW     = 0.22;  // how far crests lift the tile colour toward the glow tint
const BG_SINK     = 0.50;  // how much troughs darken the tiles
const BG_HUE      = 0.35;  // radians of hue nudge on the tiles as a wave passes

// Dot colour drift. Each dot's colour is hue-rotated by an amount that
// depends on its position (a two-armed spiral around the centre) and on a
// phase that advances over time.
const HUE_SPREAD  = 0.45;  // max hue rotation in radians (~26 degrees)
const HUE_FREQ    = 2.6;   // radial tightness of the colour spiral
const HUE_ARMS    = 2;     // number of spiral arms (integer keeps it seamless)
const HUE_SPEED   = 0.45;  // how fast the spiral turns (scaled by Depth)
const RING_COUNTER = -0.7; // ring rotates this much relative to the core

const PALETTES = [
  { // 0 - Vega (blue sphere, green field)
    core: [0.85, 0.16, 0.16],
    ring: [0.14, 0.64, 0.38],
    glow: [0.95, 0.97, 1.00],
    bg: [
      [0.00, [0.90, 0.91, 0.95]],
      [0.20, [0.66, 0.76, 0.93]],
      [0.42, [0.34, 0.48, 0.84]],
      [0.64, [0.18, 0.30, 0.66]],
      [0.86, [0.11, 0.17, 0.44]],
      [1.00, [0.06, 0.09, 0.24]],
      [1.10, [0.11, 0.44, 0.32]],
      [2.00, [0.11, 0.44, 0.32]],
    ],
  },
  { // 1 - Ember
    core: [0.98, 0.80, 0.28],
    ring: [0.55, 0.08, 0.05],
    glow: [1.00, 0.92, 0.70],
    bg: [
      [0.00, [0.98, 0.95, 0.86]],
      [0.24, [0.97, 0.66, 0.28]],
      [0.50, [0.86, 0.30, 0.12]],
      [0.78, [0.48, 0.10, 0.10]],
      [1.00, [0.16, 0.03, 0.05]],
      [1.10, [0.55, 0.32, 0.06]],
      [2.00, [0.55, 0.32, 0.06]],
    ],
  },
  { // 2 - Ice
    core: [0.96, 0.98, 1.00],
    ring: [0.10, 0.48, 0.72],
    glow: [0.85, 1.00, 1.00],
    bg: [
      [0.00, [0.94, 0.98, 1.00]],
      [0.24, [0.60, 0.86, 0.95]],
      [0.50, [0.26, 0.56, 0.86]],
      [0.78, [0.12, 0.26, 0.62]],
      [1.00, [0.04, 0.08, 0.24]],
      [1.10, [0.14, 0.46, 0.52]],
      [2.00, [0.14, 0.46, 0.52]],
    ],
  },
  { // 3 - Orchid
    core: [0.96, 0.32, 0.66],
    ring: [0.92, 0.84, 0.24],
    glow: [1.00, 0.88, 1.00],
    bg: [
      [0.00, [0.97, 0.93, 0.98]],
      [0.24, [0.78, 0.60, 0.90]],
      [0.50, [0.52, 0.28, 0.78]],
      [0.78, [0.30, 0.12, 0.52]],
      [1.00, [0.10, 0.04, 0.22]],
      [1.10, [0.46, 0.16, 0.52]],
      [2.00, [0.46, 0.16, 0.52]],
    ],
  },
  { // 4 - Inverted
    core: [0.13, 0.62, 0.40],
    ring: [0.85, 0.16, 0.16],
    glow: [0.90, 1.00, 0.90],
    bg: [
      [0.00, [0.92, 0.95, 0.92]],
      [0.22, [0.60, 0.84, 0.68]],
      [0.46, [0.24, 0.62, 0.46]],
      [0.72, [0.10, 0.34, 0.28]],
      [1.00, [0.04, 0.12, 0.10]],
      [1.10, [0.62, 0.14, 0.14]],
      [2.00, [0.62, 0.14, 0.14]],
    ],
  },  { // 5 - Neon Night: electric dots over a near-black city
    core: [1.00, 0.18, 0.78],   // hot magenta
    ring: [0.10, 0.92, 1.00],   // electric cyan
    glow: [0.80, 0.70, 1.00],
    bg: [
      [0.00, [0.92, 0.86, 1.00]],
      [0.22, [0.62, 0.42, 0.98]],
      [0.48, [0.36, 0.14, 0.78]],
      [0.76, [0.16, 0.04, 0.42]],
      [1.00, [0.02, 0.01, 0.08]],
      [1.10, [0.10, 0.08, 0.26]],
      [2.00, [0.10, 0.08, 0.26]],
    ],
  },
  { // 6 - Citrus: lime and tangerine over a deep teal pool
    core: [0.72, 1.00, 0.12],   // lime
    ring: [1.00, 0.46, 0.02],   // tangerine
    glow: [1.00, 1.00, 0.80],
    bg: [
      [0.00, [1.00, 0.98, 0.84]],
      [0.24, [1.00, 0.88, 0.36]],
      [0.50, [0.98, 0.58, 0.10]],
      [0.78, [0.60, 0.22, 0.04]],
      [1.00, [0.14, 0.05, 0.02]],
      [1.10, [0.04, 0.40, 0.42]],
      [2.00, [0.04, 0.40, 0.42]],
    ],
  },
  { // 7 - Aurora: mint lights and violet halos in a polar sky
    core: [0.62, 1.00, 0.72],   // mint
    ring: [0.62, 0.22, 1.00],   // violet
    glow: [0.80, 1.00, 0.90],
    bg: [
      [0.00, [0.86, 1.00, 0.94]],
      [0.24, [0.36, 0.92, 0.78]],
      [0.50, [0.08, 0.60, 0.70]],
      [0.78, [0.04, 0.24, 0.48]],
      [1.00, [0.01, 0.03, 0.12]],
      [1.10, [0.20, 0.06, 0.34]],
      [2.00, [0.20, 0.06, 0.34]],
    ],
  },
  { // 8 - Toxic: dark pupils in acid rings on hot pink
    core: [0.04, 0.04, 0.06],   // almost black
    ring: [1.00, 0.95, 0.10],   // acid yellow
    glow: [0.95, 1.00, 0.70],
    bg: [
      [0.00, [0.96, 1.00, 0.88]],
      [0.24, [0.74, 1.00, 0.30]],
      [0.50, [0.30, 0.86, 0.14]],
      [0.78, [0.06, 0.44, 0.10]],
      [1.00, [0.01, 0.10, 0.03]],
      [1.10, [0.78, 0.06, 0.40]],
      [2.00, [0.78, 0.06, 0.40]],
    ],
  },
  { // 9 - Deep Sea: glowing plankton around an abyssal dome
    core: [1.00, 0.62, 0.18],   // bioluminescent amber
    ring: [0.18, 1.00, 0.84],   // aqua
    glow: [0.60, 1.00, 1.00],
    bg: [
      [0.00, [0.70, 1.00, 1.00]],
      [0.20, [0.22, 0.78, 0.92]],
      [0.46, [0.04, 0.44, 0.72]],
      [0.74, [0.02, 0.18, 0.40]],
      [1.00, [0.00, 0.03, 0.10]],
      [1.10, [0.02, 0.12, 0.22]],
      [2.00, [0.02, 0.12, 0.22]],
    ],
  },
];

function clamp(v, lo, hi) { return v < lo ? lo : v > hi ? hi : v; }
function mix(a, b, t) { return a + (b - a) * t; }
function smoothstep(e0, e1, x) {
  let t = clamp((x - e0) / (e1 - e0), 0, 1);
  return t * t * (3 - 2 * t);
}

// Sample a background gradient (array of [pos, [r,g,b]] stops) at t.
function sampleBg(stops, t, out) {
  if (t <= stops[0][0]) { const c = stops[0][1]; out[0]=c[0]; out[1]=c[1]; out[2]=c[2]; return; }
  const last = stops[stops.length - 1];
  if (t >= last[0]) { out[0]=last[1][0]; out[1]=last[1][1]; out[2]=last[1][2]; return; }
  for (let i = 1; i < stops.length; i++) {
    if (t <= stops[i][0]) {
      const a = stops[i - 1], b = stops[i];
      const f = (t - a[0]) / (b[0] - a[0]);
      out[0] = mix(a[1][0], b[1][0], f);
      out[1] = mix(a[1][1], b[1][1], f);
      out[2] = mix(a[1][2], b[1][2], f);
      return;
    }
  }
}

// Rotate an RGB colour's hue by angle a (radians) around the grey axis.
// Cheap, allocation-free, keeps luminance roughly steady.
const INV_SQRT3 = 1.0 / Math.sqrt(3.0);
function hueRotate(c, a, out) {
  const cs = Math.cos(a), sn = Math.sin(a);
  const k  = (1.0 - cs) / 3.0;
  const p  = k + INV_SQRT3 * sn;
  const m  = k - INV_SQRT3 * sn;
  const d  = cs + k;
  const r = c[0], g = c[1], b = c[2];
  out[0] = d * r + m * g + p * b;
  out[1] = p * r + d * g + m * b;
  out[2] = m * r + p * g + d * b;
}

function setup(params) {
  params.timeAcc  = 0.0;
  params.huePhase = 0.0;
  params.numCells = 12;   // target from the knob
  params.gridN    = 12;   // smoothed value actually drawn
  params.bulge    = 1.05;
  params.depth    = 0.7;
  params.palette  = 0.0;
  params.bg   = [0, 0, 0];  // scratch buffers (avoid per-pixel allocation)
  params.tmp  = [0, 0, 0];
  params.core = [0, 0, 0];
  params.ring = [0, 0, 0];
}

function update(dt, input, params) {
  if (input && input.knobValues) {
    params.numCells = Math.max(4, Math.min(16, Math.round(input.knobValues[0])));
    params.bulge    = input.knobValues[1];
    params.depth    = input.knobValues[2];
    params.palette  = input.knobValues[3];
  }
  params.timeAcc  += dt;
  // Let the lattice glide to a new cell count rather than snapping.
  const g = 1.0 - Math.exp(-GRID_GLIDE * dt);
  params.gridN += (params.numCells - params.gridN) * g;
  // Colour drift only advances while there is motion, so Depth=0 is a still.
  params.huePhase += dt * HUE_SPEED * Math.min(1.0, params.depth);
}

function draw(display, params, time) {
  const w = display.width;
  const h = display.height;
  const t = params.timeAcc;
  const n = params.gridN;
  const palId = Math.max(0, Math.min(NUM_PAL - 1, Math.floor(params.palette * NUM_PAL)));
  const pal   = PALETTES[palId];
  const glow  = pal.glow;
  const bg    = params.bg;
  const tmp   = params.tmp;
  const core  = params.core;
  const ring  = params.ring;

  const aspect = w / Math.max(1, h);

  const bulge     = params.bulge;
  const depth     = params.depth;
  const motion    = Math.min(1.0, depth);
  const rippleAmp = RIPPLE_SIZE * depth;
  const lumAmp    = RIPPLE_LUM  * depth;
  const glowAmt   = BG_GLOW  * motion;
  const sinkAmt   = BG_SINK  * motion;
  const bgHueAmt  = BG_HUE   * motion;
  const phase     = params.huePhase;

  const s         = (2.0 * R) / n;
  const invHalfPi = 1.0 / (Math.PI * 0.5);

  // Per-cell dot colour cache (sub-samples of one LED usually share a cell).
  let lastIu = 1e9, lastIv = 1e9;

  // Shade one sample point; accumulates into acc[0..2].
  const acc = [0, 0, 0];
  function shade(nx, ny) {
    const r = Math.sqrt(nx * nx + ny * ny);
    const ratio = r / R;

    // Spherical bulge (inverse warp), centred at the origin
    let gx, gy;
    const inside = ratio < 1.0;
    if (inside && r > 1e-4) {
      const asinPart = invHalfPi * Math.asin(ratio);
      const rgNorm   = mix(ratio, asinPart, bulge);
      const scale    = rgNorm / ratio;
      gx = nx * scale;
      gy = ny * scale;
    } else if (inside) {
      gx = 0; gy = 0;
    } else {
      gx = nx; gy = ny;
    }

    const cu = gx / s, cv = gy / s;
    const iu = Math.round(cu), iv = Math.round(cv);
    const ou = cu - iu, ov = cv - iv;
    const dCell = Math.sqrt(ou * ou + ov * ov);

    const gcx = iu * s, gcy = iv * s;
    const rc  = Math.sqrt(gcx * gcx + gcy * gcy);

    // Dot colours: per-cell hue drift in a turning spiral
    if (iu !== lastIu || iv !== lastIv) {
      const ang   = Math.atan2(gcy, gcx);
      const shift = HUE_SPREAD * Math.sin(rc * HUE_FREQ + HUE_ARMS * ang - phase);
      hueRotate(pal.core, shift, core);
      hueRotate(pal.ring, shift * RING_COUNTER, ring);
      lastIu = iu; lastIv = iv;
    }

    // Background gradient position. The centre keeps (mostly) flat tiles,
    // taken from each cell's centre; towards the rim and outside it we use
    // the sample's own screen radius, so the silhouette is a true circle.
    const tTile = rc / R;
    let blend = inside ? smoothstep(RIM_START, RIM_END, ratio) : 1.0;
    if (blend < TILE_SOFT) blend = TILE_SOFT;
    sampleBg(pal.bg, mix(tTile, ratio, blend), tmp);

    // Ripple, and the background's response to it
    const wave = Math.sin(r * WAVE_FREQ - t * WAVE_SPEED);
    hueRotate(tmp, bgHueAmt * wave, bg);
    if (wave > 0) {
      const gl = glowAmt * wave * wave;
      bg[0] = mix(bg[0], glow[0], gl);
      bg[1] = mix(bg[1], glow[1], gl);
      bg[2] = mix(bg[2], glow[2], gl);
    } else {
      const k = 1.0 - sinkAmt * wave * wave;
      bg[0] *= k; bg[1] *= k; bg[2] *= k;
    }

    // Dot on top: ring, then core
    const rdScale  = 1.0 + rippleAmp * wave;
    const rdOut    = RD_OUTER * rdScale;
    const rdIn     = RD_INNER * rdScale;
    const ringMask = smoothstep(rdOut + AA, rdOut - AA, dCell);
    const coreMask = smoothstep(rdIn + AA, rdIn - AA, dCell);

    let rr = bg[0], gg = bg[1], bb = bg[2];
    rr = mix(rr, ring[0], ringMask); gg = mix(gg, ring[1], ringMask); bb = mix(bb, ring[2], ringMask);
    rr = mix(rr, core[0], coreMask); gg = mix(gg, core[1], coreMask); bb = mix(bb, core[2], coreMask);

    const lum = 1.0 + lumAmp * wave;
    acc[0] += clamp(rr * lum, 0, 1);
    acc[1] += clamp(gg * lum, 0, 1);
    acc[2] += clamp(bb * lum, 0, 1);
  }

  const ss    = Math.max(1, SS | 0);
  const invSS = 1.0 / ss;
  const norm  = 1.0 / (ss * ss);

  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      acc[0] = 0; acc[1] = 0; acc[2] = 0;
      for (let sy = 0; sy < ss; sy++) {
        const ny = ((y + (sy + 0.5) * invSS) / h - 0.5) * 2.0;
        for (let sx = 0; sx < ss; sx++) {
          const nx = ((x + (sx + 0.5) * invSS) / w - 0.5) * 2.0 * aspect;
          shade(nx, ny);
        }
      }
      display.setPixel(
        x, y,
        Math.floor(Math.pow(acc[0] * norm, OUTPUT_GAMMA) * 255),
        Math.floor(Math.pow(acc[1] * norm, OUTPUT_GAMMA) * 255),
        Math.floor(Math.pow(acc[2] * norm, OUTPUT_GAMMA) * 255)
      );
    }
  }
}

// -- Made with Patternflow Pattern Lab - https://patternflow.work/pattern-lab --
// Shared under CC-BY-SA-4.0. Attribution is part of this licence -
// please keep this notice and the author credit above when you reuse,
// remix, or redistribute this pattern.
  __stack.push({
    kind: "code", opacity: 1, blend: "normal", recolor: false, role: "paint", maskInvert: false,
    lut: null,
    buf: new Uint8ClampedArray(__W * __H * 4),
    params: {},
    fSetup: typeof setup === "function" ? setup : null,
    fUpdate: typeof update === "function" ? update : null,
    fDraw: typeof draw === "function" ? draw : null,
  });
})();

// Mask grouping: a mask layer paints nothing — it gates the nearest paint
// layer BELOW it (stacked masks intersect). Binary threshold: pixel-art masks
// switch on where alpha >= 0.5, code masks where luminance × alpha >= 0.5.
const __groups = [];
{
  let last = null;
  for (const L of __stack) {
    if (L.role === "mask") { if (last) last.masks.push(L); continue; }
    L.masks = [];
    last = L;
    if (L.opacity > 0) __groups.push(L);
  }
}

function __maskOn(M, j) {
  const B = M.buf;
  let on;
  if (M.kind === "pixel") on = B[j + 3] >= 128;
  else on = (0.2126 * B[j] + 0.7152 * B[j + 1] + 0.0722 * B[j + 2]) * (B[j + 3] / 255) >= 127.5;
  return M.maskInvert ? !on : on;
}

// One shared knob set: every layer receives the same input, exactly like
// patterns running on the device's four physical encoders.
function __mirror(L) {
  const inp = L.lastInput;
  if (!inp) return;
  L.params.knobValues = inp.knobValues;
  L.params.knobNormalized = inp.knobNormalized;
  L.params.knobDeltas = inp.knobDeltas;
  L.params.knobRanges = inp.knobRanges;
  L.params.btnPressed = inp.btnPressed;
  L.params.btnHeld = inp.btnHeld;
}

export function setup(params) {
  for (const L of __stack) {
    if (L.kind !== "code") continue;
    L.display = __mkDisplay(L);
    if (L.fSetup) L.fSetup(L.params);
  }
}

export function update(dt, input, params) {
  for (const L of __stack) {
    if (L.kind !== "code") continue;
    L.lastInput = input;
    __mirror(L);
    if (L.fUpdate) L.fUpdate(dt, input, L.params);
  }
}

export function draw(display, params, time) {
  for (const L of __stack) {
    if (L.kind !== "code" || !L.fDraw) continue;
    L.buf.fill(0);
    __mirror(L);
    L.fDraw(L.display, L.params, time);
    if (L.recolor && L.lut) {
      const B = L.buf, T = L.lut;
      for (let i = 0; i < B.length; i += 4) {
        if (B[i + 3] === 0) continue;
        const lum = Math.round(0.2126 * B[i] + 0.7152 * B[i + 1] + 0.0722 * B[i + 2]) * 4;
        B[i] = T[lum]; B[i + 1] = T[lum + 1]; B[i + 2] = T[lum + 2]; B[i + 3] = T[lum + 3];
      }
    }
  }

  // Composite bottom → top over the panel's opaque black.
  const count = __W * __H;
  for (let i = 0; i < count; i++) {
    const j = i * 4;
    let r = 0, g = 0, b = 0;
    for (const L of __groups) {
      let hidden = false;
      for (let m = 0; m < L.masks.length; m++) {
        if (!__maskOn(L.masks[m], j)) { hidden = true; break; }
      }
      if (hidden) continue;
      const B = L.buf;
      const a = (B[j + 3] / 255) * L.opacity;
      if (a <= 0) continue;
      const sr = B[j], sg = B[j + 1], sb = B[j + 2], inv = 1 - a;
      if (L.blend === "add") { r += sr * a; g += sg * a; b += sb * a; }
      else if (L.blend === "multiply") {
        r = r * inv + ((r * sr) / 255) * a;
        g = g * inv + ((g * sg) / 255) * a;
        b = b * inv + ((b * sb) / 255) * a;
      } else if (L.blend === "screen") {
        r = r * inv + (255 - ((255 - r) * (255 - sr)) / 255) * a;
        g = g * inv + (255 - ((255 - g) * (255 - sg)) / 255) * a;
        b = b * inv + (255 - ((255 - b) * (255 - sb)) / 255) * a;
      } else {
        r = r * inv + sr * a;
        g = g * inv + sg * a;
        b = b * inv + sb * a;
      }
    }
    display.setPixel(i % __W, (i / __W) | 0, r, g, b);
  }
}

// @stack v1 d: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

// ── Made with Patternflow Community · https://patternflow.work/community ──
// Shared under CC-BY-SA-4.0. Attribution is part of this licence —
// please keep this notice and the author credit above when you reuse,
// remix, or redistribute this pattern. Do not delete it.
