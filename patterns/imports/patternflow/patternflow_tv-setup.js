// ===== Patternflow pattern =====
// Title:    TV setup
// Author:   @petrusha
// Date:     2026-09-21
// SPDX-License-Identifier: CC-BY-SA-4.0
// ===============================

// @knobs Channel=-0.01..7, Interference=0..4, Noise=0..1, Aberration=-1..1
// @matrix 128x64


// Knob 1: Channel — 7 test cards. Only the detent nearest each whole number
//          holds vertical sync; off it the frame creeps and then jumps, so the
//          channel has to be tuned in. Every new channel retunes with static.
// Knob 2: Interference — hum bar sweeping up the picture, tearing the lines
// Knob 3: Noise — grain, snow and dropout lines
// Knob 4: Aberration — RGB convergence error, outward at both extremes
// Buttons: tap 1 = retune current channel, tap 2/3/4 = reset that knob

const W = 128;
const H = 64;

// --- helpers ---------------------------------------------------------------

function lsin(p, turns) {
    // sine from a 1024-entry table; phase in turns, not radians
    return p.sinLut[((turns * 1024) | 0) & 1023];
}

function hash3(x, y, s) {
    let h = Math.imul(x, 374761393) ^ Math.imul(y, 668265263) ^ Math.imul(s, 1274126177);
    h = Math.imul(h ^ (h >>> 13), 1274126177);
    h ^= h >>> 16;
    return (h >>> 0) * 2.3283064365386963e-10;
}

function rnd(p) {
    p.seed = (Math.imul(p.seed, 1664525) + 1013904223) | 0;
    return ((p.seed >>> 8) & 0xffffff) / 16777216;
}

function put(b, x, y, r, g, bl) {
    const i = (y * W + x) * 3;
    b[i] = r; b[i + 1] = g; b[i + 2] = bl;
}

// --- test cards ------------------------------------------------------------

// 0 · EBU / SMPTE colour bars
function cardBars(b, p) {
    for (let y = 0; y < H; y++) {
        for (let x = 0; x < W; x++) {
            const c = x >> 4;
            let r, g, bl;
            if (y < 42) {
                const s = p.bars75[c * 3];
                r = s; g = p.bars75[c * 3 + 1]; bl = p.bars75[c * 3 + 2];
            } else if (y < 49) {
                r = p.barsRev[c * 3]; g = p.barsRev[c * 3 + 1]; bl = p.barsRev[c * 3 + 2];
            } else if (x < 16) { r = 0; g = 33; bl = 76; }        // -I
            else if (x < 32) { r = 255; g = 255; bl = 255; }      // 100 % white
            else if (x < 48) { r = 61; g = 0; bl = 92; }          // +Q
            else if (x < 80) { r = 14; g = 14; bl = 14; }
            else if (x < 91) { r = 3; g = 3; bl = 3; }            // PLUGE −
            else if (x < 101) { r = 14; g = 14; bl = 14; }        // PLUGE 0
            else if (x < 112) { r = 30; g = 30; bl = 30; }        // PLUGE +
            else { r = 14; g = 14; bl = 14; }
            put(b, x, y, r, g, bl);
        }
    }
}

// 1 · Philips PM5544 style card
function cardPhilips(b, p) {
    for (let y = 0; y < H; y++) {
        const dy = y - 31.5;
        const dy2 = dy * dy;
        for (let x = 0; x < W; x++) {
            let r = 16, g = 16, bl = 24;
            const dx = x - 63.5;
            const r2 = dx * dx + dy2;
            if ((x & 7) === 0 || (y & 7) === 0) { r = 96; g = 96; bl = 106; }
            if (r2 > 720 && r2 < 860) { r = 228; g = 228; bl = 232; }   // ring
            if (y < 3 || y > 60) {                                       // castellations
                const on = (x >> 3) & 1;
                r = g = bl = on ? 235 : 4;
            }
            if (y >= 24 && y < 40) {                                     // colour band
                const c = x >> 4;
                r = p.bars75[c * 3]; g = p.bars75[c * 3 + 1]; bl = p.bars75[c * 3 + 2];
                if (x >= 48 && x < 80) {
                    const edge = (x === 48 || x === 79 || y === 24 || y === 39);
                    r = g = bl = edge ? 220 : 2;
                }
            }
            put(b, x, y, r, g, bl);
        }
    }
    // "PF" nameplate
    for (let i = 0; i < 2; i++) {
        const ox = 55 + i * 11;
        for (let gy = 0; gy < 7; gy++) {
            const row = p.glyphs[i * 7 + gy];
            for (let gx = 0; gx < 5; gx++) {
                if ((row >> (4 - gx)) & 1) put(b, ox + gx, 28 + gy, 235, 235, 235);
            }
        }
    }
}

