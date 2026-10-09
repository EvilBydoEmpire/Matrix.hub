// ===== Patternflow pattern =====
// Title:    Op-Art Squircle Spacetime Warp
// Author:   @martinnlopez
// Date:     2026-09-24
// SPDX-License-Identifier: CC-BY-SA-4.0
// ===============================

// @knobs Glitch=0..1, Speed=0.05..5, Freq=10..120, Quantize=2..10
// @matrix 64x128
// Op-Art Spacetime Warp — gentle, slow-moving kinetic squircle matrix with gravitational rubber-sheet lensing.
// Speed is internally locked to a safe, meditative drift (photosensitivity-friendly).
// Knob 1: Curvature / Geometric Morph (0.0: Concentric Circles to 1.0: Sharp Square Tunnels)
// Knob 2: Spacetime Warp & Rubber Mesh Lens Distortion (0.0: Flat Grid to 5.0: Deep Gravitational Lens)
// Knob 3: Grid Cell Scale & Ring Frequency (10.0 to 120.0, perfectly canvas-aligned)
// Knob 4: Inter-Cell Phase Propagation & Ripple (2.0 to 10.0)

function clamp(val, min, max) {
    return Math.max(min, Math.min(max, val));
}

export function setup(params) {
    params.curvature = 0.65;
    params.warp = 2.2;
    params.freq = 42.0;
    params.phaseShift = 0.0;
    params.timeAcc = 0.0;
}

export function update(dt, input, params) {
    if (input && input.knobValues) {
        let v = input.knobValues;
        params.curvature = v[0];
        params.warp = v[1];
        params.freq = v[2];

        // Gracefully handles both 2.0-10.0 and 0.0-1.0 slider configurations
        params.phaseShift = (v[3] >= 2.0) ? (v[3] - 2.0) / 8.0 : v[3];
    }

    if (input && input.btnPressed) {
        if (input.btnPressed[0]) params.curvature = 0.65;
        if (input.btnPressed[1]) params.warp = 2.2;
        if (input.btnPressed[2]) params.freq = 42.0;
        if (input.btnPressed[3]) params.phaseShift = 0.0;
    }

    // Locked to a gentle, safe, slow-moving drift (~0.35 rate) to prevent rapid strobing
    params.timeAcc += dt * 0.35;
}

