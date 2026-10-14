"""Check ordinary builds and explicit payload-regeneration dependencies."""

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class BuildTests(unittest.TestCase):
    def test_host_build_and_explicit_regeneration(self):
        with tempfile.TemporaryDirectory(prefix="ispy-build-") as work:
            root = Path(work)
            for source in [ROOT / "Makefile", *ROOT.glob("*.c"), *ROOT.glob("*.h")]:
                shutil.copy2(source, root / source.name)
            (root / "tools").mkdir()
            (root / "occam").mkdir()
            (root / "occam" / "source.occ").write_text("-- regeneration input\n")
            for name in ("tco2h.py", "fetch-toolchain.sh"):
                (root / "tools" / name).touch()
            builder = root / "tools" / "occam-build.sh"
            builder.write_text("#!/bin/sh\necho unexpected regeneration >&2\nexit 99\n")
            builder.chmod(0o755)

            def make(*goals):
                result = subprocess.run(["make", "-j2", *goals], cwd=root,
                                        capture_output=True, text=True, timeout=30)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                return result

            # No stamp, target compiler, toolset archive or real build script.
            make()
            for name in ("ispy", "mtest"):
                self.assertTrue((root / name).is_file())
            self.assertFalse((root / ".occam.stamp").exists())
            objects = [root / "check.o", root / "mtest.o"]
            before = [p.stat().st_mtime_ns for p in objects]
            builder.write_text("#!/bin/sh\ntouch \"$1/c00416.h\" \"$1/mtest32.h\"\n")
            make("occam", "all")
            self.assertTrue((root / ".occam.stamp").is_file())
            for obj, previous in zip(objects, before):
                self.assertGreater(obj.stat().st_mtime_ns, previous)
                self.assertGreaterEqual(obj.stat().st_mtime_ns,
                                        (root / ".occam.stamp").stat().st_mtime_ns)
            after = [p.stat().st_mtime_ns for p in objects]
            make()
            self.assertEqual([p.stat().st_mtime_ns for p in objects], after)


if __name__ == "__main__":
    unittest.main()
