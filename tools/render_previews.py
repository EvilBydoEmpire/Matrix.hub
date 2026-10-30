#!/usr/bin/env python3
"""Render the gallery's thumbnails with the firmware's own code and embed one atlas per panel size.

Every pattern plays from its compiled defaults on a renderer built with that firmware's panel geometry,
and its row holds 24 frames from the fifth second on; flashing patterns get one still, so the library
never flashes. WS2812 tiles are the panel's own pixels; HUB75 tiles average the 64-pixel frames down to
28. Each atlas is quantised until it fits its flash budget. Run this after changing a pattern, a palette
or the gallery; tools/test_preview_tools.py reports a header older than its sources.
"""
import concurrent.futures
import hashlib
import io
import json
import logging
import os
import pathlib
import re
import subprocess
import zlib

from PIL import Image

from build_web import CATALOG_H as CATALOG, gallery_order as gallery

ROOT = pathlib.Path(__file__).resolve().parent.parent
HEADER = ROOT / 'src/generated/PatternPreviewAtlas.h'
WORK = ROOT / '.local/previews'
FRAMES, INTERVAL_MS = 24, 67
# Name, panel size, tile size, PNG budget in bytes, the firmware's geometry and the preprocessor condition
# that selects the atlas.
PROFILES = (
    ('ws2812-16', 16, 16, 215_000, ['-DMATRIX_PROFILE_WS2812', '-DWS2812_MATRIX_WIDTH=16', '-DWS2812_MATRIX_HEIGHT=16'],
     'defined(MATRIX_PROFILE_WS2812) && WS2812_MATRIX_WIDTH == 16'),
    ('ws2812-22', 22, 22, 400_000, ['-DMATRIX_PROFILE_WS2812', '-DWS2812_MATRIX_WIDTH=22', '-DWS2812_MATRIX_HEIGHT=22'],
     'defined(MATRIX_PROFILE_WS2812) && WS2812_MATRIX_WIDTH == 22'),
    ('hub75', 64, 28, 500_000, [], 'defined(MATRIX_PROFILE_HUB75)'),
)
# Everything a thumbnail depends on.
SOURCES = ('src/patterns/*.h', 'src/patterns/*.cpp', 'src/patterns/*.inc', 'src/generated/CollatzTopology.h',
           'tools/render_previews.cpp', 'tools/render_previews.py')
log = logging.getLogger('render_previews')


def flashing(key: str) -> bool:
    low, high = map(int, re.search(r'isFlashingVariant\(int variant\)\{return variant>=(\d+)&&variant<=(\d+);\}',
                                   CATALOG.read_text(encoding='utf-8')).groups())
    return key[0] == 'p' and low <= int(key[1:]) <= high


def source_digest() -> str:
    digest = hashlib.sha256()
    for path in sorted(p for pattern in SOURCES for p in ROOT.glob(pattern) if p != HEADER):
        digest.update(path.relative_to(ROOT).as_posix().encode() + b'\0' + path.read_bytes())
    return digest.hexdigest()


def render(profile) -> Image.Image:
    """One profile's atlas: a row of FRAMES tiles per gallery entry."""
    name, panel, tile, _, defines, _ = profile
    WORK.mkdir(parents=True, exist_ok=True)
    renderer = WORK / f'render-{name}'
    subprocess.run(['clang++', '-std=c++17', '-O2', *defines, '-I' + str(ROOT / 'tools/test-support'),
                    str(ROOT / 'tools/render_previews.cpp'), str(ROOT / 'src/patterns/NativeEngines.cpp'),
                    str(ROOT / 'src/patterns/PatternflowPack.cpp'), str(ROOT / 'src/patterns/FastMath.cpp'),
                    '-o', str(renderer)], check=True)
    keys = gallery()
    atlas = Image.new('RGB', (tile * FRAMES, tile * len(keys)))
    frame_bytes = panel * panel * 3

    def row(index_key):
        index, key = index_key
        raw = WORK / f'{name}-{key}.rgb'
        subprocess.run([str(renderer), key, str(panel), str(raw)], check=True)
        data = raw.read_bytes()
        raw.unlink()
        for frame in range(FRAMES):
            # The renderer writes 30 frames a second; tiles step two of them, from the fifth second.
            source = 120 if flashing(key) else 120 + frame * 2
            image = Image.frombytes('RGB', (panel, panel), data[source * frame_bytes:(source + 1) * frame_bytes])
            if tile != panel:
                image = image.resize((tile, tile), Image.Resampling.BOX)
            atlas.paste(image, (frame * tile, index * tile))

    with concurrent.futures.ThreadPoolExecutor(os.cpu_count()) as pool:
        list(pool.map(row, enumerate(keys)))
    return atlas


