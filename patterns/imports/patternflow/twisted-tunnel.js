// ===== Patternflow pattern =====
// Title:    Twisted Tunnel
// Author:   @kpow
// Date:     2026-09-24
// SPDX-License-Identifier: CC-BY-SA-4.0
// ===============================

// @knobs Step=1.2..1.72, Tilt=0..0.5, Color=0..1, Speed=0.05..12
// @matrix 64x128
//
// A tunnel of nested frames in Mondrian's colours — red, blue, yellow, black
// and white — falling endlessly inward and twisting into a spiral as it goes.
//
// The zoom is a log scale with no seam: every ring's colour, shape, spacing,
// lean and drift is fixed by its own index the moment it appears at the
// vanishing point, so nothing pops or re-rolls on screen. The twist is keyed
// on a ring's SIZE, so the tunnel is a logarithmic spiral that turns as it
// falls. About one ring in five carries a dot, grid, diagonal, checker or bar
// fill, locked to the ring so it zooms and twists with it.
//
// Edges are anti-aliased by exact pixel coverage at the edge's real angle and
// blended in linear light, so a red/blue seam stays clean at 64 px wide.
//
// Knobs: Step (spacing between rings), Tilt (twist per ring; 0 = square to
// the frame), Color (continuous hue — dead centre is Mondrian's own red, blue
// and yellow), Speed.
//
// Adapted from a canvas sketch of concentric Bauhaus frames.




const FW = 64, FH = 128;

const SHAPE = 1.5;          // frame aspect stretch toward the tall panel
const GOLDEN = 0.61803398875;
const TAU = Math.PI * 2;
const OCTAVE_S = 5.2;       // seconds per octave at Speed 1 (the sketch's PERIOD)
const Z_WRAP = 1000;        // multiple of 5 (colour)
const W_MIN = 0.4;          // px; rings narrower than this are skipped
const W_MAX = 900;          // px; covers the frame at any offset and twist
const GAMMA = 2.2;
const DEV = 1.0;            // master deviation amount, 0 = none
const DEV_GAP = 0.3;        // steps, spacing jitter (must stay < 0.5)
const DEV_LEAN = 0.14;      // rad, fixed per-ring lean
const DEV_AR = 0.18;        // fraction, aspect jitter
const DRIFT_OFF = 0.06;     // fraction of ring size, offset wander
const DRIFT_ROT = 0.06;     // rad, angle wander
const DRIFT_HZ = [0.05, 0.16];  // per-ring wander rate range, cycles/s
const DRIFT_WRAP = 3600;    // s; the drift clock wraps here

// Pattern fills. Some rings carry a two-colour pattern instead of a flat
// field. The pattern lives in the ring's OWN rotated frame, centred on it,
// with a period proportional to the ring's width — so it zooms, twists and
// drifts with the ring and never slides across it. Too small to read (period
// under ~5 px) it fades to the flat colour, so the dots emerge as a ring
// approaches instead of shimmering as moire at the vanishing point.
const PAT_P = 0.2;          // fraction of rings that get a pattern
const PAT_PER = 0.1;        // pattern period as a fraction of ring width
const PAT_FADE = [2, 3.5];  // px of period: flat below, full pattern above
const PAT_DOTS = 0, PAT_GRID = 1, PAT_DIAG = 2, PAT_CHECK = 3, PAT_BARS = 4;
const NUM_PATS = 5;
const LUT_N = 4096;         // linear -> sRGB encode table

const frac = (v) => v - Math.floor(v);
// Integer hash -> [0,1), same as Bessel Dish's. Ring k's dice are
// hash01(km * 16 + slot).
function hash01(n) {
  n = Math.imul(n >>> 0, 2654435761) >>> 0; n = (n ^ (n >>> 15)) >>> 0;
  n = Math.imul(n, 2246822519) >>> 0; n = (n ^ (n >>> 13)) >>> 0;
  return (n & 0xFFFFFF) / 16777216;
}
const dice = (km, slot) => hash01(km * 16 + slot);
const sdice = (km, slot) => dice(km, slot) * 2 - 1;
// Colour: random, but never the same as the ring outside (k-1). Checked
// against k-1's RAW draw, so it is a pure function of k with no chain.
const rawCol = (km) => Math.floor(dice(km, 0) * 5);
function ringColor(km) {
  const c = rawCol(km), outer = rawCol((km + Z_WRAP - 1) % Z_WRAP);
  return c === outer ? (c + 1 + Math.floor(dice(km, 8) * 4)) % 5 : c;
}
// A per-ring wander rate, snapped to whole cycles per DRIFT_WRAP.
function driftHz(km, slot) {
  const hz = DRIFT_HZ[0] + (DRIFT_HZ[1] - DRIFT_HZ[0]) * dice(km, slot);
  return Math.round(hz * DRIFT_WRAP) / DRIFT_WRAP;
}
const clamp01 = (v) => (v < 0 ? 0 : v > 1 ? 1 : v);
const clamp = (v, a, b) => (v < a ? a : v > b ? b : v);
const sstep = (x) => { const v = clamp(x, 0, 1); return v * v * (3 - 2 * v); };
const toLin = (c) => Math.pow(c / 255, GAMMA);
const ENC = new Uint8Array(LUT_N);
for (let i = 0; i < LUT_N; i++) ENC[i] = Math.floor(255 * Math.pow(i / (LUT_N - 1), 1 / GAMMA) + 0.5);
const enc = (l) => ENC[Math.floor(clamp01(l) * (LUT_N - 1) + 0.5)];

