// ===== Patternflow pattern =====
// Title:    Entropy Glass
// Author:   @engmung
// Date:     2026-09-25
// SPDX-License-Identifier: CC-BY-SA-4.0
// ===============================

// @knobs Neck=1..3, Demon=0..1, Gravity=1..5, Blind=0..1
// @matrix 64x128
// An hourglass of layered sand. Every pass through the neck mixes the strata
// a little more: time running forward is entropy going up. While the glass
// rests, Maxwell's demon sorts the grains back into bands, one swap at a time
// (the glints are its hands), then the glass turns over and the mixing
// starts again. Turn the demon down and disorder wins; turn Blind up and the
// grains become indistinguishable, and the mixing stops being visible at
// all, which is the Gibbs paradox: entropy depends on what you can tell apart.
//


//
// Knob 1 Neck    - width of the neck: a trickle or a pour
// Knob 2 Demon   - how much order the demon restores while the glass rests
// Knob 3 Gravity - how fast time runs
// Knob 4 Blind   - how alike the grains look, from seven colours to one
// Buttons: K1, K3, K4 turn the glass now · K2 pour fresh layered sand

const EG_SAND = [
  null,
  [255, 240, 205],   // ivory
  [255, 176, 0],     // saffron
  [255, 78, 52],     // coral
  [255, 40, 140],    // magenta
  [170, 60, 255],    // violet
  [40, 90, 255],     // cobalt
  [0, 205, 190],     // teal
];
const EG_BLIND = [230, 196, 140];
const EG_FLOW = 0;
const EG_REST = 1;
const EG_TURN = 2;

function egSmooth(a, b, v) {
  const t = Math.max(0, Math.min(1, (v - a) / (b - a)));
  return t * t * (3 - 2 * t);
}

// The glass: which cells are inside it, which are its wall, and the frame.
function egBuild(params, w, h, neck) {
  const open = new Uint8Array(w * h);
  const glass = new Uint8Array(w * h);
  const frame = new Uint8Array(w * h);
  const cap = Math.max(3, Math.round(h * 0.03));
  const mid = h * 0.5;
  const len = mid - cap - 1;
  const widest = Math.floor(w * 0.5) - 6;
  for (let y = cap + 1; y < h - cap - 1; y++) {
    const t = Math.abs(y + 0.5 - mid) / len;
    const bulb = egSmooth(0.03, 0.75, t) * (1 - 0.28 * egSmooth(0.8, 1.02, t));
    const half = Math.max(neck, Math.round(neck + (widest - neck) * bulb));
    for (let x = (w >> 1) - half; x < (w >> 1) + half; x++) open[y * w + x] = 1;
  }
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      const i = y * w + x;
      if (open[i]) continue;
      let near = false;
      for (let dy = -1; dy <= 1 && !near; dy++) {
        for (let dx = -1; dx <= 1; dx++) {
          const xx = x + dx, yy = y + dy;
          if (xx >= 0 && xx < w && yy >= 0 && yy < h && open[yy * w + xx]) { near = true; break; }
        }
      }
      if (near) glass[i] = 1;
      if (y < cap || y >= h - cap) {
        if (x >= 1 && x < w - 1) frame[i] = y === 0 || y === h - cap ? 2 : 1;   // caps, lit top edge
      } else if (x === 1 || x === 2 || x === w - 3 || x === w - 2) {
        frame[i] = x === 1 || x === w - 3 ? 4 : 3;                              // pillars
      }
    }
  }
  params.open = open;
  params.glass = glass;
  params.frame = frame;
  params.builtNeck = neck;
  params.W = w;
  params.H = h;
}

