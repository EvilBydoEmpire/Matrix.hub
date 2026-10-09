// ===== Patternflow pattern =====
// Title:    Mercury Rain
// Author:   @Besoiobiy
// Date:     2026-09-23
// SPDX-License-Identifier: CC-BY-SA-4.0
// ===============================

// @ramp linear 0:#000000 1:#ffffff
// @knobs Rain=0.3..3, Size=0.5..2.5, Speed=0.5..2.5, Chrome=0.5..2.2
// @matrix 64x128





// Liquid metal surface with falling raindrops. Each drop sends an expanding
// ring wave that decays in a few seconds. Ambient waves keep the surface
// alive. Pixel brightness = local slope of the height field, which is how
// real metal reads: edges of ripples glow, flat areas stay dark. Everything
// is deterministic — drop positions come from a hash of their index.

export function setup(params) {
  params.W = 0; params.H = 0;
  params.h = null;
  params.h2 = null;
}

function hashF(i) {
  const x = Math.sin(i * 78.233 + 12.345) * 43758.5453;
  return x - Math.floor(x);
}

export function update(dt, input, params) {
  params.knobValues = input.knobValues;
}

export function draw(display, params, time) {
  const W = display.width, H = display.height;
  const N = W * H;

  if (!params.h || params.W !== W || params.H !== H) {
    params.h  = new Float32Array(N);
    params.h2 = new Float32Array(N);
    params.W = W; params.H = H;
  }
  const h = params.h;

  const k = params.knobValues || [1.0, 1.2, 1.2, 1.0];
  const rainRate = Math.max(0.05, k[0]);   // drops per second
  const size     = k[1];                    // ripple size scale
  const speed    = k[2];                    // ripple expansion speed
  const chrome   = k[3];                    // gradient amplification
  const t = time;

  // ==================== height field ====================
  // clear
  for (let i = 0; i < N; i++) h[i] = 0;

  // Ambient: two slow diagonal ripples travelling across, never fade
  const aA1 = t * 0.55;
  const aA2 = t * 0.80;
  const kAmbX = 0.09;
  const kAmbY = 0.06;
  for (let y = 0; y < H; y++) {
    const ya = y * kAmbY;
    for (let x = 0; x < W; x++) {
      const i = y * W + x;
      h[i] += 0.10 * Math.sin(x * kAmbX + ya * 1.3 + aA1)
            + 0.07 * Math.cos(x * 0.13 - ya * 0.9 - aA2);
    }
  }

  // Drops
  const lifetime = 4.0;
  const period = 1 / rainRate;
  const kMax = Math.floor(t / period);
  const maxActive = Math.ceil(lifetime * rainRate) + 1;
  const kMin = kMax - maxActive;

  const speedPx = 55 * speed;              // expansion speed in px/sec
  const kRing  = 0.28 / size;              // wavenumber inside a ring
  const sig0   = 1.6 * size;               // initial ring width
  const sigGrow = 0.35 * size;             // how much ring widens with age

  for (let kk = kMin; kk <= kMax; kk++) {
    if (kk < 0) continue;
    const t0 = kk * period;
    const age = t - t0;
    if (age < 0 || age > lifetime) continue;

    // drop position from hash of index
    const px = hashF(kk * 7 + 1) * W;
    const py = hashF(kk * 7 + 2) * H;

    const decay = Math.exp(-age * 0.9);        // overall fade
    if (decay < 0.02) continue;

    const front = age * speedPx;
    const sig = sig0 + age * sigGrow;
    const invSig2 = 1 / (sig * sig);
    const sigGate2 = invSig2 * 0.09;           // (3·sigma)^-2

    // only touch pixels within a bounded box
    const rMax = front + sig * 4;
    const x0 = Math.max(0, Math.floor(px - rMax));
    const x1 = Math.min(W - 1, Math.ceil(px + rMax));
    const y0 = Math.max(0, Math.floor(py - rMax));
    const y1 = Math.min(H - 1, Math.ceil(py + rMax));
    const rMax2 = rMax * rMax;

    for (let y = y0; y <= y1; y++) {
      const dy = y - py;
      const dy2 = dy * dy;
      const row = y * W;
      for (let x = x0; x <= x1; x++) {
        const dx = x - px;
        const d2 = dx * dx + dy2;
        if (d2 > rMax2) continue;
        const d = Math.sqrt(d2);
        const dr = d - front;
        const band = dr * dr * sigGate2;       // normalized (3σ)²
        if (band > 1) continue;
        const env = Math.exp(-band * 9);       // gaussian ring envelope
        const phase = kRing * dr;              // 0 at front
        const ring = Math.sin(phase) * env;
        const fall = 1 / (1 + d * 0.05);       // energy spread
        h[row + x] += ring * decay * fall * 1.6;
      }
    }
  }

  // ==================== chrome shading ====================
  // gradient of h → brightness; multiply by chrome; tone map
  const inv2 = 0.5;
  const chromeS = chrome * 3.6;

  for (let y = 0; y < H; y++) {
    const yM = y > 0 ? y - 1 : 0;
    const yP = y < H - 1 ? y + 1 : H - 1;
    const rM = yM * W;
    const rP = yP * W;
    const rC = y * W;
    for (let x = 0; x < W; x++) {
      const xM = x > 0 ? x - 1 : 0;
      const xP = x < W - 1 ? x + 1 : W - 1;

      const gx = (h[rC + xP] - h[rC + xM]) * inv2;
      const gy = (h[rP + x] - h[rM + x]) * inv2;

      let g = Math.sqrt(gx * gx + gy * gy) * chromeS;
      if (g > 1) g = 1;
      // smoothstep for nicer edges
      g = g * g * (3 - 2 * g);
      display.setValue(x, y, g);
    }
  }
}

// ── Made with Patternflow Community · https://patternflow.work/community ──
// Shared under CC-BY-SA-4.0. Attribution is part of this licence —
// please keep this notice and the author credit above when you reuse,
// remix, or redistribute this pattern. Do not delete it.
