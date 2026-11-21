#!/usr/bin/env python3
"""Interactive browser transport for the *native* C++ host renderer.

The browser contains a framebuffer canvas and sends pointer/keyboard events.
It does not implement the UI or physics. Every pixel comes from App::draw in
out/eulolab-host. The PS4 build has no HTTP or browser dependency.
"""
from __future__ import annotations

import argparse
import atexit
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import threading
import time
import zlib

ROOT = Path(__file__).resolve().parents[1]
WIDTH, HEIGHT = 1280, 720
RAW_SIZE = WIDTH * HEIGHT * 4

PAGE = b'''<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>EuloLab | Native host preview</title>
<style>
  html { background: #e9eeed; color: #27343b; font: 16px system-ui, sans-serif; }
  body { margin: 0; min-height: 100vh; display: grid; place-items: center; }
  main { width: min(1280px, 100%); padding: 12px; box-sizing: border-box; }
  canvas { width: 100%; height: auto; display: block; aspect-ratio: 16 / 9;
           background: #f2f4f3; touch-action: none; cursor: default; }
  canvas:focus-visible { outline: 3px solid #235b93; outline-offset: 2px; }
  .assist { font-size: .875rem; line-height: 1.5; margin: .7rem 0 0;
            display: flex; justify-content: space-between; gap: 1rem; }
  .assist p { margin: 0; }
  .import { display: flex; align-items: center; flex-wrap: wrap; gap: .4rem 1rem;
            margin-top: .65rem; font-size: .875rem; }
  .import small { font-size: .8rem; }
  .assist button, .import button { border: 1px solid #a4b4b9; background: #fff;
                   color: #27343b; font: inherit; padding: .35rem .65rem; cursor: pointer; }
  .assist button:hover, .assist button:focus-visible,
  .import button:hover, .import button:focus-visible { outline: 2px solid #235b93; }
  .assist button:active, .import button:active { background: #deebf4; }
  .sr-only { position: absolute; width: 1px; height: 1px; padding: 0; margin: -1px;
             overflow: hidden; clip: rect(0, 0, 0, 0); white-space: nowrap; border: 0; }
  nav.sr-only:focus-within { position: fixed; top: 0; left: 0; width: auto; height: auto;
                            margin: 0; overflow: visible; clip: auto; background: #fff;
                            z-index: 1; padding: .5rem; }
</style>
</head>
<body>
<header class="sr-only"><h1>EuloLab native host preview</h1></header>
<nav class="sr-only" aria-label="Skip link"><a href="#workbench">Skip to workbench</a></nav>
<main aria-label="Interactive native application preview">
  <canvas id="workbench" width="1280" height="720" tabindex="0"
    role="application" aria-label="EuloLab native Rocket Studio workbench"
    aria-describedby="instructions">Your browser cannot render this preview canvas.</canvas>
  <div class="assist">
    <p id="instructions">Host preview, not PS4. Click to focus; Tab moves between native controls.
      F2 leaves the canvas. F1 shows the native controls guide.
      <span id="previewStatus" role="status">Connecting to the native renderer...</span></p>
    <button id="focus" type="button">Focus workbench</button>
  </div>
  <div class="import">
    <label for="recording">Open recorded sensor CSV:</label>
    <input id="recording" type="file" accept=".csv,text/csv" aria-label="Import recorded sensor CSV">
    <small>Copied to this sandbox for offline playback; not a live device.</small>
  </div>
  <div class="import">
    <label for="fins">Import operator fin readings:</label>
    <input id="fins" type="file" accept=".csv,text/csv" aria-label="Import fin inspection CSV">
    <small>Separate from design and flight model; not hardware-verified.</small>
  </div>
  <div class="import">
    <label for="mesh">Import visual reference:</label>
    <input id="mesh" type="file" accept=".obj,.stl" aria-label="Import OBJ or STL reference">
    <button id="download" type="button">Download visual OBJ</button>
    <small>File &gt; Export visual OBJ first. Meshes are not CAD solids.</small>
  </div>
</main>
<footer class="sr-only">This is a host framebuffer preview, not a PlayStation 4 recording.</footer>
<script>
(() => {
  const canvas = document.getElementById('workbench');
  const focus = document.getElementById('focus');
  const recording = document.getElementById('recording');
  const fins = document.getElementById('fins');
  const mesh = document.getElementById('mesh');
  const download = document.getElementById('download');
  const status = document.getElementById('previewStatus');
  const ctx = canvas.getContext('2d', {alpha: false});
  let pending = Promise.resolve(), down = false, mode = '', lastHover = 0, ready = false;
  let nativePage = 0; // supplied by the C++ app with every framebuffer
  const namedKeys = new Set(['Enter', 'Escape', 'Tab', 'Backspace', 'Alt',
                             'ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown',
                             'F1', 'F3', 'F4', 'F5']);
  function send(message) {
    pending = pending.then(() => fetch('/input', {method: 'POST',
      headers: {'Content-Type': 'application/json'}, body: JSON.stringify(message)}));
    pending = pending.catch(err => {
      status.textContent = 'Input unavailable. Restart make preview and refresh.';
      console.warn('Native input unavailable', err);
    });
    return pending;
  }
  const coords = event => {
    const r = canvas.getBoundingClientRect();
    return [Math.max(0, Math.min(1279, Math.floor((event.clientX - r.left) * canvas.width / r.width))),
            Math.max(0, Math.min(719, Math.floor((event.clientY - r.top) * canvas.height / r.height)))];
  };
  canvas.addEventListener('pointerdown', event => {
    const [x, y] = coords(event);
    canvas.focus(); canvas.setPointerCapture(event.pointerId);
    down = true;
    mode = nativePage !== 3 && x >= 244 && x < 960 && y >= 147 && y < 558 ? 'orbit' :
           nativePage !== 3 && x >= 314 && x < 920 && y >= 604 && y <= 660 ? 'timeline' : '';
    send({type: 'click', x, y});
  });
  canvas.addEventListener('pointermove', event => {
    const [x, y] = coords(event);
    canvas.style.cursor = down && mode === 'orbit' ? 'grabbing' :
      nativePage !== 3 && x >= 244 && x < 960 && y >= 147 && y < 558 ? 'grab' :
      (nativePage !== 3 && x >= 711 && x < 910 && y >= 113 && y < 141) ||
      (y >= 32 && y < 108) ||
      (nativePage !== 3 && nativePage !== 2 && x < 244 && y >= 170 && y < 494) ||
      (nativePage === 3 ?
        (x >= 976 && x < 1264 && y >= 178 && y < 669) ||
        (x >= 270 && x < 934 && y >= 486 && y < 519) ||
        (x >= 354 && x < 924 && y >= 420 && y < 458) :
        x >= 960 && y >= 178 && y < 669) ||
      (nativePage !== 3 && x >= 314 && x < 920 && y >= 604 && y <= 660) ?
      'pointer' : 'default';
    if (!down) {
      if (Date.now() - lastHover > 65) {
        lastHover = Date.now();
        send({type: 'hover', x, y});
      }
      return;
    }
    if (mode === 'orbit' && (event.movementX || event.movementY)) {
      const r = canvas.getBoundingClientRect();
      const clampDelta = v => Math.max(-128, Math.min(128, Math.round(v)));
      send({type: 'drag', x: clampDelta(event.movementX * 1280 / r.width),
                          y: clampDelta(event.movementY * 720 / r.height)});
    } else if (mode === 'timeline') send({type: 'click', x, y});
  });
  canvas.addEventListener('pointerup', () => { down = false; mode = ''; });
  canvas.addEventListener('pointercancel', () => { down = false; mode = ''; });
  canvas.addEventListener('pointerleave', () => send({type: 'hover', x: -1, y: -1}));
  canvas.addEventListener('wheel', event => {
    event.preventDefault();
    const [x, y] = coords(event);
    send({type: 'wheel', x, y, direction: event.deltaY < 0 ? 1 : -1});
  }, {passive: false});
  canvas.addEventListener('keydown', event => {
    if (event.key === 'F2') { event.preventDefault(); focus.focus(); return; }
    if (event.repeat && (event.key === 'Alt' || event.key === 'F1')) return;
    const key = event.key === ' ' ? 'Space' : event.key;
    if (namedKeys.has(key) || key === 'Space' || key.length === 1 ||
        (event.ctrlKey && /^[a-z]$/i.test(key))) {
      event.preventDefault();
      send({type: 'key', name: key, ctrl: event.ctrlKey, shift: event.shiftKey});
    }
  });
  focus.addEventListener('click', () => canvas.focus());
  recording.addEventListener('change', async () => {
    const file = recording.files && recording.files[0];
    if (!file) return;
    if (file.size > 2097152) {
      status.textContent = 'CSV exceeds the 2 MiB recording limit.';
      return;
    }
    try {
      await pending;
      status.textContent = 'Copying recorded CSV into the sandbox...';
      const response = await fetch('/recording', {method: 'POST',
        headers: {'Content-Type': 'text/csv'}, body: file});
      if (!response.ok) throw new Error('CSV upload rejected');
      await send({type: 'key', name: 'l', ctrl: true, shift: false});
      status.textContent = 'Native CSV importer checked the file. See the workbench status.';
      canvas.focus();
    } catch (err) {
      status.textContent = 'CSV upload failed; check the format and size.';
      console.warn('Recording upload failed', err);
    }
  });
  fins.addEventListener('change', async () => {
    const file = fins.files && fins.files[0];
    if (!file) return;
    if (file.size > 8192) {
      status.textContent = 'Fin inspection exceeds the 8 KiB limit.';
      return;
    }
    try {
      await pending;
      const response = await fetch('/fins', {method: 'POST',
        headers: {'Content-Type': 'text/csv'}, body: file});
      if (!response.ok) throw new Error('Fin inspection rejected');
      await send({type: 'key', name: 'i', ctrl: true, shift: false});
      status.textContent = 'Native fin parser checked the operator log; see Design > Fin set.';
      canvas.focus();
    } catch (err) {
      status.textContent = 'Fin inspection rejected; check schema and actual readings.';
      console.warn('Fin inspection upload failed', err);
    }
  });
  mesh.addEventListener('change', async () => {
    const file = mesh.files && mesh.files[0];
    if (!file) return;
    const ext = file.name.split('.').pop().toLowerCase();
    if (!['obj', 'stl'].includes(ext) || file.size > 2097152) {
      status.textContent = 'Only OBJ/STL meshes up to 2 MiB are supported.';
      return;
    }
    try {
      await pending;
      status.textContent = 'Copying visual mesh into the sandbox...';
      const response = await fetch('/mesh/' + ext, {method: 'POST',
        headers: {'Content-Type': 'application/octet-stream'}, body: file});
      if (!response.ok) throw new Error('Mesh upload rejected');
      await send({type: 'key', name: 'm', ctrl: true, shift: ext === 'stl'});
      status.textContent = 'Native mesh importer checked the file. See the workbench status.';
      canvas.focus();
    } catch (err) {
      status.textContent = 'Mesh upload failed; check the file and size.';
      console.warn('Mesh upload failed', err);
    }
  });
  download.addEventListener('click', async () => {
    try {
      const response = await fetch('/assembly.obj');
      if (!response.ok) {
        status.textContent = 'Use File > Export visual OBJ in the native workbench first.';
        return;
      }
      const blob = await response.blob();
      const url = URL.createObjectURL(blob);
      const anchor = document.createElement('a');
      anchor.href = url; anchor.download = 'assembly.obj'; anchor.click();
      setTimeout(() => URL.revokeObjectURL(url), 1000);
      status.textContent = 'Downloaded the visual, non-watertight OBJ.';
    } catch (err) {
      status.textContent = 'Visual OBJ download failed.';
    }
  });
  async function frame() {
    try {
      await pending;
      if (!document.hidden) {
        const response = await fetch('/frame', {cache: 'no-store'});
        if (!response.ok) throw new Error('Native renderer exited');
        nativePage = Number(response.headers.get('X-EuloLab-Page') || '0');
        const bitmap = await createImageBitmap(await response.blob());
        ctx.drawImage(bitmap, 0, 0);
        bitmap.close();
        if (!ready) { status.textContent = 'Native host renderer ready.'; ready = true; }
      }
    } catch (err) {
      ready = false;
      status.textContent = 'Native renderer unavailable. Restart make preview and refresh.';
      console.warn('Frame unavailable', err);
    }
    setTimeout(frame, document.hidden ? 550 : 110);
  }
  frame();
})();
</script>
</body>
</html>
'''