// Layered sand resting in the upper bulb, bottom band first.
function egPour(params) {
  const w = params.W, h = params.H, open = params.open;
  const cells = params.cells;
  cells.fill(0);
  let capacity = 0;
  for (let i = 0; i < (h >> 1) * w; i++) capacity += open[i];
  const total = Math.floor(capacity * 0.62);
  let placed = 0;
  for (let y = (h >> 1) - 1; y >= 0 && placed < total; y--) {
    for (let x = 0; x < w && placed < total; x++) {
      const i = y * w + x;
      if (!open[i]) continue;
      const band = 7 - Math.min(6, Math.floor(placed / total * 7));
      cells[i] = band | (Math.floor(Math.random() * 32) << 3);
      placed++;
    }
  }
  params.phase = EG_FLOW;
  params.phaseT = 0;
}

// One tick of falling sand: straight down if it can, else a random diagonal.
function egFall(params) {
  const w = params.W, h = params.H, cells = params.cells, open = params.open;
  let moves = 0;
  params.tick = (params.tick + 1) & 1;
  for (let y = h - 2; y >= 0; y--) {
    const ltr = ((y + params.tick) & 1) === 0;
    for (let n = 0; n < w; n++) {
      const x = ltr ? n : w - 1 - n;
      const i = y * w + x;
      const c = cells[i];
      if (!c) continue;
      const below = i + w;
      if (open[below] && !cells[below]) { cells[below] = c; cells[i] = 0; moves++; continue; }
      const d = Math.random() < 0.5 ? -1 : 1;
      if (x + d >= 0 && x + d < w && open[below + d] && !cells[below + d]) {
        cells[below + d] = c; cells[i] = 0; moves++; continue;
      }
      if (x - d >= 0 && x - d < w && open[below - d] && !cells[below - d]) {
        cells[below - d] = c; cells[i] = 0; moves++;
      }
    }
  }
  return moves;
}

// One demon pass: odd-even transposition down every column; lighter bands rise.
function egSort(params) {
  const w = params.W, h = params.H, cells = params.cells, spark = params.spark;
  let swaps = 0;
  params.sortParity ^= 1;
  for (let y = params.sortParity; y < h - 1; y += 2) {
    for (let x = 0; x < w; x++) {
      const i = y * w + x;
      const a = cells[i];
      const b = cells[i + w];
      if (a && b && (a & 7) > (b & 7)) {
        cells[i] = b;
        cells[i + w] = a;
        spark[i] = 255;
        spark[i + w] = 255;
        swaps++;
      }
    }
  }
  return swaps;
}

function egTurnOver(params) {
  const w = params.W, h = params.H, cells = params.cells;
  for (let y = 0; y < h >> 1; y++) {
    const a = y * w, b = (h - 1 - y) * w;
    for (let x = 0; x < w; x++) {
      const t = cells[a + x];
      cells[a + x] = cells[b + x];
      cells[b + x] = t;
    }
  }
}

export function setup(params) {
  params.neck = 1;
  params.demon = 1;
  params.gravity = 1;
  params.blind = 0;
  params.W = 0;
  params.cells = null;
  params.phase = EG_FLOW;
  params.phaseT = 0;
  params.tick = 0;
  params.sortParity = 0;
  params.stepAcc = 0;
  params.sortAcc = 0;
  params.sortBudget = 0;
  params.sorted = false;
  params.turnNow = false;
  params.pourNow = false;
}

