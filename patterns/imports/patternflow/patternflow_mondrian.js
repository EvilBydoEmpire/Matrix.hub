// ===== Patternflow pattern =====
// Title:    Mondrian
// Author:   @kpow
// Date:     2026-09-18
// SPDX-License-Identifier: CC-BY-SA-4.0
// ===============================

// @knobs Color=0..1, Hold=0..1, Cells=0..1, Speed=0.05..3
// @matrix 64x128

//
// A De Stijl composition that will not hold still. Black rules divide a pale
// ground into rectangles and a few of them carry colour — but nothing here is
// drawn as a picture of a Mondrian. The canvas IS a guillotine subdivision,
// the rule set his paintings obey, so every frame is a legal composition.
//
// Between moves it holds perfectly still, every rule on an exact pixel. Then
// it remixes: every split glides to a new position at once, one region (the
// whole canvas or one half of it) closes behind a rule and reopens with a new
// arrangement, and a coloured plane slides to a new cell through the channels
// under the rules. No cuts and no fades — every pixel has a continuous path.
//
// Colour is one continuous hue with no scheme table. What rotates is
// Mondrian's own palette relationship, read off his pigments: from his red,
// his blue sits 0.592 of the way round the wheel and his yellow 0.132. Not a
// triad — its own lopsided thing. Dead centre of the Color knob is his actual
// red, ultramarine and chrome yellow.
//
// Knobs
//   Color   hue, continuous; centre = Mondrian's own palette
//   Hold    how long a composition sits before it remixes
//   Cells   how many regions, 5..16 — the coloured share scales with it
//   Speed   how fast the moves run
//
// A few things that were not obvious:
//   * Split axis is chosen by PROBABILITY, w/(w+h), not "split the longer
//     side". The frame is 1:2, so the obvious rule gives nothing but
//     horizontal stripes.
//   * Accents are given a LUMINANCE, not a value. Hue sets brightness, so the
//     same numbers read as gouache at one hue and neon at the next. One accent
//     is always dark and one always bright, and bright ones give up
//     saturation — only yellow can be bright and saturated at once.
//   * Split positions are snapped to the pixel grid when STORED, so a rule
//     eases straight onto a crisp edge instead of landing near it and jumping.
//     At rest every pixel is ground, rule or an accent — nothing in between.
//   * No cell is ever thinner than 8 px. Each split is bounded by how much
//     room its whole subtree needs along that axis, and a remix picks new
//     positions top-down so each one is bounded by the span it will really
//     have once its ancestors have moved.
//   * Moves ease hard at real starts and stops but quickly through reversals
//     and axis handovers. A symmetric ease cannot tell them apart, and "more
//     easing" just makes the turnarounds hitch.
//
// On an LED panel, turn GROUND_V down. The ground is most of the frame, so a
// full-white one is both the glare and most of the current draw; around 0.68
// still reads as white on a panel. 1.0 here suits a screen, where something
// brighter is always in view.




const FW = 64, FH = 128;

const MAX_DEPTH = 5, NODES = 63;
const MIN_CELL  = 15;
const CELL_MIN_PX = 8;
const RULE_W    = 2;
const AA_SHARP  = 2.2;
const GROUND_V  = 1.0;
const RC_LEG    = 2.2;
const TV_L1_END   = 0.62;
const TV_L2_START = 0.38;
const TV_INSET    = 1.0;
const REFLOW_MIN  = 0.20;
const REMIX_MIN_AREA = 900;
const GROUND_WEIGHT  = 0.12;
const REMIX_TRIES    = 6;

const MOVE_MIN_S  = 0.30, MOVE_MAX_S  = 7.0;
const GAP_MIN_S   = 0.6, GAP_MAX_S   = 240.0;
const SLIDE_MIN_S = 0.35, SLIDE_MAX_S = 9.0;

const clamp = (v, a, b) => (v < a ? a : v > b ? b : v);
const sstep = (x) => { const v = clamp(x, 0, 1); return v * v * (3 - 2 * v); };
const sstep5 = (x) => { const v = clamp(x, 0, 1); return v * v * v * (v * (v * 6 - 15) + 10); };

