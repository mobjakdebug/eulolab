"""Only test-injected serial bytes; these are NOT recorded hardware data."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from capture_serial import (HEADER, RECORDER, RECEIVER, CaptureError,
                            parse_stream, save)


def line(time_s, pressure=100000, gyro=0, accel=9.81):
    return (f"{time_s:.2f},{pressure},21,0,{gyro},0,0,{accel},0\n").encode()


class CaptureTests(unittest.TestCase):
    def test_direct_serial_and_real_unit_contract(self):
        stream = [b'boot text\n', (RECORDER + '\n').encode(),
                  (HEADER + '\n').encode(), line(0), line(.05, 99999),
                  line(.10, 99998), b'# EULOLAB_STOP time_or_size_limit\n']
        result = parse_stream(stream)
        self.assertEqual(result.rows, 3)
        self.assertEqual(result.transport, 'serial recorder')
        self.assertEqual(result.gaps_over_250ms, 0)
        self.assertEqual(result.csv, (HEADER + '\n').encode() + b''.join(stream[3:6]))
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'recording.csv'
            meta = save(result, path, '/dev/ttyACM_user_selected', 'my-STM32')
            self.assertEqual(path.read_bytes(), result.csv)
            contents = json.loads(meta.read_text())
            self.assertEqual(contents['csv_sha256'], hashlib.sha256(result.csv).hexdigest())
            self.assertEqual(contents['samples'], 3)
            self.assertEqual(contents['board_label_user_supplied'], 'my-STM32')
            self.assertEqual(contents['device_and_sensor_authenticity'],
                             'not independently verified')
            with self.assertRaises(CaptureError):
                save(result, path, 'another port', '')  # never overwrite evidence

    def test_radio_gaps_not_filled_and_errors_abort(self):
        result = parse_stream([(RECEIVER + '\n').encode(), (HEADER + '\n').encode(),
                               line(0), b'# EULOLAB_RADIO_LINK rssi_dbm=-83 snr_db=6.5\n',
                               b'# EULOLAB_RADIO_GAP missing_packets\n',
                               line(60, 99925)])
        self.assertEqual(result.rows, 2)
        self.assertEqual(result.gaps_over_250ms, 1)
        self.assertEqual(result.diagnostic_lines, 2)
        self.assertNotIn(b'30.00', result.csv)  # no imaginary midpoint
        with self.assertRaises(CaptureError):
            parse_stream([(RECEIVER + '\n').encode(), (HEADER + '\n').encode(),
                          line(0), b'# EULOLAB_ERROR receiver failed\n', line(60)])

    def test_invalid_raw_rows_and_restart_fail_closed(self):
        prefix = [(RECORDER + '\n').encode(), (HEADER + '\n').encode(), line(0)]
        for broken in [line(0), line(.1, pressure=0), line(.1, gyro='nan'),
                       line(.1, accel=300), b'0.1,3,4\n', b'0.1,3,4',
                       b'0.1,3,4,5,6,7,8,9,10,11\n',
                       (RECORDER + '\n').encode(), b'garbage\n']:
            with self.subTest(broken=broken), self.assertRaises(CaptureError):
                parse_stream(prefix + [broken, line(.2)])
        with self.assertRaises(CaptureError):
            parse_stream([b'# EULOLAB_ERROR hardware not identified\n'] + prefix)
        with self.assertRaises(CaptureError):
            parse_stream([b'no device\n', (HEADER + '\n').encode()])
        self.assertEqual(parse_stream([(RECORDER + '\n').encode(),
                  (HEADER + '\n').encode(), (RECORDER + '\n').encode(),
                  (HEADER + '\n').encode(), line(0), line(.1)]).rows, 2)


if __name__ == '__main__':
    unittest.main()
