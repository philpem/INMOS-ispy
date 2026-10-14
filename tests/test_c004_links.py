#!/usr/bin/env python3
"""C004 helper channel-placement regressions using bootstrap register values.

Run directly with python3 tests/test_c004_links.py; no host compiler is needed.
"""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class C004LinkTests(unittest.TestCase):
    def test_compiled_revision_probe_preserves_switch(self):
        from c004_monitor import Chip, Monitor
        for bits in (16, 32):
            for bootlink, chiplink in ((b, c) for b in range(4) for c in range(4) if b != c):
                for revision in ("a", "b"):
                    selections = list(range(32))
                    cases = range(32) if revision == "b" else (0, 7, 31)
                    for input_port in cases:
                        selections[2] = input_port
                        with self.subTest(bits=bits, bootlink=bootlink,
                                          revision=revision, input=input_port):
                            chip = Chip(revision, selections)
                            vm = Monitor(ROOT / f"c004{bits}.h", bytes([8, chiplink]),
                                         chip, bootlink, chiplink)
                            self.assertEqual(vm.run(), b"\x01\x00" + revision.encode())
                            self.assertEqual(chip.ports, selections)
                            self.assertFalse(chip.pending)
                            self.assertFalse(chip.replies)
                            self.assertTrue(chip.finished)
            # A disconnected port must remain disconnected after a B probe.
            selections[2] = -1
            chip = Chip("b", selections)
            vm = Monitor(ROOT / f"c004{bits}.h", bytes([8, 1]), chip)
            self.assertEqual(vm.run(), b"\x01\x00b")
            self.assertEqual(chip.ports, selections)

    def test_compiled_monitor_enquiry_reset_and_connect(self):
        from c004_monitor import Chip, Monitor
        for bits in (16, 32):
            for bootlink in range(4):
                for chiplink in range(4):
                    if chiplink == bootlink:
                        continue
                    with self.subTest(bits=bits, bootlink=bootlink, chiplink=chiplink):
                        header = ROOT / f"c004{bits}.h"
                        ports = list(range(32))
                        chip = Chip("b", ports)
                        vm = Monitor(header, bytes([2, chiplink]), chip, bootlink, chiplink)
                        self.assertEqual([vm.load(vm.mint + i * vm.word) for i in range(11)],
                                         [vm.mint] * 11)
                        # The return router is separate; inspect the reply queue.
                        self.assertEqual(vm.run(), b"")
                        self.assertEqual(chip.replies, bytes(range(128, 160)))
                        self.assertEqual(chip.wire, bytes(value for port in range(32)
                                                         for value in (2, port)))
                        Monitor(header, bytes([4, chiplink]), chip, bootlink, chiplink).run()
                        self.assertEqual(chip.ports, [-1] * 32)
                        ports = [7] * 32
                        chip = Chip("b", [-1] * 32)
                        Monitor(header, bytes([0, chiplink]) + bytes(ports),
                                chip, bootlink, chiplink).run()
                        self.assertEqual(chip.ports, ports)
                        self.assertTrue(chip.finished)

    def test_compiled_router_halt_resets_link_channels(self):
        from c004_monitor import Chip, Monitor
        for bits in (16, 32):
            for bootlink in range(4):
                # sendid's framed id, then SETPATH with HALT as final link.
                request = bytes.fromhex("02000100ffff000100ff")
                vm = Monitor(ROOT / f"c004{bits}.h", request, Chip("b", [-1] * 32),
                             bootlink=bootlink, chiplink=(bootlink + 1) % 4, process="router")
                vm.run()
                expected = [vm.mint + (bootlink + i) % 4 * vm.word
                            for i in range(1, 4)]
                expected = [address for output in expected for address in (output + 4 * vm.word, output)]
                self.assertEqual(vm.resets, expected)

    def test_compiled_router_forwards_raw_programming(self):
        from c004_monitor import Chip, Monitor
        for bits in (16, 32):
            for bootlink, chiplink in ((b, c) for b in range(4) for c in range(4) if b != c):
                ports = list(range(32))
                stream = bytes(value for output, source in enumerate(ports)
                               for value in (0, source, output)) + b"\x03"
                request = (bytes.fromhex("02000100ffff000100") + bytes([chiplink])
                           + bytes.fromhex("ffff02") + len(stream).to_bytes(2, "little") + stream)
                chip = Chip("b", [-1] * 32)
                vm = Monitor(ROOT / f"c004{bits}.h", request, chip,
                             bootlink=bootlink, chiplink=chiplink, process="router")
                vm.run()
                self.assertEqual(chip.wire, stream)
                self.assertEqual(chip.ports, ports)
                self.assertTrue(chip.finished)


if __name__ == "__main__":
    unittest.main()
