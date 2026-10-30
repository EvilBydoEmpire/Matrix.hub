// ===== Patternflow pattern =====
// Title:    Collatz Kelp
// Author:   @engmung
// Date:     2026-10-02
// SPDX-License-Identifier: CC-BY-SA-4.0
// ===============================

// @knobs Odd=0.28..0.8, Grow=0.3..5, Even=0.15..0.45, Seeds=300..1600
// @matrix 64x128

// Odd/Even: turn at 3n+1 / halving steps · Grow: growth and current speed · Seeds: how many numbers · any button regrows the reef

const CK_MAXN = 1600;
const CK_CAP = 4096;
const ckVal = new Float64Array(CK_CAP);
const ckPar = new Int32Array(CK_CAP);
const ckDep = new Int32Array(CK_CAP);
const ckOdd = new Uint8Array(CK_CAP);
const ckUpTo = new Int32Array(CK_MAXN + 1); // nodes in the trie once n = 1..k are in
const ckCnt = new Float32Array(CK_CAP);
const ckTop = new Int32Array(CK_CAP); // deepest leaf below each node
const ckAng = new Float32Array(CK_CAP);
const ckX = new Float32Array(CK_CAP);
const ckY = new Float32Array(CK_CAP);
const ckSway = new Float32Array(256);
const ckPath = [];
let ckNodes = 0;
let ckMaxDep = 1;

// Pastel-free reef palette along depth: abyss teal -> jade -> coral -> gold.
const CK_STOPS = [
  0.0, 10, 120, 130,
  0.3, 20, 200, 130,
  0.55, 110, 235, 120,
  0.75, 255, 110, 80,
  0.88, 255, 80, 130,
  1.0, 255, 205, 70,
];
const ckPal = new Float32Array(64 * 3);

function ckBuild() {
  const idx = new Map();
  idx.set(1, 0);
  ckVal[0] = 1; ckPar[0] = -1; ckDep[0] = 0; ckOdd[0] = 1;
  ckNodes = 1;
  ckUpTo[0] = 1; ckUpTo[1] = 1;
  for (let n = 2; n <= CK_MAXN; n++) {
    let v = n;
    ckPath.length = 0;
    while (!idx.has(v)) { ckPath.push(v); v = (v % 2) ? 3 * v + 1 : v / 2; }
    let p = idx.get(v);
    for (let i = ckPath.length - 1; i >= 0; i--) {
      const c = ckPath[i];
      const k = ckNodes++;
      ckVal[k] = c; ckPar[k] = p; ckDep[k] = ckDep[p] + 1; ckOdd[k] = c % 2;
      idx.set(c, k);
      if (ckDep[k] > ckMaxDep) ckMaxDep = ckDep[k];
      p = k;
    }
    ckUpTo[n] = ckNodes;
  }
  for (let i = 0; i < 64; i++) {
    const f = i / 63;
    let s = 0;
    while (s < CK_STOPS.length - 8 && f > CK_STOPS[s + 4]) s += 4;
    const a = CK_STOPS[s], b = CK_STOPS[s + 4];
    const u = Math.max(0, Math.min(1, (f - a) / (b - a)));
    for (let c = 0; c < 3; c++) ckPal[i * 3 + c] = CK_STOPS[s + 1 + c] + (CK_STOPS[s + 5 + c] - CK_STOPS[s + 1 + c]) * u;
  }
}

export function setup(params) {
  if (ckNodes === 0) ckBuild();
  params.odd = 0.28;
  params.grow = 2;
  params.even = 0.15;
  params.seeds = 300;
  params.t = 0;
  params.reveal = 0;
  params.fit = null;
  params.acc = null;
}

export function update(dt, input, params) {
  const k = input.knobValues;
  if (k) {
    params.odd = k[0];
    params.grow = k[1];
    params.even = k[2];
    params.seeds = k[3];
  }
  const b = input.btnPressed || [];
  for (let i = 0; i < b.length; i++) if (b[i]) params.reveal = 0;
  params.t += dt * (0.35 + 0.5 * params.grow);
  if (params.t > 1000 * Math.PI) params.t -= 1000 * Math.PI;
  if (params.reveal < 400) params.reveal += dt * (0.4 + params.grow) * (params.revRate || 20);
}