export function update(dt, input, params) {
  const k = input.knobValues;
  if (k) {
    params.neck = k[0];
    params.demon = k[1];
    params.gravity = k[2];
    params.blind = k[3];
  }
  const b = input.btnPressed || [];
  if (b[0] || b[2] || b[3]) params.turnNow = true;
  if (b[1]) params.pourNow = true;
  if (!params.cells) return;                    // the glass is built on the first draw

  const neck = Math.max(1, Math.min(3, Math.round(params.neck)));
  if (neck !== params.builtNeck) egBuild(params, params.W, params.H, neck);
  if (params.pourNow) { egPour(params); params.pourNow = false; }
  if (params.turnNow && params.phase !== EG_TURN) {
    params.phase = EG_TURN;
    params.phaseT = 0;
  }
  params.turnNow = false;

  const spark = params.spark;
  const fade = Math.exp(-dt * 14);
  for (let i = 0; i < spark.length; i++) if (spark[i]) spark[i] = Math.floor(spark[i] * fade);

  params.phaseT += dt;
  if (params.phase === EG_FLOW) {
    params.stepAcc += dt * params.gravity * 55;
    let steps = 0;
    while (params.stepAcc >= 1 && steps < 12) {
      params.stepAcc -= 1;
      steps++;
      if (egFall(params) === 0) {
        // Everything has come to rest: the demon gets its turn.
        params.phase = EG_REST;
        params.phaseT = 0;
        params.sortBudget = Math.round(Math.max(0, Math.min(1, params.demon)) * 36);
        params.sorted = params.sortBudget === 0;
        params.stepAcc = 0;
        break;
      }
    }
  } else if (params.phase === EG_REST) {
    if (!params.sorted) {
      params.sortAcc += dt * 25;
      while (params.sortAcc >= 1 && params.sortBudget > 0 && !params.sorted) {
        params.sortAcc -= 1;
        params.sortBudget--;
        const swaps = egSort(params) + egSort(params);
        if (swaps === 0 || params.sortBudget <= 0) { params.sorted = true; params.phaseT = 0; }
      }
    } else if (params.phaseT > 0.9) {
      params.phase = EG_TURN;
      params.phaseT = 0;
    }
  } else if (params.phaseT >= 0.8) {
    egTurnOver(params);
    params.phase = EG_FLOW;
    params.phaseT = 0;
    params.stepAcc = 0;
  }
}

export function draw(display, params, time) {
  const w = display.width;
  const h = display.height;
  if (!params.cells || params.W !== w || params.H !== h) {
    egBuild(params, w, h, Math.max(1, Math.min(3, Math.round(params.neck))));
    params.cells = new Uint8Array(w * h);
    params.spark = new Uint8Array(w * h);
    egPour(params);
  }
  const cells = params.cells, glass = params.glass, frame = params.frame, spark = params.spark;
  const blind = Math.max(0, Math.min(1, params.blind));
  const mid = h * 0.5;

  // Turning over: the glass squashes through edge-on and comes back mirrored.
  let scale = 1;
  if (params.phase === EG_TURN) scale = Math.cos(Math.PI * Math.min(1, params.phaseT / 0.8));
  const turning = params.phase === EG_TURN;
  const dim = turning ? 0.5 + 0.5 * Math.abs(scale) : 1;

  for (let y = 0; y < h; y++) {
    let sy = y;
    if (turning) {
      if (Math.abs(scale) < 0.02) sy = -1;
      else sy = Math.floor(mid + (y + 0.5 - mid) / scale);
    }
    for (let x = 0; x < w; x++) {
      if (sy < 0 || sy >= h) { display.setPixel(x, y, 0, 0, 0); continue; }
      const i = sy * w + x;
      let r = 0, g = 0, bl = 0;
      const c = cells[i];
      if (c) {
        const col = EG_SAND[c & 7];
        const shade = 0.7 + 0.3 * ((c >> 3) / 31);
        r = (col[0] + (EG_BLIND[0] - col[0]) * blind) * shade;
        g = (col[1] + (EG_BLIND[1] - col[1]) * blind) * shade;
        bl = (col[2] + (EG_BLIND[2] - col[2]) * blind) * shade;
        const sp = spark[i] / 3;
        r += sp; g += sp; bl += sp;
      } else if (frame[i]) {
        const f = frame[i];
        if (f === 2) { r = 255; g = 205; bl = 110; }
        else if (f === 1) { r = 190; g = 130; bl = 40; }
        else if (f === 4) { r = 230; g = 165; bl = 70; }
        else { r = 140; g = 92; bl = 28; }
      } else if (glass[i]) {
        r = 55; g = 110; bl = 140;
      }
      display.setPixel(x, y, r * dim, g * dim, bl * dim);
    }
  }
}

// ── Made with Patternflow Community · https://patternflow.work/community ──
// Shared under CC-BY-SA-4.0. Attribution is part of this licence —
// please keep this notice and the author credit above when you reuse,
// remix, or redistribute this pattern. Do not delete it.