// ── colour: Mondrian's scheme, verbatim from mondrian/pattern.js ──────────
// See that file for the reasoning behind every constant here. The one local
// choice is GROUND_V, set to the value the sat's Mondrian port runs (0.68):
// the ground is a big share of every frame and the power cap throttles the
// whole panel for it.
const GROUND_V = 0.68;

// PFColor::hsvToRgb, transcribed exactly — including the truncation.
function hsv(h, s, v) {
  h = h - Math.floor(h);
  s = clamp(s, 0, 1); v = clamp(v, 0, 1);
  const c = v * s, hh = h * 6;
  let i = Math.floor(hh); if (i > 5) i = 5;
  const f = hh - i, x = (i & 1) ? c * (1 - f) : c * f, m = v - c;
  let r, g, b;
  switch (i) {
    case 0: r = c; g = x; b = 0; break;
    case 1: r = x; g = c; b = 0; break;
    case 2: r = 0; g = c; b = x; break;
    case 3: r = 0; g = x; b = c; break;
    case 4: r = x; g = 0; b = c; break;
    default: r = c; g = 0; b = x;
  }
  return [((r + m) * 255) | 0, ((g + m) * 255) | 0, ((b + m) * 255) | 0];
}
const LUMA = (c) => 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2];
// An accent at a chosen hue and saturation, forced to a target luminance.
function accent(h, s, L) {
  const Ls = LUMA(hsv(h, s, 1));
  if (Ls >= L) return hsv(h, s, L / Ls);
  const L1 = LUMA(hsv(h, 1, 1));
  return hsv(h, clamp((255 - L) / Math.max(1, 255 - L1), 0, s), 1);
}
// Mondrian's own angular relationship, read off his pigments (from his red).
const MOND_OFF = [0.0, 0.5920, 0.1322];
const MOND_SAT = [0.904, 0.893, 0.959];
const LUMA_LO = 48, LUMA_HI = 195;
const SAT_FALLOFF = 0.35;
const natLuma = (h) => LUMA(hsv(h, 1, 1));
function lumaTargets(hues) {
  const nat = hues.map(natLuma);
  const lo = Math.min(...nat), hi = Math.max(...nat);
  const span = Math.max(1, hi - lo);
  return nat.map((v) => LUMA_LO + (LUMA_HI - LUMA_LO) * ((v - lo) / span));
}
const DE_STIJL_BAND = 0.055;
const DE_STIJL = {
  ground: [0xFF, 0xFA, 0xEA],
  frame:  [0x12, 0x11, 0x10],
  acc: [[0xD1, 0x14, 0x14], [0x0F, 0x47, 0x8C], [0xF2, 0xC2, 0x0A]],
};
function mondrianPalette(u) {
  const hue = (u - 0.5) - Math.floor(u - 0.5);     // knob centre = Mondrian red
  const hues = [hue + MOND_OFF[0], hue + MOND_OFF[1], hue + MOND_OFF[2]];
  const L = lumaTargets(hues);
  const acc = hues.map((h, a) => {
    const t = (L[a] - LUMA_LO) / (LUMA_HI - LUMA_LO);
    return accent(h, MOND_SAT[a] * (1 - SAT_FALLOFF * t), L[a]);
  });
  const pal = { ground: hsv(hue, 0.07, GROUND_V), frame: hsv(hue + 0.5, 0.50, 0.075), acc };
  const w = 1 - sstep(Math.abs(u - 0.5) / DE_STIJL_BAND);
  if (w <= 0) return pal;
  const mix = (x, y) => x.map((v, i) => (v + (y[i] - v) * w) | 0);
  return {
    ground: mix(pal.ground, DE_STIJL.ground.map((v) => (v * GROUND_V) | 0)),
    frame:  mix(pal.frame, DE_STIJL.frame),
    acc:    pal.acc.map((c, i) => mix(c, DE_STIJL.acc[i])),
  };
}
// Ring colours in the sketch's order: red, blue, yellow, frame, ground. Only
// the three accents come from the Mondrian palette. The frame is true black
// and the ground true white at every hue: the tinted paper and complement-
// tinted black suit a still painting, but in a zoom they read as grey bands.
// Cost: a frame with a lot of white asks the sat's power cap to throttle the
// whole panel — if the colours look dim on hardware, pull WHITE down first.
const BLACK = [0, 0, 0];
const WHITE = [255, 255, 255];
function ringPalette(u) {
  const m = mondrianPalette(u);
  const srgb = [m.acc[0], m.acc[1], m.acc[2], BLACK, WHITE];
  return { srgb, lin: srgb.map((c) => c.map(toLin)), bg: BLACK.map(toLin) };
}