// 2 · FuBK / PM5644 full service card
function cardFubk(b, p) {
    // grey field, white grid, the big circle
    for (let y = 0; y < H; y++) {
        const dy = y - 31.5;
        const dy2 = dy * dy;
        for (let x = 0; x < W; x++) {
            const dx = x - 63.5;
            const r2 = dx * dx + dy2;
            const v = ((x & 7) === 0 || (y & 7) === 0) ? 170 : 120;
            if (r2 > 806 && r2 < 930) put(b, x, y, 240, 232, 232);
            else put(b, x, y, v, v, v);
        }
    }
    // dark nameplates above and below centre
    for (let y = 4; y < 11; y++) if (y !== 7)
        for (let x = 54; x < 75; x++) put(b, x, y, 34, 34, 38);
    for (let y = 53; y < 60; y++) if (y !== 56)
        for (let x = 54; x < 75; x++) put(b, x, y, 34, 34, 38);

    // 75 % bars
    for (let y = 15; y < 24; y++) {
        for (let x = 0; x < W; x++) {
            const c = (x >> 4) * 3;
            put(b, x, y, p.fubkTop[c], p.fubkTop[c + 1], p.fubkTop[c + 2]);
        }
    }
    // greyscale staircase
    for (let y = 24; y < 29; y++) {
        for (let x = 0; x < W; x++) {
            const v = ((x >> 4) * 255 / 7) | 0;
            put(b, x, y, v, v, v);
        }
    }
    // chroma checks
    for (let y = 29; y < 33; y++) {
        for (let x = 30; x < 98; x++) {
            const k = ((x - 30) / 3) | 0;
            if (k & 1) { put(b, x, y, 6, 6, 6); }
            else {
                const c = (((k >> 1) % 6) + 1) * 3;
                put(b, x, y, p.fubk100[c], p.fubk100[c + 1], p.fubk100[c + 2]);
            }
        }
    }
    // black boxes with a diagonal, grey block between
    for (let y = 33; y < 38; y++) {
        for (let x = 22; x < 56; x++) put(b, x, y, 4, 4, 4);
        for (let x = 56; x < 72; x++) put(b, x, y, 120, 120, 120);
        for (let x = 72; x < 106; x++) put(b, x, y, 4, 4, 4);
    }
    for (let i = 0; i < 34; i++) {
        const d = (i * 4 / 33) | 0;
        put(b, 22 + i, 37 - d, 235, 235, 235);
        put(b, 72 + i, 33 + d, 235, 235, 235);
    }
    // green → white → magenta ramp
    for (let y = 38; y < 42; y++) {
        for (let x = 30; x < 98; x++) {
            const t = (x - 30) / 67;
            if (t < 0.5) { const u = (510 * t) | 0; put(b, x, y, u, 255, u); }
            else { const u = (255 - 510 * (t - 0.5)) | 0; put(b, x, y, 255, u, 255); }
        }
    }
    // multiburst: coarse at the edges, finest in the middle
    for (let s = 0; s < 7; s++) {
        const x0 = 2 + s * 18;
        const per = p.burst[s];
        for (let y = 42; y < 46; y++) {
            for (let x = x0; x < x0 + 18 && x < W; x++) {
                const v = ((((x - x0) / per) | 0) & 1) ? 8 : 250;
                put(b, x, y, v, v, v);
            }
        }
    }
    // 100 % bars
    for (let y = 46; y < 53; y++) {
        for (let x = 0; x < W; x++) {
            const c = (x >> 4) * 3;
            put(b, x, y, p.fubk100[c], p.fubk100[c + 1], p.fubk100[c + 2]);
        }
    }
    // corner aspect-ratio discs, filled with a fine grating
    for (let d = 0; d < 4; d++) {
        const cx = (d & 1) ? 114 : 13;
        const cy = d < 2 ? 8 : 57;
        const rr = d < 2 ? 30 : 20;
        for (let y = cy - 6; y <= cy + 6; y++) {
            if (y < 0 || y > 63) continue;
            const ey = y - cy;
            for (let x = cx - 6; x <= cx + 6; x++) {
                const ex = x - cx;
                if (ex * ex + ey * ey > rr) continue;
                const v = (x & 1) ? 62 : 242;
                put(b, x, y, v, v, v);
            }
        }
    }
    // castellation on all four sides
    for (let y = 0; y < H; y++) {
        const v = ((y >> 3) & 1) ? 6 : 245;
        for (let k = 0; k < 3; k++) { put(b, k, y, v, v, v); put(b, 125 + k, y, v, v, v); }
    }
    for (let x = 0; x < W; x++) {
        const v = ((x >> 3) & 1) ? 6 : 245;
        for (let k = 0; k < 3; k++) { put(b, x, k, v, v, v); put(b, x, 61 + k, v, v, v); }
    }
}

