"""Host checks for WIMP state transitions and coordinate conversion."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent


class Core(unittest.TestCase):
    def test_core(self):
        with tempfile.TemporaryDirectory() as work:
            executable = Path(work) / 'core'
            subprocess.run([
                'cc', '-std=c90', '-pedantic-errors', '-Wall', '-Wextra', '-Werror',
                str(ROOT / 'test_core.c'), str(ROOT / 'graph.c'), str(ROOT / 'quality.c'), str(ROOT / 'examples.c'),
                str(ROOT / 'job.c'), str(ROOT / 'present.c'), '-o', str(executable)
            ], check=True)
            subprocess.run([str(executable)], check=True, timeout=5)


if __name__ == '__main__':
    unittest.main()
