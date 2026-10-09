#!/usr/bin/env python3
"""Benchmark every firmware pattern and sample its real rendered preview frames."""

from __future__ import annotations

import argparse
import glob
import json
import pathlib
import struct
import time
import zlib
import urllib.request


ROOT = pathlib.Path(__file__).resolve().parent.parent
ASSET = ROOT / "assets" / "pattern-preview-atlas.png"
HEADER = ROOT / "src" / "generated" / "PatternPreviewAtlas.h"
REPORT = ROOT / "docs" / "pattern-benchmark.json"
BUILT_INS = [
    "Starfield", "Warped Checkerboard", "Moiré Rings", "Moiré Lines",
    "Warping Checkers", "Shockwave", "Warp Corridors", "Pixel Avalanche",
    "Kaleidoscope", "Bayer Flow", "Floyd Flow", "Snake", "Squares",
    "Perspective Flow", "Flow Sphere", "2D Flow Field",
]


def discover_port() -> str:
    ports = sorted(glob.glob("/dev/cu.usbmodem*"))
    if not ports:
        raise RuntimeError("No USB MatrixPortal found")
    return ports[0]


class MatrixUsb:
    def __init__(self, port: str):
        import serial
        self.serial = serial.Serial(port, 115200, timeout=0.25, write_timeout=2)
        time.sleep(1.2)
        self.serial.reset_input_buffer()

    def close(self):
        self.serial.close()

    def request(self, command: str, prefix: str, timeout: float = 15.0) -> str:
        self.serial.reset_input_buffer()
        self.serial.write((command + "\n").encode())
        self.serial.flush()
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            line = self.serial.readline().decode(errors="replace").strip()
            if line.startswith(prefix):
                return line[len(prefix):]
        raise TimeoutError(f"No response to {command}")

    def json_get(self, resource: str):
        return json.loads(self.request(
            f"USBGET,{resource}", f"USBJSON,{resource},", 20.0
        ))

    def command(self, command: str, value):
        payload = json.dumps({"command": command, "value": value}, separators=(",", ":"))
        raw = self.request("USBPOST,CONTROL," + payload, "USBJSON,RESULT,")
        status, body = raw.split(",", 1)
        result = json.loads(body)
        if int(status) >= 400 or not result.get("success"):
            raise RuntimeError(result.get("message", body))

    def preview(self) -> tuple[int, int, bytes]:
        raw = self.request("USBGET,PREVIEW", "USBPREVIEW,", 20.0)
        width_text, height_text, pixels = raw.split(",", 2)
        width, height = int(width_text), int(height_text)
        data = bytes.fromhex(pixels)
        if len(data) != width * height:
            raise RuntimeError(f"Preview has {len(data)} bytes; expected {width*height}")
        return width, height, data


class MatrixHttp:
    def __init__(self, host: str):
        self.base = host.rstrip('/') if host.startswith('http') else 'http://' + host

    def close(self):
        pass

    def json_get(self, resource: str):
        with urllib.request.urlopen(self.base + '/' + resource.lower(), timeout=30) as response:
            return json.load(response)

    def command(self, command: str, value):
        # setParam's fields are top-level; most commands use a single value.
        fields = value if command == 'setParam' else {'value': value}
        payload = json.dumps({'command':command, **fields}).encode()
        request = urllib.request.Request(self.base + '/control', payload, {'Content-Type':'application/json'})
        with urllib.request.urlopen(request, timeout=30) as response:
            result = json.load(response)
        if not result.get('success'):
            raise RuntimeError(result.get('message','Command failed'))

    def preview(self):
        data = self.json_get('preview?format=rgb888')
        pixels = bytes.fromhex(data['pixels'])
        if data.get('encoding') == 'rgb888-area':
            # Display-transfer correction only for thumbnails: low-coverage
            # LED filaments must not disappear on an sRGB browser canvas.
            # No change to the actual LED colors or brightness setting.
            levels = [[round((v/255)**.5 * steps) for v in range(256)] for steps in (7,7,3)]
            pixels = bytes((levels[0][pixels[i]]<<5)|(levels[1][pixels[i+1]]<<2)|levels[2][pixels[i+2]] for i in range(0,len(pixels),3))
        return data['width'], data['height'], pixels


