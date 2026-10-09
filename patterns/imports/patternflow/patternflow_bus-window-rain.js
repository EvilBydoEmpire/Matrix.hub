// ===== Patternflow pattern =====
// Title:    Bus Window Rain
// Author:   @engmung
// Date:     2026-09-27
// SPDX-License-Identifier: CC-BY-SA-4.0
// ===============================

// @knobs Rain=2..16, Fog=0.5..4, Speed=0..1, Focus=0.06..1
// @matrix 64x128

// Rain drops/s · Fog regrowth · Speed of the bus · Focus of the city lights · any button: a hand wipes a band of fog

const BW_MAXD = 130;
const BW_K = 5.5;          // drop lens: how wide a view each bead inverts

// u, v (0..1 of the frame), kind, size, depth
// kinds: 0 sodium lamp, 1 traffic stack, 2 neon pink, 3 neon cyan, 4 tail light, 5 headlight, 6 window
const BW_SCENE = [
  [0.10, 0.10, 0, 1.3, 2.2], [0.46, 0.07, 0, 1.1, 3.0], [0.82, 0.13, 0, 1.4, 1.8],
  [0.30, 0.20, 6, 0.8, 3.2], [0.62, 0.17, 6, 0.8, 3.4], [0.95, 0.24, 6, 0.9, 2.8],
  [0.16, 0.24, 2, 1.0, 1.6], [0.16, 0.29, 2, 1.0, 1.6], [0.16, 0.34, 2, 1.0, 1.6],
  [0.86, 0.33, 3, 1.1, 1.5], [0.40, 0.31, 0, 1.0, 2.6],
  [0.66, 0.38, 1, 1.2, 1.3], [0.28, 0.46, 1, 0.8, 2.4],
  [0.08, 0.55, 5, 1.2, 1.1], [0.24, 0.55, 5, 1.2, 1.1],
  [0.52, 0.60, 4, 1.1, 1.0], [0.70, 0.60, 4, 1.1, 1.0],
  [0.86, 0.66, 4, 1.3, 0.8], [1.08, 0.66, 4, 1.3, 0.8],
  [0.36, 0.70, 5, 1.4, 0.7], [0.58, 0.70, 5, 1.4, 0.7],
];
const BW_ROAD = 0.76;

function bwRgb(kind, i, p, out) {
  let r = 255, g = 150, b = 40, a = 1;
  if (kind === 1) {
    const ph = (p.t * 0.1 + i * 0.37) % 1;       // green, amber, red
    const s = ph < 0.5 ? 0 : ph < 0.62 ? 1 : 2;
    if (s === 0) { r = 20; g = 255; b = 120; } else if (s === 1) { r = 255; g = 170; b = 0; } else { r = 255; g = 20; b = 20; }
  } else if (kind === 2) {
    r = 255; g = 30; b = 170;
    a = p.flick > 0 && i === 7 ? 0.1 : 1;
  } else if (kind === 3) { r = 0; g = 225; b = 255; }
  else if (kind === 4) { r = 255; g = 18; b = 28; }
  else if (kind === 5) { r = 225; g = 235; b = 255; }
  else if (kind === 6) { r = 255; g = 205; b = 120; a = 0.6; }
  out[0] = r; out[1] = g; out[2] = b; out[3] = a;
}

export function setup(params) {
  params.t = 0;
  params.flick = 0;
  params.acc = 0;
  params.gust = -1;
  params.u = new Float32Array(BW_SCENE.length);
  params.v = new Float32Array(BW_SCENE.length);
  params.own = new Float32Array(BW_SCENE.length);
  for (let i = 0; i < BW_SCENE.length; i++) {
    params.u[i] = BW_SCENE[i][0];
    params.v[i] = BW_SCENE[i][1];
    const k = BW_SCENE[i][2];
    params.own[i] = k === 5 ? 0.035 : k === 4 ? -0.02 : 0;   // cross traffic even while parked
  }
  params.W = 0;
}

