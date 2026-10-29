"""Exercise the callable boundary with discovery and wire-message doubles."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent


class Hardware(unittest.TestCase):
    def test_returning_failures_and_ram_results(self):
        with tempfile.TemporaryDirectory() as work:
            executable = Path(work) / 'hardware'
            subprocess.run([
                'cc', '-std=c90', '-pedantic-errors',
                '-I', str(ROOT.parent), '-I', str(ROOT),
                str(ROOT / 'test_hw.c'), str(ROOT / 'hardware.c'),
                str(ROOT / 'graph.c'), str(ROOT / 'quality.c'), str(ROOT / 'examples.c'), '-o', str(executable)
            ], check=True)
            subprocess.run([str(executable)], check=True, timeout=5)