def rgb332(value: int) -> tuple[int, int, int]:
    r, g, b = (value >> 5) & 7, (value >> 2) & 7, value & 3
    return round(r * 255 / 7), round(g * 255 / 7), round(b * 255 / 3)


def png_bytes(width: int, height: int, rgb: bytes) -> bytes:
    signature = b"\x89PNG\r\n\x1a\n"

    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(
            ">I", zlib.crc32(kind + data) & 0xFFFFFFFF
        )

    rows = b"".join(
        b"\0" + rgb[y * width * 3:(y + 1) * width * 3]
        for y in range(height)
    )
    return (
        signature
        + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(rows, 9))
        + chunk(b"IEND", b"")
    )


def write_header(data: bytes, frame_width: int, frame_height: int,
                 frames: int, patterns: int, interval_ms: int = 67):
    HEADER.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "#pragma once",
        "#include <Arduino.h>",
        "",
        f"constexpr uint16_t PATTERN_PREVIEW_WIDTH = {frame_width};",
        f"constexpr uint16_t PATTERN_PREVIEW_HEIGHT = {frame_height};",
        f"constexpr uint8_t PATTERN_PREVIEW_FRAMES = {frames};",
        f"constexpr uint8_t PATTERN_PREVIEW_COUNT = {patterns};",
        f"constexpr uint16_t PATTERN_PREVIEW_INTERVAL_MS = {interval_ms};",
        f"constexpr size_t PATTERN_PREVIEW_ATLAS_SIZE = {len(data)};",
        f"constexpr uint32_t PATTERN_PREVIEW_ATLAS_KEY = {zlib.crc32(data)&0xFFFFFFFF}U;",
        "const uint8_t PATTERN_PREVIEW_ATLAS[] PROGMEM = {",
    ]
    for offset in range(0, len(data), 16):
        lines.append("  " + ",".join(f"0x{value:02X}" for value in data[offset:offset+16]) + ",")
    lines.extend(["};", ""])
    HEADER.write_text("\n".join(lines), encoding="utf-8")