function bwAlloc(params, w, h) {
  const n = w * h;
  params.W = w; params.H = h; params.S = Math.min(w, h) / 64;
  params.fog = new Float32Array(n).fill(1);
  params.grain = new Float32Array(n);
  for (let i = 0; i < n; i++) params.grain[i] = 0.95 + Math.random() * 0.1;
  params.bgS = new Float32Array(n * 3);
  params.bgF = new Float32Array(n * 3);
  params.out = new Float32Array(n * 3);
  params.dx = new Float32Array(BW_MAXD);
  params.dy = new Float32Array(BW_MAXD);
  params.dr = new Float32Array(BW_MAXD);
  params.dp = new Float32Array(BW_MAXD);
  params.dn = 0;
  for (let i = 0; i < 90; i++) bwSpawn(params, 0);
}

function bwSpawn(params, r0) {
  if (params.dn >= BW_MAXD) return;
  const S = params.S, k = params.dn++;
  params.dx[k] = Math.random() * params.W;
  params.dy[k] = Math.random() * params.H;
  const q = Math.random();
  params.dr[k] = (r0 || (q < 0.78 ? 0.9 + q * 0.9 : 1.6 + (q - 0.78) * 5)) * S;
  params.dp[k] = Math.random() * 0.5;
}

function bwBead(params, x, y, r) {
  if (params.dn >= BW_MAXD) return;
  const k = params.dn++;
  params.dx[k] = x; params.dy[k] = y; params.dr[k] = r; params.dp[k] = 0;
}

function bwKill(params, k) {
  const j = --params.dn;
  params.dx[k] = params.dx[j]; params.dy[k] = params.dy[j];
  params.dr[k] = params.dr[j]; params.dp[k] = params.dp[j];
}

function bwWipe(params, x, y, rad) {
  const w = params.W, h = params.H, fog = params.fog;
  const x0 = Math.max(0, Math.floor(x - rad)), x1 = Math.min(w - 1, Math.ceil(x + rad));
  const y0 = Math.max(0, Math.floor(y - rad)), y1 = Math.min(h - 1, Math.ceil(y + rad));
  const r2 = rad * rad;
  for (let yy = y0; yy <= y1; yy++) {
    const ey = yy + 0.5 - y;
    for (let xx = x0; xx <= x1; xx++) {
      const ex = xx + 0.5 - x;
      const d2 = ex * ex + ey * ey;
      if (d2 < r2) {
        const v = d2 / r2 * 0.5;
        const i = yy * w + xx;
        if (fog[i] > v) fog[i] = v;
      }
    }
  }
}

