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
        self.assertEqual(command_from({'type': 'key', 'name': 'F4',
                                       'ctrl': False, 'shift': False}), 'KEY F4 0 0')
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
            native.input('CLICK 80 251')  # fin set in the assembly tree
            second = native.render()
            self.assertNotEqual(first, second)
        finally:
            native.close()

    def test_recorded_csv_upload_routes_to_native_station(self):
        file = Path(__file__).resolve().parents[1] / 'out/recording.csv'
        previous = file.read_bytes() if file.exists() else None
        native = Native()
        Handler.native = native
        server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        address = f'http://127.0.0.1:{server.server_port}'
        try:
            with urllib.request.urlopen(address + '/frame') as response:
                before = response.read()
            header = 'time_s,altitude_m,gx_deg_s,gy_deg_s,gz_deg_s\n'
            content = header + ''.join(f'{i/10:.1f},100.0,0.1,0,0\n' for i in range(22))
            upload = urllib.request.Request(address + '/recording',
                                            data=content.encode('ascii'),
                                            headers={'Content-Type': 'text/csv'})
            with urllib.request.urlopen(upload) as response:
                self.assertEqual(response.status, 204)
            self.assertEqual(file.read_text(), content)
            load = urllib.request.Request(address + '/input',
                                          data=json.dumps({'type': 'key', 'name': 'l',
                                                           'ctrl': True, 'shift': False}).encode(),
                                          headers={'Content-Type': 'application/json'})
            with urllib.request.urlopen(load) as response:
                self.assertEqual(response.status, 204)
            with urllib.request.urlopen(address + '/frame') as response:
                loaded = response.read()
            self.assertNotEqual(before, loaded)
            click = urllib.request.Request(address + '/input',
                                           data=json.dumps({'type': 'click', 'x': 1000,
                                                            'y': 650}).encode(),
                                           headers={'Content-Type': 'application/json'})
            urllib.request.urlopen(click).close()  # calibrate in the native inspector
            with urllib.request.urlopen(address + '/frame') as response:
                self.assertNotEqual(loaded, response.read())
            bad = urllib.request.Request(address + '/recording',
                data=(header + 'not,a,valid,number,row\n').encode('ascii'),
                headers={'Content-Type': 'text/csv'})
            with self.assertRaises(urllib.error.HTTPError) as caught:
                urllib.request.urlopen(bad)
            self.assertEqual(caught.exception.code, 400)
            self.assertEqual(file.read_text(), content)
        finally:
            server.shutdown()
            thread.join(timeout=4)
            server.server_close()
            native.close()
            if previous is None:
                file.unlink(missing_ok=True)
            else:
                file.write_bytes(previous)

    def test_visual_mesh_upload_and_export(self):
        root = Path(__file__).resolve().parents[1] / 'out'
        paths = (root / 'reference.obj', root / 'assembly.obj')
        original = [p.read_bytes() if p.exists() else None for p in paths]
        native = Native()
        Handler.native = native
        server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        address = f'http://127.0.0.1:{server.server_port}'
        try:
            first = urllib.request.urlopen(address + '/frame').read()
            body = b'v 0 0 0\nv 20 0 0\nv 0 20 0\nf 1 2 3\n'
            upload = urllib.request.Request(address + '/mesh/obj', data=body,
                headers={'Content-Type': 'application/octet-stream'})
            self.assertEqual(urllib.request.urlopen(upload).status, 204)
            self.assertEqual(paths[0].read_bytes(), body)
            def press(letter):
                request = urllib.request.Request(address + '/input', data=json.dumps({
                    'type': 'key', 'name': letter, 'ctrl': True,
                    'shift': False}).encode(), headers={'Content-Type': 'application/json'})
                with urllib.request.urlopen(request) as response:
                    self.assertEqual(response.status, 204)
            press('m')
            self.assertNotEqual(first, urllib.request.urlopen(address + '/frame').read())
            with self.assertRaises(urllib.error.HTTPError) as stale:
                urllib.request.urlopen(address + '/assembly.obj')
            self.assertEqual(stale.exception.code, 404)
            press('e')
            with urllib.request.urlopen(address + '/assembly.obj') as response:
                self.assertEqual(response.status, 200)
                self.assertIn(b'Not watertight', response.read())
            bad = urllib.request.Request(address + '/mesh/obj',
                data=b'v 0 0 0\nf 1 2 3\n',
                headers={'Content-Type': 'application/octet-stream'})
            with self.assertRaises(urllib.error.HTTPError) as caught:
                urllib.request.urlopen(bad)
            self.assertEqual(caught.exception.code, 400)
            self.assertEqual(paths[0].read_bytes(), body)
        finally:
            server.shutdown()
            thread.join(timeout=4)
            server.server_close()
            native.close()
            for path, data in zip(paths, original):
                if data is None:
                    path.unlink(missing_ok=True)
                else:
                    path.write_bytes(data)

    def test_binary_stl_upload_is_checked_by_native_parser(self):
        file = Path(__file__).resolve().parents[1] / 'out/reference.stl'
        previous = file.read_bytes() if file.exists() else None
        native = Native()
        Handler.native = native
        server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        address = f'http://127.0.0.1:{server.server_port}'
        try:
            header = b'EuloLab binary STL' + bytes(80 - len('EuloLab binary STL'))
            triangle = struct.pack('<12fH',
                0, 0, 1, 0, 0, 0, 20, 0, 0, 0, 20, 0, 0)
            body = header + struct.pack('<I', 1) + triangle
            request = urllib.request.Request(address + '/mesh/stl', data=body,
                headers={'Content-Type': 'application/octet-stream'})
            self.assertEqual(urllib.request.urlopen(request).status, 204)
            self.assertEqual(file.read_bytes(), body)
            bad = urllib.request.Request(address + '/mesh/stl', data=body[:-2],
                headers={'Content-Type': 'application/octet-stream'})
            with self.assertRaises(urllib.error.HTTPError) as caught:
                urllib.request.urlopen(bad)
            self.assertEqual(caught.exception.code, 400)
            self.assertEqual(file.read_bytes(), body)
            event = urllib.request.Request(address + '/input', data=json.dumps({
                'type': 'key', 'name': 'm', 'ctrl': True, 'shift': True}).encode(),
                headers={'Content-Type': 'application/json'})
            self.assertEqual(urllib.request.urlopen(event).status, 204)
            self.assertEqual(urllib.request.urlopen(address + '/frame').status, 200)
        finally:
            server.shutdown()
            thread.join(timeout=4)
            server.server_close()
            native.close()
            if previous is None:
                file.unlink(missing_ok=True)
            else:
                file.write_bytes(previous)

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
                self.assertEqual(response.headers['X-EuloLab-Page'], '0')
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
                self.assertEqual(response.headers['X-EuloLab-Page'], '1')
            bench = urllib.request.Request(address + '/input',
                data=json.dumps({'type': 'click', 'x': 892, 'y': 85}).encode(),
                headers={'Content-Type': 'application/json'})
            self.assertEqual(urllib.request.urlopen(bench).status, 204)
            with urllib.request.urlopen(address + '/frame') as response:
                self.assertEqual(response.headers['X-EuloLab-Page'], '3')
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
