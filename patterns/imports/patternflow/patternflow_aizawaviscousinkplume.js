// ===== Patternflow pattern =====
// Title:    AizawaViscousInkPlume
// Author:   @engmung
// Date:     2026-08-14
// SPDX-License-Identifier: CC-BY-SA-4.0
// ===============================

// @ramp oklchShort 0:#000000 0.984:#ffffff 0.837:#e100ff
// @knobs ViscosityBifur=0.7..1.05, Downflow=0.3..4.5, VortexCore=1.5..5, PlumeThickness=0.5..3
// @matrix 64x128



// Critical knob = ViscosityBifurcation (parameter 'a'), turning it crosses smooth coherent oil columns ↔ turbulent boiling plume.

/*
  Controls:
  - Knob 1 (ViscosityBifurcation): Aizawa parameter 'a'. Regulates laminar coherence vs chaotic vortex shedding.
  - Knob 2 (Downflow): Downward propagation velocity along the tall axis.
  - Knob 3 (VortexCore): Spiral compression parameter 'd'.
  - Knob 4 (PlumeThickness): Kernel splat size & ink density multiplier (fine threads ↔ thick heavy oil).
*/

export function setup(params) {
  const W = 64;
  const H = 128;
  params.w = W;
  params.h = H;
  params.buffer = new Float32Array(W * H);
  params.t = 0;

  // LCG seeded RNG
  params.rngState = 98712345;
  params.rand = () => {
    params.rngState = (1664525 * params.rngState + 1013904223) | 0;
    return (params.rngState >>> 0) / 4294967296;
  };

  // 800 volumetric viscous droplets
  params.count = 800;
  params.px = new Float32Array(params.count);
  params.py = new Float32Array(params.count);
  params.pz = new Float32Array(params.count);

  for (let i = 0; i < params.count; i++) {
    params.px[i] = (params.rand() - 0.5) * 1.4;
    params.py[i] = (params.rand() - 0.5) * 1.4;
    params.pz[i] = 0.4 + params.rand() * 2.2;
  }

  // Pre-fill buffer
  for (let step = 0; step < 60; step++) {
    for (let i = 0; i < params.count; i++) {
      let x = params.px[i];
      let y = params.py[i];
      let z = params.pz[i];

      const dx = (z - 0.7) * x - 3.5 * y;
      const dy = 3.5 * x + (z - 0.7) * y;
      const dz = 0.6 + 0.95 * z - (z * z * z) / 3.0 - (x * x + y * y) * (1.0 + 0.25 * z);

      x += dx * 0.014;
      y += dy * 0.014;
      z += dz * 0.014;

      params.px[i] = x;
      params.py[i] = y;
      params.pz[i] = z;

      const sx = Math.floor((x + 1.55) * 20.5);
      const sy = Math.floor((2.6 - z) * 44.0);
      if (sx >= 0 && sx < W && sy >= 0 && sy < H) {
        params.buffer[sy * W + sx] += 0.5;
      }
    }
  }
}

export function update(dt, input, params) {
  const safeDt = Math.min(Math.max(dt, 0.001), 0.1);
  params.t += safeDt;

  const a = input.knobValues ? input.knobValues[0] : 0.95;
  const speed = input.knobValues ? input.knobValues[1] : 1.1;
  const d = input.knobValues ? input.knobValues[2] : 3.5;
  params.thickness = input.knobValues ? input.knobValues[3] : 1.6;

  const b = 0.7;
  const c = 0.6;
  const e = 0.25;
  const f = 0.1;

  const W = params.w;
  const H = params.h;
  const buf = params.buffer;

  // Buffer fade
  const fade = Math.pow(0.87, safeDt * 60.0);
  for (let i = 0; i < W * H; i++) {
    buf[i] *= fade;
  }

  const h = 0.012 * speed;
  const count = params.count;

  for (let i = 0; i < count; i++) {
    let x = params.px[i];
    let y = params.py[i];
    let z = params.pz[i];

    const rSq = x * x + y * y;
    const dx = (z - b) * x - d * y;
    const dy = d * x + (z - b) * y;
    const dz = c + a * z - (z * z * z) / 3.0 - rSq * (1.0 + e * z) + f * z * (x * x * x);

    x += dx * h;
    y += dy * h;
    z += dz * h;

    if (!Number.isFinite(x) || !Number.isFinite(y) || !Number.isFinite(z) || z < -0.8 || rSq + z * z > 25.0) {
      x = (params.rand() - 0.5) * 1.5;
      y = (params.rand() - 0.5) * 0.5;
      z = 2.4 + params.rand() * 0.3;
    }

    params.px[i] = x;
    params.py[i] = y;
    params.pz[i] = z;

    // Splat a 3x3 soft cross kernel for thick, visceral volumetric weight
    const cx = Math.floor((x + 1.55) * 20.5);
    const cy = Math.floor((2.6 - z) * 44.0);

    if (cx >= 0 && cx < W && cy >= 0 && cy < H) {
      const idx = cy * W + cx;
      buf[idx] += 0.32;
      if (cx > 0) buf[idx - 1] += 0.16;
      if (cx < W - 1) buf[idx + 1] += 0.16;
      if (cy > 0) buf[idx - W] += 0.16;
      if (cy < H - 1) buf[idx + W] += 0.16;
    }
  }
}

export function draw(display, params, time) {
  const W = display.width;
  const H = display.height;
  const buf = params.buffer;
  const thick = params.thickness || 1.6;

  for (let y = 0; y < H; y++) {
    const row = y * W;
    for (let x = 0; x < W; x++) {
      let raw = buf[row + x];
      if (!Number.isFinite(raw)) raw = 0.0;

      // Heavy ink response curve with thick volumetric body
      let val = 1.0 - Math.exp(-raw * 0.38 * thick);
      val = val > 1.0 ? 1.0 : (val < 0.0 ? 0.0 : val);

      // Accentuate bright core while keeping deep black borders
      const v = Math.pow(val, 0.85);
      display.setValue(x, y, v);
    }
  }
}

// ── Made with Patternflow Community · https://patternflow.work/community ──
// Shared under CC-BY-SA-4.0. Attribution is part of this licence —
// please keep this notice and the author credit above when you reuse,
// remix, or redistribute this pattern. Do not delete it.