def read_generated_atlas(expected_width: int, expected_height: int) -> bytearray:
    """Read our own filter-zero RGB PNG for updating selected preview rows."""
    data = ASSET.read_bytes()
    assert data[:8] == b"\x89PNG\r\n\x1a\n"
    offset, compressed = 8, bytearray()
    while offset < len(data):
        size = struct.unpack_from('>I', data, offset)[0]
        kind, payload = data[offset+4:offset+8], data[offset+8:offset+8+size]
        if kind == b'IHDR':
            assert struct.unpack('>IIBBBBB', payload) == (expected_width, expected_height, 8, 2, 0, 0, 0)
        elif kind == b'IDAT':
            compressed.extend(payload)
        offset += size+12
    rows = zlib.decompress(compressed)
    stride = expected_width*3+1
    assert len(rows) == stride*expected_height
    assert all(rows[y*stride] == 0 for y in range(expected_height))
    return bytearray(b''.join(rows[y*stride+1:(y+1)*stride] for y in range(expected_height)))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default=None)
    parser.add_argument("--host", default=None, help="Matrix IP/hostname for Wi-Fi profiling")
    parser.add_argument("--frames", type=int, default=24)
    parser.add_argument("--interval", type=float, default=0.067)
    parser.add_argument("--settle", type=float, default=1.25)
    parser.add_argument("--benchmark-only", action="store_true")
    parser.add_argument(
        "--only", type=int, action="append",
        help="Benchmark/capture only this gallery index; preview mode updates the existing atlas rows",
    )
    parser.add_argument("--report", type=pathlib.Path, default=REPORT)
    args = parser.parse_args()

    device = MatrixHttp(args.host) if args.host else MatrixUsb(args.port or discover_port())
    initial = device.json_get("CONTROL")
    catalog = initial.get('catalog') or json.loads((ROOT/'assets/pattern-catalog.json').read_text())
    entries = [("builtin" if item['variant'] is None else "variant",
                item['engine'] if item['variant'] is None else item['variant'], gallery)
               for gallery,item in enumerate(catalog)]
    total_entries = len(entries)
    if args.only:
        if any(index < 0 or index >= total_entries for index in args.only):
            parser.error("--only gallery index is out of range")
        entries = [entries[index] for index in args.only]
    sampled: list[list[bytes]] = []
    benchmarks = []
    try:
        device.command("setAutoCycle", False)
        device.command("setFPS", 240)
        for number, (kind, index, gallery_index) in enumerate(entries):
            device.command("setPattern" if kind == "builtin" else "setPatternVariant", index)
            time.sleep(args.settle)
            frames = []
            if args.benchmark_only:
                time.sleep(1.1)
            else:
                next_frame = time.monotonic()
                for _ in range(args.frames):
                    time.sleep(max(0, next_frame-time.monotonic()))
                    width, height, pixels = device.preview()
                    frames.append(pixels)
                    next_frame += args.interval
            diagnostics = device.json_get("DIAGNOSTICS")
            benchmarks.append({
                "index": gallery_index,
                "kind": kind,
                "engineIndex": index,
                "name": diagnostics["pattern"],
                "actualFPS": diagnostics["actualFPS"],
                "averageRenderUs": diagnostics["averageRenderUs"],
                "averageDrawUs": diagnostics["averageDrawUs"],
                "averageShowUs": diagnostics["averageShowUs"],
                "worstRenderUs": diagnostics["worstRenderUs"],
                "panelRefreshHz": diagnostics["panelRefreshHz"],
                "renderWidth": diagnostics.get("renderWidth"),
                "renderHeight": diagnostics.get("renderHeight"),
            })
            sampled.append(frames)
            print(
                f"[{number+1:02d}/{len(entries)}] {diagnostics['pattern']}: "
                f"{diagnostics['actualFPS']:.1f} FPS, "
                f"{diagnostics['averageRenderUs']/1000:.2f} ms",
                flush=True,
            )
    finally:
        try:
            device.command("setFPS", initial["targetFPS"])
            if initial["currentPattern"] == 16:
                device.command("setPatternVariant", initial["params"].get("variant", 0))
            else:
                device.command("setPattern", initial["currentPattern"])
            if args.host:
                for key, value in initial['params'].items():
                    if key != 'variant':
                        device.command('setParam', {'animIndex':initial['currentPattern'],
                            'paramName':key, 'value':value})
            device.command("setAutoCycle", initial["autoCycleEnabled"])
        finally:
            device.close()

    if not args.benchmark_only:
        atlas_width = width * args.frames
        atlas_height = height * total_entries
        atlas = read_generated_atlas(atlas_width, atlas_height) if args.only else bytearray(atlas_width * atlas_height * 3)
        for number, frames in enumerate(sampled):
            pattern = benchmarks[number]['index']
            for frame, pixels in enumerate(frames):
                for y in range(height):
                    for x in range(width):
                        rgb = rgb332(pixels[y * width + x])
                        target = ((pattern * height + y) * atlas_width + frame * width + x) * 3
                        atlas[target:target+3] = bytes(rgb)
        encoded = png_bytes(atlas_width, atlas_height, bytes(atlas))
        ASSET.parent.mkdir(parents=True, exist_ok=True)
        ASSET.write_bytes(encoded)
        write_header(encoded, width, height, args.frames, total_entries, round(args.interval*1000))
    else:
        width = height = 28
    if args.only and args.report.exists():
        previous = json.loads(args.report.read_text())
        merged = {item['index']:item for item in previous.get('patterns', [])}
        merged.update({item['index']:item for item in benchmarks})
        benchmarks = [merged[index] for index in sorted(merged)]
    args.report.write_text(json.dumps({
        "capturedAt": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
        "targetFPS": 240,
        "transport": "wifi" if args.host else "usb",
        "frameWidth": width,
        "frameHeight": height,
        "previewFrames": args.frames,
        "patterns": benchmarks,
    }, indent=2) + "\n", encoding="utf-8")
    if args.benchmark_only:
        print(f"report={args.report}")
    else:
        print(f"atlas={ASSET} bytes={len(encoded)} report={args.report}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
