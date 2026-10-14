"""Check relocation auditing and source/map agreement without external ROMs."""
import importlib.util
import json
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('ucode_audit', ROOT / 'tools/ucode-audit.py')
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


class MicrocodeAudit(unittest.TestCase):
    def test_condition_probe_rejects_status_overwrite_even_with_correct_mux(self):
        bits = ['0'] * 118
        bits[30] = '1'  # CondMux1 input 8.
        rom = ({0: ''.join(bits)}, {'CondMux1': [31, 32, 28, 30]})
        audit.check_condition_probe(rom, 0, 1, 8)
        bits[36] = '1'
        rom[0][0] = ''.join(bits)
        with self.assertRaisesRegex(ValueError, 'overwrites STATUS'):
            audit.check_condition_probe(rom, 0, 1, 8)

    def test_two_reads_probe_uses_a_status_preserving_microstep(self):
        for family in ('85', '45'):
            text = (ROOT / 'occam' / ('test' + family + 'cm.occ')).read_text()
            section = text.split('--{{{  Cond1FromNotTwoReadsToStart', 1)[1].split(
                '--{{{  Cond1FromProcPriority', 1)[0]
            self.assertEqual(re.findall(r'OPR u\.(\w+)', section),
                             ['moveformwritecount', 'moveformwritecount'])

    def rom(self, rows):
        # Synthetic control bit, Next bit, two selector bits, two feedback bits.
        return rows, {'Next': [1], 'CondMux0': [2], 'CondMux1': [3],
                      'ROMFeedback': [4, 5]}

    def test_entry_comparison_follows_relocated_continuations(self):
        left = self.rom({0: '100010', 1: '010000'})
        right = self.rom({0: '100001', 2: '010000'})
        self.assertTrue(audit.equivalent_entry(left, 0, right, 0))
        wrong = self.rom({0: '100001', 2: '110000'})
        self.assertFalse(audit.equivalent_entry(left, 0, wrong, 0))

    def test_entry_comparison_checks_each_conditional_successor(self):
        left = self.rom({2: '101000', 0: '010000', 1: '110000'})
        right = self.rom({2: '101000', 0: '010000', 1: '010000'})
        self.assertFalse(audit.equivalent_entry(left, 2, right, 2))

    def test_map_matches_named_includes_and_covers_every_source_operation(self):
        manifest = json.loads((ROOT / 'diagnostics/microsteps.json').read_text())
        for family, include, prefix in zip(manifest['families'], ('u805', 'u425'), ('85', '45')):
            constants = {name: int(number, 16) for name, number in re.findall(
                r'VAL INT u\.(\w+) IS #([0-9A-F]+)', (ROOT / 'occam' / (include + '.occ')).read_text())}
            expected = {name: item['address'] | (0x800 if item['step'] else 0)
                        for name, item in family['operations'].items()}
            self.assertEqual(constants, expected)
            used = set()
            for name in ['probe' + prefix] + ['test' + prefix + part for part in ('cm', 'st', 'co', 'in')]:
                text = (ROOT / 'occam' / (name + '.occ')).read_text()
                self.assertNotIn('PFIX', text)
                used.update(re.findall(r'OPR u\.(\w+)', text))
                self.assertLessEqual(set(re.findall(r'^\s*STEP[ \t]+(\w+)', text, re.M)),
                                     set(family['symbolic_steps']))
                header = (ROOT / (name + '.h')).read_text()
                size = int(re.search(r'(\d+),\s*/\* code size', header)[1])
                workspace = int(re.search(r'(\d+),\s*/\* workspace', header)[1])
                self.assertLess(size + (workspace + 3) * 4 + 0x70 + 4, 0x1000)
            self.assertEqual(used, set(expected))
