#!/usr/bin/env python3
"""Measure firmware control round-trip latency over WebSocket or USB serial."""

import argparse
import base64
import hashlib
import os
import socket
import statistics
import struct
import time


def summary(samples):
    ordered = sorted(samples)

    def percentile(value):
        return ordered[min(len(ordered) - 1, int(value * (len(ordered) - 1)))]

    return {
        "count": len(ordered),
        "min_ms": ordered[0],
        "median_ms": percentile(0.50),
        "p95_ms": percentile(0.95),
        "p99_ms": percentile(0.99),
        "max_ms": ordered[-1],
        "mean_ms": statistics.fmean(ordered),
    }


class WebSocketClient:
    def __init__(self, host, port):
        self.socket = socket.create_connection((host, port), timeout=2)
        key = base64.b64encode(os.urandom(16)).decode()
        request = (
            f"GET / HTTP/1.1\r\nHost: {host}:{port}\r\n"
            "Upgrade: websocket\r\nConnection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n"
        )
        self.socket.sendall(request.encode())
        response = b""
        while b"\r\n\r\n" not in response:
            response += self.socket.recv(4096)
        if not response.startswith(b"HTTP/1.1 101"):
            raise RuntimeError(response.decode(errors="replace"))
        expected = base64.b64encode(
            hashlib.sha1((key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()).digest()
        )
        if expected not in response:
            raise RuntimeError("WebSocket accept key mismatch")

    def send_text(self, message):
        payload = message.encode()
        mask = os.urandom(4)
        header = bytes((0x81, 0x80 | len(payload))) + mask
        masked = bytes(value ^ mask[index & 3] for index, value in enumerate(payload))
        self.socket.sendall(header + masked)

    def receive_text(self):
        while True:
            first, second = self._receive_exact(2)
            opcode = first & 0x0F
            length = second & 0x7F
            if length == 126:
                length = struct.unpack("!H", self._receive_exact(2))[0]
            elif length == 127:
                length = struct.unpack("!Q", self._receive_exact(8))[0]
            mask = self._receive_exact(4) if second & 0x80 else None
            payload = self._receive_exact(length)
            if mask:
                payload = bytes(value ^ mask[index & 3] for index, value in enumerate(payload))
            if opcode == 0x9:
                self._send_control(0xA, payload)
                continue
            if opcode == 0x1:
                return payload.decode()
            if opcode == 0x8:
                raise RuntimeError("WebSocket closed during benchmark")

    def _send_control(self, opcode, payload):
        mask = os.urandom(4)
        header = bytes((0x80 | opcode, 0x80 | len(payload))) + mask
        self.socket.sendall(
            header + bytes(value ^ mask[index & 3] for index, value in enumerate(payload))
        )

    def _receive_exact(self, count):
        data = b""
        while len(data) < count:
            chunk = self.socket.recv(count - len(data))
            if not chunk:
                raise RuntimeError("Unexpected socket close")
            data += chunk
        return data

    def close(self):
        self.socket.close()


def benchmark_wifi(host, port, count):
    client = WebSocketClient(host, port)
    samples = []
    try:
        for sequence in range(count):
            expected = f"PONG,{sequence}"
            started = time.perf_counter_ns()
            client.send_text(f"PING,{sequence}")
            while client.receive_text() != expected:
                pass
            samples.append((time.perf_counter_ns() - started) / 1_000_000)
    finally:
        client.close()
    return summary(samples)


def benchmark_usb(port, count):
    import serial

    samples = []
    with serial.Serial(port, 115200, timeout=1, write_timeout=1) as connection:
        connection.reset_input_buffer()
        for sequence in range(count):
            expected = f"PONG,{sequence}"
            started = time.perf_counter_ns()
            connection.write(f"PING,{sequence}\n".encode())
            connection.flush()
            deadline = time.monotonic() + 1
            while time.monotonic() < deadline:
                if connection.readline().decode(errors="replace").strip() == expected:
                    samples.append((time.perf_counter_ns() - started) / 1_000_000)
                    break
            else:
                raise TimeoutError(f"No reply for USB sequence {sequence}")
    return summary(samples)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("transport", choices=("wifi", "usb"))
    parser.add_argument("target", help="Wi-Fi host or USB serial device")
    parser.add_argument("--port", type=int, default=81, help="WebSocket port")
    parser.add_argument("--count", type=int, default=200)
    arguments = parser.parse_args()
    result = (
        benchmark_wifi(arguments.target, arguments.port, arguments.count)
        if arguments.transport == "wifi"
        else benchmark_usb(arguments.target, arguments.count)
    )
    print(" ".join(f"{key}={value:.3f}" if isinstance(value, float) else f"{key}={value}"
                   for key, value in result.items()))


if __name__ == "__main__":
    main()
