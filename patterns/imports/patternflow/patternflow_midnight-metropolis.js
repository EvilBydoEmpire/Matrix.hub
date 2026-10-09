// ===== Patternflow pattern =====
// Title:    Midnight Metropolis
// Author:   @martinnlopez
// Date:     2026-09-29
// SPDX-License-Identifier: CC-BY-SA-4.0
// ===============================

// @knobs Scroll=0.1..2, Time=0..1, Haze=0..1, Activity=0.1..2
// @matrix 64x128

// Title:    Midnight Metropolis
// Authors:  Patternflow Collaborative Design
// License:  CC-BY-SA-4.0
// Knob 1: Scroll — Continuous vertical upward scroll velocity (0.1 to 2.0)
// Knob 2: Time — Diurnal city mood (0.0: Late-Night Slumber, 0.5: Dusk Golden Hour, 1.0: Electric Midnight)
// Knob 3: Haze — Atmospheric urban light pollution & misty street glow (0.0 to 1.0)
// Knob 4: Activity — Occupant window toggle rate & beacon blinking cadence (0.1 to 2.0)
// Button: Press any knob to trigger an electrical power pulse across the city grid!

function clamp(x, a, b) {
  return Math.max(a, Math.min(b, x));
}

function mlClamp(x, a, b) {
  return Math.max(a, Math.min(b, x));
}

function mlHash(n) {
  const v = Math.sin(n * 127.1 + 19.7) * 43758.5453;
  return v - Math.floor(v);
}

function mlSmooth(a, b, x) {
  x = mlClamp((x - a) / (b - a), 0.0, 1.0);
  return x * x * (3.0 - 2.0 * x);
}

// Window color palette directly extracted from sample artwork:
// Lemon Yellow, Warm Amber, Neon Coral, Electric Cyan, Celery Lime, Sky Blue
const ARTWORK_WINDOW_COLORS = [
  [254, 240, 50],  // Lemon Yellow (#fef032)
  [255, 175, 30],  // Warm Amber (#ffaf1e)
  [255, 60, 95],   // Neon Coral (#ff3c5f)
  [0, 235, 215],   // Electric Cyan (#00ebd7)
  [210, 245, 135], // Celery Lime (#d2f587)
  [65, 145, 255]   // Sky Blue (#4191ff)
];

const MAP_W = 128;
const MAP_H = 128;

export function setup(params) {
  params.W = 0;
  params.H = 0;
  params.S = 1.0;

  // Knob state with exponential decay inertia
  params.k = [0.65, 0.45, 0.50, 0.85];
  params.target = [0.65, 0.45, 0.50, 0.85];

  params.scrollOffset = 0.0;
  params.gridPulse = 10.0; // Inactive until button press
  params.timeAcc = 0.0;

  // Pre-rendered 128x128 seamless city tilemap
  const totalCells = MAP_W * MAP_H;
  params.worldR = new Uint8Array(totalCells);
  params.worldG = new Uint8Array(totalCells);
  params.worldB = new Uint8Array(totalCells);

  params.winId = new Uint16Array(totalCells);
  params.winColorIdx = new Uint8Array(totalCells);
  params.winDarkR = new Uint8Array(totalCells);
  params.winDarkG = new Uint8Array(totalCells);
  params.winDarkB = new Uint8Array(totalCells);

  buildGoldStandardCityMap(params);
}