// Soft round dab of radius r (px) and weight wt into the RGB accumulator.
function ckDab(acc, w, h, x, y, r, wt, pr, pg, pb) {
  if (r < 0.75) {
    const x0 = Math.floor(x - 0.5), y0 = Math.floor(y - 0.5);
    const fx = x - 0.5 - x0, fy = y - 0.5 - y0;
    for (let j = 0; j < 2; j++) {
      const yy = y0 + j;
      if (yy < 0 || yy >= h) continue;
      const wy = j ? fy : 1 - fy;
      for (let i = 0; i < 2; i++) {
        const xx = x0 + i;
        if (xx < 0 || xx >= w) continue;
        const q = wt * wy * (i ? fx : 1 - fx);
        const o = (yy * w + xx) * 3;
        acc[o] += pr * q; acc[o + 1] += pg * q; acc[o + 2] += pb * q;
      }
    }
    return;
  }
  const x0 = Math.max(0, Math.floor(x - r)), x1 = Math.min(w - 1, Math.floor(x + r));
  const y0 = Math.max(0, Math.floor(y - r)), y1 = Math.min(h - 1, Math.floor(y + r));
  const norm = wt / (0.5 + 0.9 * r);
  const inv = 1 / (r + 0.5);
  for (let yy = y0; yy <= y1; yy++) {
    const dy = yy + 0.5 - y;
    for (let xx = x0; xx <= x1; xx++) {
      const dx = xx + 0.5 - x;
      const d2 = dx * dx + dy * dy;
      const c = 1 - Math.sqrt(d2) * inv;
      if (c <= 0) continue;
      const q = norm * c;
      const o = (yy * w + xx) * 3;
      acc[o] += pr * q; acc[o + 1] += pg * q; acc[o + 2] += pb * q;
    }
  }
}

