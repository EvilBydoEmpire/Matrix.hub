// ===== Patternflow pattern =====
// Title:    Murmuration2x
// Author:   @engmung
// Date:     2026-08-07
// SPDX-License-Identifier: CC-BY-SA-4.0
// ===============================

// @ramp linear 0:#000000 0.983:#ffffff 0.552:#0008ff 0.506:#ff0000 0.508:#000000
// @matrix 64x128


// @knobs Flock=20..1000, Fear=6..40, Bond=0.2..2.5, Falcons=1..10

// Knobs: Flock   = number of starlings (Up to 1000)
//        Fear    = falcon panic radius
//        Bond    = cohesion+alignment strength
//        Falcons = number of falcons (1 to 10)
// Button: btn1   = scatter the flock to the winds

const MAXB = 1000;
const MAXF = 10;

export function setup(params) {
  params.buf = null; params.W = 0; params.H = 0;
  params.fbuf = null;
  params.bx = new Float64Array(MAXB);
  params.by = new Float64Array(MAXB);
  params.vx = new Float64Array(MAXB);
  params.vy = new Float64Array(MAXB);

  params.fx = new Float64Array(MAXF);
  params.fy = new Float64Array(MAXF);
  params.fvx = new Float64Array(MAXF);
  params.fvy = new Float64Array(MAXF);

  params.seed = 3141;
  params.dtAcc = 0;
  params.scatter = true;
}

function rnd(p) { p.seed = (p.seed * 1664525 + 1013904223) >>> 0; return p.seed / 4294967296; }

export function update(dt, input, params) {
  if (input.btnPressed && input.btnPressed[0]) params.scatter = true;
  params.dtAcc += dt;
}

