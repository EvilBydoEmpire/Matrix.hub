import importlib.util
import unittest
from pathlib import Path

spec = importlib.util.spec_from_file_location('hub', Path(__file__).with_name('matrix_hub.py'))
hub = importlib.util.module_from_spec(spec)
spec.loader.exec_module(hub)

class IdentityTests(unittest.TestCase):
    def test_profiles(self):
        for size, (profile, _) in hub.PROFILES.items():
            state = dict(deviceProfile=profile, displayWidth=int(size), displayHeight=int(size), backend='hub75-dma-exact20-test' if size == '64' else 'ws2812')
            hub.validate(size, state, {'otaReady': True})
            with self.assertRaises(RuntimeError): hub.validate(size, state, {'otaReady': False})
            with self.assertRaises(RuntimeError): hub.validate(size, dict(state, displayWidth=999), {'otaReady': True})
            with self.assertRaises(RuntimeError): hub.validate(size, dict(state, deviceProfile='other'), {'otaReady': True})
    def test_no_silent_driver_switch(self):
        with self.assertRaises(RuntimeError):
            hub.validate('64', dict(deviceProfile=hub.PROFILES['64'][0], displayWidth=64, displayHeight=64, backend='hub75-protomatter'), {'otaReady': True})

if __name__ == '__main__': unittest.main()