// 3 · Monoscope — crosshatch, circles, frequency bursts
function cardMono(b, p) {
    for (let y = 0; y < H; y++) {
        const dy = y - 31.5;
        const dy2 = dy * dy;
        for (let x = 0; x < W; x++) {
            let v = 0;
            const dx = x - 63.5;
            const r2 = dx * dx + dy2;
            if ((x & 7) === 4 || (y & 7) === 4) v = 150;
            if (x === 0 || x === W - 1 || y === 0 || y === H - 1) v = 255;
            if (r2 > 812 && r2 < 961) v = 255;
            if (x === 63 || x === 64 || y === 31 || y === 32) v = 255;
            put(b, x, y, v, v, v);
        }
    }
    // frequency bursts: 4 blocks, finer to the right
    const per = [4, 3, 2, 1];
    for (let k = 0; k < 4; k++) {
        const x0 = 15 + k * 25;
        for (let y = 45; y < 56; y++) {
            for (let x = x0 - 2; x < x0; x++) put(b, x, y, 0, 0, 0);
            for (let x = x0; x < x0 + 23; x++) {
                const on = (((x - x0) / per[k]) | 0) & 1;
                const v = on ? 255 : 0;
                put(b, x, y, v, v, v);
            }
        }
    }
}

// 4 · Zone plate — sin(r²), the moiré generator
function cardZone(b, p) {
    const ph = p.t * 0.12;
    for (let y = 0; y < H; y++) {
        const dy = y - 31.5;
        const dy2 = dy * dy;
        for (let x = 0; x < W; x++) {
            const dx = x - 63.5;
            const v = 0.5 + 0.5 * lsin(p, (dx * dx + dy2) * 0.0021 + ph);
            const s = (v * 255) | 0;
            put(b, x, y, (s * 0.88) | 0, s, (s * 0.96) | 0);
        }
    }
}

// 5 · Greyscale staircase, ramp and hue sweep
function cardGrey(b, p) {
    for (let y = 0; y < H; y++) {
        for (let x = 0; x < W; x++) {
            let r, g, bl;
            if (y < 20) {
                const v = ((x >> 4) * 255 / 7) | 0;
                r = g = bl = v;
            } else if (y === 20 || y === 41) {
                r = g = bl = 0;
            } else if (y < 41) {
                const v = ((x * 255) / 127) | 0;
                r = g = bl = v;
            } else {
                r = p.huePal[x * 3]; g = p.huePal[x * 3 + 1]; bl = p.huePal[x * 3 + 2];
            }
            put(b, x, y, r, g, bl);
        }
    }
}

