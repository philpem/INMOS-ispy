"""Native wrapper errors and monotonic deadline wrap using OSLib/driver doubles."""
import subprocess
import tempfile
import unittest
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
class NativeLink(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        root = Path(cls.temp.name)
        (root/'oslib').mkdir()
        (root/'oslib/os.h').write_text('void *xos_read_monotonic_time(int *);\n')
        cls.exe = root/'native'
        subprocess.run(['cc', '-std=c90', '-pedantic-errors', '-I', str(root),
                        str(ROOT/'tests/riscos_link.c'), '-o', str(cls.exe)], check=True)
        subprocess.run(['cc', '-std=c90', '-pedantic-errors', '-fsyntax-only',
                        '-DRISCOS', '-I', str(root), '-I', str(ROOT),
                        str(ROOT/'ltest.c')], check=True)
    @classmethod
    def tearDownClass(cls): cls.temp.cleanup()
    def run_case(self, mode, wrap=False):
        r = subprocess.run([str(self.exe), str(mode), *(['wrap'] if wrap else [])],
                           capture_output=True, text=True, timeout=2)
        self.assertEqual(r.returncode, 0, r.stderr)
        return tuple(map(int, r.stdout.split()))
    def test_ready_bytes_use_bounded_bursts(self):
        self.assertEqual(self.run_case(0), (8,1))
        self.assertEqual(self.run_case(6), (600,3))
    def test_no_ready_bytes_timeout_across_counter_wrap(self):
        self.assertEqual(self.run_case(1), (0,0))
        self.assertEqual(self.run_case(1, True), (0,0))
    def test_raw_and_readiness_and_clock_errors_fail(self):
        for mode in (2,3,4,5):
            with self.subTest(mode=mode): self.assertEqual(self.run_case(mode)[0], -1)
