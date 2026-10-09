#!/usr/bin/env python3
"""Serve the embedded matrix dashboard locally and tunnel its API over USB."""

from __future__ import annotations

import glob
import os
import atexit
import json
import pathlib
import threading
import time
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse

import serial


HOST = "127.0.0.1"
PORT = int(os.environ.get("MATRIX_DASHBOARD_PORT", "8765"))
PROJECT_DIR = pathlib.Path(__file__).resolve().parent.parent
SKETCH = PROJECT_DIR / "64x64_matrix_01.ino"
PREVIEW_ATLAS = PROJECT_DIR / "assets" / "pattern-preview-atlas.png"
SERIAL_LOCK = threading.Lock()
SERIAL_DEVICE = None


def close_serial():
    global SERIAL_DEVICE
    if SERIAL_DEVICE is not None:
        SERIAL_DEVICE.close()
        SERIAL_DEVICE = None


atexit.register(close_serial)


def matrix_port() -> str:
    explicit = os.environ.get("MATRIX_SERIAL_PORT")
    if explicit:
        return explicit
    ports = sorted(glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/cu.usbserial*"))
    if len(ports) != 1:
        raise RuntimeError("Connect one matrix, or select it with MATRIX_SERIAL_PORT")
    return ports[0]


def dashboard_html() -> bytes:
    source = SKETCH.read_text(encoding="utf-8")
    marker = 'const char DASHBOARD_HTML[] PROGMEM = R"HTML('
    start = source.index(marker) + len(marker)
    end = source.index(')HTML";', start)
    return source[start:end].encode("utf-8")


def serial_request(command: str, prefix: str, timeout: float = 12.0) -> str:
    global SERIAL_DEVICE
    with SERIAL_LOCK:
        if SERIAL_DEVICE is None or not SERIAL_DEVICE.is_open:
            device = serial.Serial(port=None, baudrate=115200, timeout=0.25)
            # Keep one connection: some USB-UART drivers pulse reset even when
            # DTR/RTS are deasserted before open. Never reopen for each slider.
            device.dtr = False
            device.rts = False
            device.port = matrix_port()
            device.open()
            SERIAL_DEVICE = device
            time.sleep(2)
        device = SERIAL_DEVICE
        try:
            device.reset_input_buffer()
            device.write((command + "\n").encode("utf-8"))
            device.flush()
            deadline = time.monotonic() + timeout
            waiting_for_payload = False
            while time.monotonic() < deadline:
                line = device.readline().decode("utf-8", errors="replace").strip()
                if line.startswith(prefix):
                    payload = line[len(prefix):]
                    if payload:
                        return payload
                    waiting_for_payload = True
                elif waiting_for_payload and line.startswith(("{", "[")):
                    return line
        except (serial.SerialException, OSError):
            close_serial()
            raise
    raise RuntimeError(f"No USB response for {command.split(',', 1)[0]}")


def usb_json(command: str, response_prefix: str, timeout: float = 12.0):
    return json.loads(serial_request(command, response_prefix, timeout))


class DashboardHandler(BaseHTTPRequestHandler):
    server_version = "MatrixUSB/1.0"

    def log_message(self, format_string, *args):
        print(f"USB dashboard: {format_string % args}")

    def send_bytes(self, status: int, content_type: str, data: bytes):
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def send_json(self, status: int, payload):
        self.send_bytes(
            status,
            "application/json; charset=utf-8",
            json.dumps(payload, separators=(",", ":")).encode("utf-8"),
        )

    def read_json(self):
        length = int(self.headers.get("Content-Length", "0"))
        if length <= 0:
            return {}
        raw = self.rfile.read(length).decode("utf-8")
        return json.loads(raw) if raw else {}

    def do_GET(self):
        path = urlparse(self.path).path
        try:
            if path in ("/", "/index.html"):
                self.send_bytes(200, "text/html; charset=utf-8", dashboard_html())
            elif path == "/preview-atlas.png":
                self.send_bytes(200, "image/png", PREVIEW_ATLAS.read_bytes())
            elif path == "/control":
                state = usb_json("USBGET,CONTROL", "USBJSON,CONTROL,")
                # This bridge serves the local 28px source atlas, not the
                # flash-optimized 16px WROOM image. Keep its coordinates honest.
                state["previewWidth"] = state["previewHeight"] = 28
                self.send_json(200, state)
            elif path == "/library":
                self.send_json(200, usb_json("USBGET,LIBRARY", "USBJSON,LIBRARY,"))
            elif path == "/diagnostics":
                self.send_json(
                    200,
                    usb_json("USBGET,DIAGNOSTICS", "USBJSON,DIAGNOSTICS,"),
                )
            elif path == "/wifi/scan":
                self.send_json(
                    200,
                    usb_json("USBGET,WIFISCAN", "USBJSON,WIFISCAN,", 20.0),
                )
            else:
                self.send_json(404, {"success": False, "message": "Not found"})
        except Exception as error:
            self.send_json(503, {"success": False, "message": str(error)})

    def do_POST(self):
        path = urlparse(self.path).path
        try:
            body = self.read_json()
            if path == "/control":
                compact = json.dumps(body, separators=(",", ":"))
                raw = serial_request(
                    "USBPOST,CONTROL," + compact, "USBJSON,RESULT,"
                )
                status_text, payload = raw.split(",", 1)
                self.send_json(int(status_text), json.loads(payload))
            elif path == "/wifi/add":
                compact = json.dumps({"command": "saveWifiNetwork", "value": {
                    "ssid": body.get("ssid", ""), "password": body.get("password", "")
                }}, separators=(",", ":"))
                raw = serial_request("USBPOST,CONTROL," + compact, "USBJSON,RESULT,")
                status_text, payload = raw.split(",", 1)
                self.send_json(int(status_text), json.loads(payload))
            elif path == "/wifi/reset":
                serial_request("WIFI,RESET", "WIFI,RESET,", 8.0)
                self.send_json(
                    200, {"success": True, "message": "Wi-Fi networks erased"}
                )
            else:
                self.send_json(404, {"success": False, "message": "Not found"})
        except Exception as error:
            self.send_json(503, {"success": False, "message": str(error)})


def main() -> int:
    print(f"Matrix found on {matrix_port()}")
    server = ThreadingHTTPServer((HOST, PORT), DashboardHandler)
    url = f"http://{HOST}:{PORT}/"
    print(f"USB dashboard ready: {url}")
    webbrowser.open(url)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
        close_serial()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
