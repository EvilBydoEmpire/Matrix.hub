// ===== Patternflow pattern =====
// Title:    ChladniAcousticWaves
// Author:   @engmung
// Date:     2026-07-31
// SPDX-License-Identifier: CC-BY-SA-4.0
// ===============================

// @ramp linear 0.002:#000000 1:#ff0000 0.49:#6600ff
// @matrix 64x128
// Pattern: Chladni Acoustic Waves
// Author: your name here
// SPDX-License-Identifier: CC-BY-SA-4.0
// Date: 2026-07-31
// Made with Patternflow Pattern Lab — https://patternflow.work/pattern-lab


// @knobs ModeN=1..8, ModeM=1..12, Speed=0.2..2, EdgeSharp=1..20

export function setup(params) {}

export function update(dt, input, params) {}

export function draw(display, params, time) {
  const kv = params.knobValues || [3, 5, 0.5, 8];
  const n = kv[0];
  const m = kv[1];
  const speed = kv[2];
  const sharp = kv[3];
  const w = display.width;
  const h = display.height;
  const t = time * speed;
  const nMod = n + Math.sin(t) * 0.5;
  const mMod = m + Math.cos(t * 0.8) * 0.5;

  for (let y = 0; y < h; y++) {
    const ny = (y / h - 0.5) * 2;
    for (let x = 0; x < w; x++) {
      const nx = (x / w - 0.5) * 2;
      const chladni = Math.cos(nMod * Math.PI * nx) * Math.cos(mMod * Math.PI * ny) -
                      Math.cos(mMod * Math.PI * nx) * Math.cos(nMod * Math.PI * ny);
      const nodeLine = Math.exp(-Math.abs(chladni) * sharp);
      display.setValue(x, y, Math.min(1.0, Math.max(0.0, nodeLine)));
    }
  }
}

// ── Made with Patternflow Community · https://patternflow.work/community ──
// Shared under CC-BY-SA-4.0. Attribution is part of this licence —
// please keep this notice and the author credit above when you reuse,
// remix, or redistribute this pattern. Do not delete it.
