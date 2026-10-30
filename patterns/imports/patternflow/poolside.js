// ===== Patternflow pattern =====
// Title:    Poolside
// Author:   @engmung
// Date:     2026-09-15
// SPDX-License-Identifier: CC-BY-SA-4.0
// ===============================

// @ramp linear 0:#000000 0.513:#ff0000 0.129:#1100ff
// @knobs Depth=3..30, Drift=0.2..4, Chop=0.4..2, Swell=20..80
// @matrix 64x128


// Knobs: Depth = pool depth (refraction focus: soft blur -> razor filaments -> chaos)
//        Drift = wave travel speed
//        Chop  = wave steepness (calm glass vs churned water)
//        Swell = dominant wavelength (ripples vs long rolling swell)
// Photons are refracted through a 3-wave surface and accumulated —
// the bright web is real light convergence, not a voronoi fake.

export function setup(params) {
  params.nb = null; params.acc = null;
  params.W = 0; params.H = 0;
}

export function update(dt, input, params) {
  // knobs are mirrored onto params by the harness; nothing else needed
}

export function draw(display, params, time) {
  const W = display.width, H = display.height;
  if (!params.nb || params.W !== W || params.H !== H) {
    params.nb = new Float32Array(W * H);
    params.acc = new Float32Array(W * H);
    params.W = W; params.H = H;
  }
  const k = params.knobValues || [12, 1.2, 1.2, 16];
  const depth = k[0], drift = k[1], chop = k[2], swell = k[3];
  const nb = params.nb, acc = params.acc;
  const len = W * H;
  nb.fill(0);

  // three travelling wave trains, mostly running down the tall axis
  const k1 = 6.28318 / swell, k2 = 6.28318 / (swell * 0.57), k3 = 6.28318 / (swell * 0.31);
  const d1x = 0.14, d1y = 0.99, d2x = -0.42, d2y = 0.91, d3x = 0.88, d3y = 0.47;
  const kx1 = k1 * d1x, ky1 = k1 * d1y;
  const kx2 = k2 * d2x, ky2 = k2 * d2y;
  const kx3 = k3 * d3x, ky3 = k3 * d3y;
  const a1 = chop * 0.55, a2 = chop * 0.40, a3 = chop * 0.26;
  const t1 = time * drift * 1.9, t2 = -time * drift * 2.6, t3 = time * drift * 4.1;

  for (let y = 0; y < H; y++) {
    const p1r = y * ky1 + t1, p2r = y * ky2 + t2, p3r = y * ky3 + t3;
    for (let x = 0; x < W; x++) {
      const c1 = Math.cos(x * kx1 + p1r);
      const c2 = Math.cos(x * kx2 + p2r);
      const c3 = Math.cos(x * kx3 + p3r);
      const gx = a1 * c1 * d1x + a2 * c2 * d2x + a3 * c3 * d3x;
      const gy = a1 * c1 * d1y + a2 * c2 * d2y + a3 * c3 * d3y;
      let tx = (x + gx * depth) | 0;
      let ty = (y + gy * depth) | 0;
      tx = ((tx % W) + W) % W;
      ty = ((ty % H) + H) % H;
      nb[ty * W + tx] += 0.2;
    }
  }

  // temporal blend smooths photon noise into liquid motion
  for (let i = 0; i < len; i++) acc[i] = acc[i] * 0.45 + nb[i] * 0.55;

  for (let y = 0, i = 0; y < H; y++) {
    for (let x = 0; x < W; x++, i++) {
      const a = acc[i];
      let v = a > 0.1 ? ((a - 0.1) * 1.6) / (1 + a) : 0;
      display.setValue(x, y, v > 1 ? 1 : v);
    }
  }
}

// ── Made with Patternflow Community · https://patternflow.work/community ──
// Shared under CC-BY-SA-4.0. Attribution is part of this licence —
// please keep this notice and the author credit above when you reuse,
// remix, or redistribute this pattern. Do not delete it.
