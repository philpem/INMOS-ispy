#!/usr/bin/env python3
"""Audit diagnostic microsteps against externally supplied INMOS CPU ROM dumps.

The JSON map records addresses, not ROM contents. Single-step operations must
have identical control words apart from ROM feedback (which is not executed).
Normal microcode entries additionally require equivalent continuation graphs.
"""
import argparse
import json
from pathlib import Path
import re
import zipfile


def read_rom(archive, name):
    text = archive.read('ucode/' + name + '.rom').decode('ascii')
    rows = {int(n, 16): bits for n, bits in
            re.findall(r'^\s*([0-9A-F]{3}) ([01]+)', text, re.M)}
    fields = {}
    for field in ('ROMFeedback', 'CondMux0', 'CondMux1', 'Next'):
        match = re.search(field + r' = ([0-9, ]+)', text)
        fields[field] = [int(n) for n in match.group(1).split(',')]
    return rows, fields


def value(bits, positions):
    return sum(int(bits[p]) << n for n, p in enumerate(positions))


def controls(rom, address):
    rows, fields = rom
    bits = rows[address]
    return ''.join(bit for n, bit in enumerate(bits)
                   if n not in fields['ROMFeedback'])


def successors(rom, address):
    rows, fields = rom
    bits = rows[address]
    if value(bits, fields['Next']):
        return []
    target = value(bits, fields['ROMFeedback'])
    masks = [1 << n for n in (0, 1)
             if value(bits, fields['CondMux' + str(n)])]
    targets = [target]
    for mask in masks:
        targets = [v & ~mask | b for v in targets for b in (0, mask)]
    return targets


def equivalent_entry(left, a, right, b, seen=None):
    if seen is None:
        seen = set()
    if (a, b) in seen:
        return True
    if controls(left, a) != controls(right, b):
        return False
    seen.add((a, b))
    aa, bb = successors(left, a), successors(right, b)
    return len(aa) == len(bb) and all(
        equivalent_entry(left, x, right, y, seen) for x, y in zip(aa, bb))


def check_condition_probe(rom, address, mux, selector):
    rows, fields = rom
    bits = rows[address]
    if value(bits, fields['CondMux' + str(mux)]) != selector:
        raise ValueError('Condition probe selects the wrong input')
    # Bit 36 is the STATUS destination enable on the T425/T800/T805 layouts.
    # A STATUS write can preserve stale condition bits instead of exposing the
    # probed input; MOVEintr is unsuitable despite selecting the right mux.
    if bits[36] == '1':
        raise ValueError('Condition probe overwrites STATUS')


def audit(archive, manifest):
    cache = {}
    def rom(name):
        if name not in cache:
            cache[name] = read_rom(archive, name)
        return cache[name]
    total = 0
    for family in manifest['families']:
        reference = rom(family['reference'])
        for revision in family['revisions']:
            target = rom(revision)
            for name, item in family['operations'].items():
                a, b = item['reference'], item['address']
                if controls(reference, a) != controls(target, b):
                    raise ValueError(f'{revision}: control mismatch for {name}')
                if not item['step'] and not equivalent_entry(reference, a, target, b):
                    raise ValueError(f'{revision}: continuation mismatch for {name}')
                total += 1
            for name, address in family['symbolic_steps'].items():
                if controls(reference, address) != controls(target, address):
                    raise ValueError(f'{revision}: symbolic STEP mismatch for {name}')
                total += 1
            for name, check in family.get('condition_checks', {}).items():
                check_condition_probe(target, family['operations'][name]['address'],
                                      check['mux'], check['selector'])
        print('{}: {} named operations, revisions {}'.format(
            family['name'], len(family['operations']), ', '.join(family['revisions'])))
    return total


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path, help='external ucode.zip')
    parser.add_argument('--map', type=Path, default=Path(__file__).resolve().parents[1]
                        / 'diagnostics/microsteps.json')
    parser.add_argument('--write-includes', action='store_true',
                        help='regenerate named Occam includes after successful audit')
    args = parser.parse_args()
    with zipfile.ZipFile(args.archive) as archive:
        manifest = json.loads(args.map.read_text())
        total = audit(archive, manifest)
    print(f'PASS: {total} operation/revision comparisons')
    if args.write_includes:
        root = Path(__file__).resolve().parents[1]
        for family in manifest['families']:
            text = '-- Named diagnostic opcodes; generated from diagnostics/microsteps.json.\n'
            for name, item in family['operations'].items():
                opcode = item['address'] | (0x800 if item['step'] else 0)
                text += f'VAL INT u.{name} IS #{opcode:X} :\n'
            (root / 'occam' / (family['include'] + '.occ')).write_text(text)


if __name__ == '__main__':
    main()