export function update(dt, input, params) {
  const kv = input.knobValues;
  params.rain = kv[0]; params.fogRate = kv[1]; params.speed = kv[2]; params.focus = kv[3];
  if (dt > 0.05) dt = 0.05;
  const speed = params.speed;
  params.t = (params.t + dt) % 1000;
  params.flick = params.flick > 0 ? params.flick - dt : (Math.random() < dt * 0.25 ? 0.12 + Math.random() * 0.3 : 0);

  // city drifts past (normalized coordinates, no frame needed)
  for (let i = 0; i < BW_SCENE.length; i++) {
    const depth = BW_SCENE[i][4];
    params.u[i] += dt * (params.own[i] - speed * 0.45 / depth);
    if (params.u[i] < -0.3) { params.u[i] += 1.6; if (BW_SCENE[i][2] >= 4) params.v[i] = BW_SCENE[i][1] + (Math.random() - 0.5) * 0.04; }
    if (params.u[i] > 1.3) params.u[i] -= 1.6;
  }

  const b = input.btnPressed || [];
  if (b[0] || b[1] || b[2] || b[3]) {
    params.gust = 0;
    params.gy = 0.2 + Math.random() * 0.6;
    params.gs = (Math.random() < 0.5 ? -1 : 1) * (0.6 + Math.random() * 0.5);
  }
  if (!params.W) return;
  const w = params.W, h = params.H, S = params.S, fog = params.fog;

  const reg = dt * params.fogRate * 0.125;
  for (let i = 0; i < fog.length; i++) { const f = fog[i] + reg; fog[i] = f > 1 ? 1 : f; }

  params.acc += dt * params.rain * (1 + speed);
  while (params.acc >= 1) { params.acc -= 1; bwSpawn(params, 0); }

  const dx = params.dx, dy = params.dy, dr = params.dr, dp = params.dp;
  const rc = (1.75 - 0.4 * speed) * S;
  for (let k = params.dn - 1; k >= 0; k--) {
    let r = dr[k];
    if (r < rc) {                      // stuck bead: condensation slowly fattens it
      if (r > 0.95 * S) r += dt * 0.012 * S;
      dr[k] = r;
      if (r > 0.9 * S) bwWipe(params, dx[k], dy[k], r * 0.9);
      continue;
    }
    if (dp[k] > 0) { dp[k] -= dt; bwWipe(params, dx[k], dy[k], r * 1.05); continue; }
    const m = r * r / (S * S), mc = rc * rc / (S * S);
    const v = (7 + 5.5 * (m - mc)) * S;
    let sx = -speed * (6 + v * 0.9 / S) * S * dt;
    let sy = v * dt * (1 - 0.35 * speed);
    const steps = Math.max(1, Math.ceil(Math.max(Math.abs(sx), sy)));
    sx /= steps; sy /= steps;
    for (let s = 0; s < steps; s++) {
      dx[k] += sx + (Math.random() - 0.5) * 0.25 * S;
      dy[k] += sy;
      bwWipe(params, dx[k], dy[k], r * 1.05);
      if (Math.random() < 0.035 * Math.abs(sy) / S && r > 1.4 * S) {
        const br = (0.55 + Math.random() * 0.35) * S;
        bwBead(params, dx[k] + (Math.random() - 0.5) * r * 0.6, dy[k] - r * 0.9, br);
        r = Math.sqrt(Math.max(0, r * r - br * br * 0.6));
      }
    }
    dr[k] = r;
    if (Math.random() < dt * 2.2 * mc / m) dp[k] = 0.1 + Math.random() * 0.8;   // stick
    if (dy[k] - r > h || dx[k] + r < 0 || dx[k] - r > w) bwKill(params, k);
  }

  // merge touching drops (larger keeps its place)
  for (let i = 0; i < params.dn; i++) {
    for (let j = i + 1; j < params.dn; j++) {
      const ex = dx[j] - dx[i], ey = dy[j] - dy[i], rs = (dr[i] + dr[j]) * 0.72;
      if (ex * ex + ey * ey < rs * rs) {
        const mi = dr[i] * dr[i], mj = dr[j] * dr[j], wj = mj / (mi + mj);
        dx[i] += ex * wj; dy[i] += ey * wj;
        dr[i] = Math.min(5.5 * S, Math.sqrt(mi + mj));
        dp[i] = Math.min(dp[i], dp[j]) * 0.5;
        bwKill(params, j);
        j = i;
      }
    }
  }

  // a hand wipes a diagonal band across the glass
  if (params.gust >= 0) {
    const g0 = params.gust;
    params.gust += dt / 0.4;
    const xa = (g0 * 1.3 - 0.15) * w, xb = (params.gust * 1.3 - 0.15) * w;
    const half = h * 0.13;
    for (let x = Math.max(0, Math.floor(xa)); x < Math.min(w, Math.ceil(xb)); x++) {
      const yc = params.gy * h + (x - w / 2) * params.gs;
      for (let y = Math.max(0, Math.floor(yc - half)); y < Math.min(h, Math.ceil(yc + half)); y++) {
        const e = Math.abs(y - yc) / half;
        const i = y * w + x;
        const f = e * e * e * 0.8;
        if (fog[i] > f) fog[i] = f;
      }
      for (let k = params.dn - 1; k >= 0; k--) {
        if (dx[k] >= x && dx[k] < x + 1 && Math.abs(dy[k] - yc) < half) bwKill(params, k);
      }
      if (Math.random() < 0.35) bwBead(params, x + 0.5, yc + (Math.random() < 0.5 ? -1 : 1) * half * (0.8 + Math.random() * 0.25), (0.6 + Math.random() * 0.4) * S);
    }
    if (params.gust >= 1) params.gust = -1;
  }
}

