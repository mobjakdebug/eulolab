#!/usr/bin/env python3
"""Capture an identified Arduino recorder's actual serial bytes, not a demo.

No synthetic samples, no 'connect to preview browser' shortcut. This program
cannot authenticate a device or certify that its sensors are installed/correct.
It demands a sketch banner, strict CSV units and monotonically increasing time.
Only accepted rows are atomically saved; an error aborts rather than inventing
missing values. Install pyserial only on the computer with the physical port.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
import tempfile
import time
from typing import Iterable

HEADER = "time_s,pressure_pa,temperature_c,gx_deg_s,gy_deg_s,gz_deg_s,ax_m_s2,ay_m_s2,az_m_s2"
RECORDER = "# EULOLAB_RECORDER/1"
RECEIVER = "# EULOLAB_RADIO_RECEIVER/1"
MAX_BYTES = 2097152
MAX_ROWS = 20000


class CaptureError(ValueError):
    """A device or data contract failed; do not publish a partial recording."""


@dataclass(frozen=True)
class CaptureResult:
    csv: bytes
    transport: str
    rows: int
    first_time_s: float
    last_time_s: float
    gaps_over_250ms: int
    diagnostic_lines: int


def checked_row(text: str, previous_time: float) -> float:
    if len(text) > 192:
        raise CaptureError("Sensor CSV line exceeds 192 characters")
    cells = text.split(",")
    if len(cells) != 9:
        raise CaptureError("Sensor row must have exactly nine numeric columns")
    try:
        values = [float(v) for v in cells]
    except ValueError as exc:
        raise CaptureError("Sensor row has a nonnumeric column") from exc
    t, p, temp, gx, gy, gz, ax, ay, az = values
    if not all(math.isfinite(v) for v in values) or not (previous_time < t <= 3600):
        raise CaptureError("Nonfinite, repeated, decreasing or out-of-range timestamp/value")
    if not (30000 <= p <= 125000 and -40 <= temp <= 85 and
            all(abs(v) <= 2000 for v in (gx, gy, gz)) and
            all(abs(v) <= 200 for v in (ax, ay, az))):
        raise CaptureError("Pressure/IMU value outside the declared sensor bounds")
    return t


def parse_stream(lines: Iterable[bytes]) -> CaptureResult:
    """Consume a real (or test-injected) serial byte stream. No USB is opened here."""
    state = "banner"
    transport = ""
    out = bytearray((HEADER + "\n").encode("ascii"))
    first = previous = -1.0
    rows = gaps = diagnostics = 0
    for raw in lines:
        if len(raw) > 256 or not raw.endswith(b"\n"):
            raise CaptureError("Truncated or oversized serial line")
        try:
            text = raw.decode("ascii").rstrip("\r\n")
        except UnicodeDecodeError as exc:
            raise CaptureError("Non-ASCII serial data") from exc
        if text.startswith("# EULOLAB_ERROR"):
            raise CaptureError(f"Device reported an error: {text[:150]}")
        if state == "banner":
            if text == RECORDER or text == RECEIVER:
                transport = "serial recorder" if text == RECORDER else "SX127x receiver serial"
                state = "header"
            # Ignore pre-handshake serial boot text; it is not a sensor sample.
            continue
        if state == "header":
            if text == (RECORDER if transport == "serial recorder" else RECEIVER):
                continue  # host H and device boot announcement can race
            if text != HEADER:
                raise CaptureError("Device banner was not followed by the exact sensor header")
            state = "rows"
            continue
        if text in (RECORDER, RECEIVER, HEADER):
            if rows:
                raise CaptureError("Device re-announced/restarted during a recording")
            if text in (RECORDER, RECEIVER):
                if text != (RECORDER if transport == "serial recorder" else RECEIVER):
                    raise CaptureError("Two device roles appeared on one port")
                state = "header"
            continue
        if text.startswith("# EULOLAB_STOP"):
            break
        if text.startswith("#"):
            diagnostics += 1  # radio RSSI, rejects and packet gaps, not data
            continue
        if not text:
            continue
        t = checked_row(text, previous)
        if rows == 0:
            first = t
        elif t - previous > .25:
            gaps += 1  # never interpolate a missing serial/radio interval
        previous = t
        row = (text + "\n").encode("ascii")
        if len(out) + len(row) > MAX_BYTES or rows >= MAX_ROWS:
            raise CaptureError("CSV exceeds the application's 2 MiB / 20000 row limit")
        out.extend(row)
        rows += 1
    if state != "rows" or rows < 2:
        raise CaptureError("No complete device header and at least two real rows")
    return CaptureResult(bytes(out), transport, rows, first, previous, gaps, diagnostics)


def serial_lines(port, seconds: float) -> Iterable[bytes]:
    start = time.monotonic()
    next_hello = start
    saw_banner = False
    while time.monotonic() - start < seconds:
        now = time.monotonic()
        if not saw_banner and now >= next_hello:
            port.write(b"H")  # read-only handshake; does not arm PWM/RF
            port.flush()
            next_hello = now + 2
        raw = port.readline()
        if raw:
            if raw.startswith((RECORDER.encode(), RECEIVER.encode())):
                saw_banner = True
            yield raw
        if not saw_banner and time.monotonic() - start > 10:
            raise CaptureError("No Arduino recorder banner after 10 seconds; check port/core settings")


def save(result: CaptureResult, output: Path, port: str, board_label: str) -> Path:
    """Keep a timestamp/hash/port trail. It does NOT attest real sensor wiring."""
    sidecar = output.with_name(output.name + ".meta.json")
    if output.exists() or sidecar.exists():
        raise CaptureError("Output or metadata already exists; choose a fresh filename")
    output.parent.mkdir(parents=True, exist_ok=True)
    manifest = {
        "format": "EuloLab serial capture/1",
        "captured_utc": datetime.now(timezone.utc).isoformat(),
        "port_user_supplied": port,
        "board_label_user_supplied": board_label or None,
        "transport_device_reported": result.transport,
        "device_and_sensor_authenticity": "not independently verified",
        "samples": result.rows,
        "first_time_s": result.first_time_s,
        "last_time_s": result.last_time_s,
        "sample_gaps_over_250ms": result.gaps_over_250ms,
        "diagnostic_lines": result.diagnostic_lines,
        "csv_sha256": hashlib.sha256(result.csv).hexdigest(),
    }
    csv_tmp = meta_tmp = None
    try:
        with tempfile.NamedTemporaryFile(dir=output.parent, prefix=".capture-", delete=False) as f:
            csv_tmp = Path(f.name)
            f.write(result.csv)
            f.flush()
        with tempfile.NamedTemporaryFile(dir=output.parent, prefix=".capture-meta-",
                                         delete=False, mode="w", encoding="utf-8") as f:
            meta_tmp = Path(f.name)
            json.dump(manifest, f, indent=2, sort_keys=True)
            f.write("\n")
            f.flush()
        # Do not overwrite an existing recording if another writer raced us.
        if output.exists() or sidecar.exists():
            raise CaptureError("Output appeared while recording; choose a fresh filename")
        csv_tmp.replace(output)
        meta_tmp.replace(sidecar)
    finally:
        if csv_tmp is not None:
            csv_tmp.unlink(missing_ok=True)
        if meta_tmp is not None:
            meta_tmp.unlink(missing_ok=True)
    return sidecar


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="Observed OS serial port for YOUR board")
    parser.add_argument("--output", required=True, type=Path, help="Fresh recorded CSV filename")
    parser.add_argument("--seconds", type=float, default=180,
                        help="Wall-clock capture duration, 2..3600 seconds (default 180)")
    parser.add_argument("--board-label", default="",
                        help="Operator's board label (unverified; optional)")
    args = parser.parse_args()
    if not math.isfinite(args.seconds) or not 2 <= args.seconds <= 3600:
        parser.error("--seconds must be between 2 and 3600")
    try:
        import serial  # type: ignore[import-not-found]
    except ImportError:
        parser.error("Install pyserial on the computer with the board: python3 -m pip install pyserial")
    try:
        with serial.Serial(args.port, 115200, timeout=.5, write_timeout=2) as device:
            result = parse_stream(serial_lines(device, args.seconds))
        sidecar = save(result, args.output, args.port, args.board_label)
    except (CaptureError, OSError, serial.SerialException) as exc:
        parser.exit(1, f"Capture aborted without invented data: {exc}\n")
    print(f"Saved {result.rows} device-reported rows to {args.output}; "
          f"metadata {sidecar}; gaps >250 ms: {result.gaps_over_250ms}.")
    print("This is NOT independent verification of the board, sensors or radio.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