def _chunk(kind: bytes, content: bytes) -> bytes:
    data = kind + content
    return (struct.pack('>I', len(content)) + data +
            struct.pack('>I', zlib.crc32(data) & 0xFFFFFFFF))


def as_png(raw: bytes) -> bytes:
    """Lossless RGBA PNG from little-endian ARGB pixels; standard-library only."""
    if len(raw) != RAW_SIZE:
        raise ValueError('Incomplete native framebuffer')
    rgba = bytearray(RAW_SIZE)
    rgba[0::4], rgba[1::4] = raw[2::4], raw[1::4]
    rgba[2::4], rgba[3::4] = raw[0::4], raw[3::4]
    stride = WIDTH * 4
    rows = bytearray((stride + 1) * HEIGHT)
    for y in range(HEIGHT):
        rows[y * (stride + 1) + 1:(y + 1) * (stride + 1)] = rgba[y * stride:(y + 1) * stride]
    return (b'\x89PNG\r\n\x1a\n' +
            _chunk(b'IHDR', struct.pack('>IIBBBBB', WIDTH, HEIGHT, 8, 6, 0, 0, 0)) +
            _chunk(b'IDAT', zlib.compress(rows, level=2)) +
            _chunk(b'IEND', b''))


class Native:
    def __init__(self) -> None:
        export = ROOT / 'out' / 'assembly.obj'
        self.initial_export_mtime_ns = export.stat().st_mtime_ns if export.exists() else 0
        self.process = subprocess.Popen(
            [str(ROOT / 'out/eulolab-host'), '--interactive'],
            cwd=ROOT, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
        )
        self.lock = threading.Lock()
        self.last = time.monotonic()
        self.page = 0

    def command(self, line: str) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write((line + '\n').encode('ascii'))
        self.process.stdin.flush()

    def render(self) -> bytes:
        with self.lock:
            now = time.monotonic()
            self.command(f'TICK {min(0.25, max(0, now - self.last)):.4f}')
            self.last = now
            self.command('FRAME')
            assert self.process.stdout is not None
            raw = self.process.stdout.read(RAW_SIZE)
            self.command('PAGE')
            page = self.process.stdout.read(1)
            if page not in (b'0', b'1', b'2', b'3'):
                raise ValueError('Native page state unavailable')
            self.page = int(page)
            return as_png(raw)

    def input(self, line: str) -> None:
        with self.lock:
            self.command(line)

    def close(self) -> None:
        if self.process.stdin and not self.process.stdin.closed:
            self.process.stdin.close()  # release the native read loop
        if self.process.poll() is None:
            try:
                self.process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.process.terminate()
                try:
                    self.process.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    self.process.kill()
                    self.process.wait()
        if self.process.stdout and not self.process.stdout.closed:
            self.process.stdout.close()