const bwC = [0, 0, 0, 0];
const bwLx = new Float32Array(64), bwLy = new Float32Array(64), bwLr = new Float32Array(64);
const bwLc = new Float32Array(64 * 3);
const bwSx = new Float32Array(64), bwSy = new Float32Array(64), bwSr = new Float32Array(64);
const bwSc = new Float32Array(64 * 3);

function bwSplat(buf, w, h, cx, cy, rx, ry, cr, cg, cb, soft) {
  const x0 = Math.max(0, Math.floor(cx - rx)), x1 = Math.min(w - 1, Math.ceil(cx + rx));
  const y0 = Math.max(0, Math.floor(cy - ry)), y1 = Math.min(h - 1, Math.ceil(cy + ry));
  if (x0 > x1 || y0 > y1) return;
  const irx = 1 / (rx * rx), iry = 1 / (ry * ry);
  const edge = Math.min(rx, ry);
  for (let y = y0; y <= y1; y++) {
    const ey = y + 0.5 - cy, ty = ey * ey * iry;
    if (ty >= 1) continue;
    let o = (y * w + x0) * 3;
    for (let x = x0; x <= x1; x++, o += 3) {
      const ex = x + 0.5 - cx;
      const t = ex * ex * irx + ty;
      if (t >= 1) continue;
      let a;
      if (soft) { a = 1 - t; a *= a; }
      else {
        a = (1 - Math.sqrt(t)) * edge * 1.3;     // anti-aliased disc rim
        if (a > 1) a = 1;
        a *= 0.7 + 0.45 * t;                     // bokeh: slightly brighter ring
      }
      buf[o] += cr * a; buf[o + 1] += cg * a; buf[o + 2] += cb * a;
    }
  }
}

