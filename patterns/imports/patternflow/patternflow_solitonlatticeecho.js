// ===== Patternflow pattern =====
// Title:    SolitonLatticeEcho
// Author:   @engmung
// Based on: "Hyperbolic Soliton Lattice Echo" by @engmung — https://patternflow.work/community/p/5c6720bf26b24d81
// Date:     2026-08-19
// SPDX-License-Identifier: CC-BY-SA-4.0
// ===============================

// @ramp oklab 0:#000000 0.243:#a1a1a1
// @knobs KinkEnergy=0.3..1108, RelPace=0.2..12, SolitonCount=1..5, EchoDecay=0.4..0.98
// @matrix 64x128

// critical knob = KinkEnergy, turning it crosses localized bound breathers ↔ relativistic kink-antikink collision cascades

/*
  Mechanics:
    사인-고르돈(Sine-Gordon) 비선형 파동 방정식의 위상 킨크(Kink)와 안티킨크(Antikink)의 상대론적 충돌을 구현.
    KinkEnergy 노브가 낮으면 에너지가 갇힌 진동하는 '브리더(Breather)' 상태를 유지하다가, 
    임계점을 넘으면 위상 점프(2pi phase flip)를 동반한 상대론적 충돌 파면이 세로 프레임 전체에 충격파 간섭 무늬를 새깁니다.
*/

export function setup(params) {
  params.W = 64;
  params.H = 128;
  const N = params.W * params.H;

  params.t = 0;
  params.maxNorm = 1.0;

  params.phi = new Float32Array(N);
  params.phiVel = new Float32Array(N);
  params.echo = new Float32Array(N);

  params.sampleEcho = function(u, v) {
    const x = (u + 0.5) * params.W - 0.5;
    const y = (v + 0.5) * params.H - 0.5;
    const x0 = Math.floor(x);
    const y0 = Math.floor(y);
    const fx = x - x0;
    const fy = y - y0;
    const cx0 = Math.max(0, Math.min(params.W - 1, x0));
    const cx1 = Math.max(0, Math.min(params.W - 1, x0 + 1));
    const cy0 = Math.max(0, Math.min(params.H - 1, y0));
    const cy1 = Math.max(0, Math.min(params.H - 1, y0 + 1));

    const v00 = params.echo[cy0 * params.W + cx0];
    const v10 = params.echo[cy0 * params.W + cx1];
    const v01 = params.echo[cy1 * params.W + cx0];
    const v11 = params.echo[cy1 * params.W + cx1];

    return (v00 * (1 - fx) + v10 * fx) * (1 - fy) + (v01 * (1 - fx) + v11 * fx) * fy;
  };

  // Warmup initialization with localized kink wavepackets
  for (let y = 0; y < params.H; y++) {
    const ny = (y / params.H) * 4.0 - 2.0;
    for (let x = 0; x < params.W; x++) {
      const nx = (x / params.W) * 2.0 - 1.0;
      params.phi[y * params.W + x] = 4.0 * Math.atan(Math.exp(nx * 3.0)) * Math.sin(ny * 2.0);
      params.phiVel[y * params.W + x] = 0;
      params.echo[y * params.W + x] = 0;
    }
  }
}

export function update(dt, input, params) {
  const safeDt = Math.min(dt || 0.016, 0.05);
  params.t += safeDt;

  const energy = input.knobValues ? input.knobValues[0] : 2.2;
  const pace   = input.knobValues ? input.knobValues[1] : 1.4;
  const count  = input.knobValues ? input.knobValues[2] : 3.0;
  const decay  = input.knobValues ? input.knobValues[3] : 0.88;

  if (input.btnPressed && input.btnPressed[0]) {
    for (let i = 0; i < params.W * params.H; i++) {
      params.phi[i] = 0;
      params.phiVel[i] = 0;
      params.echo[i] = 0;
    }
  }

  const W = params.W;
  const H = params.H;
  const t = params.t * pace;

  let maxInt = 0.01;

  for (let y = 0; y < H; y++) {
    const ny = (y / H) * 2.0 - 1.0;
    const yU = y > 0 ? y - 1 : H - 1;
    const yD = y < H - 1 ? y + 1 : 0;
    const row = y * W;

    for (let x = 0; x < W; x++) {
      const nx = (x / W) * 2.0 - 1.0;
      const xL = x > 0 ? x - 1 : W - 1;
      const xR = x < W - 1 ? x + 1 : 0;
      const idx = row + x;

      const p = params.phi[idx];
      const lap = params.phi[row + xL] + params.phi[row + xR] + params.phi[yU * W + x] + params.phi[yD * W + x] - 4.0 * p;

      // Sine-Gordon wave equation: d2phi/dt2 = c^2 * lap(phi) - sin(phi)
      const force = 0.35 * lap - Math.sin(p);
      let vel = (params.phiVel[idx] + safeDt * force) * 0.985;
      let nextPhi = p + safeDt * vel;

      // Relativistic kink injector
      const kx = nx + 0.35 * Math.sin(t * 1.3);
      const ky = ny - (t * 0.3) % 2.0;
      const kink1 = 4.0 * Math.atan(Math.exp((kx * 2.0 - ky * 1.5) * count * energy));
      const kink2 = 4.0 * Math.atan(Math.exp((-kx * 2.0 - ky * 1.5) * count * energy));
      nextPhi += (kink1 - kink2) * 0.035;

      if (!isFinite(nextPhi)) { nextPhi = 0; vel = 0; }

      params.phi[idx] = nextPhi;
      params.phiVel[idx] = vel;

      // Hyperbolic echo feedback sampling
      const u = nx * 0.48 + 0.02 * Math.sin(nextPhi * 2.0);
      const v = ny * 0.48 + 0.008 * Math.cos(nextPhi * 2.0) + 0.006;
      const prevEcho = params.sampleEcho(u, v);

      // Local energy density ~ (dphi/dt)^2 + |grad(phi)|^2 + (1 - cos(phi))
      const gradX = (params.phi[row + xR] - params.phi[row + xL]) * 0.5;
      const gradY = (params.phi[yD * W + x] - params.phi[yU * W + x]) * 0.5;
      const energyDensity = vel * vel + (gradX * gradX + gradY * gradY) * 0.5 + (1.0 - Math.cos(nextPhi));

      const nextEcho = prevEcho * decay + energyDensity * 0.3;
      params.echo[idx] = isFinite(nextEcho) ? nextEcho : 0;

      if (nextEcho > maxInt) maxInt = nextEcho;
    }
  }

  params.maxNorm += (maxInt - params.maxNorm) * 0.08;
}

export function draw(display, params, time) {
  const W = display.width;
  const H = display.height;
  const norm = 1.0 / Math.max(0.001, params.maxNorm);

  for (let y = 0; y < H; y++) {
    const row = y * W;
    for (let x = 0; x < W; x++) {
      const idx = row + x;
      const e = params.echo[idx] * norm;
      const p = params.phi[idx];

      // Structural phase crests
      const crest = Math.abs(Math.sin(p * 0.5));
      let tonal = e * 0.7 + crest * 0.35;
      tonal = Math.max(0.0, Math.min(1.0, tonal));

      const v = tonal * tonal * (3.0 - 2.0 * tonal);
      display.setValue(x, y, v);
    }
  }
}

// ── Made with Patternflow Community · https://patternflow.work/community ──
// Shared under CC-BY-SA-4.0. Attribution is part of this licence —
// please keep this notice and the author credit above when you reuse,
// remix, or redistribute this pattern. Do not delete it.