const EASE_HOLD = 4;
const EASE_TURN = 2;
const ipow = (u, n) => { let r = 1; for (let i = 0; i < n; i++) r *= u; return r; };
const easeAB = (x, a, b) => {
  const u = clamp(x, 0, 1);
  if (u <= 0) return 0;
  if (u >= 1) return 1;
  const pa = ipow(u, a), pb = ipow(1 - u, b);
  return pa / (pa + pb);
};
const depthOf = (i) => 31 - Math.clz32(i + 1);
const ruleW = () => RULE_W;

function rnd(P) {
  P.seed = (Math.imul(P.seed, 1664525) + 1013904223) >>> 0;
  return (P.seed >>> 8) / 16777216;
}

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

function accent(h, s, L) {
  const Ls = LUMA(hsv(h, s, 1));
  if (Ls >= L) return hsv(h, s, L / Ls);
  const L1 = LUMA(hsv(h, 1, 1));
  return hsv(h, clamp((255 - L) / Math.max(1, 255 - L1), 0, s), 1);
}

// ── colour: one continuous equation, no scheme table ──────────────────────
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

function palette(u) {
  const hue = (u - 0.5) - Math.floor(u - 0.5);
  const hues = [hue + MOND_OFF[0], hue + MOND_OFF[1], hue + MOND_OFF[2]];
  const L = lumaTargets(hues);
  const acc = hues.map((h, a) => {
    const t = (L[a] - LUMA_LO) / (LUMA_HI - LUMA_LO);
    return accent(h, MOND_SAT[a] * (1 - SAT_FALLOFF * t), L[a]);
  });
  const pal = {
    ground: hsv(hue, 0.07, GROUND_V),
    frame:  hsv(hue + 0.5, 0.50, 0.075),
    acc,
  };
  const w = 1 - sstep(Math.abs(u - 0.5) / DE_STIJL_BAND);
  if (w <= 0) return pal;
  const mix = (x, y) => x.map((v, i) => (v + (y[i] - v) * w) | 0);
  return {
    ground: mix(pal.ground, DE_STIJL.ground.map((v) => (v * GROUND_V) | 0)),
    frame:  mix(pal.frame, DE_STIJL.frame),
    acc:    pal.acc.map((c, i) => mix(c, DE_STIJL.acc[i])),
  };
}

const holdSeconds = (u) => 1.2 + 43.8 * u * u;

const DE_STIJL = {
  ground: [0xFF, 0xFA, 0xEA],
  frame:  [0x12, 0x11, 0x10],
  acc: [[0xD1, 0x14, 0x14], [0x0F, 0x47, 0x8C], [0xF2, 0xC2, 0x0A]],
};

// ── the tree ────────────────────────────────────────────────────────────────

function newNodes() {
  const N = new Array(NODES);
  for (let i = 0; i < NODES; i++)
    N[i] = { used: 0, leaf: 1, axis: 0, fill: 0,
             home: 0.5, cur: 0.5, hFrom: 0.5, hTo: 0.5,
             x0: 0, y0: 0, x1: 0, y1: 0 };
  return N;
}

function layoutTree(P, useHome) {
  const walk = (i, x0, y0, x1, y1) => {
    const n = P.N[i];
    n.x0 = x0; n.y0 = y0; n.x1 = x1; n.y1 = y1;
    if (n.leaf) return;
    const t = useHome ? n.home : n.cur;
    if (n.axis === 0) {
      const xm = x0 + t * (x1 - x0);
      walk(2 * i + 1, x0, y0, xm, y1); walk(2 * i + 2, xm, y0, x1, y1);
    } else {
      const ym = y0 + t * (y1 - y0);
      walk(2 * i + 1, x0, y0, x1, ym); walk(2 * i + 2, x0, ym, x1, y1);
    }
  };
  walk(0, 0, 0, FW, FH);
}

function ridesRemix(P, i) {
  if (P.rcNode < 0) return false;
  if (i === P.rcNode) return true;
  const collapsing = (P.rcEdge === 1) ? 2 * P.rcNode + 2 : 2 * P.rcNode + 1;
  for (let k = i; ; k = (k - 1) >> 1) {
    if (k === collapsing) return true;
    if (k <= 0) return false;
  }
}

function eachLeaf(P, root, fn) {
  const walk = (i) => { const n = P.N[i]; if (!n.used) return; if (n.leaf) fn(i); else { walk(2 * i + 1); walk(2 * i + 2); } };
  walk(root);
}
function leafCount(P, root) { let c = 0; eachLeaf(P, root, () => c++); return c; }

