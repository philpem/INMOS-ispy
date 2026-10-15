"""Linux deadline behavior with real sockets and a readiness ioctl double."""
import subprocess
import tempfile
import unittest
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
class LinkAdapter(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.exe = Path(cls.temp.name)/'adapter'
        subprocess.run(['cc', '-std=c90', '-pedantic-errors', '-I', str(ROOT),
                        str(ROOT/'tests/linux_link.c'), '-o', str(cls.exe)], check=True)
    @classmethod
    def tearDownClass(cls): cls.temp.cleanup()
    def run_case(self, case):
        r = subprocess.run([str(self.exe), case], capture_output=True, text=True, timeout=2)
        self.assertEqual(r.returncode, 0, r.stderr)
        n, elapsed = r.stdout.split()
        return int(n), float(elapsed)
    def test_empty_read_deadline_uses_tenths(self):
        n, elapsed = self.run_case('empty')
        self.assertEqual(n, 0)
        self.assertGreaterEqual(elapsed, .09)
        self.assertLess(elapsed, .5)
    def test_short_read_returns_progress_at_same_deadline(self):
        n, elapsed = self.run_case('short')
        self.assertEqual(n, 3)
        self.assertGreaterEqual(elapsed, .09)
        self.assertLess(elapsed, .5)
    def test_ready_write_preserves_bytes(self):
        self.assertEqual(self.run_case('write')[0], 8)
    def test_readiness_error_returns_without_looping(self):
        n, elapsed = self.run_case('error')
        self.assertEqual(n, -1)
        self.assertLess(elapsed, .1)
