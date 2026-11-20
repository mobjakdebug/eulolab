"""Exercise the thin preview transport against the *actual* compiled C++ app."""
import json
from pathlib import Path
import struct
import sys
import threading
import unittest
import urllib.error
import urllib.request
import zlib
from http.server import ThreadingHTTPServer

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.host_preview import Handler, Native, WIDTH, HEIGHT, command_from  # noqa: E402


class HostPreviewTest(unittest.TestCase):
    def test_whitelist_and_png_roundtrip(self):
        self.assertEqual(command_from({'type': 'click', 'x': 12, 'y': 88}), 'CLICK 12 88')
        self.assertEqual(command_from({'type': 'hover', 'x': -1, 'y': -1}), 'HOVER -1 -1')
        self.assertEqual(command_from({'type': 'key', 'name': 'ArrowRight',
                                       'ctrl': False, 'shift': True}), 'KEY ArrowRight 0 1')
        for payload in (
            {'type': 'click', 'x': True, 'y': 88},
            {'type': 'click', 'x': 1280, 'y': 88},
            {'type': 'drag', 'x': 99999, 'y': 0},
            {'type': 'key', 'name': 'Enter\nFRAME', 'ctrl': False, 'shift': False},
            {'type': 'key', 'name': 's', 'ctrl': 'false', 'shift': False},
            {'type': 'unknown'},
        ):
            with self.subTest(payload=payload), self.assertRaises(ValueError):
                command_from(payload)
        native = Native()
        try:
            first = native.render()
            self.assertEqual(first[:8], b'\x89PNG\r\n\x1a\n')
            self.assertEqual(struct.unpack('>II', first[16:24]), (WIDTH, HEIGHT))
            idat_len = struct.unpack('>I', first[33:37])[0]
            rows = zlib.decompress(first[41:41 + idat_len])
            self.assertEqual(len(rows), (WIDTH*4 + 1)*HEIGHT)
            native.input('CLICK 80 251')  # nose cone in the assembly tree
            second = native.render()
            self.assertNotEqual(first, second)
        finally:
            native.close()

    def test_http_events_drive_native_pixels(self):
        native = Native()
        Handler.native = native
        server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        address = f'http://127.0.0.1:{server.server_port}'
        try:
            with urllib.request.urlopen(address + '/health') as response:
                self.assertEqual(response.status, 200)
            with urllib.request.urlopen(address + '/frame') as response:
                first = response.read()
                self.assertEqual(response.headers['Content-Type'], 'image/png')
            page = urllib.request.urlopen(address).read()
            self.assertIn(b'canvas', page)
            self.assertIn(b'Connecting to the native renderer', page)
            self.assertIn(b'F2 leaves the canvas', page)
            request = urllib.request.Request(
                address + '/input',
                data=json.dumps({'type': 'click', 'x': 388, 'y': 85}).encode(),
                headers={'Content-Type': 'application/json'},
            )
            with urllib.request.urlopen(request) as response:
                self.assertEqual(response.status, 204)
            with urllib.request.urlopen(address + '/frame') as response:
                self.assertNotEqual(first, response.read())
            bad = urllib.request.Request(address + '/input', data=b'{"type":"evil"}',
                                         headers={'Content-Type': 'application/json'})
            with self.assertRaises(urllib.error.HTTPError) as caught:
                urllib.request.urlopen(bad)
            self.assertEqual(caught.exception.code, 400)
        finally:
            server.shutdown()
            thread.join(timeout=4)
            server.server_close()
            native.close()


if __name__ == '__main__':
    unittest.main()