export function update(dt, input, params) {
  dt = mlClamp(Number.isFinite(dt) ? dt : 0, 0.0, 0.1);
  const kv = (input && input.knobValues) || [];

  // Smooth weighted encoder inertia
  const alphaInertia = 1.0 - Math.exp(-dt / 0.28);
  for (let i = 0; i < 4; i++) {
    if (Number.isFinite(kv[i])) {
      params.target[i] = kv[i];
    }
    params.k[i] += (params.target[i] - params.k[i]) * alphaInertia;
  }

  // Meditative upward scroll velocity (<0.18 Hz cycle rate)
  const scrollSpeed = 0.04 + clamp(params.k[0], 0.1, 2.0) * 0.085;
  params.scrollOffset = (params.scrollOffset + dt * scrollSpeed * 22.0) % MAP_H;
  params.timeAcc += dt * scrollSpeed;

  // Interactive Button Event: Trigger grid power wave
  const b = (input && input.btnPressed) || [];
  if (b[0] || b[1] || b[2] || b[3]) {
    params.gridPulse = 0.0; // Reset power ripple
  }
  params.gridPulse = Math.min(10.0, params.gridPulse + dt * 1.8);
}

export function draw(display, params, time) {
  const w = display.width;
  const h = display.height;
  if (params.W !== w || params.H !== h) {
    allocBaseAtmosphere(params, w, h);
  }

  const t = params.timeAcc;
  const timeOfDay = clamp(params.k[1], 0.0, 1.0);
  const hazeGain = clamp(params.k[2], 0.0, 1.0);
  const activity = clamp(params.k[3], 0.1, 2.0);

  // Sub-pixel continuous vertical scroll offset (smooth 60 FPS motion without stepping)
  const sY = params.scrollOffset;
  const yInt = Math.floor(sY);
  const yFrac = sY - yInt;
  const yIntNext = (yInt + 1) % MAP_H;

  const worldR = params.worldR;
  const worldG = params.worldG;
  const worldB = params.worldB;
  const winId = params.winId;
  const winColorIdx = params.winColorIdx;
  const winDarkR = params.winDarkR;
  const winDarkG = params.winDarkG;
  const winDarkB = params.winDarkB;
  const baseAtmosphere = params.base;

  // Diurnal tone shifts: Late Night -> Dusk Golden Hour -> Electric Midnight
  const isDusk = Math.sin(Math.PI * timeOfDay);
  const isMidnight = mlSmooth(0.5, 1.0, timeOfDay);
  const isLateNight = 1.0 - mlSmooth(0.0, 0.5, timeOfDay);

  const litRatio = 0.40 + isDusk * 0.28 + isMidnight * 0.20 - isLateNight * 0.15;

  const cx = w * 0.5;
  const cy = h * 0.5;

  for (let y = 0; y < h; y++) {
    const wy0 = (y + yInt) % MAP_H;
    const wy1 = (y + yIntNext) % MAP_H;

    const rowOffset0 = wy0 * MAP_W;
    const rowOffset1 = wy1 * MAP_W;
    const baseOffset = y * w * 3;

    // Distance for grid power pulse wave
    const dyCenter = (y - cy) / params.S;

    for (let x = 0; x < w; x++) {
      const mapX = x % MAP_W;
      const idx0 = rowOffset0 + mapX;
      const idx1 = rowOffset1 + mapX;
      const bIdx = baseOffset + x * 3;

      // Sample Row 0
      let r0 = worldR[idx0], g0 = worldG[idx0], b0 = worldB[idx0];
      const wId0 = winId[idx0];
      if (wId0 > 0) {
        const c0 = sampleWindow(wId0, winColorIdx[idx0], winDarkR[idx0], winDarkG[idx0], winDarkB[idx0], t, activity, litRatio, timeOfDay);
        r0 = c0[0]; g0 = c0[1]; b0 = c0[2];
      }

      // Sample Row 1
      let r1 = worldR[idx1], g1 = worldG[idx1], b1 = worldB[idx1];
      const wId1 = winId[idx1];
      if (wId1 > 0) {
        const c1 = sampleWindow(wId1, winColorIdx[idx1], winDarkR[idx1], winDarkG[idx1], winDarkB[idx1], t, activity, litRatio, timeOfDay);
        r1 = c1[0]; g1 = c1[1]; b1 = c1[2];
      }

      // Bilinear vertical blend for liquid-smooth motion
      let cityR = r0 * (1.0 - yFrac) + r1 * yFrac;
      let cityG = g0 * (1.0 - yFrac) + g1 * yFrac;
      let cityB = b0 * (1.0 - yFrac) + b1 * yFrac;

      // Combine with pre-baked atmospheric haze & urban sky glow
      const hazeFactor = (y / h) * (0.15 + hazeGain * 0.25);
      cityR = cityR * (1.0 - hazeFactor) + baseAtmosphere[bIdx] * hazeFactor;
      cityG = cityG * (1.0 - hazeFactor) + baseAtmosphere[bIdx + 1] * hazeFactor;
      cityB = cityB * (1.0 - hazeFactor) + baseAtmosphere[bIdx + 2] * hazeFactor;

      // Diurnal color grading
      if (isDusk > 0.05) {
        // Warm sunset amber glow on buildings and atmosphere
        cityR += isDusk * 18.0;
        cityG += isDusk * 6.0;
      }
      if (isMidnight > 0.05) {
        // High-saturation cobalt / ultraviolet punch
        cityB += isMidnight * 12.0;
      }

      // Interactive button-triggered grid power surge wave
      if (params.gridPulse < 5.0) {
        const dxCenter = (x - cx) / params.S;
        const distCenter = Math.sqrt(dxCenter * dxCenter + dyCenter * dyCenter);
        const waveDist = Math.abs(distCenter - params.gridPulse * 18.0);
        if (waveDist < 3.5) {
          const waveInt = (1.0 - waveDist / 3.5) * (1.0 - params.gridPulse / 5.0) * 1.5;
          cityR = Math.min(255, cityR + 120.0 * waveInt);
          cityG = Math.min(255, cityG + 220.0 * waveInt);
          cityB = Math.min(255, cityB + 255.0 * waveInt);
        }
      }

      display.setPixel(
        x, y,
        mlClamp(cityR | 0, 0, 255),
        mlClamp(cityG | 0, 0, 255),
        mlClamp(cityB | 0, 0, 255)
      );
    }
  }
}

