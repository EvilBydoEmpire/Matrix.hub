"""The embedded thumbnails are current, follow the gallery for every panel size, and the local preview serves them."""
import io
import json
import pathlib
import sys
import types
import unittest
import zlib
from unittest.mock import patch

from PIL import Image

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import build_web  # noqa: E402
import preview_app  # noqa: E402
import render_previews  # noqa: E402


class AtlasTest(unittest.TestCase):
    def test_thumbnails_are_rendered_from_the_current_sources(self):
        self.assertEqual(render_previews.recorded_digest(), render_previews.source_digest(), 'Run tools/render_previews.py')

    def test_every_panel_size_has_a_row_per_gallery_entry(self):
        keys = render_previews.gallery()
        self.assertEqual(keys[:3], ['n0', 'n1', 'n3'])
        self.assertTrue(all(render_previews.flashing(k) for k in ('p65', 'p66', 'p67')) and not render_previews.flashing('n65'))
        header = render_previews.HEADER.read_text(encoding='utf-8')
        for name, panel, tile, budget, _, condition in render_previews.PROFILES:
            with self.subTest(profile=name):
                atlas = render_previews.atlas(panel)
                self.assertEqual(atlas['tile'], tile)
                self.assertLessEqual(len(atlas['png']), budget)
                self.assertEqual(atlas['key'], zlib.crc32(atlas['png']))
                self.assertIn(f'#if {condition}  // {name}', header)
                with Image.open(io.BytesIO(atlas['png'])) as image:
                    self.assertEqual(image.size, (tile * render_previews.FRAMES, tile * len(keys)))

    def test_encoding_quantises_until_the_budget_fits(self):
        image = Image.new('RGB', (96, 96))
        image.putdata([(x * 2, y * 2, x * y % 256) for y in range(96) for x in range(96)])
        data, colors, error = render_previews.encode(image, 4000)
        self.assertLessEqual(len(data), 4000)
        self.assertEqual(colors, 160)
        self.assertGreater(error, 0)
        with self.assertRaises(ValueError):
            render_previews.encode(image, 1000)


class LocalPreviewTest(unittest.TestCase):
    def setUp(self):
        snapshot = {'stateRevision': 1, 'currentPattern': 0, 'displayWidth': 16, 'params': {'palette': 3, 'original': ['#444444'] * 4}}
        for name, value in (('state', snapshot), ('diagnostics', {'stateRevision': 1})):
            patcher = patch.object(preview_app, name, value)
            patcher.start()
            self.addCleanup(patcher.stop)

    def request(self, method, path, body=None):
        replies = []
        raw = json.dumps(body).encode()
        handler = types.SimpleNamespace(path=path, headers={'Content-Length': str(len(raw))}, rfile=io.BytesIO(raw),
                                        send=lambda data, kind='application/json': replies.append((data, kind)),
                                        send_error=replies.append)
        getattr(preview_app.Handler, 'do_' + method)(handler)
        return replies[0]

    def test_query_strings_reach_their_routes(self):
        self.assertEqual(self.request('GET', '/?view=device'), (build_web.bundle('index.html').encode(), 'text/html; charset=utf-8'))
        self.assertEqual(self.request('GET', '/preview-atlas.png?v=1'), (render_previews.atlas(16)['png'], 'image/png'))
        self.assertEqual(self.request('GET', '/control?fresh=1')[0], preview_app.state)

    def test_palettes_recolour_and_unknown_ones_fail_clearly(self):
        reply, _ = self.request('POST', '/control', {'command': 'setParam', 'paramName': 'palette', 'value': 999})
        self.assertEqual(reply, {'success': False, 'message': 'Palette 999 does not exist'})
        self.assertEqual(preview_app.state['params']['palette'], 3)
        reply, _ = self.request('POST', '/control', {'command': 'setParam', 'paramName': 'palette', 'value': 0})
        self.assertTrue(reply['success'])
        self.assertEqual([preview_app.state['params'][f'color{i}'] for i in range(4)], preview_app.PALETTES[0])
        self.request('POST', '/control', {'command': 'setParam', 'paramName': 'palette', 'value': 7})
        self.assertEqual(preview_app.state['params']['color0'], '#444444', 'Original shows the authored colours')


if __name__ == '__main__':
    unittest.main()
