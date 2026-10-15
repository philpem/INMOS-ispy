"""Exporter contracts: topology, switch direction and output suitable for configurers."""
import os
import shlex
import subprocess
import tempfile
import unittest
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
SNAPSHOT = '''Using mock ispy 3.23
# Part rate Link [ Link0 Link1 Link2 Link3 ]
# Extra diagnostic comment
0 T425b-20 1.0M 0 [ HOST 1:1 1:2 2:C ]
1 T800d-25 1.0M 1 [ ... 0:1 0:2 ... ]
2 C004b (7>0 7>1 2-3 0>31)
'''
class NetworkExports(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.exe = Path(cls.temp.name)/'netcfg'
        subprocess.run(['cc', '-std=c90', '-pedantic-errors',
                        *shlex.split(os.getenv('C004_TEST_CFLAGS', '-O1 -g')),
                        '-I', str(ROOT), str(ROOT/'netcfg.c'), str(ROOT/'cklib.c'),
                        str(ROOT/'b004link.c'), '-o', str(cls.exe)], check=True)
    @classmethod
    def tearDownClass(cls): cls.temp.cleanup()
    def export(self, flag, text=SNAPSHOT):
        return subprocess.run([str(self.exe), flag], input=text, text=True,
                              capture_output=True, timeout=5)
    def test_c_format_deduplicates_edges_and_omits_control_connections(self):
        r = self.export('-cc')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('T425 node0;', r.stdout)
        self.assertIn('T800 node1;', r.stdout)
        self.assertEqual(r.stdout.count('connect '), 3)
        self.assertIn('connect host to node0.link[0];', r.stdout)
        self.assertNotIn('node2', r.stdout)
        self.assertNotIn('#', r.stdout)
    def test_occam_has_complete_hardware_section(self):
        r = self.export('-co')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('NODE node0:', r.stdout)
        self.assertIn('ARC hostlink:', r.stdout)
        self.assertIn('SET node1 (type := "T800")', r.stdout)
        self.assertEqual(r.stdout.count('CONNECT'), 3)
        self.assertTrue(r.stdout.endswith(':\n'))
        self.assertNotIn('#', r.stdout)
    def test_switch_map_preserves_direction_fanout_and_physical_path(self):
        r = self.export('-sw')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('path 3', r.stdout)
        self.assertIn('connect 7>0 7>1 3>2 2>3 0>31', r.stdout)
    def test_switch_export_requires_captured_selections(self):
        r = self.export('-sw', SNAPSHOT.replace('(7>0 7>1 2-3 0>31)', ''))
        self.assertEqual(r.returncode, 1)
        self.assertIn('configuration', r.stderr)
        self.assertNotIn('switch', r.stdout)
    def test_default_snapshot_cannot_export_unread_switch_as_empty(self):
        text = SNAPSHOT.replace('2:C', 'C004').replace('2 C004b (7>0 7>1 2-3 0>31)\n', '')
        r = self.export('-sw', text)
        self.assertEqual(r.returncode, 1)
        self.assertIn('use ispy -c4', r.stderr)
    def test_nested_switch_path(self):
        text = SNAPSHOT.replace('1:2 2:C', '1:2 ...').replace('0:2 ...', '0:2 2:C')
        r = self.export('-sw', text)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('path 1 3', r.stdout)
    def test_bad_topology_fails(self):
        for text in (SNAPSHOT.replace('0:2 ...', '0:3 ...'),
                     SNAPSHOT.replace('HOST', '...'),
                     SNAPSHOT.replace(' 0 [ HOST', ' 4 [ HOST')):
            with self.subTest(text=text):
                r = self.export('-cc', text)
                self.assertEqual(r.returncode, 1)
                self.assertNotIn('connect ', r.stdout)
    def test_generic_processor_types_are_not_falsely_exported(self):
        r = self.export('-cc', SNAPSHOT.replace('T425b', 'T32  '))
        self.assertNotEqual(r.returncode, 0)
    def test_named_file_matches_stdin(self):
        path = Path(self.temp.name)/'network'
        path.write_text(SNAPSHOT)
        r = subprocess.run([str(self.exe), '-sw', '-f', str(path)],
                           capture_output=True, text=True, timeout=5)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(r.stdout, self.export('-sw').stdout)
