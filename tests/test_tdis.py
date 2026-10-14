"""Regressions for target operands and recursive code-pointer discovery."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import tdis


class TdisTests(unittest.TestCase):
    def test_prefix_operands_and_branch_targets(self):
        # Include padding prefixes and a negative prefix followed by a positive
        # prefix, as emitted in the checked-in Occam helpers.
        for raw, operand in (("60 bc", -4), ("20 20 60 bc", -4),
                             ("61 2c 01", -319), ("21 60 4f", -257),
                             ("60 60 40", 240), ("22 2a 4a", 682)):
            with self.subTest(raw=raw):
                code = bytes.fromhex(raw)
                length, _, kind, target, value = tdis.decode(code, 0)
                self.assertEqual(length, len(code))
                self.assertEqual(value, operand)
                if kind == "j":
                    self.assertEqual(target, len(code) + operand)

    def test_backward_branch_reachability(self):
        # LDC 0; J -4 returns to the first instruction. Trailing data is unused.
        code = bytes.fromhex("40 20 60 0c ff")
        self.assertEqual(tdis.decode(code, 1)[3], 0)
        starts, _, seen = tdis.analyse(code)
        self.assertEqual(starts, {0, 1})
        self.assertEqual(seen, set(range(4)))

    def test_truncated_prefix(self):
        self.assertEqual(tdis.decode(bytes.fromhex("20 61"), 0),
                         (2, "(truncated)", "other", None, None))

    def test_ldpi_forward_pointer_uses_end_of_instruction(self):
        code = bytes.fromhex("44 21 fb 22 f0 aa aa 22 f0")
        starts, _, seen = tdis.analyse(code)
        self.assertEqual(tdis.codepointer_seeds(code, starts), [7])
        self.assertIn(7, starts)
        self.assertNotIn(5, seen)
        self.assertNotIn(6, seen)

    def test_ldpi_backward_pointer(self):
        # Jump over a process body, then form its address with a negative LDC.
        code = bytes.fromhex("03 22 f0 aa 60 48 21 fb 22 f0")
        starts, _, seen = tdis.analyse(code)
        self.assertEqual(tdis.codepointer_seeds(code, starts), [0])
        # Change offset from -8 to -7: target becomes the skipped process at 1.
        code = code[:5] + b"\x49" + code[6:]
        starts, _, seen = tdis.analyse(code)
        self.assertEqual(tdis.codepointer_seeds(code, starts), [1])
        self.assertIn(1, starts)
        self.assertNotIn(3, seen)

    def test_ldpi_outside_blob_does_not_seed_code(self):
        for offset in ("4f", "60 40"):
            code = bytes.fromhex(offset + " 21 fb 22 f0")
            starts, _, _ = tdis.analyse(code)
            self.assertEqual(tdis.codepointer_seeds(code, starts), [])


if __name__ == "__main__":
    unittest.main()
