"""Execute actual compiled T2/TA link and sizing payloads against link/RAM doubles."""
import re
import binascii
import unittest
from pathlib import Path
from diag_vm import Worker
ROOT = Path(__file__).resolve().parents[1]

def pattern(round, n, direction):
    return bytes((i + (round & 255)*17 + direction) & 255 for i in range(n))

def command(role, peer, rounds, n):
    config = bytes([role, peer, rounds & 255, rounds >> 8, n, 0, 0, 0])
    return b'\xff\xff\0\1\0\4\10\0' + config

class PayloadTests(unittest.TestCase):
    def test_link_workers_use_requested_hardware_ports(self):
        for bits in (16, 32):
            for boot in range(4):
                for peer in range(4):
                    if boot == peer: continue
                    for n in (1, 64):
                        with self.subTest(bits=bits, boot=boot, peer=peer, n=n):
                            incoming = b''.join(pattern(r, n, 0) for r in range(2))
                            vm = Worker(ROOT/f'ltest{bits}.h', command(1, peer, 2, n), boot, peer, incoming)
                            vm.run()
                            self.assertEqual(vm.outputs[boot], b'\1\0\1')
                            result = vm.outputs[peer]
                            self.assertEqual(result[:-4], b''.join(pattern(r, n, 113) for r in range(2)))
                            self.assertEqual(result[-4:], b'\0\0\xff\xff')
    def test_initiator_reports_both_direction_errors(self):
        for bits in (16, 32):
            with self.subTest(bits=bits):
                incoming = bytearray(pattern(0, 64, 113)+pattern(1, 64, 113))
                incoming[15] ^= 1
                incoming += b'\3\0\xfc\xff'
                vm = Worker(ROOT/f'ltest{bits}.h', command(2, 1, 2, 64), incoming=incoming)
                vm.run()
                self.assertEqual(vm.outputs[1], pattern(0, 64, 0)+pattern(1, 64, 0))
                packet = vm.outputs[0]
                self.assertEqual(packet[:2], b'\16\0')
                self.assertEqual(packet[2:4], b'\3\0')
                self.assertEqual(packet[6:8], b'\1\0')
                self.assertEqual(packet[10:14], b'\2\0\100\2')
                self.assertEqual(int.from_bytes(packet[14:16], 'little'), binascii.crc_hqx(packet[2:14], 65535))
    def test_worker_storage_fits_reserved_two_kib(self):
        for name in ('size16', 'size32', 'ltest16', 'ltest32'):
            text = (ROOT/f'{name}.h').read_text()
            total = int(re.search(r'(\d+)\s*/\* total code requirement', text)[1])
            self.assertLess(total+32, 2048, name)

    def sizing(self, bits, kib=32, **settings):
        params = b'\0\0\4\0' + ((kib*1024//(bits//8))-1).to_bytes(4, 'little')
        vm = Worker(ROOT/f'size{bits}.h', b'\10\0'+params)
        for key, value in settings.items(): setattr(vm, key, value)
        vm.run()
        data = vm.internal_outputs
        packets = []
        while data:
            length = int.from_bytes(data[:bits//8], 'little')
            self.assertEqual(length, 16)
            packets.append(data[bits//8:bits//8+length])
            data = data[bits//8+length:]
        return vm, packets

    def test_sparse_ceiling_and_linear_access_count(self):
        for bits in (16, 32):
            for kib in (8, 16, 32):
                with self.subTest(bits=bits, kib=kib):
                    vm, packets = self.sizing(bits, kib)
                    self.assertEqual(packets[-1][3], 2)
                    self.assertEqual(int.from_bytes(packets[-1][4:8], 'little') * (bits//8), kib*1024-2048)
                    self.assertEqual(vm.ram_writes, 2*(kib//2-1))
                    self.assertLessEqual(vm.ram_reads, 4*(kib//2))
    def test_sparse_unwritable_boundary_and_address_wrap(self):
        for bits in (16, 32):
            with self.subTest(bits=bits):
                vm, packets = self.sizing(bits, ram_limit=8192)
                self.assertEqual(packets[-1][3], 1)
                vm, packets = self.sizing(bits, alias=8192)
                self.assertEqual(packets[-1][3], 255)
    def test_sparse_final_sweep_detects_alias_missing_first_anchor(self):
        for bits in (16, 32):
            vm, packets = self.sizing(bits, alias_at=((1 << (bits-1))+8192, (1 << (bits-1))+4096))
            self.assertEqual(packets[-1][3], 254)
    def test_sparse_does_not_claim_unprobed_words_tested(self):
        for bits in (16, 32):
            vm, packets = self.sizing(bits, ram_fault=(1 << (bits-1))+4100)
            self.assertEqual(packets[-1][3], 2)

    def test_link_router_consumes_boot_command_only_at_final_parent(self):
        for bits in (16, 32):
            for final in (False, True):
                with self.subTest(bits=bits, final=final):
                    path = b'\1' if final else b'\1\2'
                    route = b'\xff\xff\0' + len(path).to_bytes(2, 'little') + path
                    request = route+b'\xff\xff\2\3\0abc\2\0de'
                    vm = Worker(ROOT/f'ltest{bits}.h', request)
                    vm.run()
                    expected = b'abcde' if final else b'\xff\xff\0\1\0\2\xff\xff\2\3\0abc\2\0de'
                    self.assertEqual(vm.outputs[1], expected)

    def test_maximum_exchange_count_retains_high_byte(self):
        for bits in (16, 32):
            incoming = b''.join(pattern(r, 1, 113) for r in range(256)) + b'\0\0\xff\xff'
            vm = Worker(ROOT/f'ltest{bits}.h', command(2, 1, 256, 1), incoming=incoming)
            vm.run()
            packet = vm.outputs[0]
            self.assertEqual(packet[10:12], b'\0\1')
            self.assertEqual(int.from_bytes(packet[14:16], 'little'), binascii.crc_hqx(packet[2:14], 65535))
