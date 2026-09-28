#!/usr/bin/env python3
"""tco2h.py - turn an `ilist /C` TCOFF listing into an ISpy code-blob header.

Replacement for the lost INMOS `tco2h.pl` (and the `YUK` equivalent).  The
original build did:

    oc <src> ... /O <src>.tco
    ilist /C <src>.tco > <src>.lst
    perl tco2h.pl < <src>.lst > <src>.h

and <src>.h defines `struct <src>_struct <src>_code = { Code[], Target,
Mode, CodeSize, Offset, WorkSpace, VectorSpace, BytesPerWord, TotalSpace }`.

For an occam 2 object the fields come from:

    START_MODULE ... BIT16|BIT32 ...        -> BytesPerWord (2 or 4)
    LOAD_TEXT bytes: N + hex dump           -> Code[] / CodeSize
    DEFINE_SYMBOL id: 3 SV:<a>+<b>          -> Offset = b
    DEFINE_SYMBOL id: 4 <n>                 -> WorkSpace
    DEFINE_SYMBOL id: 5 <n>                 -> VectorSpace
    TotalSpace = CodeSize + BytesPerWord * WorkSpace

Target/Mode are informational (the loader ignores them) and are emitted as
"TA"/"Undefined" to match the historical headers.

Usage:
    tco2h.py <listing>            # write header to stdout
    tco2h.py -o out.h <listing>
    tco2h.py < <listing>
"""

import argparse
import os
import re
import sys


def parse_listing(text):
    m = re.search(r'START_MODULE.*?\bBIT(\d+)', text)
    if not m:
        raise ValueError('no START_MODULE/BITnn in listing')
    bpw = int(m.group(1)) // 8

    m = re.search(r'LOAD_TEXT bytes:\s*(\d+)', text)
    if not m:
        raise ValueError('no LOAD_TEXT in listing')
    code_size = int(m.group(1))

    code = bytearray()
    for line in text[m.end():].splitlines():
        parts = re.split(r'\s{2,}', line.strip())
        if not parts:
            continue
        tokens = parts[0].split()
        if len(tokens) < 2 or not re.fullmatch(r'[0-9A-Fa-f]{8}', tokens[0]):
            if code:
                break
            continue
        if not all(re.fullmatch(r'[0-9A-Fa-f]{2,8}', t) for t in tokens[1:]):
            if code:
                break
            continue
        for tok in tokens[1:]:
            code += bytes.fromhex(tok)
        if len(code) >= code_size:
            break
    if len(code) < code_size:
        raise ValueError('short code dump: got %d of %d' % (len(code), code_size))
    code = bytes(code[:code_size])

    def symbol(idx):
        m = re.search(r'DEFINE_SYMBOL id:\s*%d\s+(\S+)' % idx, text)
        if not m:
            raise ValueError('no DEFINE_SYMBOL id %d' % idx)
        return m.group(1)

    sv = symbol(3)
    m = re.search(r'SV:(\d+)\+(\d+)', sv)
    if not m:
        raise ValueError('unexpected SV value %r' % sv)
    offset = int(m.group(2))
    workspace = int(symbol(4))
    vectorspace = int(symbol(5))
    total = code_size + bpw * workspace

    m = re.search(r'origin:\s*(\S+)', text)
    name = os.path.splitext(os.path.basename(m.group(1)))[0] if m else 'code'
    return {
        'name': name, 'code': code, 'code_size': code_size, 'offset': offset,
        'workspace': workspace, 'vectorspace': vectorspace, 'bpw': bpw,
        'total': total, 'target': 'TA', 'mode': 'Undefined',
    }


def emit(obj):
    n = obj['name']
    out = []
    out.append('/* automatically generated code - basename %s */\n\n' % n)
    out.append('struct %s_struct {\n' % n)
    out.append('\tunsigned char Code[%d];\n' % obj['code_size'])
    out.append('\tchar Target[10];\n\tchar Mode[10];\n')
    out.append('        long CodeSize;\n        long Offset;\n')
    out.append('        long WorkSpace;\n        long VectorSpace;\n')
    out.append('        int  BytesPerWord;\n        int  TotalSpace;\n};\n\n')
    out.append('struct %s_struct %s_code = {\n\n' % (n, n))
    out.append('    {\t\t\t/* code */\n')
    code = obj['code']
    for i in range(0, len(code), 12):
        out.append('\t' + ','.join('0x%02X' % b for b in code[i:i + 12]) + ', \n')
    out.append('\t},\n\n')
    out.append('\t"%s",\t/* target */\n' % obj['target'])
    out.append('    "%s",\t/* mode */\n' % obj['mode'])
    out.append('\t%d,\t/* code size */\n' % obj['code_size'])
    out.append('\t%d,\t/* offset */\n' % obj['offset'])
    out.append('\t%d,\t/* workspace */\n' % obj['workspace'])
    out.append('\t%d,\t/* vectorspace */\n' % obj['vectorspace'])
    out.append('\t%d,\t/* bytes per word */\n' % obj['bpw'])
    out.append('\t%d\t/* total code requirement not including parameter space  */\n'
               % obj['total'])
    out.append('\n};\t/* end of generated code */\n')
    return ''.join(out)


def main(argv=None):
    p = argparse.ArgumentParser(description='Convert an ilist /C listing to an ISpy code blob header')
    p.add_argument('listing', nargs='?', help='listing file (default: stdin)')
    p.add_argument('-o', '--output', help='output file (default: stdout)')
    args = p.parse_args(argv)

    text = open(args.listing, 'r', errors='replace').read() if args.listing else sys.stdin.read()
    header = emit(parse_listing(text))
    if args.output:
        open(args.output, 'w').write(header)
    else:
        sys.stdout.write(header)
    return 0


if __name__ == '__main__':
    sys.exit(main())
