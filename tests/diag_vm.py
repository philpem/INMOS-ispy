"""Strict single-process target executor for the new diagnostic workers.

Reuses instruction decoding from C004 tests. Hardware queues and time are
controlled doubles; electrical handshakes and real timing are not modelled.
"""
from c004_monitor import Monitor, Chip, Complete

class Worker(Monitor):
    def __init__(self, header, request, boot=0, peer=1, incoming=b''):
        super().__init__(header, request, Chip('b', [-1]*32), boot, peer, 'router')
        self.queues = {self.mint+(4+boot)*self.word: bytearray(request),
                       self.mint+(4+peer)*self.word: bytearray(incoming)}
        self.outputs = {}
        self.ticks = 0
        self.internal = bytearray(request)
        self.internal_outputs = bytearray()
        self.alias_at = None
        self.alias = None
        self.ram_limit = None
        self.ram_fault = None
        self.ram_reads = 0
        self.ram_writes = 0

    def load(self, address):
        if hasattr(self, 'ram_limit') and address >= self.mint+2048:
            self.ram_reads += 1
            if self.alias_at and address == self.alias_at[0]: address = self.alias_at[1]
            if self.alias: address = self.mint+2048+(address-self.mint-2048) % self.alias
            if self.ram_limit and address >= self.mint+self.ram_limit: return 0
            if self.ram_fault == address: return super().load(address) ^ 1
        return super().load(address)

    def store(self, address, value):
        if hasattr(self, 'ram_limit') and address >= self.mint+2048:
            self.ram_writes += 1
            if self.alias_at and address == self.alias_at[0]: address = self.alias_at[1]
            if self.alias: address = self.mint+2048+(address-self.mint-2048) % self.alias
            if self.ram_limit and address >= self.mint+self.ram_limit: return
        super().store(address, value)

    def input(self, channel, pointer, count):
        if channel < self.mint: queue = self.internal
        else:
            assert channel in self.queues, f'Unexpected input channel {channel:#x}'
            queue = self.queues[channel]
        if not queue: raise Complete
        assert len(queue) >= count, (count, len(queue), hex(channel))
        for i in range(count): self.mem[(pointer+i)&self.mask] = queue.pop(0)

    def output(self, channel, data):
        if channel < self.mint:
            self.internal_outputs.extend(data)
        else:
            assert self.mint <= channel < self.mint+4*self.word, hex(channel)
            self.outputs.setdefault((channel-self.mint)//self.word, bytearray()).extend(data)

    def operate(self, op):
        if op in (0x8, 0x53, 0x52, 0x40, 0x41, 0x4b, 0x33):
            a, b = self.a, self.b
            self.a = {0x52: lambda: b+a, 0x8: lambda: b*a, 0x53: lambda: b*a, 0x40: lambda: b >> a,
                      0x41: lambda: b << a, 0x4b: lambda: b | a,
                      0x33: lambda: b ^ a}[op]() & self.mask
            self.b = self.c
        elif op == 0x56:
            assert self.b < self.a or self.b >= (self.mask+1-self.a), ('conversion', self.a, self.b)
            self.a, self.b = self.b, self.c
        elif op == 0x34: self.a = (self.a*self.word) & self.mask
        elif op == 0x39:
            pass  # Sizing's router runs separately; framed channels are injected.
        elif op == 0x32: self.a = (~self.a) & self.mask
        elif op == 0x4a:
            data = bytes(self.mem.get((self.c+i)&self.mask, 0) for i in range(self.a))
            for i, byte in enumerate(data): self.mem[(self.b+i)&self.mask] = byte
        elif op == 0x13:
            assert self.b < self.a, ('bounds', self.a, self.b)
            self.a, self.b = self.b, self.c
        elif op == 0x22:
            self.push(self.ticks); self.ticks += 100
        elif op in (0x43, 0x4e): self.selected = None
        elif op in (0x44, 0x51):
            if self.selected is None:
                # Guards are selected at DISC (as in the monitor double).
                pass
        elif op == 0x2f:
            offset, enabled, channel = self.a, self.b, self.c
            ready = bool(enabled and self.queues.get(channel))
            self.a = int(ready)
            if ready and self.selected is None: self.selected = offset
        elif op == 0x45:
            if self.selected is None: raise Complete
            self.pc += self.signed(self.selected)
        elif op == 0x48: self.b = self.c
        else: super().operate(op)

    def run(self):
        try:
            for _ in range(1000000): self.step()
        except Complete: return
        raise AssertionError('Worker did not finish or wait for another request')
