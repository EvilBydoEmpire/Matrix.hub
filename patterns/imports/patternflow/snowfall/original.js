// ===== Patternflow pattern =====
// Title:    SnowFall
// Author:   @engmung
// Date:     2026-07-29
// SPDX-License-Identifier: CC-BY-SA-4.0
// ===============================

// @ramp linear 0.044:#000000 0.825:#ffffff
// @matrix 64x128
// Title: SnowFall - Aerodynamic Flutter (High Density)

// @knobs FallSpeed=1..15, FlakeCount=20..300, FlutterAmt=0..10, TrailLen=1..8

// Knob Roles:
// Knob 1: Snowfall speed
// Knob 2: Number of active snow particles (20 to 300)
// Knob 3: High-frequency aerodynamic oscillation & jitter
// Knob 4: Trailing streak length

export function setup(params) {
  params.flakes = [];
  params.t = 0.0;

  // Pre-allocate up to 300 snow flakes
  for (let i = 0; i < 300; i++) {
    params.flakes.push({
      x: (Math.random() - 0.5) * 150,
      y: (Math.random() - 0.5) * 300,
      z: Math.random() * 100 + 1,
      phase: Math.random() * 6.28,
      freq: 3.0 + Math.random() * 4.0,
      isPearl: Math.random() < 0.25,
      speedMult: 0.85 + Math.random() * 0.3
    });
  }
}

export function update(dt, input, params) {
  if (input && input.knobValues) {
    params.speed = input.knobValues[0];
    params.count = input.knobValues[1]; // Knob 2: FlakeCount (20..300)
    params.flutter = input.knobValues[2]; // Knob 3: FlutterAmt
    params.trail = input.knobValues[3];
  }

  const speed = params.speed || 5;
  params.t += dt * (speed * 0.2);

  const count = Math.min(300, Math.floor(params.count || 120));

  for (let i = 0; i < count; i++) {
    let f = params.flakes[i];

    // Z-axis movement toward viewer & Y-axis fall
    f.z -= dt * speed * 20.0 * f.speedMult;
    f.y += dt * speed * 2.0;

    // Respawn at random back distance when passing viewer or falling below viewport
    if (f.z <= 0.8 || f.y > 160) {
      f.z = 100;
      f.x = (Math.random() - 0.5) * 150;
      f.y = (Math.random() - 0.8) * 300;
      f.isPearl = Math.random() < 0.25;
    }
  }
}

export function draw(display, params, time) {
  const w = display.width;
  const h = display.height;

  const speed = params.speed || 5;
  const flutter = params.flutter || 0;
  const trailScale = params.trail || 3;
  const count = Math.min(300, Math.floor(params.count || 120));
  const t = params.t || 0;
  const fov = 34.0;

  const cx = w * 0.5;
  const cy = h * 0.4;

  // Background ambient frost dust
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      let frost = (Math.sin(x * 0.08 + t) * Math.cos(y * 0.04 - t * 1.2)) * 0.02 + 0.015;
      display.setValue(x, y, frost);
    }
  }

  // Render up to 300 3D Flutter Snow Particles
  for (let i = 0; i < count; i++) {
    let f = params.flakes[i];

    // Aerodynamic micro-flutter offsets
    let flutterX = Math.sin(t * f.freq + f.phase) * (flutter * 1.8);
    let flutterY = Math.cos(t * f.freq * 1.5 + f.phase) * (flutter * 0.9);

    let k = fov / f.z;
    let px = cx + (f.x + flutterX) * k;
    let py = cy + (f.y + flutterY) * k;

    let dx = px - cx;
    let dy = py - cy;
    let r = Math.sqrt(dx * dx + dy * dy);

    // Skip particles outside viewport boundaries
    if (px < -20 || px > w + 20 || py < -20 || py > h + 20) continue;

    if (r < 0.5) {
      let sx = Math.floor(px);
      let sy = Math.floor(py);
      if (sx >= 0 && sx < w && sy >= 0 && sy < h) {
        display.setValue(sx, sy, 0.6);
      }
      continue;
    }

    let ux = dx / r;
    let uy = dy / r;

    // Trailing streak length with exponential saturation
    let maxTail = 7.0 + trailScale * 4.0;
    let rawTail = r * (speed / (f.z + 5.0)) * trailScale * 0.38;
    let tailLen = maxTail * (1.0 - Math.exp(-rawTail / maxTail));
    let steps = Math.max(1, Math.floor(tailLen));

    // Smooth depth and viewport edge fading
    let nearFade = Math.min(1.0, Math.max(0.0, (f.z - 0.8) / 8.0));
    let farFade = Math.min(1.0, (100.0 - f.z) / 25.0);
    let edgeX = Math.min(px, w - px);
    let edgeY = Math.min(py, h - py);
    let edgeFade = Math.min(1.0, Math.max(0.0, Math.min(edgeX, edgeY) / 5.0));

    let brightness = nearFade * farFade * edgeFade;
    if (brightness <= 0.01) continue;

    let headX = Math.floor(px);
    let headY = Math.floor(py);

    // Glowing flare core for large snow pearls
    if (f.isPearl && f.z < 45 && headX >= 1 && headX < w - 1 && headY >= 1 && headY < h - 1) {
      let coreBright = Math.min(1.0, brightness * 1.3);
      display.setValue(headX, headY, coreBright);
      display.setValue(headX + 1, headY, coreBright * 0.4);
      display.setValue(headX - 1, headY, coreBright * 0.4);
      display.setValue(headX, headY + 1, coreBright * 0.4);
      display.setValue(headX, headY - 1, coreBright * 0.4);
    }

    // Trailing streak rendering
    for (let step = 0; step < steps; step++) {
      let sx = Math.floor(px - ux * step);
      let sy = Math.floor(py - uy * step);

      if (sx >= 0 && sx < w && sy >= 0 && sy < h) {
        let progress = step / steps;
        let fade = brightness * Math.pow(1.0 - progress, 1.8);
        display.setValue(sx, sy, Math.min(1.0, 0.02 + fade));
      }
    }
  }
}

// ── Made with Patternflow Community · https://patternflow.work/community ──
// Shared under CC-BY-SA-4.0. Attribution is part of this licence —
// please keep this notice and the author credit above when you reuse,
// remix, or redistribute this pattern. Do not delete it.
