"""A refresh must invalidate old outputs and retain a verifiable source record."""
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from stage import stage


class Staging(unittest.TestCase):
    def test_reuses_wimp_directory_and_invalidates_native_outputs(self):
        with tempfile.TemporaryDirectory() as scratch:
            share = Path(scratch)
            (share / 'net').write_text('preserve')
            (share / 'wimp/o').mkdir(parents=True)
            (share / 'wimp/o/main,ffd').write_bytes(b'old object')
            (share / 'wimp/o/tmblink,ffd').write_bytes(b'old assembly object')
            (share / 'wimp/!Ispy').mkdir()
            (share / 'wimp/!Ispy/!RunImage,ff8').write_bytes(b'old executable')

            def export_fixture(destination, ccres):
                for sub in ('c', 'h', 's', 'o', '!Ispy'):
                    (destination / sub).mkdir(parents=True, exist_ok=True)
                (destination / 'c/main').write_bytes(b'new source')
                (destination / 's/tmblink').write_bytes(b'assembly source')

            with patch('stage.export', export_fixture):
                stage(share, 'trial', 'unused')
                self.assertEqual((share / 'wimp/c/main').read_bytes(), b'new source')
                self.assertFalse((share / 'wimp/o/main,ffd').exists())
                self.assertFalse((share / 'wimp/o/tmblink,ffd').exists())
                self.assertFalse((share / 'wimp/!Ispy/!RunImage,ff8').exists())
                self.assertEqual((share / 'net').read_text(), 'preserve')
                manifest = json.loads((share / 'logs/trialsrc').read_text())
                self.assertEqual(manifest['sha256']['c/main'],
                                 hashlib.sha256(b'new source').hexdigest())
                with self.assertRaises(ValueError):
                    stage(share, 'trial', 'unused')


if __name__ == '__main__':
    unittest.main()
