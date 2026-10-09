// ===== Patternflow pattern =====
// Title:    Pareidolia
// Author:   @martinnlopez
// Date:     2026-09-24
// SPDX-License-Identifier: CC-BY-SA-4.0
// ===============================

// @ramp oklchLong recolor 0:#000000 1:#000000 0.447:#00f900 0:#212865
// @knobs Speed=0.05..3, Detail=2..8, Density=0.3..0.7, Warp=0..2.5
// @matrix 64x128
// Rorschach — a slow, fluid ink-blot that never stops reshaping itself.
// Mirrored left/right like a real ink-blot card. A domain-warped fractal
// noise field is sampled through several incommensurate slow oscillators,
// so the blot endlessly drifts through countless silhouettes without ever
// looping or visibly scrolling in one direction.
//


// ---------- tiny hash / value-noise kit (no Math.random at runtime) ----------

function hash2(x, y) {
  const s = Math.sin(x * 127.1 + y * 311.7) * 43758.5453123;
  return s - Math.floor(s);
}

function valueNoise(x, y) {
  const xi = Math.floor(x);
  const yi = Math.floor(y);
  const xf = x - xi;
  const yf = y - yi;

  // quintic smoothstep for a softer, less "grid-like" interpolation
  const u = xf * xf * xf * (xf * (xf * 6 - 15) + 10);
  const v = yf * yf * yf * (yf * (yf * 6 - 15) + 10);

  const a = hash2(xi, yi);
  const b = hash2(xi + 1, yi);
  const c = hash2(xi, yi + 1);
  const d = hash2(xi + 1, yi + 1);

  const ab = a + (b - a) * u;
  const cd = c + (d - c) * u;
  return ab + (cd - ab) * v; // 0..1
}

// Fractal sum of valueNoise octaves. Returns roughly -1..1.
function fbm(x, y, octaves) {
  let sum = 0;
  let amp = 0.5;
  let freq = 1;
  let norm = 0;
  for (let i = 0; i < octaves; i++) {
    sum += amp * (valueNoise(x * freq, y * freq) * 2 - 1);
    norm += amp;
    amp *= 0.5;
    freq *= 2.03; // slightly irrational lacunarity avoids octave alignment
  }
  return sum / norm;
}

function smoothstep(e0, e1, x) {
  const t = Math.min(1, Math.max(0, (x - e0) / (e1 - e0)));
  return t * t * (3 - 2 * t);
}

// Reuse one persistent scratch buffer (allocated once, resized only if the
// canvas dimensions ever change) instead of allocating per frame/pixel.
function ensureField(params, w, h) {
  const n = w * h;
  if (!params._field || params._field.length !== n) {
    params._field = new Float64Array(n);
  }
  return params._field;
}

// ---------- pattern ----------

export function setup(params) {
  // Random per-load origin so every boot starts on a different blot,
  // while the live motion itself stays fully deterministic from `time`.
  params.originX = Math.random() * 1000;
  params.originY = Math.random() * 1000;

  params.speed = 0.6;
  params.detail = 5;
  params.density = 0.5;
  params.warp = 1.2;
}

export function update(dt, input, params) {
  params.speed = input.knobValues[0];
  params.detail = Math.round(Math.min(8, Math.max(1, input.knobValues[1])));
  params.density = input.knobValues[2];
  params.warp = input.knobValues[3];

  // Knob 1 push: jump to a brand new blot lineage instantly.
  if (input.btnPressed[0]) {
    params.originX = Math.random() * 1000;
    params.originY = Math.random() * 1000;
  }
}

export function draw(display, params, time) {
  const w = display.width;
  const h = display.height;
  const cx = w * 0.5;
  const cy = h * 0.5;

  const t = time * params.speed;

  // Several slow, incommensurate oscillators standing in for a 3rd noise
  // dimension. Together they take an enormous time to approximately repeat,
  // so the blot keeps flowing into fresh shapes rather than looping or
  // visibly scrolling across the canvas.
  const driftX = Math.sin(t * 0.070) * 1.7 + Math.cos(t * 0.031) * 0.9;
  const driftY = Math.cos(t * 0.058) * 1.7 + Math.sin(t * 0.024) * 0.9;
  const wobbleX = Math.sin(t * 0.091) * 0.35;
  const wobbleY = Math.cos(t * 0.077) * 0.35;

  const scale = 0.045; // base spatial frequency, tuned for a 64x128 field
  const warpOctaves = 2; // the warp field itself stays coarse & cheap
  const detailOctaves = params.detail;
  const edge = 0.1; // ink-edge feather width
  const threshold = 1.0 - params.density;

  const field = ensureField(params, w, h);

  // --- pass 1: evaluate the raw warped noise field, track its statistics.
  // The field's overall level drifts as it slowly flows (that's what makes
  // it feel alive), so each frame is re-normalized against its own mean in
  // pass 2 rather than a fixed threshold — otherwise the blot would swell
  // to fill the canvas or shrink to near-nothing for long stretches.
  let sum = 0;
  let sumSq = 0;

  for (let y = 0; y < h; y++) {
    const ny0 = (y + 0.5) * scale + params.originY + driftY;

    for (let x = 0; x < w; x++) {
      const dx = Math.abs(x + 0.5 - cx); // mirror left/right about center
      const nx0 = dx * scale + params.originX + driftX;

      // domain warp: displace the sample point by a coarse noise field
      const qx = fbm(nx0 + 4.2, ny0 + 1.7, warpOctaves);
      const qy = fbm(nx0 + 9.1, ny0 + 8.4, warpOctaves);

      const nx = nx0 + params.warp * qx + wobbleX;
      const ny = ny0 + params.warp * qy + wobbleY;

      const v = fbm(nx, ny, detailOctaves);

      field[y * w + x] = v;
      sum += v;
      sumSq += v * v;
    }
  }

  const n = w * h;
  const mean = sum / n;
  const variance = Math.max(1e-5, sumSq / n - mean * mean);
  const invStdev = 1 / Math.sqrt(variance);

  // --- pass 2: normalize relative to this frame's own spread, apply the
  // elliptical card vignette, then threshold into ink.
  for (let y = 0; y < h; y++) {
    const ellipY = Math.abs(y + 0.5 - cy) / (h * 0.5);

    for (let x = 0; x < w; x++) {
      const dx = Math.abs(x + 0.5 - cx);
      const raw = field[y * w + x];

      let v = 0.5 + (raw - mean) * invStdev * 0.22;

      const ellipX = dx / (w * 0.5);
      const radial = Math.sqrt(ellipX * ellipX + ellipY * ellipY);
      v *= 1 - smoothstep(0.55, 1.25, radial);

      const ink = smoothstep(threshold - edge, threshold + edge, v);

      display.setValue(x, y, ink);
    }
  }
}

// ── Made with Patternflow Community · https://patternflow.work/community ──
// Shared under CC-BY-SA-4.0. Attribution is part of this licence —
// please keep this notice and the author credit above when you reuse,
// remix, or redistribute this pattern. Do not delete it.