// Samples dynamic window on/off states with soft-switch transitions & diurnal temperature
function sampleWindow(id, colorIdx, darkR, darkG, darkB, t, activity, litRatio, timeOfDay) {
  const hash = mlHash(id * 17.13 + 31.7);
  const wRand = hash;

  if (wRand >= litRatio) {
    return [darkR, darkG, darkB];
  }

  // Continuous occupant activity cycle
  const togglePeriod = 7.0 + (wRand * 100.0 % 1.0) * 18.0;
  const togglePhase = (t * activity * 1.4 + wRand * 50.0) / togglePeriod;
  const lightWave = Math.sin(togglePhase * 6.2831853);

  if (lightWave > -0.22) {
    const baseColor = ARTWORK_WINDOW_COLORS[colorIdx % ARTWORK_WINDOW_COLORS.length];

    // Soft easing near transition avoids harsh 1-frame pops
    let dim = 1.0;
    if (lightWave < 0.15) {
      dim = mlClamp((lightWave + 0.22) / 0.37, 0.25, 1.0);
    }

    let r = Math.floor(darkR + (baseColor[0] - darkR) * dim);
    let g = Math.floor(darkG + (baseColor[1] - darkG) * dim);
    let b = Math.floor(darkB + (baseColor[2] - darkB) * dim);

    return [r, g, b];
  }

  return [darkR, darkG, darkB];
}

// Precomputes atmospheric urban night backdrop into Float32Array once
function allocBaseAtmosphere(p, w, h) {
  p.W = w;
  p.H = h;
  p.S = Math.min(w, h) / 64.0;
  p.base = new Float32Array(w * h * 3);

  for (let y = 0; y < h; y++) {
    const v = y / (h - 1);
    const rowOffset = y * w * 3;

    for (let x = 0; x < w; x++) {
      const u = x / (w - 1);
      const idx = rowOffset + x * 3;

      // Urban night sky gradient: dark obsidian zenith to warm amber/rose smog at horizon
      const skyR = 8.0 + v * 32.0;
      const skyG = 12.0 + v * 22.0;
      const skyB = 34.0 + v * 18.0;

      // Distant micro-lights scattered via deterministic hash (Monet-style stippling)
      const starHash = mlHash(x * 13.17 + y * 79.31);
      let starGlow = (starHash > 0.985 && v > 0.25) ? 45.0 : 0.0;

      p.base[idx] = skyR + starGlow;
      p.base[idx + 1] = skyG + starGlow * 0.9;
      p.base[idx + 2] = skyB + starGlow * 0.5;
    }
  }
}