// Area of the unit pixel square (centred on 0) where n.q <= t, for a unit
// normal whose |components| are hi >= lo. Exact: a linear middle and two
// quadratic corners.
function halfPlane(t, hi, lo, a, b, inv2) {
  if (t <= -b) return 0;
  if (t >= b) return 1;
  if (t < -a) { const e = t + b; return e * e * inv2; }
  if (t > a) { const e = b - t; return 1 - e * e * inv2; }
  return 0.5 + t / hi;
}

export function setup(params) {
  params.z = 40;            // start mid-sequence so the first frame is full
  params.t = 0;             // real seconds, drives the drift
  params.k = [1.46, 0.25, 0.5, 1];
  params.rings = [];
  params.colorU = 0.5;
  params.pal = ringPalette(0.5);
}

export function update(dt, input, params) {
  params.k = input.knobValues;
  const u = clamp(params.k[2], 0, 1);
  if (u !== params.colorU) { params.colorU = u; params.pal = ringPalette(u); }
  params.z += dt * params.k[3] / OCTAVE_S;
  params.t += dt;
  // Every drift rate is a whole number of cycles per DRIFT_WRAP seconds (see
  // driftHz), so wrapping the clock lands every sine exactly where it was.
  if (params.t >= DRIFT_WRAP) params.t -= DRIFT_WRAP;
  if (params.z >= Z_WRAP) params.z -= Z_WRAP;
}

function makeRing(cx, cy, w, h, ox, oy, rot) {
  const c = Math.cos(rot), s = Math.sin(rot);
  const hi = Math.max(Math.abs(c), Math.abs(s)), lo = Math.min(Math.abs(c), Math.abs(s));
  const a = (hi - lo) / 2, b = (hi + lo) / 2;
  return { ex: cx + ox * w, ey: cy + oy * h, hw: w / 2, hh: h / 2, c, s,
           hi, lo, a, b, inv2: lo > 1e-6 ? 1 / (2 * hi * lo) : 0,
           // axis-aligned bound of the rotated box, + the pixel footprint
           bx: Math.abs(c) * w / 2 + Math.abs(s) * h / 2 + 1,
           by: Math.abs(s) * w / 2 + Math.abs(c) * h / 2 + 1 };
}

// Coverage of pixel centre (px,py) by ring q, 0..1.
// Also leaves the pixel's ring-local position in LX / LY for the pattern.
let LX = 0, LY = 0;
function coverage(q, px, py) {
  const dx = px - q.ex, dy = py - q.ey;
  if (dx > q.bx || dx < -q.bx || dy > q.by || dy < -q.by) return 0;
  const lx = dx * q.c + dy * q.s, ly = -dx * q.s + dy * q.c;
  LX = lx; LY = ly;
  const ax = Math.abs(lx), ay = Math.abs(ly);
  if (ax >= q.hw + q.b || ay >= q.hh + q.b) return 0;
  if (ax <= q.hw - q.b && ay <= q.hh - q.b) return 1;
  const cxv = halfPlane(q.hw - lx, q.hi, q.lo, q.a, q.b, q.inv2) - halfPlane(-q.hw - lx, q.hi, q.lo, q.a, q.b, q.inv2);
  const cyv = halfPlane(q.hh - ly, q.hi, q.lo, q.a, q.b, q.inv2) - halfPlane(-q.hh - ly, q.hi, q.lo, q.a, q.b, q.inv2);
  return cxv * cyv;
}

