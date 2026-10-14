"""Execute C004 monitor and outgoing-router code from the T2/TA blobs.

This small interpreter covers instructions reached by selected requests. Link
I/O and the A/B probe's timed ALT are synchronous test doubles; other router
processes, boot loading, link handshakes and electrical timing are not modelled.
Unsupported instructions fail explicitly. Word size comes from the header.
"""

import re
from pathlib import Path


class Complete(Exception):
    pass


class Chip:
    def __init__(self, revision, selections):
        self.revision = revision
        self.ports = list(selections)
        self.pending = []
        self.replies = bytearray()
        self.wire = bytearray()
        self.finished = True

    def send(self, data):
        for byte in data:
            self.wire.append(byte)
            self.pending.append(byte)
            command = self.pending[0]
            sizes = {0: 3, 1: 3, 2: 2, 3: 1, 4: 1, 5: 2, 6: 3}
            if self.revision == "a" and command in (5, 6):
                sizes[command] = 1  # Revision A treats disconnect as a no-op.
            if command not in sizes:
                raise AssertionError(f"undefined C004 command {command}")
            if len(self.pending) != sizes[command]:
                continue
            args = self.pending[1:]
            self.pending = []
            if any(port > 31 for port in args):
                raise AssertionError(f"invalid C004 port operand in {[command, *args]}")
            if command in (0, 1):
                source, output = args
                self.ports[output] = source
                if command == 1:
                    self.ports[source] = output
                self.finished = False
            elif command == 2:
                source = self.ports[args[0]]
                if self.revision == "b":
                    self.replies.append(0 if source < 0 else 128 | source)
                else:
                    self.replies.append(source & 31)
            elif command == 3:
                self.finished = True
            elif command == 4:
                self.ports = [-1] * 32
            elif command == 5 and self.revision == "b":
                self.ports[args[0]] = -1
            elif command == 6 and self.revision == "b":
                for output in args:
                    self.ports[output] = -1