// =====================================================================
// GAPLESS MAP BUILDER (Guarantees 100% vertical and horizontal coverage)
// =====================================================================

function buildGoldStandardCityMap(params) {
  const wR = params.worldR;
  const wG = params.worldG;
  const wB = params.worldB;
  const wId = params.winId;
  const wCIdx = params.winColorIdx;
  const wDR = params.winDarkR;
  const wDG = params.winDarkG;
  const wDB = params.winDarkB;

  let globalWinCounter = 1;

  // Facades: [BaseR, BaseG, BaseB, DarkGlassR, DarkGlassG, DarkGlassB]
  const FACADES = [
    [16, 20, 62,  22, 28, 80],   // 0: Darkest Navy (#10143e)
    [22, 38, 115, 30, 48, 135],  // 1: Royal Cobalt (#162673)
    [30, 60, 165, 42, 75, 185],  // 2: Medium Bright Blue (#1e3ca5)
    [26, 18, 68,  36, 26, 88]    // 3: Midnight Indigo (#1a1244)
  ];

  // 1. LAYER 0: CONTINUOUS BACKDROP TOWERS (Span full MAP_H = 128 to eliminate any gap)
  const BG_TOWERS = [
    [0,   18, 0, 0, 3],
    [16,  16, 0, 3, 1],
    [30,  18, 0, 1, 0],
    [46,  16, 0, 0, 2],
    [60,  18, 0, 3, 1],
    [76,  16, 0, 1, 3],
    [90,  20, 0, 0, 0],
    [108, 22, 0, 3, 1]
  ];

  for (let b = 0; b < BG_TOWERS.length; b++) {
    const tDef = BG_TOWERS[b];
    rasterizeTower(tDef[0], tDef[1], 0, MAP_H, 0, FACADES[tDef[3]], wR, wG, wB, wId);
    globalWinCounter = stampWindows(tDef[0], tDef[1], 0, MAP_H, tDef[4], FACADES[tDef[3]][3], FACADES[tDef[3]][4], FACADES[tDef[3]][5], wR, wG, wB, wId, wCIdx, wDR, wDG, wDB, globalWinCounter);
  }

  // 2. LAYER 1: MIDGROUND TOWERS (Staggered roofs with ambient drop shadow)
  const MG_TOWERS = [
    [4,  16, 25,  85, 1, 2],
    [18, 18, 65,  80, 2, 0],
    [34, 16, 18,  90, 1, 1],
    [48, 20, 72,  75, 2, 0],
    [66, 18, 28,  85, 1, 2],
    [82, 16, 68,  78, 2, 1],
    [96, 18, 38,  88, 1, 0],
    [112,18, 78,  72, 2, 1]
  ];

  for (let b = 0; b < MG_TOWERS.length; b++) {
    const tDef = MG_TOWERS[b];
    rasterizeTower(tDef[0], tDef[1], tDef[2], tDef[3], 1, FACADES[tDef[4]], wR, wG, wB, wId);
    globalWinCounter = stampWindows(tDef[0], tDef[1], tDef[2], tDef[3], tDef[5], FACADES[tDef[4]][3], FACADES[tDef[4]][4], FACADES[tDef[4]][5], wR, wG, wB, wId, wCIdx, wDR, wDG, wDB, globalWinCounter);
  }

  // 3. LAYER 2: FOREGROUND TOWERS (Stepped cornices & large window geometries)
  const FG_TOWERS = [
    [8,  20, 52,  76, 2, 0],
    [26, 18, 92,  72, 1, 2],
    [52, 22, 45,  80, 2, 4],
    [72, 18, 88,  75, 1, 1],
    [98, 20, 58,  74, 2, 0],
    [116,18, 95,  70, 1, 2]
  ];

  for (let b = 0; b < FG_TOWERS.length; b++) {
    const tDef = FG_TOWERS[b];
    rasterizeTower(tDef[0], tDef[1], tDef[2], tDef[3], 2, FACADES[tDef[4]], wR, wG, wB, wId);
    globalWinCounter = stampWindows(tDef[0], tDef[1], tDef[2], tDef[3], tDef[5], FACADES[tDef[4]][3], FACADES[tDef[4]][4], FACADES[tDef[4]][5], wR, wG, wB, wId, wCIdx, wDR, wDG, wDB, globalWinCounter);
  }
}

