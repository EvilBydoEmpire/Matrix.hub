#!/usr/bin/env python3
"""Inspect a selected Matrix.hub device or build/upload its matching Wi-Fi image."""
import argparse
import json
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
    '64': ('matrixportal-s3-hub75-64x64', 'matrixportal_s3_hub75_dma_4bit_ota'),
}

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
        raise RuntimeError('Wrong target profile: expected ' + expected + ', found ' + str(state.get('deviceProfile')))
    if state.get('displayWidth') != int(profile) or state.get('displayHeight') != int(profile):
        raise RuntimeError('Device dimensions do not match the selected profile')
    if profile == '64' and not str(state.get('backend', '')).startswith('hub75-dma'):
        raise RuntimeError('Refusing a silent display-driver change: Matrix.64 updater targets the verified DMA backend')
    if not diagnostics.get('otaReady'):
        raise RuntimeError('Device does not report its OTA receiver ready')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('inspect', 'update'))
    parser.add_argument('--host', required=True, help='Verified local IP or hostname; no http:// prefix')
    parser.add_argument('--profile', choices=PROFILES, required=True)
    args = parser.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9.-]*', args.host):
        parser.error('Use a plain local hostname or IPv4 address')
    state, diagnostics = read_device(args.host)
    validate(args.profile, state, diagnostics)
    print(json.dumps({k: state.get(k) for k in ('deviceName', 'deviceProfile', 'hostname', 'ip', 'backend')}, indent=2))
    print('OTA ready:', diagnostics['otaReady'])
    if args.action == 'inspect':
        return
    pio = ROOT / '.venv/bin/pio'
    executable = str(pio) if pio.exists() else shutil.which('pio')
    if not executable:
        raise RuntimeError('PlatformIO missing. Follow gift/INSTALL_WITH_AI.txt first.')
    env = PROFILES[args.profile][1]
    subprocess.run([executable, 'run', '-e', env], cwd=ROOT, check=True)
    validate(args.profile, *read_device(args.host))
    subprocess.run([executable, 'run', '-e', env, '-t', 'upload', '--upload-port', args.host], cwd=ROOT, check=True)
    host = state.get('ip') or args.host
    for _ in range(30):
        time.sleep(2)
        try:
            final, diag = read_device(host)
            validate(args.profile, final, diag)
            if final.get('deviceName') == 'Matrix.' + args.profile and final.get('projectName') == 'Matrix.hub':
                print('Verified updated device:', final['deviceName'], 'http://' + host + '/', 'OTA ready')
                return
        except (OSError, ValueError, RuntimeError):
            pass
    raise RuntimeError('Upload completed but reboot verification failed. Check router/DHCP or USB; do not erase flash.')

if __name__ == '__main__':
    main()
