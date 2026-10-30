"""The embedded pages match web/, stay self-contained and fit their budgets."""
import gzip
import pathlib
import re
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import build_web  # noqa: E402


def embedded(symbol: str) -> bytes:
    body = build_web.HEADER.read_text(encoding='utf-8').split(f'{symbol}_HTML_GZ[] PROGMEM = {{', 1)[1].split('};', 1)[0]
    return bytes(int(value, 16) for value in re.findall(r'0x([0-9A-F]{2})', body))


class WebBuildTest(unittest.TestCase):
    def test_generated_files_are_current(self):
        self.assertEqual(build_web.stale(), [], 'Run tools/build_web.py')

    def test_embedded_pages_decompress_to_the_bundles(self):
        header = build_web.HEADER.read_text(encoding='utf-8')
        for symbol, page, budget in build_web.PAGES:
            with self.subTest(page=page):
                packed = embedded(symbol)
                self.assertLessEqual(len(packed), budget)
                self.assertIn(f'{symbol}_HTML_GZ_SIZE = {len(packed)};', header)
                self.assertEqual(gzip.decompress(packed).decode('utf-8'), build_web.bundle(page))

    def test_bundles_are_self_contained(self):
        for _, page, _ in build_web.PAGES:
            with self.subTest(page=page):
                self.assertNotRegex(build_web.bundle(page), r'<script src=|<link rel="stylesheet"|(src|href)="(https?:)?//')
        app = build_web.bundle('index.html')
        for name in build_web.SCRIPT.findall((build_web.WEB / 'index.html').read_text(encoding='utf-8')):
            self.assertIn((build_web.WEB / name).read_text(encoding='utf-8'), app)

    def test_palettes_follow_firmware_ids(self):
        text = build_web.palettes_js()
        self.assertIn('[0,"Ember",["#4C110A","#A63C0C","#F1AF57","#D76300"]]', text)
        self.assertTrue(text.rstrip().endswith('[7,"Original",null]];'), 'Original comes last')

    def test_a_script_cannot_end_its_own_tag(self):
        with tempfile.TemporaryDirectory() as directory:
            web = pathlib.Path(directory)
            (web / 'page.html').write_text('<script src="bad.js"></script>', encoding='utf-8')
            (web / 'bad.js').write_text("const tag='</script>';", encoding='utf-8')
            with patch.object(build_web, 'WEB', web), self.assertRaises(ValueError):
                build_web.bundle('page.html')


if __name__ == '__main__':
    unittest.main()
