// ===== Patternflow pattern =====
// Title:    Bioluminescent Topo Vortex
// Author:   @martinnlopez
// Date:     2026-09-11
// SPDX-License-Identifier: CC-BY-SA-4.0
// ===============================

// @knobs Rings=8..24, Speed=0.1..4, Twist=0..5, ColorBias=0..1
// @matrix 64x128


function hash1(n) {
  const s = Math.sin(n * 127.1 + 311.7) * 43758.5453;
  return s - Math.floor(s);
}

export function setup(params) {
  params.t = 0;
  params.seed = Math.random() * 1000;
  params.layoutIndex = 0;
  params.boost = 0;
  params.k = [16, 1.0, 2.2, 0.5];
}

export function update(dt, input, params) {
  params.k = input.knobValues;

  if (input.btnPressed[0]) {
    params.seed = Math.random() * 1000;
  }
  if (input.btnPressed[2]) {
    params.layoutIndex = (params.layoutIndex + 1) % 3;
  }
  params.boost = input.btnHeld[3] ? 1 : 0;

  if (!input.btnHeld[1]) {
    params.t += dt * params.k[1];
  }
}

export function draw(display, params, time) {
  const w = display.width;
  const h = display.height;
  const t = params.t;
  const seed = params.seed;

  const numRings = Math.max(8, Math.floor(params.k[0]));
  const twist = params.k[2];
  const colorBias = params.k[3];
  const boost = params.boost;

  // Clear to stark OLED black
  for (let i = 0; i < w * h; i++) {
    display.setPixel(i % w, Math.floor(i / w), 0, 0, 0);
  }

  // Vortex Center Layouts (Matching the reference's composition)
  // Layout 0: Dual Vertical Vortex (Top-Right and Lower-Left hubs)
  // Layout 1: Triple Intersecting Swirls
  // Layout 2: Single Giant Centered Vortex
  let centers;
  if (params.layoutIndex === 0) {
    centers = [
      [w * 0.62, h * 0.28], // Top-Right prominent vortex
      [w * 0.22, h * 0.72]  // Lower-Left swirling hub
    ];
  } else if (params.layoutIndex === 1) {
    centers = [
      [w * 0.68, h * 0.22],
      [w * 0.25, h * 0.55],
      [w * 0.75, h * 0.82]
    ];
  } else {
    centers = [
      [w * 0.50, h * 0.50]
    ];
  }

  const ringSpacing = 3.6;
  const minDim = Math.min(w, h);

  // Helper to safely write a glowing dot with optional 2x2 footprint
  const drawDot = (px, py, r, g, b, size) => {
    const ix = Math.floor(px);
    const iy = Math.floor(py);
    if (ix >= 0 && ix < w && iy >= 0 && iy < h) {
      display.setPixel(ix, iy, r, g, b);
      if (size > 1) {
        if (ix + 1 < w) display.setPixel(ix + 1, iy, r, g, b);
        if (iy + 1 < h) display.setPixel(ix, iy + 1, r, g, b);
        if (size > 2 && ix + 1 < w && iy + 1 < h) {
          display.setPixel(ix + 1, iy + 1, r, g, b);
        }
      }
    }
  };

  // Draw rings from outside inward so bright green cores render crisply on top
  for (let c = 0; c < centers.length; c++) {
    const cx = centers[c][0];
    const cy = centers[c][1];
    const centerSeed = seed + c * 47.13;

    for (let k = numRings; k >= 0; k--) {
      // Base radial distance for this contour ring
      const baseR = k * ringSpacing;
      const ringNorm = k / numRings; // 0.0 at core, 1.0 at outer edge

      // 1. Color Gradient Logic (Exact match to reference)
      // Normalized progression shifted by ColorBias knob:
      // Core = Neon Green -> Mid = Cyan/Turquoise -> Outer = Cobalt/Deep Blue
      const colorPos = Math.max(0, Math.min(1, ringNorm + (colorBias - 0.5) * 0.6));

      let rCol = 0, gCol = 0, bCol = 0;
      let dotSize = 1;

      if (colorPos < 0.22) {
        // Bright Neon Lime Green Core
        rCol = 0;
        gCol = 255;
        bCol = 45;
        dotSize = (k <= 2 || boost) ? 2 : 1;
      } else if (colorPos < 0.42) {
        // Spring Green -> Bright Turquoise
        const f = (colorPos - 0.22) / 0.20;
        rCol = 0;
        gCol = Math.floor(255 - f * 35);
        bCol = Math.floor(45 + f * 170);
        dotSize = boost ? 2 : 1;
      } else if (colorPos < 0.68) {
        // Vivid Cyan -> Sky Azure
        const f = (colorPos - 0.42) / 0.26;
        rCol = 0;
        gCol = Math.floor(220 - f * 120);
        bCol = 255;
      } else if (colorPos < 0.88) {
        // Royal Cobalt Blue
        const f = (colorPos - 0.68) / 0.20;
        rCol = 0;
        gCol = Math.floor(100 - f * 65);
        bCol = Math.floor(255 - f * 35);
      } else {
        // Deep Indigo Blue (Sparser, subtle outer waves)
        rCol = 0;
        gCol = Math.floor(35 * (1.0 - (colorPos - 0.88) * 4));
        bCol = 195;
      }

      // Center seed dot
      if (k === 0) {
        drawDot(cx, cy, 0, 255, 60, 2 + boost, w, h);
        continue;
      }

      // 2. Circumferential Dot Count
      // Outer rings have wider dot spacing, matching the stretched dashed look
      const circumference = 2 * Math.PI * baseR;
      const dotPitch = 3.6 + ringNorm * 2.8; // dots spread out in the blue zone
      const numDots = Math.max(6, Math.floor(circumference / dotPitch));

      for (let j = 0; j < numDots; j++) {
        const baseAngle = (j * 6.28318) / numDots;

        // 3. Swirling Vortex Twist + Traveling Wave
        // Angular rotation accelerates toward the vortex center
        const swirlAngle = baseAngle + (twist * 8.0) / (baseR + 8.0) + (t * 0.35 * (c % 2 === 0 ? 1 : -1));

        // Multi-harmonic organic contour ripples (creates the undulating topo-lines)
        const ripple1 = Math.sin(baseR * 0.14 - t * 1.6 + baseAngle * 2.0 + centerSeed) * (baseR * 0.16);
        const ripple2 = Math.cos(baseAngle * 3.0 + t * 0.8) * 1.8;
        const currentR = baseR + ripple1 + ripple2;

        // Global drift wave (causes the contours to weave organically into one another)
        const waveX = Math.sin(baseR * 0.08 + t * 0.6) * 3.2;
        const waveY = Math.cos(baseAngle * 2.0 - t * 0.5) * 3.2;

        const px = cx + Math.cos(swirlAngle) * currentR + waveX;
        const py = cy + Math.sin(swirlAngle) * (currentR * 0.92) + waveY; // Slight vertical ovality

        // Boosted brightness modulation along the strand
        let bMod = 1.0;
        if (boost) {
          bMod = 1.0 + Math.sin(j * 0.4 + t * 3.0) * 0.25;
        }

        drawDot(
          px, py,
          Math.min(255, Math.floor(rCol * bMod)),
          Math.min(255, Math.floor(gCol * bMod)),
          Math.min(255, Math.floor(bCol * bMod)),
          dotSize,
          w, h
        );
      }
    }
  }
}

// ── Made with Patternflow Community · https://patternflow.work/community ──
// Shared under CC-BY-SA-4.0. Attribution is part of this licence —
// please keep this notice and the author credit above when you reuse,
// remix, or redistribute this pattern. Do not delete it.
