#!/usr/bin/env python3
"""CHECK payload halt must reset hardware channels, preserving the boot link."""

import unittest
from pathlib import Path

from c004_monitor import Chip, Monitor

ROOT = Path(__file__).resolve().parents[1]


class CheckLinkTests(unittest.TestCase):
    def test_halt_resets_hardware_channels(self):
        for bits in (16, 32):
            for bootlink in range(4):
                with self.subTest(bits=bits, bootlink=bootlink):
                    # SENDID framing, followed by SETPATH whose final byte is HALT.
                    request = bytes.fromhex("02000100ffff000100ff")
                    vm = Monitor(ROOT / f"check{bits}.h", request,
                                 Chip("b", [-1] * 32), bootlink=bootlink,
                                 chiplink=(bootlink + 1) % 4, process="router")
                    vm.run()
                    expected = [vm.mint + (((bootlink + i) % 4) + direction) * vm.word
                                for i in range(1, 4) for direction in (4, 0)]
                    self.assertEqual(vm.resets, expected)
                    self.assertFalse(vm.request)
                    self.assertNotIn(vm.mint + bootlink * vm.word, vm.resets)
                    self.assertNotIn(vm.mint + (bootlink + 4) * vm.word, vm.resets)


    def test_raw_forwarding_preserves_program_stream(self):
        # Exercise router I/O and its segmented packet copy as well as halt.
        ports = list(range(32))
        stream = bytes(value for output, source in enumerate(ports)
                       for value in (0, source, output)) + b"\x03"
        for bits in (16, 32):
            for bootlink in range(4):
                for chiplink in range(4):
                    if chiplink == bootlink:
                        continue
                    with self.subTest(bits=bits, bootlink=bootlink, chiplink=chiplink):
                        request = (bytes.fromhex("02000100ffff000100") + bytes([chiplink])
                                   + bytes.fromhex("ffff02")
                                   + len(stream).to_bytes(2, "little") + stream)
                        chip = Chip("b", [-1] * 32)
                        vm = Monitor(ROOT / f"check{bits}.h", request, chip,
                                     bootlink=bootlink, chiplink=chiplink, process="router")
                        vm.run()
                        self.assertEqual(chip.wire, stream)
                        self.assertEqual(chip.ports, ports)
                        self.assertTrue(chip.finished)
                        self.assertFalse(vm.request)


if __name__ == "__main__":
    unittest.main()