export function draw(display, params, time) {
  const w = display.width, h = display.height;
  if (params.W !== w || params.H !== h) bwAlloc(params, w, h);
  const S = params.S, speed = params.speed || 0;
  const focus = params.focus === undefined ? 0.06 : params.focus;
  const bgS = params.bgS, bgF = params.bgF, out = params.out, fog = params.fog, grain = params.grain;

  // night street ambience
  for (let y = 0; y < h; y++) {
    const v = y / h;
    const road = v > BW_ROAD ? 1 : 0;
    const sr = 3 + road * 6, sg = 4 + road * 4, sb = 12 + road * 4;
    const fr = 34 + v * 10, fg = 42 + v * 6, fb = 60 - v * 6;
    let o = y * w * 3;
    for (let x = 0; x < w; x++, o += 3) {
      bgS[o] = sr; bgS[o + 1] = sg; bgS[o + 2] = sb;
      bgF[o] = fr; bgF[o + 1] = fg; bgF[o + 2] = fb;
    }
  }

  // lights: sharp bokeh for clear glass, wide dim glow for frost, points for drop lenses
  let nl = 0;
  const blur = 1 - focus;
  for (let i = 0; i < BW_SCENE.length; i++) {
    const L = BW_SCENE[i], kind = L[2], size = L[3], depth = L[4];
    bwRgb(kind, i, params, bwC);
    let cy = params.v[i] * h, cx = params.u[i] * w;
    let amp = bwC[3];
    if (kind === 1) {                          // traffic stack: one lamp lit
      const ph = (params.t * 0.1 + i * 0.37) % 1;
      const s = ph < 0.5 ? 0 : ph < 0.62 ? 1 : 2;
      for (let lamp = 0; lamp < 3; lamp++) {
        const ly = cy + (lamp - 1) * 3.2 * size * S;
        const on = (2 - lamp) === s;
        const lr = on ? bwC[0] : 60, lg = on ? bwC[1] : 40, lb = on ? bwC[2] : 30;
        const la = on ? 1 : 0.12;
        nl = bwLight(params, w, h, cx, ly, size, depth, lr, lg, lb, la, blur, speed, S, nl, true);
      }
      continue;
    }
    nl = bwLight(params, w, h, cx, cy, size, depth, bwC[0], bwC[1], bwC[2], amp, blur, speed, S, nl, kind >= 4 || kind === 3);
  }
  params.nl = nl;

  // glass: frost where fogged, the sharp city where wiped
  const n = w * h;
  for (let i = 0, o = 0; i < n; i++, o += 3) {
    let f = fog[i];
    f = f * f * (3 - 2 * f);
    const g = grain[i] * f, c = 1 - f;
    out[o] = bgS[o] * c + bgF[o] * g;
    out[o + 1] = bgS[o + 1] * c + bgF[o + 1] * g;
    out[o + 2] = bgS[o + 2] * c + bgF[o + 2] * g;
  }

  // drops: each bead is a little inverted lens of the city
  const dx = params.dx, dy = params.dy, dr = params.dr;
  for (let k = 0; k < params.dn; k++) {
    const cx = dx[k], cy = dy[k], r = dr[k];
    if (r < 1.0 * S) {
      const px = cx | 0, py = cy | 0;
      if (px < 0 || py < 0 || px >= w || py >= h) continue;
      const o = (py * w + px) * 3;
      out[o] = out[o] * 0.5 + 95; out[o + 1] = out[o + 1] * 0.5 + 105; out[o + 2] = out[o + 2] * 0.5 + 125;
      continue;
    }
    // lights this lens can see
    let m = 0, hr = 0, hg = 0, hb = 0, hw = 0;
    const reach = r * BW_K + 3 * S;
    for (let l = 0; l < nl && m < 64; l++) {
      const ex = bwLx[l] - cx, ey = bwLy[l] - cy;
      const d2 = ex * ex + ey * ey;
      if (d2 < reach * reach) {
        bwSx[m] = bwLx[l]; bwSy[m] = bwLy[l]; bwSr[m] = bwLr[l];
        bwSc[m * 3] = bwLc[l * 3]; bwSc[m * 3 + 1] = bwLc[l * 3 + 1]; bwSc[m * 3 + 2] = bwLc[l * 3 + 2];
        const wgt = 1 / (1 + d2 * 0.01);
        hr += bwSc[m * 3] * wgt; hg += bwSc[m * 3 + 1] * wgt; hb += bwSc[m * 3 + 2] * wgt; hw += wgt;
        m++;
      }
    }
    const hl = 0.35 + 0.4 * Math.min(1, hw);
    const hn = hw > 0 ? 1 / Math.max(hw, 0.6) : 0;
    const hR = 150 * hl + hr * hn * 0.4, hG = 160 * hl + hg * hn * 0.4, hB = 185 * hl + hb * hn * 0.4;
    const x0 = Math.max(0, Math.floor(cx - r)), x1 = Math.min(w - 1, Math.ceil(cx + r));
    const y0 = Math.max(0, Math.floor(cy - r)), y1 = Math.min(h - 1, Math.ceil(cy + r));
    const ir = 1 / r, rp = 1.15 * S * BW_K, irp = 1 / (rp * rp);
    for (let py = y0; py <= y1; py++) {
      const ey = py + 0.5 - cy;
      for (let px = x0; px <= x1; px++) {
        const ex = px + 0.5 - cx;
        const d2 = ex * ex + ey * ey;
        if (d2 >= r * r) continue;
        const nd = Math.sqrt(d2) * ir;
        const sx = cx - ex * BW_K, sy = cy - ey * BW_K;
        // blurred city through the bead (from the frost glow buffer, upside down)
        let qx = sx | 0, qy = sy | 0;
        if (qx < 0) qx = 0; else if (qx >= w) qx = w - 1;
        if (qy < 0) qy = 0; else if (qy >= h) qy = h - 1;
        const q = (qy * w + qx) * 3;
        let R = (bgF[q] - 34) * 1.1 + 10, G = (bgF[q + 1] - 42) * 1.1 + 12, B = (bgF[q + 2] - 56) * 1.1 + 20;
        for (let l = 0; l < m; l++) {
          const lx = sx - bwSx[l], ly = sy - bwSy[l];
          const e = 1 - (lx * lx + ly * ly) * irp * bwSr[l];
          if (e > 0) { const a = e < 0.5 ? e * 3 : 1.5; R += bwSc[l * 3] * a; G += bwSc[l * 3 + 1] * a; B += bwSc[l * 3 + 2] * a; }
        }
        if (nd > 0.62) {
          const edge = (nd - 0.62) / 0.38;
          if (ey > 0.2) { const a = edge * Math.min(1, ey * ir * 1.6); R += hR * a; G += hG * a; B += hB * a; }
          else if (ey < -0.2) { const a = 1 - 0.75 * edge * Math.min(1, -ey * ir * 1.6); R *= a; G *= a; B *= a; }
        }
        const o = (py * w + px) * 3;
        out[o] = R; out[o + 1] = G; out[o + 2] = B;
      }
    }
  }

  for (let y = 0, o = 0; y < h; y++) {
    for (let x = 0; x < w; x++, o += 3) {
      let r = out[o], g = out[o + 1], b = out[o + 2];
      r = r > 255 ? 255 : r < 0 ? 0 : r;
      g = g > 255 ? 255 : g < 0 ? 0 : g;
      b = b > 255 ? 255 : b < 0 ? 0 : b;
      display.setPixel(x, y, r | 0, g | 0, b | 0);
    }
  }
}