export function draw(display, params, time) {
  const W = display.width, H = display.height;
  if (!params.buf || params.W !== W || params.H !== H) {
    params.buf = new Float32Array(W * H);
    params.fbuf = new Float32Array(W * H);
    params.W = W; params.H = H;
    params.scatter = true;
  }

  const k = params.knobValues || [250, 20, 1.2, 1];
  const n = Math.min(MAXB, Math.max(3, Math.round(k[0])));
  const fear = k[1] * 1.5; // 매 크기 확대에 맞춰 체감 공포 반경 확대
  const bond = k[2];
  const numFalcons = Math.min(MAXF, Math.max(1, Math.round(k[3])));

  const fade = 0.86;

  const buf = params.buf, fbuf = params.fbuf, len = W * H;
  const bx = params.bx, by = params.by, vx = params.vx, vy = params.vy;
  const fx = params.fx, fy = params.fy, fvx = params.fvx, fvy = params.fvy;

  let dt = params.dtAcc; params.dtAcc = 0; if (dt > 0.05) dt = 0.05;

  if (params.scatter) {
    for (let i = 0; i < MAXB; i++) {
      bx[i] = rnd(params) * W;
      by[i] = rnd(params) * H;
      const a = rnd(params) * 6.28318;
      vx[i] = Math.cos(a) * 20; vy[i] = Math.sin(a) * 20;
    }
    for (let f = 0; f < MAXF; f++) {
      fx[f] = rnd(params) * W;
      fy[f] = rnd(params) * H;
      fvx[f] = 0; fvy[f] = 0;
    }
    params.scatter = false;
  }

  for (let i = 0; i < len; i++) {
    buf[i] *= fade;
    fbuf[i] *= 0.82;
  }

  let cx = 0, cy = 0;
  for (let i = 0; i < n; i++) { cx += bx[i]; cy += by[i]; }
  cx /= n; cy /= n;

  // 매 시뮬레이션 및 2배 크기 그리기
  for (let f = 0; f < numFalcons; f++) {
    const offset = f * 1.3;
    let ax = (cx - fx[f]) * 1.6 + Math.sin(time * 1.7 + offset) * 35;
    let ay = (cy - fy[f]) * 1.6 + Math.cos(time * 1.3 + offset) * 35;

    fvx[f] = (fvx[f] + ax * dt) * 0.995;
    fvy[f] = (fvy[f] + ay * dt) * 0.995;

    const fsp = Math.sqrt(fvx[f] * fvx[f] + fvy[f] * fvy[f]);
    if (fsp > 55) { fvx[f] *= 55 / fsp; fvy[f] *= 55 / fsp; }

    fx[f] += fvx[f] * dt;
    fy[f] += fvy[f] * dt;

    // --- [매 크기 2배 확대: 2x2 영역 버퍼 기입] ---
    const ffx = fx[f] | 0, ffy = fy[f] | 0;
    for (let dx = 0; dx <= 1; dx++) {
      for (let dy = 0; dy <= 1; dy++) {
        const px = ffx + dx, py = ffy + dy;
        if (px >= 0 && px < W && py >= 0 && py < H) {
          fbuf[py * W + px] = 1.0;
        }
      }
    }
  }

  const R = 13, R2 = R * R, S2 = 16;
  const fear2 = fear * fear;

  for (let i = 0; i < n; i++) {
    let ncx = 0, ncy = 0, nvx = 0, nvy = 0, sx2 = 0, sy2 = 0, cnt = 0;
    const xi = bx[i], yi = by[i];
    for (let j = 0; j < n; j++) {
      if (j === i) continue;
      const dx = bx[j] - xi, dy = by[j] - yi;
      const d2 = dx * dx + dy * dy;
      if (d2 < R2) {
        ncx += dx; ncy += dy;
        nvx += vx[j]; nvy += vy[j];
        cnt++;
        if (d2 < S2 && d2 > 0.01) { sx2 -= dx / d2; sy2 -= dy / d2; }
      }
    }
    let fxx = 0, fyy = 0;
    if (cnt > 0) {
      fxx += (ncx / cnt) * bond * 2.2 + (nvx / cnt - vx[i]) * bond * 1.4;
      fyy += (ncy / cnt) * bond * 2.2 + (nvy / cnt - vy[i]) * bond * 1.4;
    }
    fxx += sx2 * 130; fyy += sy2 * 130;

    // 매에 대한 패닉 힘 (매 크기가 커졌으므로 2배 반경 및 연산 보정)
    for (let f = 0; f < numFalcons; f++) {
      const pdx = xi - fx[f], pdy = yi - fy[f];
      const pd2 = pdx * pdx + pdy * pdy;
      if (pd2 < fear2 && pd2 > 0.01) {
        const pd = Math.sqrt(pd2);
        const p = (1 - pd / fear) * 850 / pd;
        fxx += pdx * p; fyy += pdy * p;
      }
    }

    if (xi < 8) fxx += (8 - xi) * 9; else if (xi > W - 8) fxx -= (xi - W + 8) * 9;
    if (yi < 10) fyy += (10 - yi) * 9; else if (yi > H - 10) fyy -= (yi - H + 10) * 9;

    vx[i] += fxx * dt; vy[i] += fyy * dt;
    const sp = Math.sqrt(vx[i] * vx[i] + vy[i] * vy[i]);
    if (sp > 34) { vx[i] *= 34 / sp; vy[i] *= 34 / sp; }
    else if (sp < 13 && sp > 0.01) { vx[i] *= 13 / sp; vy[i] *= 13 / sp; }
    bx[i] += vx[i] * dt; by[i] += vy[i] * dt;

    const ix = bx[i] | 0, iy = by[i] | 0;
    if (ix >= 0 && ix < W && iy >= 0 && iy < H) {
      const idx = iy * W + ix;
      const v = buf[idx] + 0.34;
      buf[idx] = v > 1 ? 1 : v;
    }
  }

  for (let y = 0, i = 0; y < H; y++) {
    for (let x = 0; x < W; x++, i++) {
      const birdVal = buf[i];
      const falconVal = fbuf[i];

      let finalV = 0;

      if (birdVal > 0.01) {
        const t = birdVal * birdVal * (3 - 2 * birdVal);
        finalV = 0.51 + t * 0.48;
      }

      if (falconVal > 0.01) {
        finalV = 0.05 + falconVal * 0.44;
      }

      display.setValue(x, y, finalV);
    }
  }
}

// ── Made with Patternflow Community · https://patternflow.work/community ──
// Shared under CC-BY-SA-4.0. Attribution is part of this licence —
// please keep this notice and the author credit above when you reuse,
// remix, or redistribute this pattern. Do not delete it.