export function draw(display, params, time) {
  const w = display.width, h = display.height;
  if (ckNodes === 0) ckBuild();
  if (!params.acc || params.acc.length !== w * h * 3) params.acc = new Float32Array(w * h * 3);
  const acc = params.acc;
  acc.fill(0);

  const seeds = Math.max(2, Math.min(CK_MAXN, Math.floor(params.seeds)));
  const N = ckUpTo[seeds];
  const odd = params.odd, even = params.even, t = params.t;

  // Subtree sizes: children always sit after their parent.
  let maxDep = 1;
  for (let i = 0; i < N; i++) { ckCnt[i] = 1; ckTop[i] = ckDep[i]; if (ckDep[i] > maxDep) maxDep = ckDep[i]; }
  for (let i = N - 1; i > 0; i--) {
    const p = ckPar[i];
    ckCnt[p] += ckCnt[i];
    if (ckTop[i] > ckTop[p]) ckTop[p] = ckTop[i];
  }
  params.revRate = maxDep / 9; // full reef in ~4 s at Grow 2
  const logMax = Math.log(1 + ckCnt[0]);

  // Current: a wave travelling up the stalks.
  for (let d = 0; d <= maxDep && d < 256; d++) ckSway[d] = 0.02 * Math.sin(t * 0.7 - d * 0.08) + 0.007 * Math.sin(t * 1.3 - d * 0.19 + 1.7);

  // Turtle walk (unrotated), then rotate so the frond's mass points up.
  ckAng[0] = 0; ckX[0] = 0; ckY[0] = 0;
  let sx = 0, sy = 0;
  for (let i = 1; i < N; i++) {
    const p = ckPar[i], d = ckDep[i];
    const a = ckAng[p] + (ckOdd[i] ? -odd : even) + ckSway[d < 256 ? d : 255];
    ckAng[i] = a;
    const st = 1.3 / (1 + 0.012 * d);
    ckX[i] = ckX[p] + st * Math.sin(a);
    ckY[i] = ckY[p] + st * Math.cos(a);
    sx += ckX[i]; sy += ckY[i];
  }
  const target = Math.atan2(sx, sy);
  if (!params.fit) params.fit = { rot: target, cx: 0, cy: 0, s: 1, init: false };
  const F = params.fit;
  let dr = target - F.rot;
  dr -= Math.round(dr / (2 * Math.PI)) * 2 * Math.PI;
  F.rot += dr * 0.08;
  const cr = Math.cos(F.rot), sr = Math.sin(F.rot);
  let minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
  for (let i = 0; i < N; i++) {
    const x = ckX[i] * cr - ckY[i] * sr;
    const y = ckX[i] * sr + ckY[i] * cr;
    ckX[i] = x; ckY[i] = y;
    if (x < minX) minX = x; if (x > maxX) maxX = x;
    if (y < minY) minY = y; if (y > maxY) maxY = y;
  }
  const m = 3;
  // Fit each axis, but never stretch one more than 1.6x the other.
  let fsx = (w - 2 * m) / Math.max(1e-3, maxX - minX), fsy = (h - 2 * m) / Math.max(1e-3, maxY - minY);
  if (fsx > fsy * 1.6) fsx = fsy * 1.6;
  if (fsy > fsx * 1.6) fsy = fsx * 1.6;
  const cx = (minX + maxX) * 0.5, cy = (minY + maxY) * 0.5;
  if (!F.init) { F.sx = fsx; F.sy = fsy; F.cx = cx; F.cy = cy; F.init = true; }
  else { F.sx += (fsx - F.sx) * 0.08; F.sy += (fsy - F.sy) * 0.08; F.cx += (cx - F.cx) * 0.08; F.cy += (cy - F.cy) * 0.08; }
  const SX = F.sx, SY = F.sy, S = Math.sqrt(SX * SY), ox = w * 0.5 - F.cx * SX, oy = h * 0.5 + F.cy * SY;

  // Draw every revealed segment; the growth front is a bright bud.
  const rev = params.reveal;
  const thick = Math.max(0.6, S * 0.9);
  for (let i = 1; i < N; i++) {
    const d = ckDep[i];
    if (d > rev + 1) continue;
    const p = ckPar[i];
    const grow = Math.min(1, rev + 1 - d);
    const lc = Math.log(1 + ckCnt[i]) / logMax;
    const pi = Math.min(63, Math.floor((d / ckTop[i]) * 63.99)) * 3;
    const sp = Math.max(0, Math.sin(d * 0.16 - t * 2.4));
    const pulse = 0.6 + 0.9 * sp * sp;
    const wt = (0.45 + 1.1 * lc) * pulse;
    const r = Math.min(1.9, 0.4 + thick * lc * lc * 1.6);
    const x0 = ox + ckX[p] * SX, y0 = oy - ckY[p] * SY;
    const x1 = x0 + (ckX[i] - ckX[p]) * SX * grow, y1 = y0 - (ckY[i] - ckY[p]) * SY * grow;
    const len = Math.abs(x1 - x0) + Math.abs(y1 - y0);
    const steps = 1 + Math.floor(len * 1.4);
    const sw = wt / steps * Math.min(1.4, len + 0.35);
    for (let k = 1; k <= steps; k++) {
      const u = k / steps;
      ckDab(acc, w, h, x0 + (x1 - x0) * u, y0 + (y1 - y0) * u, r, sw, ckPal[pi], ckPal[pi + 1], ckPal[pi + 2]);
    }
    if (ckCnt[i] === 1) ckDab(acc, w, h, x1, y1, 0.5, d >= rev ? 1.3 : 0.55 * pulse, 255, 225, 140);
  }

  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      const o = (y * w + x) * 3;
      let r = acc[o], g = acc[o + 1], b = acc[o + 2];
      const mx = Math.max(r, g, b);
      if (mx > 1) {
        // Soft knee: lift faint stalks for the LEDs, compress the trunk, bleed only the hottest toward white.
        let k = 1.5 / (mx + 128);
        if (mx * k > 1) k = 1 / mx;
        const e = mx > 400 ? Math.min(0.6, (mx - 400) / 1500) : 0;
        r *= k; g *= k; b *= k;
        r = (r + (1 - r) * e) * 255; g = (g + (1 - g) * e) * 255; b = (b + (1 - b) * e) * 255;
      }
      display.setPixel(x, y, r | 0, g | 0, b | 0);
    }
  }
}

// ── Made with Patternflow Community · https://patternflow.work/community ──
// Shared under CC-BY-SA-4.0. Attribution is part of this licence —
// please keep this notice and the author credit above when you reuse,
// remix, or redistribute this pattern. Do not delete it.