class Monitor:
    def __init__(self, header, request, chip, bootlink=0, chiplink=1, process="monitor"):
        text = Path(header).read_text()
        code = text.split("/* code */")[1].split("},")[0]
        self.code = bytes(int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]{2})", code))
        self.word = int(re.search(r"(\d+),\s*/\* bytes per word", text)[1])
        self.mask = (1 << (8 * self.word)) - 1
        self.mint = 1 << (8 * self.word - 1)
        self.mem = {}
        self.a = self.b = self.c = 0
        self.wp = 0x6000
        self.pc = int(re.search(r"(\d+),\s*/\* offset", text)[1])
        self.codebase = 0x1000
        self.chip = chip
        self.chiplink = chiplink
        self.bootlink = bootlink
        self.request = bytearray(len(request).to_bytes(2, "little") + request
                                 if process == "monitor" else request)
        self.result = bytearray()
        self.monitor = False
        self.process = process
        self.resets = []
        self.selected = None
        self.store(self.wp + self.word, bootlink)
        for offset, byte in enumerate(self.code):
            self.mem[self.codebase + offset] = byte
        # boot.h initializes all eleven link/timer words to NotProcess (MINT).
        # Channel-pointer tables must be constructed by the compiled helper.
        for index in range(11):
            self.store(self.mint + index * self.word, self.mint)

    def signed(self, value):
        value &= self.mask
        return value - self.mask - 1 if value & self.mint else value

    def load(self, address):
        return sum(self.mem.get((address + i) & self.mask, 0) << (8 * i) for i in range(self.word))

    def store(self, address, value):
        for i in range(self.word):
            self.mem[(address + i) & self.mask] = (value >> (8 * i)) & 255

    def push(self, value):
        self.c, self.b, self.a = self.b, self.a, value & self.mask

    def pop(self):
        self.a, self.b = self.b, self.c

    def input(self, channel, pointer, count):
        if channel == self.mint + (4 + self.chiplink) * self.word:
            queue = self.chip.replies
        elif (channel == self.mint + (4 + self.bootlink) * self.word
              or channel < self.mint):
            queue = self.request
            if not queue:
                raise Complete
        else:
            raise AssertionError(f"unexpected input channel {channel:#x}")
        if len(queue) < count:
            raise AssertionError("monitor attempted an input without enough data")
        for i, byte in enumerate(queue[:count]):
            self.mem[(pointer + i) & self.mask] = byte
        del queue[:count]

    def output(self, channel, data):
        if channel == self.mint + self.chiplink * self.word:
            self.chip.send(data)
        elif channel == self.mint + self.bootlink * self.word:
            self.result += data
        elif self.mint <= channel < self.mint + 11 * self.word:
            raise AssertionError(f"unexpected output channel {channel:#x}")
        # Soft-channel pause messages are acknowledged by a test double.

    def run(self):
        try:
            for _ in range(10000):
                self.step()
        except Complete:
            return bytes(self.result)
        raise AssertionError("monitor did not complete the request")

    def step(self):
        operand = 0
        while True:
            byte = self.code[self.pc]
            self.pc += 1
            opcode = byte >> 4
            operand |= byte & 15
            if opcode == 2:
                operand <<= 4
            elif opcode == 6:
                operand = (~operand) << 4
            else:
                break
        value = self.signed(operand)
        if opcode == 0: self.pc += value
        elif opcode == 1: self.push(self.wp + value * self.word)
        elif opcode == 3: self.a = self.load(self.a + value * self.word)
        elif opcode == 4: self.push(value)
        elif opcode == 5: self.a = (self.a + value * self.word) & self.mask
        elif opcode == 7: self.push(self.load(self.wp + value * self.word))
        elif opcode == 8: self.a = (self.a + value) & self.mask
        elif opcode == 9:
            old_wp = self.wp
            self.wp = (old_wp - 4 * self.word) & self.mask
            for slot, content in enumerate((self.pc, self.a, self.b, self.c)):
                self.store(self.wp + slot * self.word, content)
            self.a = self.pc
            self.pc += value
        elif opcode == 10:
            if self.a == 0: self.pc += value
            else: self.pop()
        elif opcode == 11: self.wp = (self.wp + value * self.word) & self.mask
        elif opcode == 12: self.a = int(self.a == (value & self.mask))
        elif opcode == 13: self.store(self.wp + value * self.word, self.a); self.pop()
        elif opcode == 14:
            self.store(self.a + value * self.word, self.b)
            self.a = self.c
        elif opcode == 15: self.operate(operand)
        else: raise AssertionError(f"unsupported primary opcode {opcode}")

    def operate(self, op):
        if op == 0: self.a, self.b = self.b, self.a
        elif op == 1: self.a = self.mem.get(self.a, 0)
        elif op in (2, 4, 5, 10, 12, 0x46):
            a, b = self.a, self.b
            self.a = {2: lambda: b + a, 4: lambda: b - a, 5: lambda: b + a,
                      10: lambda: a + b * self.word, 12: lambda: b - a,
                      0x46: lambda: b & a}[op]() & self.mask
            self.b = self.c
        elif op == 7:
            self.input(self.b, self.c, self.a)
        elif op == 9:
            self.a = int(self.signed(self.b) > self.signed(self.a)); self.b = self.c
        elif op == 11:
            self.output(self.b, bytes(self.mem.get((self.c + i) & self.mask, 0) for i in range(self.a)))
        elif op == 13:
            # The first PAR child is the monitor. Run it directly; the router
            # processes are replaced by the synchronous I/O doubles above.
            if self.process == "router": return
            if self.monitor: raise AssertionError("unexpected nested process")
            self.pc += self.signed(self.b)
            self.wp = self.a
            self.monitor = True
        elif op in (14, 15):
            self.output(self.b, (self.a & ((1 << (8 if op == 14 else 8 * self.word)) - 1)).to_bytes(1 if op == 14 else self.word, "little"))
            self.a = self.c
        elif op == 0x1B: self.a = (self.a + self.codebase + self.pc) & self.mask
        elif op == 0x12:
            address = self.a
            self.resets.append(address)
            self.a = self.load(address)
            self.store(address, self.mint)
        elif op == 0x20:
            self.pc = self.load(self.wp)
            self.wp = (self.wp + 4 * self.word) & self.mask
        elif op == 0x1F: self.a = self.signed(self.b) % self.signed(self.a); self.b = self.c
        elif op == 0x21:
            count = self.load(self.b + self.word) - 1
            self.store(self.b + self.word, count)
            if count > 0:
                self.store(self.b, self.load(self.b) + 1)
                self.pc -= self.signed(self.a)
        elif op == 0x22: self.push(0)  # ldtimer
        elif op == 0x2E:  # disable timer guard
            offset = self.a
            self.a = int(not self.chip.replies)
            if self.a: self.selected = offset
        elif op == 0x2F:  # disable input guard
            offset = self.a
            self.a = int(bool(self.chip.replies))
            if self.a: self.selected = offset
        elif op == 0x3A:  # xword: extend B from the sign bit specified by A
            self.a = (self.b - (self.a << 1) if self.b & self.a else self.b) & self.mask
            self.b = self.c
        elif op == 0x3B:
            self.mem[self.a] = self.b & 255; self.a = self.c
        elif op == 0x42: self.push(self.mint)
        elif op == 0x45:
            if self.selected is None: raise AssertionError("ALT selected no guard")
            self.pc += self.signed(self.selected)
        elif op in (0x47, 0x48): self.b = self.c  # enable timer/input guard
        elif op == 0x4E: self.selected = None  # talt
        elif op == 0x51: pass  # taltwt: timer/input readiness is supplied above
        elif op == 0x1FF: raise Complete  # reboot
        else: raise AssertionError(f"unsupported operate opcode {op:#x}")
