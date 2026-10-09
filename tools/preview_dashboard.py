#!/usr/bin/env python3
"""Preview the embedded dashboard locally. POSTs affect only a mock, never hardware."""
import argparse
import copy
import json
import pathlib
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ROOT = pathlib.Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--device', required=True, help='Device IP for a read-only initial snapshot')
parser.add_argument('--port', type=int, default=8766)
args = parser.parse_args()
with urllib.request.urlopen('http://' + args.device + '/control', timeout=10) as response:
    state = json.load(response)
with urllib.request.urlopen('http://' + args.device + '/diagnostics', timeout=10) as response:
    diagnostics = json.load(response)
original = copy.deepcopy(state)
commands = []
library = {'success': True, 'readable': True, 'revision': 0,
           'folders': [], 'families': [], 'tags': [], 'entries': []}
try:
    with urllib.request.urlopen('http://' + args.device + '/library', timeout=3) as response:
        library = json.load(response)
except Exception:
    pass  # Preview a new dashboard against the previous firmware.
state['libraryRevision'] = library['revision']


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
        if self.path == '/control':
            self.send(state)
        elif self.path == '/diagnostics':
            self.send(diagnostics)
        elif self.path == '/library':
            self.send(library)
        elif self.path == '/test-commands':
            self.send(commands)
        elif self.path.split('?',1)[0] == '/preview-atlas.png':
            self.send((ROOT / 'assets/pattern-preview-atlas.png').read_bytes(), 'image/png')
        elif self.path in ('/', '/index.html'):
            source = (ROOT / '64x64_matrix_01.ino').read_text()
            html = source.split('const char DASHBOARD_HTML[] PROGMEM = R"HTML(', 1)[1].split(')HTML";', 1)[0]
            self.send(html.encode(), 'text/html; charset=utf-8')
        else:
            self.send_error(404)

    def do_POST(self):
        command = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        commands.append(command)
        name = command.get('command')
        if name == 'setParam':
            state['params'][command['paramName']] = command['value']
        elif name == 'setBookmark':
            value = command['value']
            state['bookmarks'][value['index']] = value['enabled']
        elif name in ('setPattern', 'setPatternVariant'):
            state['currentPattern'] = 16 if name == 'setPatternVariant' else command['value']
            state['params'] = copy.deepcopy(original['params'])
            for key in ('c0', 'c1', 'c2', 'c3'):
                state['params'][key] = 50
            if name == 'setPatternVariant':
                state['params']['variant'] = command['value']
        elif name == 'setPatternQuality':
            state['patternQuality'] = command['value']
        elif name == 'setFPS':
            state['targetFPS'] = command['value']
        elif name == 'setAutoCycle':
            state['autoCycleEnabled'] = command['value']
        elif name == 'setLibraryMetadata':
            value = command['value']
            old = next((row for row in library['entries'] if row[0] == value['id']), [])
            archived = bool(old[5]) if len(old) > 5 else False
            def intern(key, text):
                if not text: return 0
                if text not in library[key]: library[key].append(text)
                return library[key].index(text) + 1
            folder = intern('folders', value['folder'])
            family = intern('families', value['family'])
            tags = sum(1 << (intern('tags', tag) - 1) for tag in value['tags'])
            library['entries'] = [row for row in library['entries'] if row[0] != value['id']]
            library['entries'].append([value['id'], value['rating'], folder, family, tags, archived])
            library['revision'] += 1
            state['libraryRevision'] = library['revision']
        elif name == 'setPatternArchived':
            value = command['value']
            row = next((r for r in library['entries'] if r[0] == value['id']), None)
            if row is None:
                row = [value['id'], 0, 0, 0, 0, False]
                library['entries'].append(row)
            while len(row) < 6: row.append(False)
            row[5] = value['archived']
            library['revision'] += 1
            state['libraryRevision'] = library['revision']
        self.send({'success': True, 'message': 'Preview only / hardware unchanged'})

    def log_message(self, *unused):
        pass


print(f'Preview http://127.0.0.1:{args.port} — writes are mocked, hardware unchanged', flush=True)
ThreadingHTTPServer(('127.0.0.1', args.port), Handler).serve_forever()