function clearSubtree(P, i) {
  if (i >= NODES || !P.N[i].used) return;
  P.N[i].used = 0; P.N[i].leaf = 1; P.N[i].fill = 0;
  if (2 * i + 2 < NODES) { clearSubtree(P, 2 * i + 1); clearSubtree(P, 2 * i + 2); }
}

function needAlong(P, i, axis) {
  const n = P.N[i];
  if (!n.used) return 0;
  if (n.leaf) return CELL_MIN_PX;
  const a = needAlong(P, 2 * i + 1, axis), b = needAlong(P, 2 * i + 2, axis);
  return n.axis === axis ? a + b : Math.max(a, b);
}

function splitRange(P, i) {
  const n = P.N[i];
  const span = n.axis === 0 ? (n.x1 - n.x0) : (n.y1 - n.y0);
  if (span <= 0) return [0.5, 0.5];
  let lo = Math.max(0.18, needAlong(P, 2 * i + 1, n.axis) / span);
  let hi = Math.min(0.82, 1 - needAlong(P, 2 * i + 2, n.axis) / span);
  if (lo > hi) { const mid = clamp(0.5 * (lo + hi), 0.05, 0.95); return [mid, mid]; }
  return [lo, hi];
}

function splitLeaf(P, i) {
  const n = P.N[i];
  const w = n.x1 - n.x0, h = n.y1 - n.y0;
  let axis = rnd(P) < w / (w + h) ? 0 : 1;
  if ((axis === 0 ? w : h) < 2 * MIN_CELL) axis ^= 1;
  n.leaf = 0; n.axis = axis;
  let t = 0.30 + 0.40 * rnd(P);
  if (rnd(P) < 0.22) t = rnd(P) < 0.5 ? 0.16 + 0.08 * rnd(P) : 0.76 + 0.08 * rnd(P);
  const span = axis === 0 ? w : h;
  if (span > 2 * CELL_MIN_PX) {
    const lo = CELL_MIN_PX / span;
    t = clamp(t, lo, 1 - lo);
  }
  n.home = t; n.cur = t;
  for (const c of [2 * i + 1, 2 * i + 2]) {
    const C = P.N[c];
    C.used = 1; C.leaf = 1; C.fill = 0;
  }
}

const COLOR_RATIO = 0.48;
const MAX_COLORED = 6;

function enforceFills(P, prefer) {
  const all = [], pref = [];
  eachLeaf(P, 0, (i) => all.push(i));
  if (prefer >= 0) eachLeaf(P, prefer, (i) => pref.push(i));
  const want = clamp(Math.round(all.length * COLOR_RATIO), 3, MAX_COLORED);

  const seen = [[], [], []];
  if (P.tvActive) seen[P.tvColor - 1].push(-1);
  for (const i of all) { const f = P.N[i].fill; if (f >= 1 && f <= 3) seen[f - 1].push(i); }
  let total = seen[0].length + seen[1].length + seen[2].length;

  const freeCell = () => {
    const inPref = pref.filter((i) => !P.N[i].fill);
    const pool = inPref.length ? inPref : all.filter((i) => !P.N[i].fill);
    return pool.length ? pool[Math.floor(rnd(P) * pool.length)] : -1;
  };

  for (let a = 0; a < 3; a++) {
    if (seen[a].length) continue;
    const c = freeCell();
    if (c < 0) continue;
    P.N[c].fill = a + 1; seen[a].push(c); total++;
  }
  while (total < want) {
    const c = freeCell();
    if (c < 0) break;
    const a = Math.floor(rnd(P) * 3);
    P.N[c].fill = a + 1; seen[a].push(c); total++;
  }
  while (total > want) {
    let a = -1, most = 1;
    for (let k = 0; k < 3; k++) if (seen[k].length > most) { most = seen[k].length; a = k; }
    if (a < 0) break;
    const cells = seen[a].filter((v) => v >= 0);
    if (cells.length < 2) break;
    const inPref = cells.filter((v) => pref.includes(v));
    const victim = (inPref.length ? inPref : cells)[0];
    P.N[victim].fill = 0;
    seen[a].splice(seen[a].indexOf(victim), 1);
    total--;
  }
}

