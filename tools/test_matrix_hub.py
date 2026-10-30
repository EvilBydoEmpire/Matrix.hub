import http.client
import importlib.util
import unittest
from pathlib import Path
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('hub', Path(__file__).with_name('matrix_hub.py'))
hub = importlib.util.module_from_spec(spec)
spec.loader.exec_module(hub)

class IdentityTests(unittest.TestCase):
    def test_profiles(self):
        for size, (profile, _) in hub.PROFILES.items():
            state = dict(deviceProfile=profile, displayWidth=int(size), displayHeight=int(size), backend='hub75-dma' if size == '64' else 'ws2812')
            hub.validate(size, state, {'otaReady': True})
            with self.assertRaises(RuntimeError): hub.validate(size, state, {'otaReady': False})
            with self.assertRaises(RuntimeError): hub.validate(size, dict(state, displayWidth=999), {'otaReady': True})
            with self.assertRaises(RuntimeError): hub.validate(size, dict(state, deviceProfile='other'), {'otaReady': True})
    def test_no_silent_driver_switch(self):
        with self.assertRaises(RuntimeError):
            hub.validate('64', dict(deviceProfile=hub.PROFILES['64'][0], displayWidth=64, displayHeight=64, backend='hub75-protomatter'), {'otaReady': True})
    def test_restart_detection(self):
        self.assertTrue(hub.restarted(600000, 45, 3000))
        self.assertTrue(hub.restarted(2000, 45, 9000), 'A freshly booted device still shows its restart')
        self.assertFalse(hub.restarted(600000, 45, 645200), 'Uptime that kept counting means no restart')
    def test_restart_polling_rides_out_broken_replies(self):
        state = dict(deviceProfile=hub.PROFILES['16'][0], displayWidth=16, displayHeight=16, backend='ws2812')
        replies = [ConnectionRefusedError(), http.client.IncompleteRead(b'{'), http.client.RemoteDisconnected(), ValueError(), (state, {'otaReady': True, 'uptimeMs': 2000})]
        def read_device(host):
            reply = replies.pop(0)
            if isinstance(reply, Exception): raise reply
            return reply
        with patch.object(hub, 'read_device', read_device), patch.object(hub.time, 'sleep'):
            self.assertIs(hub.await_restart('16', 'matrix', 600000, hub.time.monotonic()), state)
        self.assertEqual(replies, [])
    def test_update_confirms_the_firmware_it_built(self):
        hub.confirm_firmware({'firmware': '0e02abc'}, '0e02abc')
        hub.confirm_firmware({}, '')
        for reported in ({'firmware': '8c11b50'}, {}):
            with self.assertRaises(RuntimeError): hub.confirm_firmware(reported, '0e02abc')
    def test_restart_polling_gives_up(self):
        with patch.object(hub, 'read_device', side_effect=http.client.IncompleteRead(b'')), patch.object(hub.time, 'sleep'), self.assertRaises(RuntimeError):
            hub.await_restart('16', 'matrix', 600000, hub.time.monotonic())

if __name__ == '__main__': unittest.main()
