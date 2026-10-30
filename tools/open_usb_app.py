#!/usr/bin/env python3
"""Serve the matrix app locally and tunnel its API over USB.

Opening the port restarts the matrix once into normal boot; see open_serial().
"""

from __future__ import annotations

import atexit
import json
import logging
import os
import threading
import time
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse

import serial
from serial.tools import list_ports

import build_web
import render_previews


HOST = "127.0.0.1"
PORT = int(os.environ.get("MATRIX_APP_PORT", "48615"))
SERIAL_LOCK = threading.Lock()
BOOT_TIMEOUT = 10.0  # The firmware serves within seconds and joins Wi-Fi in the background.
SERIAL_DEVICE = None
log = logging.getLogger("open_usb_app")


def close_serial():
    global SERIAL_DEVICE
    if SERIAL_DEVICE is not None:
        # Both lines low before closing, so a hang-up changes nothing.
        SERIAL_DEVICE.dtr = False
        SERIAL_DEVICE.rts = False
        SERIAL_DEVICE.close()
        SERIAL_DEVICE = None


atexit.register(close_serial)


def matrix_port() -> str:
    explicit = os.environ.get("MATRIX_SERIAL_PORT")
    if explicit:
        return explicit
    # USB serial ports report a vendor ID; Bluetooth and debug consoles do not.
    ports = sorted(port.device for port in list_ports.comports() if port.vid is not None)
    if len(ports) != 1:
        raise RuntimeError("Connect one matrix, or select it with MATRIX_SERIAL_PORT")
    return ports[0]


def open_serial() -> serial.Serial:
    """Open the port and restart the matrix into its firmware.

    On ESP32 boards DTR drives GPIO0 (boot mode) and RTS drives EN (reset).
    Drivers toggle both while a port opens or closes, and a reset released
    while GPIO0 is low leaves the board waiting in download mode. Keeping DTR
    off holds GPIO0 high while RTS pulses EN, like esptool's hard reset.
    """
    device = serial.Serial()
    device.port = matrix_port()
    device.baudrate = 115200
    device.timeout = 0.25
    device.dtr = False
    device.rts = True
    device.open()
    time.sleep(0.1)
    device.rts = False
    deadline = time.monotonic() + BOOT_TIMEOUT
    while time.monotonic() < deadline:
        if b"HTTP server started" in device.readline():
            time.sleep(0.5)
            break
    return device


def serial_request(command: str, prefix: str, timeout: float = 12.0) -> str:
    global SERIAL_DEVICE
    with SERIAL_LOCK:
        if SERIAL_DEVICE is None or not SERIAL_DEVICE.is_open:
            # One connection for the whole session; never reopen per request.
            SERIAL_DEVICE = open_serial()
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


class AppHandler(BaseHTTPRequestHandler):
    server_version = "MatrixUSB/1.0"

    def log_message(self, format_string, *args):
        log.info(format_string, *args)

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
                self.send_bytes(200, "text/html; charset=utf-8", build_web.bundle("index.html").encode("utf-8"))
            elif path == "/preview-atlas.png":
                panel = usb_json("USBGET,CONTROL", "USBJSON,CONTROL,")["displayWidth"]
                self.send_bytes(200, "image/png", render_previews.atlas(panel)["png"])
            elif path == "/control":
                state = usb_json("USBGET,CONTROL", "USBJSON,CONTROL,")
                # Thumbnails come from this checkout's atlas for the panel's size.
                local = render_previews.atlas(state["displayWidth"])
                state["previewWidth"] = state["previewHeight"] = local["tile"]
                state["previewAtlasKey"] = local["key"]
                self.send_json(200, state)
            elif path == "/diagnostics":
                self.send_json(
                    200,
                    usb_json("USBGET,DIAGNOSTICS", "USBJSON,DIAGNOSTICS,"),
                )
            else:
                self.send_json(404, {"success": False, "message": "Not found"})
        except Exception as error:
            self.send_json(503, {"success": False, "message": str(error)})

    def do_POST(self):
        path = urlparse(self.path).path
        # Like the firmware, accept writes only as JSON: browsers send that across
        # origins only after a preflight this bridge never answers.
        if not self.headers.get("Content-Type", "").startswith("application/json"):
            self.send_json(415, {"success": False, "message": "Commands must be sent as application/json."})
            return
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
                result = serial_request("WIFI,RESET", "WIFI,RESET,", 8.0)
                if result != "restarting":
                    raise RuntimeError("Could not verify Wi-Fi erase; device did not confirm restart")
                self.send_json(
                    200, {"success": True, "message": "Wi-Fi networks erased"}
                )
            else:
                self.send_json(404, {"success": False, "message": "Not found"})
        except Exception as error:
            self.send_json(503, {"success": False, "message": str(error)})


def main() -> int:
    log.info("Matrix found on %s", matrix_port())
    server = ThreadingHTTPServer((HOST, PORT), AppHandler)
    url = f"http://{HOST}:{PORT}/"
    log.info("USB app ready: %s", url)
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
    logging.basicConfig(level=logging.INFO, format="%(message)s")
    raise SystemExit(main())