def encode(atlas: Image.Image, budget: int) -> tuple:
    """The indexed PNG with the most colours that fits the budget, and its mean error per channel."""
    for colors in (256, 224, 192, 160, 128, 96, 64):
        indexed = atlas.quantize(colors=colors, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
        stream = io.BytesIO()
        indexed.save(stream, format='PNG', optimize=True)
        data = stream.getvalue()
        if len(data) <= budget:
            error = sum(abs(a - b) for a, b in zip(atlas.tobytes(), indexed.convert('RGB').tobytes())) / (atlas.width * atlas.height * 3)
            return data, colors, error
    raise ValueError(f'{len(data)} bytes exceed the {budget}-byte budget')


def section(profile, data: bytes) -> str:
    name, _, tile, _, _, condition = profile
    rows = ',\n'.join('  ' + ','.join(f'0x{b:02X}' for b in data[i:i + 16]) for i in range(0, len(data), 16))
    return (f'#if {condition}  // {name}\n'
            f'constexpr uint16_t PATTERN_PREVIEW_WIDTH = {tile};\n'
            f'constexpr uint16_t PATTERN_PREVIEW_HEIGHT = {tile};\n'
            f'constexpr uint8_t PATTERN_PREVIEW_FRAMES = {FRAMES};\n'
            f'constexpr uint16_t PATTERN_PREVIEW_INTERVAL_MS = {INTERVAL_MS};\n'
            f'constexpr size_t PATTERN_PREVIEW_ATLAS_SIZE = {len(data)};\n'
            f'constexpr uint32_t PATTERN_PREVIEW_ATLAS_KEY = {zlib.crc32(data)}U;\n'
            f'const uint8_t PATTERN_PREVIEW_ATLAS[] PROGMEM = {{\n{rows}\n}};\n'
            f'#endif\n')


def recorded_digest() -> str | None:
    found = re.search(r'Source SHA-256: (\w+)', HEADER.read_text(encoding='utf-8')) if HEADER.exists() else None
    return found and found[1]


def atlas(panel: int) -> dict:
    """The embedded atlas a panel of this size serves: its PNG, tile size and key, as the firmware reports them."""
    name = next((p[0] for p in PROFILES if p[1] == panel), 'hub75')
    body = HEADER.read_text(encoding='utf-8').split(f'// {name}\n', 1)[1].split('#endif', 1)[0]
    data = bytes(int(v, 16) for v in re.findall(r'0x([0-9A-F]{2})', body.split('PROGMEM = {', 1)[1]))
    return {'png': data, 'tile': int(re.search(r'PATTERN_PREVIEW_WIDTH = (\d+)', body)[1]),
            'key': int(re.search(r'PATTERN_PREVIEW_ATLAS_KEY = (\d+)U', body)[1])}


def main() -> None:
    digest, sections, report = source_digest(), [], {}
    for profile in PROFILES:
        data, colors, error = encode(render(profile), profile[3])
        sections.append(section(profile, data))
        report[profile[0]] = {'tile': profile[2], 'colors': colors, 'bytes': len(data), 'meanAbsoluteRgb8Error': round(error, 3)}
    HEADER.write_text('#pragma once\n// Generated by tools/render_previews.py; run it after changing a pattern.\n'
                      f'// Source SHA-256: {digest}\n#include <Arduino.h>\n\n' + '\n'.join(sections), encoding='utf-8')
    log.info('%s', json.dumps(report))


if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO, format='%(message)s')
    main()