function snapNode(P, i) {
  const n = P.N[i];
  if (!n.used || n.leaf) return;
  const vert = n.axis === 0;
  const base = vert ? n.x0 : n.y0;
  const span = (vert ? n.x1 : n.y1) - base;
  if (span < 1) return;
  const w = RULE_W;
  const edge = Math.round(base + n.home * span - w / 2);
  n.home = clamp((edge + w / 2 - base) / span, 0.04, 0.96);
}
function snapTree(P) {
  for (let i = 0; i < NODES; i++) {
    if (!P.N[i].used || P.N[i].leaf) continue;
    layoutTree(P, true);
    snapNode(P, i);
  }
  layoutTree(P, true);
}

function buildSubtree(P, root, budget) {
  budget = Math.max(1, budget);
  clearSubtree(P, root);
  const r = P.N[root];
  r.used = 1; r.leaf = 1; r.fill = 0;
  for (let s = 1; s < budget; s++) {
    layoutTree(P, true);
    let best = -1, bestScore = -1;
    eachLeaf(P, root, (i) => {
      if (depthOf(i) >= MAX_DEPTH || 2 * i + 2 >= NODES) return;
      const n = P.N[i];
      const w = n.x1 - n.x0, h = n.y1 - n.y0;
      if (Math.min(w, h) < MIN_CELL || Math.max(w, h) < 2 * MIN_CELL) return;
      const sc = w * h * (0.65 + 0.7 * rnd(P));
      if (sc > bestScore) { bestScore = sc; best = i; }
    });
    if (best < 0) break;
    splitLeaf(P, best);
  }
  layoutTree(P, true);
}

// ── recompose + slide ───────────────────────────────────────────────────────

function startRecompose(P, node) {
  P.rcNode = node;
  const cA = leafCount(P, 2 * node + 1), cB = leafCount(P, 2 * node + 2);
  const total = (node === 0) ? P.targetLeaves : (cA + cB);
  const nearest = P.N[node].cur < 0.5 ? 0 : 1;
  const canCollapseB = (total - cA) >= 1;
  const canCollapseA = (total - cB) >= 1;
  P.rcEdge = (canCollapseA && canCollapseB) ? nearest
           : canCollapseB ? 1
           : canCollapseA ? 0
           : nearest;
  P.rcBudget = Math.max(1, total - (P.rcEdge === 1 ? cA : cB));
  P.rcFrom = P.N[node].cur;
  P.rcTo = P.rcEdge;
  P.rcVal = P.rcFrom;
  P.rcU = 0;
  P.rcPhase = 1;

  const fromHome = new Array(NODES).fill(0);
  for (let i = 0; i < NODES; i++) fromHome[i] = P.N[i].home;
  layoutTree(P, true);
  const fx0 = [], fy0 = [], fx1 = [], fy1 = [];
  for (let i = 0; i < NODES; i++) {
    const n = P.N[i];
    fx0[i] = n.x0; fy0[i] = n.y0; fx1[i] = n.x1; fy1[i] = n.y1;
  }

  const keep = new Array(NODES).fill(0);
  let bestScore = -1;
  for (let attempt = 0; attempt < REMIX_TRIES; attempt++) {
    for (let i = 0; i < NODES; i++) P.N[i].home = fromHome[i];
    for (let i = 0; i < NODES; i++) {
      const n = P.N[i];
      if (!n.used || n.leaf) continue;
      n.hFrom = fromHome[i];
      if (ridesRemix(P, i)) { n.hTo = fromHome[i]; continue; }
      layoutTree(P, true);
      let to = 0.26 + 0.48 * rnd(P);
      if (Math.abs(to - n.hFrom) < REFLOW_MIN) to = n.hFrom + (to >= n.hFrom ? REFLOW_MIN : -REFLOW_MIN);
      const [lo, hi] = splitRange(P, i);
      n.hTo = clamp(to, lo, hi);
      n.home = n.hTo;
    }
    layoutTree(P, true);
    let sc = 0;
    for (let i = 0; i < NODES; i++) {
      const n = P.N[i];
      if (!n.used || !n.leaf || ridesRemix(P, i)) continue;
      const aNow = (fx1[i] - fx0[i]) * (fy1[i] - fy0[i]);
      const aNew = (n.x1 - n.x0) * (n.y1 - n.y0);
      const iw = Math.max(0, Math.min(fx1[i], n.x1) - Math.max(fx0[i], n.x0));
      const ih = Math.max(0, Math.min(fy1[i], n.y1) - Math.max(fy0[i], n.y0));
      sc += (aNow + aNew - 2 * iw * ih) * (n.fill ? 1.0 : GROUND_WEIGHT);
    }
    sc *= 0.5;
    if (sc > bestScore) {
      bestScore = sc;
      for (let i = 0; i < NODES; i++) keep[i] = P.N[i].hTo;
    }
    if (sc >= REMIX_MIN_AREA) break;
  }
  for (let i = 0; i < NODES; i++) {
    const n = P.N[i];
    if (n.used && !n.leaf) { n.hTo = keep[i]; n.home = ridesRemix(P, i) ? fromHome[i] : keep[i]; }
  }
  snapTree(P);
  for (let i = 0; i < NODES; i++) {
    const n = P.N[i];
    if (n.used && !n.leaf) n.hTo = n.home;
  }
  P.gU = 0;

  if (!P.tvActive) {
    startTravel(P, true);
    if (P.tvActive) P.tvDur = 2 * P.legNow;
  }
}