// 6 · No signal
function cardSnow(b, p) {
    const f = (p.t * 30) | 0;
    for (let y = 0; y < H; y++) {
        const streak = 0.72 + 0.56 * hash3(911, y, f);
        for (let x = 0; x < W; x++) {
            const n = hash3(x, y, f);
            let v = (28 + n * 214) * streak;
            if (v > 255) v = 255;
            const g = v | 0;
            const c = hash3(x + 7, y, f + 3);
            put(b, x, y, c > 0.94 ? (g * 0.72) | 0 : g, g, c < 0.06 ? (g * 0.78) | 0 : g);
        }
    }
}

// --- lifecycle -------------------------------------------------------------

export function setup(params) {
    params.t = 0;
    params.seed = 20260921;
    params.chan = 0;
    params.hum = 0.5;
    params.snow = 0;
    params.fringe = 0;
    params.detune = 0;
    params.lock = 0;
    params.vphase = 0;
    params.gPrev = 0;
    params.hold = 0;
    params.tear = 0;
    params.roll = 0;
    params.rollVel = 34;     // tune in on load, the way a set warms up
    params.sw = 0.85;
    params.flash = 1;

    params.buf = new Uint8Array(W * H * 3);
    params.rowShift = new Int16Array(H);
    params.rowGain = new Float32Array(H);
    params.rowOff = new Int16Array(H);
    params.colOff = new Int16Array(W);

    params.sinLut = new Float32Array(1024);
    for (let i = 0; i < 1024; i++) params.sinLut[i] = Math.sin((i / 1024) * Math.PI * 2);

    // 75 % EBU bars + reversed chroma row
    params.bars75 = new Uint8Array([
        191, 191, 191, 191, 191, 0, 0, 191, 191, 0, 191, 0,
        191, 0, 191, 191, 0, 0, 0, 0, 191, 0, 0, 0,
    ]);
    params.barsRev = new Uint8Array([
        0, 0, 191, 14, 14, 14, 191, 0, 191, 14, 14, 14,
        0, 191, 191, 14, 14, 14, 191, 191, 191, 0, 0, 0,
    ]);

    // FuBK rows: 75 % bars ending in grey, then 100 % bars ending in black
    params.fubkTop = new Uint8Array([
        191, 191, 191, 191, 191, 0, 0, 191, 191, 0, 191, 0,
        191, 0, 191, 191, 0, 0, 0, 0, 191, 72, 72, 72,
    ]);
    params.fubk100 = new Uint8Array([
        245, 238, 238, 255, 255, 0, 0, 255, 255, 0, 220, 0,
        255, 0, 255, 235, 0, 0, 0, 0, 255, 0, 0, 0,
    ]);
    params.burst = new Uint8Array([4, 3, 2, 1, 2, 3, 4]);

    // hue sweep palette, one column each
    params.huePal = new Uint8Array(W * 3);
    for (let x = 0; x < W; x++) {
        const h = (x / W) * 6;
        const i = h | 0;
        const f = h - i;
        const q = 1 - f;
        let r = 0, g = 0, b = 0;
        if (i === 0) { r = 1; g = f; }
        else if (i === 1) { r = q; g = 1; }
        else if (i === 2) { g = 1; b = f; }
        else if (i === 3) { g = q; b = 1; }
        else if (i === 4) { r = f; b = 1; }
        else { r = 1; b = q; }
        params.huePal[x * 3] = (r * 255) | 0;
        params.huePal[x * 3 + 1] = (g * 255) | 0;
        params.huePal[x * 3 + 2] = (b * 255) | 0;
    }

    // 5×7 glyphs: P, F
    params.glyphs = new Uint8Array([
        0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10,
        0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10,
    ]);
}

function retune(params) {
    params.sw = 1;
    params.flash = 1;
    const v = 48 + rnd(params) * 62;
    params.rollVel = rnd(params) < 0.5 ? -v : v;
}

