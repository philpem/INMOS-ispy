#!/usr/bin/env python3
"""Hardware-free C004 host regressions using an in-memory link."""

import os
import random
import shlex
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def apply_commands(stream):
    """Independent C004B model: selection bytes are input, then output.

    Command 3 is required to finish a configuration sequence. Reciprocal
    command 1 and two directed command 0 operations have identical results.
    """
    ports = [-1] * 32
    finished = False
    i = 0
    while i < len(stream):
        command = stream[i]
        i += 1
        if command == 4:
            ports = [-1] * 32
            finished = False
        elif command in (0, 1):
            source, destination = stream[i:i + 2]
            i += 2
            assert 0 <= source < 32 and 0 <= destination < 32
            ports[destination] = source
            if command == 1:
                ports[source] = destination
            finished = False
        elif command == 3:
            finished = True
        else:
            raise AssertionError(f"unexpected C004 command {command}")
    return ports, finished


class C004Tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.work = tempfile.TemporaryDirectory(prefix="ispy-c004-")
        cls.harness = Path(cls.work.name) / "harness"
        command = shlex.split(os.environ.get("CC", "cc"))
        command += shlex.split(os.environ.get("C004_TEST_CFLAGS", "-O1 -g"))
        command += ["-I", str(ROOT), '-DDEFAULTLINK="/dev/link0"',
                    str(ROOT / "tests/c004_harness.c"), str(ROOT / "cklib.c"),
                    "-o", str(cls.harness)]
        subprocess.run(command, check=True, cwd=ROOT)

    @classmethod
    def tearDownClass(cls):
        cls.work.cleanup()

    def run_harness(self, *args, text=None, env=None, ok=True):
        result = subprocess.run([str(self.harness), *map(str, args)], input=text,
                                capture_output=True, text=True, timeout=5,
                                env={**os.environ, **(env or {})})
        if ok:
            self.assertEqual(result.returncode, 0, result.stderr)
        else:
            self.assertEqual(result.returncode, 1, result.stderr)
        return result

    def config(self, result):
        line = next(line for line in result.stdout.splitlines() if line.startswith("CONFIG "))
        return list(map(int, line.split()[1:]))

    def read(self, raw, revision=ord("b"), assumed=1, length=32, **kwargs):
        return self.run_harness("read", assumed, revision, *raw, length, **kwargs)

    def program(self, ports, **kwargs):
        result = self.run_harness("set", *ports, **kwargs)
        wire = bytes.fromhex(result.stdout.split("WIRE ")[1].strip())
        # Parse real ISPY routing/framing, not a reimplementation of setC004.
        self.assertEqual(wire[:8], bytes.fromhex("ffff000100040200"))
        self.assertEqual(wire[8:10], b"\x04\x01")
        stream = bytearray(b"\x04")  # reset sent via the helper application
        if len(wire) > 10:
            self.assertEqual(wire[10:19], bytes.fromhex("ffff00010001ffff02"))
            length = int.from_bytes(wire[19:21], "little")
            self.assertEqual(length, len(wire[21:]))
            stream += wire[21:]
        return bytes(stream)

    def template(self, row):
        return "0 T425b-20 1.0M 0 [ HOST 1:C ... ... ]\n" + row + "\n"

    def test_programmed_map_readback_verification(self):
        for revision in (0, 1):
            self.run_harness("verify", revision, 3, 3)
            result = self.run_harness("verify", revision, 3, 4, ok=False)
            self.assertIn("output 7: requested 3, read back 4", result.stderr)
        self.run_harness("verify", 0, -1, 0)  # C004A cannot expose disconnection.
        self.run_harness("verify", 1, -1, 0, ok=False)
        self.run_harness("verify", 1, -1, -1)

    def test_all_c004b_reply_values(self):
        for start in range(0, 256, 32):
            raw = list(range(start, start + 32))
            expected = [value & 31 if value & 128 else -1 for value in raw]
            with self.subTest(start=start):
                self.assertEqual(self.config(self.read(raw)), expected)

    def test_c004a_mask(self):
        raw = list(range(32, 64))
        self.assertEqual(self.config(self.read(raw, ord("a"), assumed=0)), list(range(32)))

    def test_invalid_revision_rejected(self):
        for revision in (0, 255):
            self.read([0] * 32, revision, ok=False)
        for length in (0, 2):
            self.read([0] * 32, env={"REVISION_LENGTH": str(length)}, ok=False)

    def test_revision_overrides_assumption(self):
        self.assertEqual(self.config(self.read([0] * 32, assumed=0)), [-1] * 32)

    def test_short_configuration_rejected(self):
        for length in (0, 31, 33):
            self.read([128] * 32, length=length, ok=False)

    def test_fragmented_configuration(self):
        self.assertEqual(self.config(self.read(list(range(128, 160)), env={"READ_CHUNK": "1"})), list(range(32)))

    def test_fragmented_packet_write(self):
        result = self.run_harness("packet", env={"WRITE_CHUNK": "1"})
        wire = bytes.fromhex(result.stdout.split("WIRE ")[1].strip())
        self.assertEqual(wire, b"\x20\x00" + bytes(range(32)))

    def test_programming_semantics(self):
        rng = random.Random(4004)
        cases = [[-1] * 32, [1, 0] + [-1] * 30, list(range(32)),
                 [7] * 32, [-1] * 31 + [0]]
        cases += [[rng.randrange(-1, 32) for _ in range(32)] for _ in range(80)]
        for index, ports in enumerate(cases):
            with self.subTest(index=index):
                actual, finished = apply_commands(self.program(ports))
                self.assertEqual(actual, ports)
                self.assertTrue(finished, "ISPY omitted the configuration-complete command")

    def test_invalid_programming_map(self):
        for bad in (32, -2, 127):
            result = self.run_harness("set", *([bad] + [-1] * 31),
                                      env={"TRACE_WRITES": "1"}, ok=False)
            self.assertIn("invalid input", result.stderr)
            self.assertNotIn("mock WriteLink", result.stderr)

    def test_reset_history_preserves_complete_routes(self):
        self.run_harness("reset-history")

    def test_programming_transfer_failure(self):
        # Routing: writes 1..3; reset header/payload: 4..5; raw routing:
        # 6..9; programming header/payload: 10..11.
        for failure in (4, 5, 10, 11):
            self.run_harness("set", *range(32), env={"FAIL_WRITE": str(failure)}, ok=False)

    def test_compact_output_round_trip(self):
        row = "1 C004b [ 01234567 89ABCDEF GHIJKLMN OPQRSTUV ]"
        result = self.run_harness("parse", text=self.template(row))
        self.assertEqual(self.config(result), list(range(32)))
        rows = [line for line in result.stdout.splitlines() if line.startswith("   1 C004b")]
        self.assertEqual(len(rows), 2)
        for row in rows:
            self.assertEqual(self.config(self.run_harness("parse", text=self.template(row))), list(range(32)))

    def test_directed_and_multicast_round_trip(self):
        # Documented long syntax is INPUT>OUTPUT; fan-out is legal.
        result = self.run_harness("parse", text=self.template("1 C004b (7>0 7>1 2-3 0>31)"))
        ports = [-1] * 32
        ports[0] = ports[1] = 7
        ports[2], ports[3], ports[31] = 3, 2, 0
        self.assertEqual(self.config(result), ports)
        for row in result.stdout.splitlines():
            if row.startswith("   1 C004b"):
                self.assertEqual(self.config(self.run_harness("parse", text=self.template(row))), ports)

    def test_invalid_templates(self):
        for row in ("1 C004b (32>0)", "1 C004b (-1>0)", "1 C004b (0>32)",
                    "1 C004b (0>)", "1 C004b (0>1", "1 C004b (0>1 2>1)",
                    "1 C004b (999999999999999999999999>0)",
                    "1 C004b [" + "0" * 33 + "]", "1 C004b [Z]", "1 C004b [0"):
            with self.subTest(row=row):
                self.run_harness("parse", text=self.template(row), ok=False)

if __name__ == "__main__":
    unittest.main()