function pickRecomposeNode(P) {
  let total = 0;
  const w = new Array(NODES).fill(0);
  if (P.N[0].used && !P.N[0].leaf) {
    const c = leafCount(P, 0);
    w[0] = c * c * 0.30 * (1 + 1.6 * 3);
    total += w[0];
  }
  for (let i = 1; i <= 2; i++) {
    const n = P.N[i];
    if (!n.used || n.leaf) continue;
    const cw = n.x1 - n.x0, chh = n.y1 - n.y0;
    if (Math.min(cw, chh) < MIN_CELL || Math.max(cw, chh) < 2 * MIN_CELL) continue;
    const c = leafCount(P, i);
    let lit = 0;
    eachLeaf(P, i, (k) => { if (P.N[k].fill) lit++; });
    w[i] = c * c * (1 + 1.6 * lit);
    total += w[i];
  }
  if (total <= 0) return 0;
  let r = rnd(P) * total;
  for (let i = 0; i < NODES; i++) { r -= w[i]; if (r <= 0 && w[i] > 0) return i; }
  return 0;
}

function startTravel(P, avoidRiding) {
  const lit = [], empty = [];
  eachLeaf(P, 0, (i) => {
    if (avoidRiding && ridesRemix(P, i)) return;
    (P.N[i].fill ? lit : empty).push(i);
  });
  if (!lit.length || !empty.length) { P.tvTimer = 3; return; }
  P.tvSrc = lit[Math.floor(rnd(P) * lit.length)];
  P.tvDst = empty[Math.floor(rnd(P) * empty.length)];
  P.tvColor = P.N[P.tvSrc].fill;
  P.N[P.tvSrc].fill = 0;
  const a = P.N[P.tvSrc], b = P.N[P.tvDst];
  P.tvXFirst = Math.abs(b.x0 - a.x0) >= Math.abs(b.y0 - a.y0) ? 1 : 0;
  P.tvT = 0;
  P.tvDur = clamp((2.0 + 0.9 * rnd(P)) / P.spd, SLIDE_MIN_S, SLIDE_MAX_S);
  P.tvActive = 1;
}

// ── pattern entry points ────────────────────────────────────────────────────

export function setup(params) {
  const P = params;
  P.seed = 0x2545F491;
  P.N = newNodes();
  P.T = 0;
  P.colorU = 0.5;
  P.pal = palette(0.5);
  P.spd = 1;
  P.hold = holdSeconds(0.35);
  P.holdReal = P.hold;
  P.targetLeaves = 8;
  P.rcNode = -1; P.rcPhase = 0; P.rcVal = 0.5; P.rcEdge = 0; P.rcBudget = 8;
  P.rcFrom = 0.5; P.rcTo = 0.5; P.rcU = 0; P.gU = 1;
  P.rcTimer = P.hold;
  P.tvActive = 0; P.tvTimer = 3.5; P.tvSrc = 0; P.tvDst = 0; P.tvColor = 1;
  P.tvT = 0; P.tvDur = 1.8; P.tvXFirst = 1;
  P.legNow = RC_LEG;
  P.densAt = -1;
  buildSubtree(P, 0, P.targetLeaves);
  enforceFills(P, 0);
  snapTree(P);
  P.buf = null;
}

