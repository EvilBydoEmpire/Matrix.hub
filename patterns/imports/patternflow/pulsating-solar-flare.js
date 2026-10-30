// ===== Patternflow pattern =====
// Title:    Pulsating Solar Flare
// Author:   @martinnlopez
// Date:     2026-08-03
// SPDX-License-Identifier: CC-BY-SA-4.0
// ===============================

// @matrix 64x128
// @knobs Density=50..400, PulseSpeed=0.1..10, WaveFreq=1..10, Glow=0..1
export function setup(params) {
  params.t = 0;
  params.k = [150, 2, 5, 0.5];
}

export function update(dt, input, params) {
  params.k = input.knobValues;
  params.t += dt * params.k[1];
}

export function draw(display, params, time) {
  const w = display.width, h = display.height;
  const cx = w / 2, cy = h / 2;
  const count = params.k[0];
  const freq = params.k[2];
  const glow = params.k[3];
  const goldenAngle = 2.39996; // Radians

  for (let i = 0; i < w * h; i++) display.setPixel(i % w, Math.floor(i / w), 0, 0, 0);

  for (let i = 1; i < count; i++) {
    const rBase = 3 * Math.sqrt(i);
    // Pulse logic: distance-based oscillation
    const pulse = Math.sin(rBase * 0.2 - params.t) * freq;
    const radius = rBase + pulse;
    const angle = i * goldenAngle + params.t * 0.2;

    const x = Math.floor(cx + Math.cos(angle) * radius);
    const y = Math.floor(cy + Math.sin(angle) * radius);

    if (x >= 0 && x < w && y >= 0 && y < h) {
      const bri = Math.floor(150 + 105 * Math.sin(pulse * 0.5));
      display.setPixel(x, y, bri, Math.floor(bri * glow), 0); // Firey orange palette
      if (glow > 0.5) { // Add a little bloom
        display.setPixel(x + 1, y, bri / 2, 0, 0);
        display.setPixel(x, y + 1, bri / 2, 0, 0);
      }
    }
  }
}

// ── Made with Patternflow Community · https://patternflow.work/community ──
// Shared under CC-BY-SA-4.0. Attribution is part of this licence —
// please keep this notice and the author credit above when you reuse,
// remix, or redistribute this pattern. Do not delete it.