// Bounded rasterizer with directional drop shadows, 1px specular bevels, and stepped cornices
function rasterizeTower(bX, bW, bRoofY, bH, bLayer, fData, wR, wG, wB, wId) {
  const baseR = fData[0], baseG = fData[1], baseB = fData[2];
  const hiR = Math.min(255, baseR + 38);
  const hiG = Math.min(255, baseG + 48);
  const hiB = Math.min(255, baseB + 75);
  const shR = Math.max(0, baseR - 10);
  const shG = Math.max(0, baseG - 12);
  const shB = Math.max(0, baseB - 25);

  // Cast ambient drop shadow onto layer behind (right & down offset)
  if (bLayer > 0) {
    for (let dy = -1; dy < bH; dy++) {
      const wy = (bRoofY + dy + 2) % MAP_H;
      const rowOffset = wy * MAP_W;
      for (let dx = 0; dx < bW + 2; dx++) {
        const wx = (bX + dx + 1) % MAP_W;
        const idx = rowOffset + wx;
        wR[idx] = Math.max(6, Math.floor(wR[idx] * 0.72));
        wG[idx] = Math.max(8, Math.floor(wG[idx] * 0.72));
        wB[idx] = Math.max(22, Math.floor(wB[idx] * 0.72));
      }
    }
  }

  // Draw tower facade
  for (let dy = 0; dy < bH; dy++) {
    const wy = (bRoofY + dy) % MAP_H;
    const rowOffset = wy * MAP_W;

    // Stepped cornice on foreground towers (2px inset on top 2 rows)
    let inset = 0;
    if (bLayer === 2 && dy < 3) inset = 2;

    for (let dx = inset; dx < bW - inset; dx++) {
      const wx = (bX + dx) % MAP_W;
      const idx = rowOffset + wx;

      let pR = baseR, pG = baseG, pB = baseB;

      if (dx === inset || (dy === 0 && bLayer > 0)) {
        pR = hiR; pG = hiG; pB = hiB; // Left edge & roof ledge highlight
      } else if (dx === bW - inset - 1) {
        pR = shR; pG = shG; pB = shB; // Right shadow edge
      }

      wR[idx] = pR;
      wG[idx] = pG;
      wB[idx] = pB;
      wId[idx] = 0;
    }
  }

  // Rooftop antenna mast with beacon on select towers
  if (bLayer > 0 && (bX % 3 === 0)) {
    const mastX = (bX + Math.floor(bW * 0.5)) % MAP_W;
    for (let aY = 1; aY <= 4; aY++) {
      const my = (bRoofY - aY + MAP_H) % MAP_H;
      const mIdx = my * MAP_W + mastX;
      wR[mIdx] = 55; wG[mIdx] = 65; wB[mIdx] = 95;
    }
  }
}