export function update(dt, input, params) {
    if (dt > 0.1) dt = 0.1;

    if (input && input.knobValues) {
        let raw = input.knobValues[0];
        if (raw >= 7) raw -= 7;
        let ch = Math.round(raw);
        params.detune = raw - ch;              // −0.5 … +0.5 around the lock point
        if (ch >= 7) ch -= 7;
        if (ch !== params.chan) { params.chan = ch; retune(params); }
        params.hum = input.knobValues[1] * 0.25;
        params.snow = input.knobValues[2];
        params.fringe = input.knobValues[3];
    }
    if (input && input.btnPressed) {
        if (input.btnPressed[0]) retune(params);
        if (input.btnPressed[1]) params.hum = 0.5;
        if (input.btnPressed[2]) params.snow = 0;
        if (input.btnPressed[3]) params.fringe = 0;
    }

    params.t += dt;
    if (params.t > 100) params.t -= 100;          // every multiplier used is × 0.01

    // ── vertical hold: off the lock point the frame creeps, then jumps a whole
    // field. The phase runs at a steady rate; raising it to the sixth power is
    // what makes the picture dwell and then bolt.
    const ad = params.detune < 0 ? -params.detune : params.detune;
    let e = (ad - 0.10) / 0.40;
    if (e < 0) e = 0; else if (e > 1) e = 1;
    params.lock = e;
    if (e <= 0) {
        params.vphase = 0; params.gPrev = 0; params.hold = 0; params.tear = 0;
    } else {
        params.vphase += dt * (0.18 + 2.0 * e * e);
        params.vphase -= Math.floor(params.vphase);
        const u = params.vphase;
        const u2 = u * u;
        const u6 = u2 * u2 * u2;
        const g = u6 + (u - u6) * e * e;
        let dg = g - params.gPrev;
        if (dg < -0.5) dg += 1;
        params.gPrev = g;
        dg = dg < 0 ? -dg : dg;
        params.tear = dg > 0.16 ? 1 : dg * 6.25;
        params.hold = (params.detune < 0 ? -64 : 64) * g;
    }

    if (params.sw > 0) { params.sw -= dt / 0.6; if (params.sw < 0) params.sw = 0; }
    if (params.flash > 0) { params.flash -= dt / 0.09; if (params.flash < 0) params.flash = 0; }

    params.rollVel *= Math.max(0, 1 - dt * 4.0);
    params.roll += params.rollVel * dt;
    if (params.sw <= 0 && params.rollVel < 3 && params.rollVel > -3) {
        const target = params.roll < 32 ? 0 : 64;
        params.roll += (target - params.roll) * Math.min(1, dt * 8);
    }
    params.roll = ((params.roll % 64) + 64) % 64;
}

