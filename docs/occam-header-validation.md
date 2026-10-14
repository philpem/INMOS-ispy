# Occam header validation and CHECK payload selection

Use the D7305-generated `check16.h` and `check32.h` committed in PR #4.
They reset the intended hardware channels on halt. The corresponding headers
from the pre-branch repository (`2edf4ed`) instead reset workspace locations
holding channel references. Retain the regenerated headers as the supported
payloads and regenerate them from `occam/check16.occ` and `occam/check32.occ`;
do not restore the older CHECK byte arrays or mix their metadata with new code.

This decision is supported by instruction tracing and a regression that fails
against both older headers and passes against both selected headers. It does
not establish hardware timing or full-network correctness.

## References and reproduction

The comparison on 14 October 2026 used three independent inputs:

- Original repository headers at commit `2edf4ed`.
- Original released 3.23 DOS executables from
  [WOTUG ispy_dos.zip](https://wotug.org/parallel/transputer/software/debuggers/ispy/ispy_dos.zip).
- Compilation with the D7305 occam compiler 2.02.10 (26 February 1993),
  `ilist /C`, and this repository's `tools/tco2h.py`.

The compilation uses `/Y /A /N /K /V /E /W /H` and `/T2` or `/TA` for
non-C004 modules. Rebuild into a separate directory with:

```sh
tools/occam-build.sh build/occam-validation
```

All eight generated header files matched the headers committed in PR #4
exactly. All six non-C004 `Code[]` arrays were independently located in the
original DOS executables. Every byte, the Target/Mode strings, and all six
numeric metadata fields matched the compilation/conversion result. This
validates the converter against historical data, rather than comparing only
its output with another copy of its own output.

SHA-256 of the original executable references:

```text
ispy.exe  8b03aaab098d211fe76a4bb9c6c3d5f298009af252bdca9b0ae7521ded1b32ca
mtest.exe fcb316587c05429aaaa53ab352593bce08e054b337dd8f7aab46d9a38557fe87
```

The DOS structs encode the numeric trailer as four 32-bit integers and two
16-bit integers, after the ten-byte Target and Mode arrays. The numeric fields
are CodeSize, Offset, WorkSpace, VectorSpace, BytesPerWord and TotalSpace.

## Differences from the older repository headers

The original repository headers are a different reference from the released
3.23 executables. Equality with the latter does not imply equality with the
former. Header formatting also changed; the comparisons below concern payload
bytes and metadata rather than whitespace or comments.

| Payload | Old/new CodeSize | Old/new Offset | Old/new WorkSpace | Old/new TotalSpace | Code comparison |
|---|---:|---:|---:|---:|---|
| type16 | 186 / 186 | 0 / 0 | 155 / 155 | 496 / 496 | Different bytes |
| type32 | 284 / 280 | 2 / 1 | 27 / 27 | 392 / 388 | Different bytes |
| check16 | 914 / 882 | 1 / 1 | 213 / 210 | 1340 / 1302 | Different bytes |
| check32 | 976 / 940 | 3 / 3 | 182 / 182 | 1704 / 1668 | Different bytes |
| mtest16 | 1378 / 1378 | 1 / 1 | 111 / 111 | 1600 / 1600 | Exact match |
| mtest32 | 1372 / 1372 | 3 / 3 | 94 / 94 | 1748 / 1748 | Exact match |

VectorSpace is zero throughout. BytesPerWord is two for 16-bit modules and
four for 32-bit modules. Both MTEST modules match all numeric metadata as well
as every code byte.

Decoded comparisons identify these changes in TYPE/CHECK:

- **Channel alias representation.** Old code often stores the address of a
  channel-pointer array element and dereferences it at each ordinary input or
  output. New code loads the channel address when creating the alias and uses
  that value directly. For example, TYPE32's boot-output alias at old offset
  `0x29` uses `LDL 15; LDLP 10; WSUB; STL 5`; the new sequence at `0x28` adds
  `LDNL 0` before `STL 5`. This is dereference placement, not removal of channel
  addressing. Inline assembly makes this representation difference observable
  in the CHECK halt path, as described below.
- **Workspace allocation.** Local slots move, and CHECK16 uses fewer workspace
  words. CHECK32's total workspace allocation is unchanged. For example, one CHECK16 process uses `AJW -8`/`AJW 8` in place of
  `AJW -9`/`AJW 9`. TYPE16 also stores the link-speed result pointer in a
  different local slot. Equal TYPE16 code size does not mean equal code.
- **Instruction selection.** Some byte sends use `OUTBYTE` in the old code and
  a one-byte `OUT` from the existing buffer in the new code. Repeated channel
  dereferences and temporary stores also disappear after alias creation.
- **Equivalent timeout arithmetic.** CHECK's `LDC 4000; BSUB` becomes
  `LDNLP 2000` on T2 or `LDNLP 1000` on TA. These all add 4000 to the timer
  value: LDNLP scales its operand by the target word size. The timeout constant
  has not changed to 2000 or 1000 ticks.
- **Code layout.** Leading PFIX padding, local slot choices and instruction
  lengths change PC-relative branch, process and data-pointer displacements.
  TYPE32's first AJW remains -14 despite one fewer padding byte. Its final
  twelve constant-data bytes are unchanged, relocated from `0x110` to `0x10c`.
- **Embedded discovery data.** CHECK's five-byte classification table
  `00 01 02 00 03` and its 23-byte primary-bootstrap string are unchanged.
  Their old/new offsets are `0x328`/`0x308` and `0x32e`/`0x30e` on T2;
  `0x35c`/`0x338` and `0x370`/`0x34c` on TA. These regions are data, not
  extra instructions.

These are observations from the generated instructions and source. They are
consistent with code-generation differences, but the exact toolchain/options
that produced the older headers have not been established. No complete
instruction-by-instruction or cycle-equivalence claim is made. Different
instruction counts may affect measured timing even where the operations agree.

## Why the regenerated CHECK headers are selected

The halt code in both Occam sources creates `inchan` and `outchan` aliases and
uses inline `LDL inchan; RESETCH; LDL outchan; RESETCH`. RESETCH must receive
the channel-word address. The INMOS [occam toolset user guide, section 13.1.3](https://manuals.plus/m/ecfffd637d6df6fa0f514ad4b0b2d79d569dcf44c9cafd0b93ea76714eabd698)
distinguishes channel words from arrays of channel pointers; the
[IMS T805 datasheet](https://www.bitsavers.org/components/inmos/transputer/T805_32-bit_Transputer.pdf)
documents the hardware link locations and RESETCH operation.

The older CHECK headers create these aliases with `WSUB; STL`, leaving an
address in the workspace channel-pointer table. Ordinary I/O later uses
`LDNL 0`, but the inline RESETCH sequence does not. It therefore resets the
workspace table entry instead of the link. The selected headers use
`WSUB; LDNL 0; STL`, so inline LDL supplies the hardware-channel address.

For boot link zero, the execution trace records these RESETCH arguments:

| Target | Old header, workspace addresses | Selected header, hardware addresses |
|---|---|---|
| T2 | `5ffa 5fe6 5ffc 5fe8 5ffe 5fea` | `800a 8002 800c 8004 800e 8006` |
| TA | `5ff4 5fcc 5ff8 5fd0 5ffc 5fd4` | `80000014 80000004 80000018 80000008 8000001c 8000000c` |

Workspace addresses here depend on the test's initial workspace at `0x6000`.
Hardware addresses are architectural: output link i is `MINT + i * wordsize`,
and input link i is `MINT + (4 + i) * wordsize`. The selected trace resets input
and output on links 1, 2 and 3, preserving the boot link. The same distinction
was reproduced for every boot link on both word sizes.

Run the regression with:

```sh
python3 tests/test_check_links.py
```

Its halt test fails all eight target/boot-link cases when given the headers
from `2edf4ed`; it passes with the selected headers. Its separate forwarding
test checks a 97-byte raw programming stream across multiple packet segments,
for all twelve distinct boot/output-link pairs on each word size. Both old and
selected headers preserve that stream, so the reset difference is not inferred
from a general inability to route messages.

The interpreter executes the compiled root router with bootstrap-correct link
register contents. Concurrent child processes, returning-router execution,
physical communication and scheduling/timing remain outside this test. The
known bad halt addresses are sufficient reason to reject the older CHECK
headers; passing these cases does not certify every discovery operation.