export function draw(display, params, time) {
    let w = display.width;
    let h = display.height;
    let t = params.timeAcc;

    let cx = (w - 1) * 0.5;
    let cy = (h - 1) * 0.5;

    let fNorm = (params.freq >= 10.0) ? clamp((params.freq - 10.0) / 110.0, 0.0, 1.0) : clamp(params.freq, 0.0, 1.0);

    // Exact integer grid subdivisions that divide the 2:1 aspect ratio without remainder
    let tier = Math.min(5, Math.floor(fNorm * 5.999));
    let rows = (tier === 0) ? 2 : (tier === 1) ? 3 : (tier === 2) ? 4 : (tier === 3) ? 5 : (tier === 4) ? 6 : 8;
    let cols = Math.round(rows * (w / h));

    let cellW = w / cols;
    let cellH = h / rows;
    let halfW = cellW * 0.5;
    let halfH = cellH * 0.5;

    let ringsPerCell = 1.8 + fNorm * 3.4;

    let kMorph = clamp(params.curvature, 0.0, 1.0);
    let kPhase = clamp(params.phaseShift, 0.0, 1.0);

    // Normalize Knob 2 (0.0 to 5.0 or 0.0 to 1.0) for spacetime distortion depth
    let kWarp = (params.warp > 1.0) ? clamp(params.warp / 5.0, 0.0, 1.0) : clamp(params.warp, 0.0, 1.0);

    // Moving gravitational lens focal point
    let lensX = cx + Math.cos(t * 0.65) * (w * 0.22);
    let lensY = cy + Math.sin(t * 0.5) * (h * 0.22);
    let lensRadiusSq = (h * 0.55) * (h * 0.55);

    let r = 0;
    let g = 0;
    let b = 0;

    for (let y = 0; y < h; y++) {
        let dyLens = y - lensY;
        let vEdge = Math.sin((y / (h - 1)) * Math.PI);

        for (let x = 0; x < w; x++) {
            let dxLens = x - lensX;
            let uEdge = Math.sin((x / (w - 1)) * Math.PI);

            // Border anchor mask so edges stay aligned while the interior rubber-stretches
            let edgeAnchor = uEdge * vEdge;
            let effWarp = kWarp * edgeAnchor;

            // Spacetime gravitational lens & rubber sheet mesh distortion
            let distLensSq = dxLens * dxLens + dyLens * dyLens;
            let lensPull = 1.0 / (1.0 + distLensSq / lensRadiusSq);

            let pinch = 1.0 - (lensPull * 0.48 * effWarp);
            let rippleX = Math.sin(y * 0.09 + t * 0.8) * (8.0 * effWarp);
            let rippleY = Math.cos(x * 0.09 - t * 0.7) * (8.0 * effWarp);

            // Warped coordinates
            let wx = clamp(lensX + dxLens * pinch + rippleX, 0.0, w - 0.001);
            let wy = clamp(lensY + dyLens * pinch + rippleY, 0.0, h - 0.001);

            let cellX = Math.floor(wx / cellW);
            let cellY = Math.floor(wy / cellH);
            if (cellX >= cols) cellX = cols - 1;
            if (cellY >= rows) cellY = rows - 1;

            let localX = (wx - cellX * cellW) - halfW;
            let localY = (wy - cellY * cellH) - halfH;

            let uNorm = localX / halfW;
            let vNorm = localY / halfH;
            let u2 = uNorm * uNorm;
            let v2 = vNorm * vNorm;
            let u4 = u2 * u2;
            let v4 = v2 * v2;

            // Superellipse / Squircle Distance Metric
            let dCircle = Math.sqrt(u2 + v2);
            let dSquircle = Math.sqrt(Math.sqrt(u4 + v4));
            let dSquare = Math.max(Math.abs(uNorm), Math.abs(vNorm));

            let d = 0.0;
            if (kMorph < 0.5) {
                let m = kMorph * 2.0;
                d = dCircle * (1.0 - m) + dSquircle * m;
            } else {
                let m = (kMorph - 0.5) * 2.0;
                d = dSquircle * (1.0 - m) + dSquare * m;
            }

            // Inter-cell phase propagation & checkerboard ripple
            let checkerboard = ((cellX + cellY) % 2 === 0) ? 0.5 : 0.0;
            let ripplePhase = (cellX * 0.35 + cellY * 0.35);
            let cellOffset = (checkerboard * 0.7 + ripplePhase * 0.3) * kPhase;

            // Concentric ring wave oscillation at a slow, comfortable breathing tempo
            let ringPhase = (d * ringsPerCell) - (t * 0.9) + cellOffset;
            let wave = Math.sin(ringPhase * 6.2831853);

            // Softened edge slope for gentle, anti-aliased transitions (easier on eyes)
            let edgeSlope = 3.8;
            let intensity = clamp(wave * edgeSlope + 0.5, 0.0, 1.0);

            // Corner star attenuation where cells intersect
            let cornerDist = Math.max(0.0, d - 1.0);
            if (cornerDist > 0.0) {
                intensity = intensity * clamp(1.0 - cornerDist * 1.6, 0.15, 1.0);
            }

            let lum = Math.floor(intensity * 255.0);

            // High-contrast op-art output with soft ambient warmth on highlights
            r = lum;
            g = lum;
            b = (lum > 220) ? 255 : (lum > 30 ? lum + 6 : 0);

            display.setPixel(x, y, r, g, b);
        }
    }
}

// ── Made with Patternflow Community · https://patternflow.work/community ──
// Shared under CC-BY-SA-4.0. Attribution is part of this licence —
// please keep this notice and the author credit above when you reuse,
// remix, or redistribute this pattern. Do not delete it.
