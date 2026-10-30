#!/usr/bin/env python3
"""Preview the app from web/ locally. POSTs affect only a mock, never hardware."""
import argparse
import json
import logging
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse

import build_web
import render_previews

PALETTES = {palette_id: stops for palette_id, _, stops in build_web.palette_table()}
# The device snapshot that mocked writes edit.
state, diagnostics = {}, {}


class Handler(BaseHTTPRequestHandler):
    def send(self, data, kind='application/json'):
        if not isinstance(data, bytes):
            data = json.dumps(data).encode()
        self.send_response(200)
        self.send_header('Content-Type', kind)
        self.send_header('Cache-Control', 'no-store')
        self.send_header('Content-Length', str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        path = urlparse(self.path).path
        if path == '/control':
            self.send(state)
        elif path == '/diagnostics':
            self.send(diagnostics)
        elif path == '/preview-atlas.png':
            self.send(render_previews.atlas(state['displayWidth'])['png'], 'image/png')
        elif path in ('/', '/index.html'):
            self.send(build_web.bundle('index.html').encode(), 'text/html; charset=utf-8')
        elif path == '/setup':
            self.send(build_web.bundle('setup.html').encode(), 'text/html; charset=utf-8')
        else:
            self.send_error(404)

    def do_POST(self):
        if urlparse(self.path).path != '/control':
            self.send({'success': False, 'message': 'Endpoint is not simulated by this preview'})
            return
        command = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        name = command.get('command')
        if 'expectedPattern' in command and (command['expectedPattern'] != state['currentPattern'] or (state['currentPattern'] == 16 and command.get('expectedVariant') != state['params'].get('variant'))):
            self.send({'success': False, 'message': 'The pattern changed; that edit was not applied.'})
            return
        if name == 'setParam':
            if command['paramName'] == 'palette' and command['value'] != state['params'].get('palette'):
                if command['value'] not in PALETTES:
                    self.send({'success': False, 'message': f"Palette {command['value']} does not exist"})
                    return
                stops = PALETTES[command['value']] or state['params']['original']
                for i, stop in enumerate(stops):
                    state['params'][f'color{i}'] = stop
            state['params'][command['paramName']] = command['value']
        elif name in ('setPattern', 'setPatternVariant', 'resetPattern'):
            self.send({'success': False, 'message': 'This snapshot cannot load another pattern or its defaults'})
            return
        elif name == 'setBrightness':
            state['brightness'] = command['value']
        elif name == 'setAutoCycle':
            state['autoCycleEnabled'] = command['value']
        elif name == 'setCycleInterval':
            state['cycleInterval'] = command['value']
        elif name == 'setColorCalibration':
            state['calibration'].update({channel: command['value'][channel] for channel in 'rgb'})
        elif name == 'setTestChart':
            state['calibration']['chart'] = command['value']
        elif name == 'setSchedule':
            for target, source in [('scheduleEnabled', 'enabled'), ('scheduleStartMinutes', 'startMinutes'), ('scheduleEndMinutes', 'endMinutes'), ('scheduleFadeInSeconds', 'fadeInSeconds'), ('scheduleFadeOutSeconds', 'fadeOutSeconds')]:
                state[target] = command[source]
        else:
            self.send({'success': False, 'message': 'Command is not simulated by this preview'})
            return
        state['stateRevision'] += 1
        diagnostics['stateRevision'] = state['stateRevision']
        self.send({'success': True, 'message': 'Preview only'})

    def log_message(self, *unused):
        pass


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', required=True, help='Verified device hostname or IP, read once for a snapshot')
    parser.add_argument('--port', type=int, default=49386)
    args = parser.parse_args()
    # The matrix is on the local network, so environment proxies are bypassed, as tools/matrix_hub.py does.
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
    for snapshot, path in ((state, '/control'), (diagnostics, '/diagnostics')):
        with opener.open('http://' + args.host + path, timeout=10) as response:
            snapshot.update(json.load(response))
    # Thumbnails come from this checkout's atlas for the device's panel size.
    local = render_previews.atlas(state['displayWidth'])
    state['previewWidth'] = state['previewHeight'] = local['tile']
    state['previewAtlasKey'] = local['key']
    logging.info('Preview http://127.0.0.1:%s — writes are mocked', args.port)
    ThreadingHTTPServer(('127.0.0.1', args.port), Handler).serve_forever()


if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO, format='%(message)s')
    main()
