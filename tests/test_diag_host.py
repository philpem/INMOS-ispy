"""Host orchestration, decoder validation and preservation of partial results."""
import os
import shlex
import subprocess
import tempfile
import unittest
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
class DiagnosticHost(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.dir = Path(cls.temp.name)
        for name, flags in [('ltest', []), ('mtest', ['-DMEMORY_TEST'])]:
            subprocess.run(['cc', '-std=c90', '-pedantic-errors',
                            *shlex.split(os.getenv('C004_TEST_CFLAGS', '-O1 -g')),
                            *flags, '-I', str(ROOT), str(ROOT/f'{name}.c'),
                            str(ROOT/'tests/diag_host.c'), '-o', str(cls.dir/name)], check=True)
    @classmethod
    def tearDownClass(cls): cls.temp.cleanup()
    def run_case(self, name, scenario='pass', args=()):
        r = subprocess.run([str(self.dir/name), *args], capture_output=True, text=True,
                           timeout=5, env={**os.environ, 'DIAG_CASE': scenario})
        self.assertNotIn('AddressSanitizer', r.stderr)
        self.assertNotIn('runtime error:', r.stderr)
        return r
    def test_tests_tree_and_non_tree_connections_in_both_directions(self):
        r = self.run_case('ltest')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(r.stdout.count(' PASS'), 6)
        self.assertIn('PAIR 0:2 2:2', r.stderr)
        self.assertIn('PAIR 1:2 2:1', r.stderr)
        self.assertEqual(r.stderr.count('LOAD'), 3)
    def test_link_corruption_is_directional_and_fails(self):
        r = self.run_case('ltest', 'corrupt')
        self.assertEqual(r.returncode, 1)
        self.assertEqual(r.stdout.count(' FAIL'), 6)
        self.assertIn('errors 1/2', r.stdout)
    def test_link_failures_stop_routing_and_mark_remaining_links(self):
        for case in ('timeout', 'short', 'bad-ready', 'bad-checksum', 'bad-counter', 'route-fail', 'load-fail'):
            with self.subTest(case=case):
                r = self.run_case('ltest', case)
                self.assertEqual(r.returncode, 1, r.stderr)
                self.assertNotIn(' PASS', r.stdout)
                self.assertIn('NOT TESTED', r.stdout)
    def test_completed_link_results_survive_later_timeout(self):
        r = self.run_case('ltest', 'later-timeout')
        self.assertEqual(r.returncode, 1)
        self.assertEqual(r.stdout.count(' PASS'), 2)
        self.assertIn('NOT COMPLETED', r.stdout)
        self.assertIn('NOT TESTED', r.stdout)
    def test_sparse_memory_result_is_explicit_about_coverage(self):
        r = self.run_case('mtest', args=['-z', '-l', '-e', '8'])
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('SPARSE', r.stdout)
        self.assertEqual(r.stdout.count('8K|'), 3)
    def test_memory_alias_cannot_be_reported_as_a_size(self):
        r = self.run_case('mtest', 'alias', ['-z', '-l', '-e', '8'])
        self.assertEqual(r.returncode, 1)
        self.assertEqual(r.stdout.count('UNKNOWN?'), 3)
        self.assertNotIn('8K|', r.stdout)
    def test_memory_read_failure_keeps_incomplete_nodes(self):
        r = self.run_case('mtest', 'timeout', ['-z', '-l'])
        self.assertEqual(r.returncode, 1)
        self.assertEqual(r.stdout.count('2K?'), 3)
    def test_invalid_limits_fail_before_open(self):
        for name, args in [('ltest', ['-n', '0']), ('ltest', ['-n', '257']),
                           ('mtest', ['-e', '8x']), ('mtest', ['-e', '3'])]:
            r = self.run_case(name, args=args)
            self.assertEqual(r.returncode, 1)
            self.assertNotIn('OPEN LINK', r.stderr)

    def test_memory_load_and_parameter_failures_mark_untested(self):
        for case in ('load-fail', 'route-fail', 'send-fail'):
            with self.subTest(case=case):
                r = self.run_case('mtest', case, ['-z', '-l'])
                self.assertEqual(r.returncode, 1, r.stderr)
                self.assertEqual(r.stdout.count('NOT TESTED'), 3)
    def test_empty_class_selection_is_not_a_successful_test(self):
        r = self.run_case('mtest', args=['-t2', '-l'])
        self.assertEqual(r.returncode, 1)
        self.assertIn('No processors tested', r.stderr)
        self.assertNotIn('LOAD', r.stderr)
