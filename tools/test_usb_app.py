import os
import pathlib
import sys
import types
import unittest
from unittest.mock import patch

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import open_usb_app as bridge  # noqa: E402
import build_web  # noqa: E402


class LineRecorder:
    """Stands in for serial.Serial and records modem-line changes in order."""
    events: list
    lines: list

    def __init__(self, events, lines=()):
        object.__setattr__(self, 'events', events)
        object.__setattr__(self, 'lines', list(lines))

    def __setattr__(self, name, value):
        if name in ('dtr', 'rts'):
            self.events.append((name, value))
        object.__setattr__(self, name, value)

    def open(self):
        self.events.append(('open', None))

    def close(self):
        self.events.append(('close', None))

    def readline(self):
        return self.lines.pop(0) if self.lines else b''


class SerialLinesTest(unittest.TestCase):
    def test_open_pulses_reset_with_the_boot_strap_high(self):
        events = []
        boot = [b'Matrix.hub on Matrix.16\n', b'HTTP server started\n']
        with patch.object(bridge.serial, 'Serial', lambda: LineRecorder(events, boot)), \
                patch.object(bridge, 'matrix_port', return_value='/dev/cu.test'), patch.object(bridge.time, 'sleep'):
            device = bridge.open_serial()
        self.assertEqual(events, [('dtr', False), ('rts', True), ('open', None), ('rts', False)])
        self.assertEqual(device.lines, [], 'Requests wait until the firmware has started')

    def test_close_lowers_both_lines_first(self):
        events = []
        with patch.object(bridge, 'SERIAL_DEVICE', LineRecorder(events)):
            bridge.close_serial()
            self.assertIsNone(bridge.SERIAL_DEVICE)
        self.assertEqual(events, [('dtr', False), ('rts', False), ('close', None)])


class PortDiscoveryTest(unittest.TestCase):
    def port(self, device, vid=None):
        return types.SimpleNamespace(device=device, vid=vid)

    def discover(self, ports, explicit=None):
        environment = {'MATRIX_SERIAL_PORT': explicit} if explicit else {}
        with patch.dict(os.environ, environment, clear=True), patch.object(bridge.list_ports, 'comports', return_value=ports):
            return bridge.matrix_port()

    def test_the_one_usb_port_is_chosen(self):
        for device in ('/dev/cu.usbserial-0001', '/dev/ttyACM0', 'COM5'):
            with self.subTest(device=device):
                found = self.discover([self.port('/dev/cu.Bluetooth-Incoming-Port'), self.port(device, 0x10C4)])
                self.assertEqual(found, device)

    def test_no_or_several_usb_ports_ask_for_a_choice(self):
        for ports in ([self.port('/dev/cu.debug-console')], [self.port('/dev/ttyUSB0', 0x10C4), self.port('/dev/ttyACM0', 0x303A)]):
            with self.subTest(ports=ports), self.assertRaisesRegex(RuntimeError, 'MATRIX_SERIAL_PORT'):
                self.discover(ports)

    def test_an_explicit_port_wins(self):
        self.assertEqual(self.discover([], explicit='/dev/ttyS3'), '/dev/ttyS3')


class BridgeRoutesTest(unittest.TestCase):
    def test_root_serves_the_web_bundle(self):
        sent = []
        handler = types.SimpleNamespace(path='/', send_bytes=lambda status, kind, data: sent.append((status, kind, data)))
        bridge.AppHandler.do_GET(handler)
        self.assertEqual(sent, [(200, 'text/html; charset=utf-8', build_web.bundle('index.html').encode('utf-8'))])


class WifiResetTest(unittest.TestCase):
    def reset_response(self, response):
        replies = []
        handler = types.SimpleNamespace(
            path='/wifi/reset', headers={'Content-Type': 'application/json'}, read_json=lambda: {},
            send_json=lambda status, body: replies.append((status, body)))
        with patch.object(bridge, 'serial_request', return_value=response) as request:
            bridge.AppHandler.do_POST(handler)
            request.assert_called_once_with('WIFI,RESET', 'WIFI,RESET,', 8.0)
        return replies[0]

    def test_confirmed_reset(self):
        status, body = self.reset_response('restarting')
        self.assertEqual(status, 200)
        self.assertTrue(body['success'])

    def test_failed_or_unknown_reset_is_not_acknowledged(self):
        for response in ['failed', 'unknown', '']:
            with self.subTest(response=response):
                status, body = self.reset_response(response)
                self.assertEqual(status, 503)
                self.assertFalse(body['success'])


class JsonOnlyWritesTest(unittest.TestCase):
    def test_other_content_types_never_reach_the_matrix(self):
        for path in ['/control', '/wifi/add', '/wifi/reset']:
            for headers in [{}, {'Content-Type': 'text/plain'}, {'Content-Type': 'application/x-www-form-urlencoded'}]:
                with self.subTest(path=path, headers=headers):
                    replies = []
                    handler = types.SimpleNamespace(
                        path=path, headers=headers, read_json=lambda: {},
                        send_json=lambda status, body: replies.append((status, body)))
                    with patch.object(bridge, 'serial_request') as request:
                        bridge.AppHandler.do_POST(handler)
                        request.assert_not_called()
                    self.assertEqual(replies[0][0], 415)


if __name__ == '__main__':
    unittest.main()