export function draw(display, params, time) {
    const p = params;
    const b = p.buf;
    const t = p.t;

    // ── 1. the card itself
    const ch = p.chan;
    if (ch === 0) cardBars(b, p);
    else if (ch === 1) cardPhilips(b, p);
    else if (ch === 2) cardFubk(b, p);
    else if (ch === 3) cardMono(b, p);
    else if (ch === 4) cardZone(b, p);
    else if (ch === 5) cardGrey(b, p);
    else cardSnow(b, p);

    // ── 2. per-row interference, per-column convergence
    const hum = p.hum;
    const sw = p.sw;
    const snow = p.snow;
    const frame = (t * 30) | 0;

    const bandH = 13 + 11 * hum;
    const bandY = 78 - ((t * 0.19) % 1) * 104;      // sweeps bottom → top
    const swing = lsin(p, t * 0.7);

    for (let y = 0; y < H; y++) {
        const d = (y - bandY) / bandH;
        let env = 0;
        if (d > -1 && d < 1) { env = 1 - d * d; env *= env; }

        let s = lsin(p, t * 0.9 + y * 0.013) * 1.1 + lsin(p, t * 3.1 + y * 0.11) * 1.5;
        s *= hum;
        s += env * hum * 20 * swing;

        // bright crest above the band, dark trough below — smooth, no seam
        let gain = 1 + hum * (d > -1 && d < 1 ? -d * (1 - d * d) * 1.95 : 0);

        if (sw > 0) {
            s += (hash3(y, 31, frame) - 0.5) * 26 * sw * sw;
            gain *= 1 - 0.35 * sw * hash3(y, 57, frame);
        }
        if (p.tear > 0) {
            s += (hash3(y, 71, frame) - 0.5) * 24 * p.tear;
            gain *= 1 - 0.3 * p.tear * hash3(y, 83, frame);
        }
        if (snow > 0 && hash3(y, 13, frame) < snow * 0.05) {
            s += (hash3(y, 99, frame) - 0.5) * 34;
            gain *= 0.45;
        }
        p.rowShift[y] = Math.round(s);
        p.rowGain[y] = gain * (1 + 0.75 * p.flash);
        p.rowOff[y] = Math.round(p.fringe * 1.8 * ((y - 31.5) / 31.5));
    }

    for (let x = 0; x < W; x++) {
        p.colOff[x] = Math.round(p.fringe * 4.6 * ((x - 63.5) / 63.5));
    }

    // ── 3. compose: roll, convergence, noise
    const rollI = (((Math.round(p.roll + p.hold) % 64) + 64) % 64);
    const dw = display.width < W ? display.width : W;
    const dh = display.height < H ? display.height : H;
    const stat = sw > 0.55 ? (sw - 0.55) / 0.45 : 0;
    let blank = sw > 0 ? Math.min(1, sw * 1.7) : 0;
    if (p.lock > 0 && blank < 0.92) blank = 0.92;

    for (let y = 0; y < dh; y++) {
        let sy = y + rollI;
        if (sy >= H) sy -= H;
        const sh = p.rowShift[y];
        const gain = p.rowGain[y];
        const vo = p.rowOff[y];

        let syR = sy + vo; if (syR < 0) syR = 0; else if (syR > 63) syR = 63;
        let syB = sy - vo; if (syB < 0) syB = 0; else if (syB > 63) syB = 63;
        const rowR = syR * W, rowG = sy * W, rowB = syB * W;

        const vbl = blank > 0 && sy < 3 ? blank : 0;

        for (let x = 0; x < dw; x++) {
            const co = p.colOff[x];
            let xr = x + sh + co; if (xr < 0) xr = 0; else if (xr > 127) xr = 127;
            let xg = x + sh; if (xg < 0) xg = 0; else if (xg > 127) xg = 127;
            let xb = x + sh - co; if (xb < 0) xb = 0; else if (xb > 127) xb = 127;

            let r = b[(rowR + xr) * 3] * gain;
            let g = b[(rowG + xg) * 3 + 1] * gain;
            let bl = b[(rowB + xb) * 3 + 2] * gain;

            if (vbl > 0) {
                const k = 1 - vbl;
                r *= k; g *= k; bl *= k;
            }

            if (stat > 0) {
                const n = 20 + hash3(x, y, frame + 4096) * 235;
                r += (n - r) * stat; g += (n - g) * stat; bl += (n - bl) * stat;
            }

            if (snow > 0) {
                const n = hash3(x, y, frame);
                const grain = (n - 0.5) * 95 * snow;
                r += grain; g += grain; bl += grain;
                if (hash3(x + 3, y + 5, frame) > 1 - snow * 0.09) {
                    const v = 150 + n * 105;
                    r = v; g = v; bl = v;
                }
            }

            display.setPixel(
                x, y,
                r < 0 ? 0 : r > 255 ? 255 : r | 0,
                g < 0 ? 0 : g > 255 ? 255 : g | 0,
                bl < 0 ? 0 : bl > 255 ? 255 : bl | 0,
            );
        }
    }
}

// ── Made with Patternflow Community · https://patternflow.work/community ──
// Shared under CC-BY-SA-4.0. Attribution is part of this licence —
// please keep this notice and the author credit above when you reuse,
// remix, or redistribute this pattern. Do not delete it.