// Pattern ink, 0..1, at pattern coords (u, v) with a 1-px AA width of fw
// (also in pattern units). Each is a distance field fed through one AA ramp.
const edge = (d, fw) => clamp01(d / fw + 0.5);          // d > 0 = inside ink
const tri = (t) => Math.abs(t - Math.floor(t) - 0.5);   // 0 at cell centre, .5 at cell edge
function ink(pat, u, v, fw) {
  switch (pat) {
    case PAT_DOTS:  { const x = tri(u), y = tri(v); return edge(0.27 - Math.sqrt(x * x + y * y), fw); }
    case PAT_GRID:  return edge(0.13 - (0.5 - Math.max(tri(u), tri(v))), fw);
    case PAT_DIAG:  return edge(0.25 - tri((u + v) * 0.7071), fw * 0.7071);
    case PAT_CHECK: { const a = edge(tri(u) - 0.25, fw), b = edge(tri(v) - 0.25, fw); return a + b - 2 * a * b; }
    default:        return edge(0.22 - tri(v), fw);        // PAT_BARS, across the ring's own axis
  }
}
// Second colour of a patterned ring: black ink on white and the accents,
// white on black — the only two that read against every accent.
const inkColor = (ci) => (ci === 3 ? 4 : 3);

export function draw(display, params, time) {
  const W = display.width, H = display.height;
  const [step, tilt] = params.k, shape = SHAPE;
  const PAL = params.pal.srgb, PAL_L = params.pal.lin, BG_L = params.pal.bg;
  const z = params.z, lnS = Math.log(step);
  const cx = W / 2, cy = H / 2;

  // Visible index range: w = step^(z-k) in [W_MIN, W_MAX].
  const kIn  = Math.floor(z - Math.log(W_MIN) / lnS);   // smallest ring
  const kOut = Math.ceil(z - Math.log(W_MAX) / lnS);    // largest ring
  const R = params.rings; R.length = 0;
  for (let k = kIn; k >= kOut; k--) {
    const km = ((k % Z_WRAP) + Z_WRAP) % Z_WRAP;
    const age = z - k;                                    // log2-ish size
    const T = TAU * params.t;
    const ar = (0.74 + 0.42 * (((km * 7) % 5) / 5)) * shape * (1 + DEV * DEV_AR * sdice(km, 2));
    const ox = (frac(km * GOLDEN) - 0.5) * 0.22 + DEV * DRIFT_OFF * Math.sin(T * driftHz(km, 9) + TAU * dice(km, 3));
    const oy = (frac(km * GOLDEN * 2) - 0.5) * 0.17 + DEV * DRIFT_OFF * Math.sin(T * driftHz(km, 10) + TAU * dice(km, 4));
    const rot = age * tilt + DEV * (DEV_LEAN * sdice(km, 5) + DRIFT_ROT * Math.sin(T * driftHz(km, 11) + TAU * dice(km, 6)));
    const w = Math.exp((age + DEV * DEV_GAP * sdice(km, 7)) * lnS);
    const q = makeRing(cx, cy, w, w * ar, ox, oy, rot);
    q.ci = ringColor(km);
    q.pat = -1;
    if (dice(km, 12) < PAT_P) {
      const per = w * PAT_PER * (0.75 + 0.5 * dice(km, 14));
      q.fade = clamp01((per - PAT_FADE[0]) / (PAT_FADE[1] - PAT_FADE[0]));
      if (q.fade > 0) {
        q.pat = Math.floor(dice(km, 13) * NUM_PATS);
        q.inv = 1 / per;
        q.c2 = PAL_L[inkColor(q.ci)];
      }
    }
    R.push(q);
  }

  for (let y = 0; y < H; y++) {
    const py = y + 0.5;
    for (let x = 0; x < W; x++) {
      const px = x + 0.5;
      let r = 0, g = 0, b = 0, rem = 1, solid = -1;
      for (let i = 0; i < R.length && rem > 0.002; i++) {
        const q = R[i];
        const cov = coverage(q, px, py);
        if (cov <= 0) continue;
        let col = PAL_L[q.ci];
        if (q.pat < 0 && cov >= 1 && rem === 1) { solid = q.ci; break; }
        if (q.pat >= 0) {
          const m = ink(q.pat, LX * q.inv, LY * q.inv, q.inv) * q.fade;
          if (m > 0) col = [col[0] + (q.c2[0] - col[0]) * m, col[1] + (q.c2[1] - col[1]) * m, col[2] + (q.c2[2] - col[2]) * m];
        }
        const a = rem * cov;
        r += a * col[0]; g += a * col[1]; b += a * col[2];
        rem -= a;
      }

      if (solid >= 0) {
        const c = PAL[solid];
        display.setPixel(x, y, c[0], c[1], c[2]);
        continue;
      }
      r += rem * BG_L[0]; g += rem * BG_L[1]; b += rem * BG_L[2];
      display.setPixel(x, y, enc(r), enc(g), enc(b));
    }
  }
}

// ── Made with Patternflow Community · https://patternflow.work/community ──
// Shared under CC-BY-SA-4.0. Attribution is part of this licence —
// please keep this notice and the author credit above when you reuse,
// remix, or redistribute this pattern. Do not delete it.