// one light: splats into both glass buffers and records a point for the lenses
function bwLight(params, w, h, cx, cy, size, depth, cr, cg, cb, amp, blur, speed, S, nl, refl) {
  const R = S * (size * 0.85 + blur * (1.6 + size * 3.2));
  const streak = S * speed * 7 / depth;
  const a = amp * Math.min(1.25, Math.max(0.6, Math.sqrt(2.2 * S / R)));
  bwSplat(params.bgS, w, h, cx, cy, R + streak, R, cr * a, cg * a, cb * a, false);
  const RFy = S * (size * 2.2 + 4.5 + blur * 6.5), RF = RFy + streak;
  const af = amp * (0.5 + 0.25 * (1 - blur));
  bwSplat(params.bgF, w, h, cx, cy, RF, RFy, cr * af, cg * af, cb * af, true);
  if (nl < 64) {
    bwLx[nl] = cx; bwLy[nl] = cy; bwLr[nl] = 1 / (size * size);
    bwLc[nl * 3] = cr * amp; bwLc[nl * 3 + 1] = cg * amp; bwLc[nl * 3 + 2] = cb * amp;
    nl++;
  }
  if (refl) {                                   // wet-road reflection, stretched down
    const ry = BW_ROAD * h + (BW_ROAD * h - cy) * 0.7 + 4 * S;
    if (ry < h + R * 3) {
      const ar = a * 0.35;
      bwSplat(params.bgS, w, h, cx, ry, (R + streak) * 0.8, R * 3.2, cr * ar, cg * ar, cb * ar, true);
      bwSplat(params.bgF, w, h, cx, ry, RF * 0.8, RF * 1.6, cr * af * 0.35, cg * af * 0.35, cb * af * 0.35, true);
      if (nl < 64) {
        bwLx[nl] = cx; bwLy[nl] = ry; bwLr[nl] = 0.5 / (size * size);
        bwLc[nl * 3] = cr * amp * 0.3; bwLc[nl * 3 + 1] = cg * amp * 0.3; bwLc[nl * 3 + 2] = cb * amp * 0.3;
        nl++;
      }
    }
  }
  return nl;
}

// ── Made with Patternflow Community · https://patternflow.work/community ──
// Shared under CC-BY-SA-4.0. Attribution is part of this licence —
// please keep this notice and the author credit above when you reuse,
// remix, or redistribute this pattern. Do not delete it.
