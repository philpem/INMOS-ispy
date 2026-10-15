"""Actual loader/framing code with adversarial short and malformed transfers."""
import os
import shlex
import subprocess
import tempfile
import unittest
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]

class TransportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.exe = Path(cls.temp.name)/'transport'
        subprocess.run(['cc', '-std=c90', '-pedantic-errors',
                        *shlex.split(os.getenv('C004_TEST_CFLAGS', '-O1 -g')),
                        '-I', str(ROOT), str(ROOT/'tests/transport.c'),
                        str(ROOT/'cklib.c'), '-o', str(cls.exe)], check=True)
    @classmethod
    def tearDownClass(cls): cls.temp.cleanup()
    def run_case(self, name, mode, status=0):
        r = subprocess.run([str(self.exe), name, str(mode)], capture_output=True,
                           text=True, timeout=5)
        self.assertEqual(r.returncode, status, r.stderr)
        return r
    def test_fragmented_raw_loader_matches_complete_stream(self):
        self.assertIn('exact stream', self.run_case('load', 3).stdout)
    def test_raw_loader_rejects_failed_and_oversized_writes(self):
        for mode in [1, 2]: self.run_case('load', mode, 1)
    def test_failed_header_initializes_length(self):
        for mode in [1, 2]:
            self.assertIn('length 0', self.run_case('receive', mode).stdout)
    def test_oversized_frame_is_rejected(self):
        self.assertIn('exceeds buffer', self.run_case('receive', 0).stderr)
    def test_application_route_failure_is_fatal_without_child_dereference(self):
        for mode in [1, 2]:
            self.assertIn('destination 4', self.run_case('route', mode, 1).stderr)
    def test_fragmented_route(self):
        self.assertIn('route 6', self.run_case('route', 3).stdout)
    def test_frame_length_cannot_truncate_to_16_bits(self):
        self.assertIn('sent 0', self.run_case('packet', 4).stdout)