export function update(dt, input, params) {
  const P = params, k = input.knobValues;
  const colorU = clamp(k[0], 0, 1);
  const dens = k[2];
  const spd = k[3];
  const dT = dt;
  P.T += dt;
  P.spd = clamp(spd, 0.05, 3);
  P.hold = holdSeconds(clamp(k[1], 0, 1));
  P.holdReal = clamp(P.hold / Math.sqrt(P.spd), GAP_MIN_S, GAP_MAX_S);

  if (colorU !== P.colorU) { P.colorU = colorU; P.pal = palette(colorU); }

  const target = 5 + Math.round(dens * 11);
  if (target !== P.targetLeaves) { P.targetLeaves = target; P.densAt = P.T + 0.35; }
  if (P.densAt > 0 && P.T >= P.densAt && P.rcPhase === 0 && !P.tvActive) {
    P.densAt = -1; startRecompose(P, 0);
  }

  const leg = clamp(Math.min(RC_LEG / P.spd, P.holdReal * 0.45), MOVE_MIN_S, MOVE_MAX_S);
  P.legNow = leg;
  if (P.rcPhase === 1 || P.rcPhase === 2) {
    P.rcU += dT / leg;
    P.gU = Math.min(1, P.gU + dT / (2 * leg));
    P.rcVal = P.rcFrom + (P.rcTo - P.rcFrom) * (P.rcPhase === 1
      ? easeAB(P.rcU, EASE_HOLD, EASE_TURN)
      : easeAB(P.rcU, EASE_TURN, EASE_HOLD));
    if (P.rcU >= 1) {
      if (P.rcPhase === 1) {
        const node = P.rcNode;
        const collapsed = (P.rcEdge === 1) ? 2 * node + 2 : 2 * node + 1;
        P.N[node].home = 0.30 + 0.40 * rnd(P);
        buildSubtree(P, collapsed, P.rcBudget);
        layoutTree(P, true);
        {
          const [lo, hi] = splitRange(P, node);
          P.N[node].home = clamp(P.N[node].home, lo, hi);
        }
        enforceFills(P, collapsed);
        snapTree(P);
        for (let i = 0; i < NODES; i++) {
          const n = P.N[i];
          if (n.used && !n.leaf && ridesRemix(P, i)) { n.hFrom = n.home; n.hTo = n.home; }
        }
        P.rcFrom = P.rcEdge;
        P.rcTo = P.N[node].home;
        P.rcVal = P.rcFrom;
        P.rcU = 0;
        P.rcPhase = 2;
      } else {
        P.rcPhase = 0; P.rcNode = -1;
        P.rcTimer = P.holdReal * (0.75 + 0.5 * rnd(P));
        if (leafCount(P, 0) !== P.targetLeaves) P.densAt = P.T + P.holdReal * 0.5;
      }
    }
  } else {
    P.rcTimer -= dT;
    if (P.rcTimer <= 0 && !P.tvActive) startRecompose(P, pickRecomposeNode(P));
  }

  const gEase = easeAB(P.gU, EASE_HOLD, EASE_HOLD);
  for (let i = 0; i < NODES; i++) {
    const n = P.N[i];
    if (!n.used || n.leaf) continue;
    if (P.rcPhase === 0) n.cur = n.home;
    else if (i === P.rcNode) n.cur = P.rcVal;
    else if (ridesRemix(P, i)) n.cur = n.home;
    else n.cur = n.hFrom + (n.hTo - n.hFrom) * gEase;
  }
  layoutTree(P, false);

  if (P.tvActive) {
    P.tvT += dT / P.tvDur;
    if (P.tvT >= 1) {
      if (P.N[P.tvDst].used && P.N[P.tvDst].leaf) P.N[P.tvDst].fill = P.tvColor;
      P.tvActive = 0;
      P.tvTimer = clamp(P.holdReal * 0.30, SLIDE_MIN_S, GAP_MAX_S * 0.4) * (0.6 + 0.8 * rnd(P));
    }
  } else if (P.rcPhase === 0) {
    P.tvTimer -= dT;
    if (P.tvTimer <= 0) startTravel(P);
  }
}