// Places diverse window geometries onto skyscraper facades matching the artwork
function stampWindows(bX, bW, bRoofY, bH, winStyle, glassR, glassG, glassB, wR, wG, wB, wId, wCIdx, wDR, wDG, wDB, winCounter) {
  let curId = winCounter;

  if (winStyle === 0) {
    // Style 0: Wide Horizontal Bars (4x2 or 5x2 pixels)
    const winW = (bW > 18) ? 5 : 4;
    const winH = 2;
    for (let y = 5; y < bH - 4; y += 4) {
      for (let x = 2; x <= bW - winW - 2; x += winW + 3) {
        curId++;
        stampRectWindow(bX + x, bRoofY + y, winW, winH, curId, curId % 6, glassR, glassG, glassB, wR, wG, wB, wId, wCIdx, wDR, wDG, wDB);
      }
    }
  } else if (winStyle === 1) {
    // Style 1: 3-Column Square Windows (2x2 pixels)
    const winW = 2, winH = 2;
    for (let y = 4; y < bH - 3; y += 4) {
      for (let x = 2; x <= bW - winW - 2; x += winW + 3) {
        curId++;
        stampRectWindow(bX + x, bRoofY + y, winW, winH, curId, (curId * 2 + y) % 6, glassR, glassG, glassB, wR, wG, wB, wId, wCIdx, wDR, wDG, wDB);
      }
    }
  } else if (winStyle === 2) {
    // Style 2: Paired Vertical Slits (|| ||, 1x3 windows)
    const winW = 1, winH = 3;
    for (let y = 4; y < bH - 4; y += 5) {
      for (let x = 2; x <= bW - 5; x += 6) {
        curId++;
        const cIdx = curId % 6;
        stampRectWindow(bX + x, bRoofY + y, winW, winH, curId, cIdx, glassR, glassG, glassB, wR, wG, wB, wId, wCIdx, wDR, wDG, wDB);
        stampRectWindow(bX + x + 2, bRoofY + y, winW, winH, curId, cIdx, glassR, glassG, glassB, wR, wG, wB, wId, wCIdx, wDR, wDG, wDB);
      }
    }
  } else if (winStyle === 3) {
    // Style 3: Dense Micro-Dotted Windows (1x1 pixels)
    for (let y = 4; y < bH - 3; y += 3) {
      for (let x = 2; x < bW - 2; x += 2) {
        curId++;
        stampRectWindow(bX + x, bRoofY + y, 1, 1, curId, (curId * 3) % 6, glassR, glassG, glassB, wR, wG, wB, wId, wCIdx, wDR, wDG, wDB);
      }
    }
  } else {
    // Style 4: Panoramic Office Blocks (3x2 pixels)
    for (let y = 5; y < bH - 4; y += 5) {
      for (let x = 2; x <= bW - 5; x += 5) {
        curId++;
        stampRectWindow(bX + x, bRoofY + y, 3, 2, curId, curId % 6, glassR, glassG, glassB, wR, wG, wB, wId, wCIdx, wDR, wDG, wDB);
      }
    }
  }

  return curId;
}

function stampRectWindow(startX, startY, winW, winH, id, cIdx, glassR, glassG, glassB, wR, wG, wB, wId, wCIdx, wDR, wDG, wDB) {
  for (let wy = 0; wy < winH; wy++) {
    const mapY = (startY + wy) % MAP_H;
    const rowOffset = mapY * MAP_W;
    for (let wx = 0; wx < winW; wx++) {
      const mapX = (startX + wx) % MAP_W;
      const idx = rowOffset + mapX;

      wId[idx] = id;
      wCIdx[idx] = cIdx;
      wDR[idx] = glassR;
      wDG[idx] = glassG;
      wDB[idx] = glassB;

      wR[idx] = glassR;
      wG[idx] = glassG;
      wB[idx] = glassB;
    }
  }
}

// ── Made with Patternflow Community · https://patternflow.work/community ──
// Shared under CC-BY-SA-4.0. Attribution is part of this licence —
// please keep this notice and the author credit above when you reuse,
// remix, or redistribute this pattern. Do not delete it.
