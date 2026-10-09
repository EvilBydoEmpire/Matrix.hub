// ===== Patternflow pattern =====
// Title:    RippleCellGrid2
// Author:   @engmung
// Date:     2026-08-26
// SPDX-License-Identifier: CC-BY-SA-4.0
// ===============================

// @ramp oklab 0.157:#000000 0.409:#ff0000 0.669:#eeff00 0.822:#000000 0.562:#ff0000
// @knobs Speed=0..2, Size=3..6, Ripple=0..5, Density=0.5..3
// @matrix 128x64


// Knobs: 1=animation speed, 2=cell spacing, 3=ripple intensity, 4=edge density/detail
export function setup(params) {
  params.speed = 1.0;
  params.size = 5.0;
  params.ripple = 2.5;
  params.density = 1.5;
  params.t = 0.0;
}

export function update(dt, input, params) {
  if (input && input.knobValues) {
    params.speed = input.knobValues[0];
    params.size = input.knobValues[1];
    params.ripple = input.knobValues[2];
    params.density = input.knobValues[3];
  }
  params.t += dt * params.speed;
}

export function draw(display, params, time) {
  const w = display.width;
  const h = display.height;
  const cx = w / 2;
  const cy = h / 2;
  const t = params.t;
  
  const cellSpacing = params.size * 2.5;
  const edgeWidth = params.density * 2.5;

  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      let dx = x - cx;
      let dy = y - cy;
      
      let dist = Math.sqrt(dx * dx + dy * dy);
      let angle = Math.atan2(dy, dx);
      let rippleOffset = params.ripple * 4 * Math.sin(dist * 0.08 - t * 1.5);
      
      let rx = (dist + rippleOffset) * Math.cos(angle);
      let ry = (dist + rippleOffset) * Math.sin(angle);

      let cx2 = Math.floor(rx / cellSpacing) * cellSpacing + cellSpacing / 2;
      let cy2 = Math.floor(ry / cellSpacing) * cellSpacing + cellSpacing / 2;
      
      let minDist = Infinity;
      for (let dy2 = -1; dy2 <= 1; dy2++) {
        for (let dx2 = -1; dx2 <= 1; dx2++) {
          let nx = cx2 + dx2 * cellSpacing;
          let ny = cy2 + dy2 * cellSpacing;
          let nd = Math.sqrt((rx - nx) * (rx - nx) + (ry - ny) * (ry - ny));
          if (nd < minDist) minDist = nd;
        }
      }
      
      let val = (minDist < edgeWidth) ? 1.0 - (minDist / edgeWidth) * 0.9 : 0.05;

      display.setValue(x, y, Math.max(0.0, Math.min(1.0, val)));
    }
  }
}

// ── Made with Patternflow Community · https://patternflow.work/community ──
// Shared under CC-BY-SA-4.0. Attribution is part of this licence —
// please keep this notice and the author credit above when you reuse,
// remix, or redistribute this pattern. Do not delete it.
