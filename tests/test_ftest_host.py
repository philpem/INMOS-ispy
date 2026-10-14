"""Exercise FTEST failure reporting without transputer hardware."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent


@unittest.skipUnless(shutil.which('gcc'), 'GCC required')
class FtestHost(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory()
        cls.binary = Path(cls.temporary.name) / 'ftest'
        subprocess.run(['gcc', '-std=c90', '-pedantic-errors', '-g',
                        '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                        '-fno-pie', '-no-pie', '-I' + str(ROOT),
                        str(ROOT / 'ftest.c'), str(ROOT / 'testlater.c'),
                        str(ROOT / 'tests/ftest_mock.c'),
                        '-o', str(cls.binary)], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def run_case(self, scenario, arguments=(), chip=16):
        result = subprocess.run([str(self.binary), '-l', *arguments],
                                env={**os.environ, 'FTEST_SCENARIO': scenario,
                                     'FTEST_TYPE': str(chip)},
                                capture_output=True, text=True, timeout=5)
        self.assertNotIn('AddressSanitizer', result.stderr)
        self.assertNotIn('runtime error:', result.stderr)
        return result

    def test_success_restores_router_and_visits_downstream(self):
        result = self.run_case('pass')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('RESTORE ROUTER 0', result.stderr)
        self.assertIn('VISIT PROCESSOR 1', result.stderr)
        self.assertIn('1 T805g-25  (OK)', result.stdout)

    def test_transport_failure_reports_root_without_attempting_downstream(self):
        for scenario in ('timeout', 'empty', 'malformed'):
            with self.subTest(scenario=scenario):
                result = self.run_case(scenario)
                self.assertEqual(result.returncode, 1)
                self.assertIn('Processor 0: TEST85CM:', result.stderr)
                self.assertNotIn('VISIT PROCESSOR 1', result.stderr)
                self.assertNotIn('RESTORE ROUTER 0', result.stderr)
                self.assertNotIn('ANALYSE ROOT', result.stderr)
                self.assertIn('0 T805g-25 (BAD)', result.stdout)
                self.assertIn('1 T805g-25 (NOT TESTED)', result.stdout)

    def test_diagnostic_failure_preserves_routing_and_returns_failure(self):
        result = self.run_case('fail')
        self.assertEqual(result.returncode, 1)
        self.assertIn('FAIL Conditional Mux', result.stderr)
        self.assertIn('RESTORE ROUTER 0', result.stderr)
        self.assertIn('VISIT PROCESSOR 1', result.stderr)
        self.assertIn('# Errors,Conditional Mux', result.stdout)

    def test_accumulated_errors_are_bounded(self):
        result = self.run_case('long-errors')
        self.assertEqual(result.returncode, 1)
        self.assertIn('...', result.stdout)
        self.assertIn('VISIT PROCESSOR 1', result.stderr)

    def test_maximum_progress_packet_is_accepted(self):
        result = self.run_case('long-progress')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('PASS ' + 'x' * 126, result.stderr)

    def test_target_checkpoint_is_not_reported_as_a_pass(self):
        result = self.run_case('checkpoint')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('Processor 0: BEGIN Conditional Mux', result.stderr)
        self.assertNotIn('PASS BEGIN', result.stderr)

    def test_opt_in_analyse_decodes_stopped_root(self):
        result = self.run_case('timeout', ['-a'])
        self.assertEqual(result.returncode, 1)
        self.assertIn('SEND STATE BOOTSTRAP', result.stderr)
        self.assertIn('Iptr=12345678 Wdesc=80000101 boot=80000010 flags=00000001',
                      result.stderr)
        self.assertNotIn('VISIT PROCESSOR 1', result.stderr)

    def test_short_analyse_response_is_not_decoded(self):
        result = self.run_case('analyse-short', ['-a'])
        self.assertEqual(result.returncode, 1)
        self.assertIn('Root Analyse snapshot unavailable', result.stderr)
        self.assertNotIn('Iptr=', result.stderr)

    def test_analyse_is_not_used_on_success(self):
        result = self.run_case('pass', ['-a'])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotIn('ANALYSE ROOT', result.stderr)

    def test_selecting_downstream_retains_root_router(self):
        result = self.run_case('timeout', ['-n', '1'])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotIn('Processor 0: TEST', result.stderr)
        self.assertNotIn('RESTORE ROUTER 0', result.stderr)
        self.assertIn('VISIT PROCESSOR 1', result.stderr)
        self.assertIn('RESTORE ROUTER 1', result.stderr)
        self.assertIn('0 T805g-25 (NOT TESTED)', result.stdout)
        self.assertIn('1 T805g-25  (OK)', result.stdout)

    def test_invalid_processor_selection_is_rejected(self):
        for args in (['-n'], ['-n', '-1'], ['-n', '1x'], ['-n', '999999999999999999999999'], ['-n', '2']):
            with self.subTest(args=args):
                result = self.run_case('pass', args)
                self.assertNotEqual(result.returncode, 0)
                self.assertNotIn('VISIT PROCESSOR', result.stderr)
                self.assertNotIn('RESTORE ROUTER', result.stderr)

    def test_skip_801_reports_partial_coverage_and_keeps_routing(self):
        result = self.run_case('timeout', ['-s'])
        self.assertEqual(result.returncode, 0, result.stderr)
        for processor in (0, 1):
            self.assertIn(f'Processor {processor}: TESTT32A', result.stderr)
            self.assertIn(f'Processor {processor}: TEST80XA', result.stderr)
            self.assertNotIn(f'Processor {processor}: TEST801A', result.stderr)
            self.assertIn(f'Processor {processor}: SKIP T801/T805', result.stderr)
            self.assertIn(f'RESTORE ROUTER {processor}', result.stderr)
            self.assertIn(f'{processor} T805g-25 (PARTIAL)', result.stdout)
        self.assertEqual(result.stdout.count('# Skipped T801/T805'), 2)
        self.assertNotIn('(OK)', result.stdout)

    def test_skip_with_selection_applies_only_to_tested_processor(self):
        result = self.run_case('timeout', ['-n', '1', '-s'])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('0 T805g-25 (NOT TESTED)', result.stdout)
        self.assertIn('1 T805g-25 (PARTIAL)', result.stdout)
        self.assertEqual(result.stdout.count('# Skipped T801/T805'), 1)

    def test_skip_does_not_hide_other_diagnostic_failures(self):
        result = self.run_case('fail-before-skip', ['-s'])
        self.assertEqual(result.returncode, 1)
        self.assertIn('0 T805g-25 (BAD)', result.stdout)
        self.assertIn('# Errors,Integer ALU', result.stdout)
        self.assertIn('1 T805g-25 (PARTIAL)', result.stdout)
        self.assertEqual(result.stdout.count('# Skipped T801/T805'), 2)

    def test_revision_dispatch_selects_all_four_later_payloads(self):
        for chip, family in ((21, '85'), (13, '85'), (16, '85'), (1, '45'), (2, '45')):
            with self.subTest(chip=chip):
                result = self.run_case('pass', chip=chip)
                self.assertEqual(result.returncode, 0, result.stderr)
                for part in ('CM', 'ST', 'CO', 'IN'):
                    self.assertIn(f'LOAD TEST{family}{part} 0', result.stderr)
                    self.assertIn(f'LOAD TEST{family}{part} 1', result.stderr)
                self.assertIn('RESTORE ROUTER 1', result.stderr)
                self.assertNotIn('(PARTIAL)', result.stdout)

    def test_unknown_revisions_run_common_tests_and_omit_chip_payloads(self):
        for chip in (20, 10, 11, 12, 14, 15, 17, 3, -10, -9):
            with self.subTest(chip=chip):
                result = self.run_case('pass', chip=chip)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertIn('TESTT32A', result.stderr)
                self.assertIn('unsupported revision', result.stdout)
                self.assertNotIn('LOAD TEST85', result.stderr)
                self.assertNotIn('LOAD TEST45', result.stderr)
                self.assertIn('(PARTIAL)', result.stdout)
                self.assertIn('RESTORE ROUTER 1', result.stderr)

    def test_probe_only_preserves_routing_and_reports_reduced_coverage(self):
        result = self.run_case('pass', ['-p'])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('LOAD PROBE85 0', result.stderr)
        self.assertIn('LOAD PROBE85 1', result.stderr)
        self.assertNotIn('TESTT32A', result.stderr)
        self.assertNotIn('LOAD TEST85', result.stderr)
        self.assertIn('probe-only run', result.stdout)
        self.assertIn('RESTORE ROUTER 1', result.stderr)

    def test_probe_timeout_stops_without_attempting_downstream(self):
        result = self.run_case('probe-timeout', ['-p', '-a'])
        self.assertEqual(result.returncode, 1)
        self.assertIn('Processor 0: PROBE85: TIMEOUT', result.stderr)
        self.assertIn('ANALYSE ROOT', result.stderr)
        self.assertNotIn('RESTORE ROUTER', result.stderr)
        self.assertNotIn('VISIT PROCESSOR 1', result.stderr)

    def test_selected_subtest_omits_other_three_and_reports_partial(self):
        for name, part in (('cm', 'CM'), ('stack', 'ST'), ('const', 'CO'), ('inc', 'IN')):
            with self.subTest(name=name):
                result = self.run_case('pass', ['-d', name, '-n', '1'])
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertIn(f'LOAD TEST85{part} 1', result.stderr)
                self.assertEqual(result.stderr.count('LOAD TEST85'), 1)
                self.assertIn('other three microcode subtests', result.stdout)
                self.assertIn('0 T805g-25 (NOT TESTED)', result.stdout)
                self.assertIn('1 T805g-25 (PARTIAL)', result.stdout)

    def test_conflicting_probe_skip_and_subtest_options_are_rejected(self):
        for arguments in (['-p', '-s'], ['-p', '-d', 'cm'], ['-s', '-d', 'inc'], ['-d'], ['-d', 'unknown']):
            result = self.run_case('pass', arguments)
            self.assertNotEqual(result.returncode, 0)
            self.assertNotIn('LOAD ', result.stderr)


if __name__ == '__main__':
    unittest.main()