NAMES = {'Enter', 'Escape', 'Tab', 'Backspace', 'Alt', 'Space',
         'ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight', 'F1', 'F3', 'F4', 'F5'}
SINGLE = re.compile(r'^[a-zA-Z0-9.\-]$')


def command_from(message: dict) -> str:
    """Allow only fixed UI commands, not shell commands or arbitrary paths."""
    kind = message.get('type')
    if kind in ('click', 'wheel', 'hover'):
        x, y = message.get('x'), message.get('y')
        if type(x) is not int or type(y) is not int or not (
            (0 <= x < WIDTH and 0 <= y < HEIGHT) or
            (kind == 'hover' and x == y == -1)
        ):
            raise ValueError('Invalid pointer coordinates')
        if kind == 'click':
            return f'CLICK {x} {y}'
        if kind == 'hover':
            return f'HOVER {x} {y}'
        direction = message.get('direction')
        if type(direction) is not int or direction not in (-1, 1):
            raise ValueError('Invalid wheel direction')
        return f'WHEEL {x} {y} {direction}'
    if kind == 'drag':
        x, y = message.get('x'), message.get('y')
        if type(x) is not int or type(y) is not int or not -128 <= x <= 128 or not -128 <= y <= 128:
            raise ValueError('Invalid orbit delta')
        return f'DRAG {x} {y}'
    if kind == 'key':
        name = message.get('name')
        if not isinstance(name, str) or (name not in NAMES and not SINGLE.fullmatch(name)):
            raise ValueError('Invalid key name')
        ctrl, shift = message.get('ctrl'), message.get('shift')
        if type(ctrl) is not bool or type(shift) is not bool:
            raise ValueError('Invalid key modifiers')
        return f'KEY {name} {int(ctrl)} {int(shift)}'
    raise ValueError('Unknown input type')


