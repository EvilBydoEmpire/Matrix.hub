// ===== Patternflow pattern =====
// Title:    Hypno Circles
// Author:   @SimonePDA
// Date:     2026-07-27
// SPDX-License-Identifier: CC-BY-SA-4.0
// ===============================

// @matrix 128x64
// Pattern: Origin - Variation 2: Kinetic Wave Intersections
// Direction: Motion Remix
// Knob 1: Hue Spectrum Center (0..1)
// Knob 2: Primary Motion Speed (0.1..10)
// Knob 3: Modulation Frequency Ratio (0..4.9)
// Knob 4: Wave Dispersion Width (0..1)

function hsvToRgb(h, s, v) {
    let r, g, b;
    let i = Math.floor(h * 6);
    let f = h * 6 - i;
    let p = v * (1 - s);
    let q = v * (1 - f * s);
    let t = v * (1 - (1 - f) * s);
    switch (((i % 6) + 6) % 6) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        case 5: r = v; g = p; b = q; break;
    }
    return [Math.floor(r * 255), Math.floor(g * 255), Math.floor(b * 255)];
}

function clamp(val, min, max) { return Math.max(min, Math.min(max, val)); }

export function setup(params) {
    params.hue = 0.0;
    params.speed = 2.0;
    params.ratio = 1.5;
    params.width = 0.5;
    params.timeAcc = 0.0;
}

export function update(dt, input, params) {
    if (input && input.knobValues) {
        let v = input.knobValues;
        params.hue = v[0];
        params.speed = v[1];
        params.ratio = v[2];
        params.width = v[3];
    }

    if (input && input.btnPressed) {
        if (input.btnPressed[0]) params.hue = 0.0;
        if (input.btnPressed[1]) params.speed = 2.0;
        if (input.btnPressed[2]) params.ratio = 1.5;
        if (input.btnPressed[3]) params.width = 0.5;
    }

    params.timeAcc += dt * params.speed;
}

export function draw(display, params, time) {
    let w = display.width;
    let h = display.height;
    let t = params.timeAcc;

    let c1x = w * 0.3 + Math.sin(t * 0.7) * (w * 0.2);
    let c1y = h * 0.5 + Math.cos(t * 0.5) * (h * 0.2);
    let c2x = w * 0.7 - Math.sin(t * 0.6) * (w * 0.2);
    let c2y = h * 0.5 - Math.cos(t * 0.8) * (h * 0.2);

    let freqRatio = 0.08 + params.ratio * 0.04;
    let thresholdWidth = 0.1 + params.width * 0.4;

    for (let y = 0; y < h; y++) {
        for (let x = 0; x < w; x++) {
            let d1 = Math.sqrt((x - c1x) * (x - c1x) + (y - c1y) * (y - c1y));
            let d2 = Math.sqrt((x - c2x) * (x - c2x) + (y - c2y) * (y - c2y));

            let w1 = Math.sin(d1 * 0.12 - t * 4.0);
            let w2 = Math.sin(d2 * freqRatio + t * 3.0);

            let energy = (w1 + w2) * 0.5;
            let diff = Math.abs(w1 - w2);

            let r = 0, g = 0, b = 0;

            if (diff < thresholdWidth) {
                let edgeGlow = 1.0 - (diff / thresholdWidth);
                r = Math.floor(255 * edgeGlow);
                g = Math.floor(255 * edgeGlow);
                b = Math.floor(255 * edgeGlow);
            } else {
                let localHue = (params.hue + energy * 0.3 + 1.0) % 1.0;
                let bright = clamp((energy + 1.0) * 0.5, 0.1, 1.0);
                
                if (energy > 0.0) {
                    let col = hsvToRgb(localHue, 0.9, bright);
                    r = col[0]; g = col[1]; b = col[2];
                } else {
                    let col = hsvToRgb((localHue + 0.5) % 1.0, 0.9, bright * 0.6);
                    r = col[0]; g = col[1]; b = col[2];
                }
            }

            display.setPixel(x, y, clamp(r, 0, 255), clamp(g, 0, 255), clamp(b, 0, 255));
        }
    }
}

// ── Made with Patternflow Community · https://patternflow.work/community ──
// Shared under CC-BY-SA-4.0. Attribution is part of this licence —
// please keep this notice and the author credit above when you reuse,
// remix, or redistribute this pattern. Do not delete it.
