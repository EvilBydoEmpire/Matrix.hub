#!/usr/bin/env python3
"""Inspect a verified matrix, or build its firmware and install it over Wi-Fi."""
import argparse
import http.client
import json
import logging
import re
import shutil
import subprocess
import time
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PROFILES = {
    '16': ('esp32-wroom-ws2812-16x16', 'esp32_wroom_ws2812_16x16_ota'),
    '22': ('esp32-wroom-ws2812-22x22', 'esp32_wroom_ws2812_22x22_ota'),
    '64': ('matrixportal-s3-hub75-64x64', 'matrixportal_s3_hub75_64x64_ota'),
}
log = logging.getLogger('matrix_hub')


def read_device(host):
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
    result = []
    for path in ('control', 'diagnostics'):
        with opener.open('http://' + host + '/' + path, timeout=10) as response:
            result.append(json.load(response))
    return result


def validate(profile, state, diagnostics):
    expected, _ = PROFILES[profile]
    if state.get('deviceProfile') != expected:
        raise RuntimeError(f"Wrong device: expected {expected}, found {state.get('deviceProfile')}")
    if state.get('displayWidth') != int(profile) or state.get('displayHeight') != int(profile):
        raise RuntimeError('The panel size does not match the selected profile')
    if profile == '64' and not str(state.get('backend', '')).startswith('hub75-dma'):
        raise RuntimeError('Matrix.64 runs a different display driver; this firmware would replace it')
    if not diagnostics.get('otaReady'):
        raise RuntimeError('The device does not report its OTA receiver ready')


def commit():
    """The commit a build of this checkout stamps, as tools/pio_prebuild.py does."""
    return subprocess.run(['git', 'describe', '--always', '--dirty', '--abbrev=7'], cwd=ROOT,
                          capture_output=True, text=True).stdout.strip()


def confirm_firmware(state, built):
    """A device that restarted on other firmware rejected the update or rolled it back."""
    if built and state.get('firmware') != built:
        raise RuntimeError(f"The device restarted on firmware {state.get('firmware')!r}, not the {built} just built")


def restarted(uptime_before_ms, elapsed_s, uptime_after_ms):
    """A restarted device counts its uptime from zero again."""
    return uptime_after_ms < uptime_before_ms + elapsed_s * 1000 - 5000


def await_restart(profile, host, uptime_before_ms, read_at):
    """Poll until the device answers from a fresh boot; a restarting device refuses, drops or truncates replies."""
    for _ in range(30):
        time.sleep(2)
        try:
            state, diagnostics = read_device(host)
            validate(profile, state, diagnostics)
            if restarted(uptime_before_ms, time.monotonic() - read_at, diagnostics['uptimeMs']):
                return state
        except (OSError, ValueError, KeyError, RuntimeError, http.client.HTTPException):
            pass
    raise RuntimeError('The upload finished but the device did not come back; check the router or USB, and never erase flash')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('inspect', 'update'))
    parser.add_argument('--host', required=True, help='Verified local IP or hostname, without http://')
    parser.add_argument('--profile', choices=PROFILES, required=True)
    args = parser.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9.-]*', args.host):
        parser.error('Use a plain local hostname or IPv4 address')
    state, diagnostics = read_device(args.host)
    validate(args.profile, state, diagnostics)
    log.info('%s', json.dumps({k: state.get(k) for k in ('deviceName', 'deviceProfile', 'hostname', 'ip', 'backend', 'firmware')}, indent=2))
    log.info('OTA ready')
    if args.action == 'inspect':
        return
    pio = ROOT / '.venv/bin/pio'
    executable = str(pio) if pio.exists() else shutil.which('pio')
    if not executable:
        raise RuntimeError('PlatformIO is missing; set it up as README.md describes')
    env = PROFILES[args.profile][1]
    # The OTA environments want their target even to build, or PlatformIO prints a spurious error.
    subprocess.run([executable, 'run', '-e', env, '--upload-port', args.host], cwd=ROOT, check=True)
    state, diagnostics = read_device(args.host)
    validate(args.profile, state, diagnostics)
    read_at = time.monotonic()
    subprocess.run([executable, 'run', '-e', env, '-t', 'upload', '--upload-port', args.host], cwd=ROOT, check=True)
    host = state.get('ip') or args.host
    final = await_restart(args.profile, host, diagnostics['uptimeMs'], read_at)
    confirm_firmware(final, commit())
    log.info('Updated %s to %s at http://%s/, OTA ready', final['deviceName'], final.get('firmware'), host)


if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO, format='%(message)s')
    main()