class Handler(BaseHTTPRequestHandler):
    native: Native

    def log_message(self, fmt: str, *args: object) -> None:
        # Avoid writing one log line per animation frame.
        if self.path not in ('/frame', '/input'):
            super().log_message(fmt, *args)

    def respond(self, code: int, body: bytes, content_type: str) -> None:
        self.send_response(code)
        self.send_header('Content-Type', content_type)
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Cache-Control', 'no-store')
        self.send_header('X-EuloLab-Page', str(self.native.page))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:
        if self.path == '/':
            self.respond(200, PAGE, 'text/html; charset=utf-8')
        elif self.path == '/health':
            self.respond(200 if self.native.process.poll() is None else 503,
                         b'native renderer ready\n', 'text/plain; charset=utf-8')
        elif self.path == '/frame':
            try:
                self.respond(200, self.native.render(), 'image/png')
            except (BrokenPipeError, OSError, ValueError):
                self.respond(503, b'native renderer unavailable\n', 'text/plain; charset=utf-8')
        elif self.path == '/assembly.obj':
            try:
                self.native.render()  # wait for any preceding native Export action
            except (BrokenPipeError, OSError, ValueError):
                self.respond(503, b'native renderer unavailable\n',
                             'text/plain; charset=utf-8')
                return
            path = ROOT / 'out' / 'assembly.obj'
            if (not path.is_file() or path.stat().st_size > 2097152 or
                    path.stat().st_mtime_ns <= self.native.initial_export_mtime_ns):
                self.respond(404, b'Export visual OBJ in the workbench first.\n',
                             'text/plain; charset=utf-8')
            else:
                self.respond(200, path.read_bytes(), 'model/obj')
        else:
            self.respond(404, b'not found\n', 'text/plain; charset=utf-8')

    def do_POST(self) -> None:
        if self.path in ('/mesh/obj', '/mesh/stl'):
            extension = self.path.rsplit('/', 1)[-1]
            try:
                length = int(self.headers.get('Content-Length', '0'))
                if not 1 <= length <= 2097152 or self.headers.get('Content-Type') != 'application/octet-stream':
                    raise ValueError('Mesh size or media type')
                body = self.rfile.read(length)
                if len(body) != length or (extension == 'obj' and b'\x00' in body):
                    raise ValueError('Incomplete or binary OBJ')
                destination = ROOT / 'out' / f'reference.{extension}'
                destination.parent.mkdir(parents=True, exist_ok=True)
                temporary = destination.with_name(f'reference.upload.{extension}')
                with self.native.lock:
                    try:
                        temporary.write_bytes(body)
                        check = subprocess.run([str(ROOT / 'out/eulolab-host'),
                                                '--check-mesh', str(temporary)],
                                               cwd=ROOT, timeout=5, capture_output=True)
                        if check.returncode != 0:
                            raise ValueError('Native mesh importer rejected the file')
                        os.replace(temporary, destination)
                    finally:
                        temporary.unlink(missing_ok=True)
            except (ValueError, subprocess.TimeoutExpired, BrokenPipeError, OSError):
                self.respond(400, b'invalid visual mesh upload\n', 'text/plain; charset=utf-8')
                return
            self.respond(204, b'', 'text/plain; charset=utf-8')
            return
        if self.path == '/recording':
            try:
                length = int(self.headers.get('Content-Length', '0'))
                if not 48 <= length <= 2097152 or self.headers.get('Content-Type') != 'text/csv':
                    raise ValueError('Recording size or media type')
                body = self.rfile.read(length)
                headers = (
                    b'time_s,altitude_m,gx_deg_s,gy_deg_s,gz_deg_s',
                    b'time_s,pressure_pa,temperature_c,gx_deg_s,gy_deg_s,gz_deg_s,'
                    b'ax_m_s2,ay_m_s2,az_m_s2',
                )
                if (len(body) != length or b'\x00' in body or not
                        any(body.startswith(h + b'\n') for h in headers)):
                    raise ValueError('Recording header or binary content')
                body.decode('ascii')
                destination = ROOT / 'out' / 'recording.csv'
                destination.parent.mkdir(parents=True, exist_ok=True)
                temporary = destination.with_suffix('.csv.upload')
                with self.native.lock:
                    try:
                        temporary.write_bytes(body)
                        check = subprocess.run([str(ROOT / 'out/eulolab-host'),
                                                '--check-recording', str(temporary)],
                                               cwd=ROOT, timeout=5, capture_output=True)
                        if check.returncode != 0:
                            raise ValueError('Native CSV importer rejected the file')
                        os.replace(temporary, destination)
                    finally:
                        temporary.unlink(missing_ok=True)
            except (ValueError, subprocess.TimeoutExpired, UnicodeDecodeError,
                    BrokenPipeError, OSError):
                self.respond(400, b'invalid recorded CSV\n', 'text/plain; charset=utf-8')
                return
            self.respond(204, b'', 'text/plain; charset=utf-8')
            return
        if self.path == '/fins':
            try:
                length = int(self.headers.get('Content-Length', '0'))
                if not 100 <= length <= 8192 or self.headers.get('Content-Type') != 'text/csv':
                    raise ValueError('Fin log size or media type')
                body = self.rfile.read(length)
                if (len(body) != length or b'\x00' in body or not
                        body.startswith((b'EULOFINS/1\n', b'EULOFINS/1\r\n'))):
                    raise ValueError('Fin log header or binary content')
                body.decode('ascii')
                destination = ROOT / 'out' / 'fin-inspection.csv'
                destination.parent.mkdir(parents=True, exist_ok=True)
                temporary = destination.with_suffix('.csv.upload')
                with self.native.lock:
                    try:
                        temporary.write_bytes(body)
                        check = subprocess.run([str(ROOT / 'out/eulolab-host'),
                                                '--check-fins', str(temporary)],
                                               cwd=ROOT, timeout=5, capture_output=True)
                        if check.returncode != 0:
                            raise ValueError('Native fin parser rejected operator log')
                        os.replace(temporary, destination)
                    finally:
                        temporary.unlink(missing_ok=True)
            except (ValueError, subprocess.TimeoutExpired, UnicodeDecodeError,
                    BrokenPipeError, OSError):
                self.respond(400, b'invalid fin-inspection CSV\n', 'text/plain; charset=utf-8')
                return
            self.respond(204, b'', 'text/plain; charset=utf-8')
            return
        if self.path != '/input':
            self.respond(404, b'not found\n', 'text/plain; charset=utf-8')
            return
        try:
            length = int(self.headers.get('Content-Length', '0'))
            if length < 2 or length > 512:
                raise ValueError('Payload size out of range')
            message = json.loads(self.rfile.read(length))
            if not isinstance(message, dict):
                raise ValueError('Expected a JSON object')
            self.native.input(command_from(message))
        except (ValueError, UnicodeDecodeError, BrokenPipeError, OSError):
            self.respond(400, b'invalid input\n', 'text/plain; charset=utf-8')
            return
        self.respond(204, b'', 'text/plain; charset=utf-8')


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', type=int, default=8000)
    args = parser.parse_args()
    native = Native()
    Handler.native = native
    atexit.register(native.close)
    server = ThreadingHTTPServer(('0.0.0.0', args.port), Handler)
    print(f'Native host preview on 0.0.0.0:{server.server_port}', flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
        native.close()


if __name__ == '__main__':
    main()