// ── raster ──────────────────────────────────────────────────────────────────
function rectAA(buf, W, H, x0, y0, x1, y1, c) {
  if (x1 <= x0 || y1 <= y0) return;
  const ix0 = Math.max(0, Math.floor(x0)), ix1 = Math.min(W, Math.ceil(x1));
  const iy0 = Math.max(0, Math.floor(y0)), iy1 = Math.min(H, Math.ceil(y1));
  for (let y = iy0; y < iy1; y++) {
    const cy = Math.min(y + 1, y1) - Math.max(y, y0);
    if (cy <= 0) continue;
    for (let x = ix0; x < ix1; x++) {
      const cx = Math.min(x + 1, x1) - Math.max(x, x0);
      if (cx <= 0) continue;
      let a = cx * cy;
      if (a < 1) a = clamp((a - 0.5) * AA_SHARP + 0.5, 0, 1);
      if (a <= 0) continue;
      const i = (y * W + x) * 3;
      buf[i]     = (buf[i]     + (c[0] - buf[i])     * a + 0.5) | 0;
      buf[i + 1] = (buf[i + 1] + (c[1] - buf[i + 1]) * a + 0.5) | 0;
      buf[i + 2] = (buf[i + 2] + (c[2] - buf[i + 2]) * a + 0.5) | 0;
    }
  }
}

export function draw(display, params, time) {
  const P = params, W = display.width, H = display.height;
  if (!P.buf || P.buf.length !== W * H * 3) P.buf = new Uint8Array(W * H * 3);
  const buf = P.buf, pal = P.pal;

  for (let i = 0; i < buf.length; i += 3) { buf[i] = pal.ground[0]; buf[i + 1] = pal.ground[1]; buf[i + 2] = pal.ground[2]; }

  if (P.tvActive) {
    const a = P.N[P.tvSrc], b = P.N[P.tvDst];
    const l1 = easeAB(P.tvT / TV_L1_END, EASE_HOLD, EASE_TURN);
    const l2 = easeAB((P.tvT - TV_L2_START) / (1 - TV_L2_START), EASE_TURN, EASE_HOLD);
    const ux = P.tvXFirst ? l1 : l2, uy = P.tvXFirst ? l2 : l1;
    const ins = TV_INSET * Math.sin(Math.PI * clamp(P.tvT, 0, 1));
    rectAA(buf, W, H,
           a.x0 + (b.x0 - a.x0) * ux + ins, a.y0 + (b.y0 - a.y0) * uy + ins,
           a.x1 + (b.x1 - a.x1) * ux - ins, a.y1 + (b.y1 - a.y1) * uy - ins,
           pal.acc[P.tvColor - 1]);
  }

  eachLeaf(P, 0, (i) => {
    const n = P.N[i];
    if (n.fill) rectAA(buf, W, H, n.x0, n.y0, n.x1, n.y1, pal.acc[n.fill - 1]);
  });

  for (let i = NODES - 1; i >= 0; i--) {
    const n = P.N[i];
    if (!n.used || n.leaf) continue;
    const w = RULE_W;
    const moving = (P.rcPhase !== 0);
    if (n.axis === 0) {
      let x0 = n.x0 + n.cur * (n.x1 - n.x0) - w / 2, y0 = n.y0, y1 = n.y1;
      if (!moving) { x0 = Math.round(x0); y0 = Math.round(y0); y1 = Math.round(y1); }
      rectAA(buf, W, H, x0, y0, x0 + w, y1, pal.frame);
    } else {
      let y0 = n.y0 + n.cur * (n.y1 - n.y0) - w / 2, x0 = n.x0, x1 = n.x1;
      if (!moving) { y0 = Math.round(y0); x0 = Math.round(x0); x1 = Math.round(x1); }
      rectAA(buf, W, H, x0, y0, x1, y0 + w, pal.frame);
    }
  }

  for (let y = 0, i = 0; y < H; y++)
    for (let x = 0; x < W; x++, i += 3)
      display.setPixel(x, y, buf[i], buf[i + 1], buf[i + 2]);
}

// ── Made with Patternflow Community · https://patternflow.work/community ──
// Shared under CC-BY-SA-4.0. Attribution is part of this licence —
// please keep this notice and the author credit above when you reuse,
// remix, or redistribute this pattern. Do not delete it.
