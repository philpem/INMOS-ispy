# Network diagnostics and configuration exports

Build with `make`. The new `ltest` and `netcfg` programs, like ISPY, MTEST and
FTEST, use checked-in target headers. Ordinary builds do not require an Occam
compiler. The native Acorn export includes all five programs.

Run a fresh discovery before each hardware diagnostic. Saved snapshots describe
the routing programs left by that discovery; another diagnostic or a reset can
invalidate those programs. On Linux:

```sh
./ispy | ./mtest -z -e 8192
./ispy | ./ltest
```

On RISC OS, use redirection in separate commands:

```text
ispy > net
mtest -z -e 8192 < net
ispy > net
ltest -f net
```

Memory tests overwrite the probed memory. Choose a ceiling appropriate to the
board: accessing an unpopulated or memory-mapped device address can stall the
target. This also applies to the existing MTEST modes.

## Memory sizing

| Command | Coverage |
|---------|----------|
| `mtest` | Existing pattern, random, execution and address tests |
| `mtest -q` | Existing address test across each 2 KiB block |
| `mtest -z` | Sparse sizing: one word at each 2 KiB boundary, plus alias checks |

The `-z` mode uses separate 16-bit and 32-bit payloads. Both fit, including
workspace, within the reserved first 2 KiB. The helper writes an inverted
address marker and an address marker, reads each back, and checks the first
marker for wrapping. When probing stops, it checks all earlier markers once.
Memory accesses therefore grow linearly with the number of sampled blocks;
the existing modes repeatedly rescan all earlier block markers.

The final sweep detects aliases which do not overwrite the first marker. Such
an alias produces `UNKNOWN?` and a non-zero exit status, rather than a supposed
contiguous size. A fault in an unsampled word can go undetected. `-z` reports
its sparse coverage and does not report memory speed; use the existing MTEST
modes for fuller coverage and timing.

`-e` sets the ceiling in KiB. Accepted values are even numbers from 4 to 262144;
16-bit targets retain their 64 KiB ceiling. A result ending in `|` reached the
chosen ceiling and does not establish that memory ends there. `;` denotes a
wrap, `.` a failed memory probe, and `?` incomplete results. Failed reads retain
partial results and return failure. A run which tests no processors also
returns failure. The default exclusion of T2 processors attached to C004
configuration links still applies; `-c` includes them.

## Link quality and latency

`ltest` loads a small router and worker on each processor, then tests each
processor-to-processor connection once, including connections outside the
boot tree. For each connection it runs:

1. One-byte exchanges for mean round-trip latency.
2. Exchanges of 64-byte patterns for directional corruption counts and
   round-trip goodput.

The default is 128 exchanges in each phase. `-n 1` through `-n 256` select the
count. `-f snapshot` selects a saved map; otherwise the map comes from stdin.
Patterns vary with the exchange number, position and direction. Errors count
mismatching bytes, not individual bits. Counts follow the endpoint order
printed on that result line.

The receiver is prepared first and acknowledges readiness through the routing
network. The initiator has an equal or shorter route, so preparing it does not
require forwarding through the suspended receiver. During measurement the
workers own the physical ports directly, then resume routing. This avoids
measuring the whole host-to-processor path as though it were one physical link.
The receiver sends its error count and its complement. The routed result packet
has a CRC-16/CCITT-FALSE checksum, which the host validates along with packet
length, exchange count, block size and role.

Timing comes from the initiator's low-priority timer, in 64 microsecond ticks.
It includes pattern generation, comparison, worker overhead and the return
trip. Goodput is the total payload in both directions divided by that duration;
it is not a measurement of the raw serial line rate. Zero ticks are reported
as below timer resolution. The packet carries the low 16 timer bits; when the
host observes a run of four seconds or more, it suppresses timing because a
wrap could make it misleading.

Corruption returns a non-zero exit status. A transfer or protocol failure stops
further routing, preserves completed results and marks remaining connections
`NOT TESTED`. Self loops are currently marked `NOT TESTED`, causing a non-zero
exit status. Host links and C004 configuration links are not measured as
processor-to-processor links. A connection routed through a configured C004
is tested as the end-to-end connection visible to its two processors.

A target can remain blocked on physical I/O after a broken link. The host's
bounded reads eventually report the incomplete test; rerun ISPY to reset and
rediscover before another diagnostic. These helpers do not claim autonomous
recovery from every physical link failure.

## INMOS configuration exports

`netcfg` converts a snapshot without opening the hardware link:

```sh
./ispy -c4 > network.txt
./netcfg -cc -f network.txt > hardware.cfs
./netcfg -co -f network.txt > hardware.inc
./netcfg -sw -f network.txt > switches.txt
```

On RISC OS use the same options with the native command names. Stdin is also
accepted, for example `netcfg -cc < net`.

| Option | Consumer and output |
|--------|---------------------|
| `-cc` | INMOS ANSI C `icconf`: processor declarations and physical connections |
| `-co` | INMOS occam `occonf`: NODE/ARC declarations and a NETWORK section |
| `-sw` | INMOS ASCII switch-file syntax: name, type, physical path and connections |

The compiler outputs describe hardware. Supply memory sizes and application
processes/mapping before configuring an application. Memory sizes are not
inferred from discovery or sparse probes. For example, edit the C declaration
to `T425 (memory = 1M) node0;`, or add
`SET node0 (memsize := 1048576)` inside the occam NETWORK section.
The occam configuration language is described in chapter 6 of the
[INMOS occam 2 Toolset User Guide](https://www.transputer.net/prog/72-tds-366-01/otds3usr.pdf).

Processor IDs become stable `node` names. Each physical connection is emitted
once, and C004 configuration links are omitted from compiler hardware
connections. Generic unidentified processor classes are rejected for compiler
exports. The C004 switch path lists the exit link at every processor from the
root to the configuration port. Directed connections, reciprocal connections,
self selections and fan-out are preserved. An entirely disconnected switch
has an empty `connect` list. Capture selections with `ispy -c4` first: a snapshot
with no recorded C004 configuration is rejected for switch export. On C004A,
readback cannot distinguish an unconnected output from a selected input.

These formats target the INMOS Toolkit. They are not claimed to be 3L Parallel
C configuration files. Switch export does not add a switch-file input option
to ISPY; ISPY's existing `-cs -f snapshot` interface still uses its own network
template syntax.

## Transfer reliability and programming verification

The Linux and RISC OS adapters use timeout units of tenths of a second;
zero retains the unlimited-wait convention. The Linux driver does not implement
poll/nonblocking I/O consistently across versions, and its timeout ioctls are
not consistently used by the read/write paths. The adapter waits for a ready
byte and transfers that byte, checking a monotonic deadline. This adds syscall
overhead to host transfers. Link-test timing is measured on the target.

Loader, route and framed writes now handle positive short transfers. Failed
or oversized transfer results are rejected, application-route failures are
reported, and an ISERVER frame length cannot silently truncate to 16 bits.
Driver errors stop the transfer; they are not retried when the number of bytes
already transferred could be ambiguous. Legacy framed reads allow ten
consecutive empty four-second reads before declaring a timeout; receiving data
resets that allowance.

After `ispy -cs` programs and rediscovers C004s, it compares requested selections
with readback and reports mismatches. Disconnected outputs cannot be verified
on revision A. `-cs -n` is rejected before opening the link because programming
requires subsequent rediscovery.

## Validation boundary

Host regressions cover short transfers, malformed replies, directional errors,
checksums, failure propagation, topology export and native build dependencies.
The compiled T2 and TA diagnostic payloads execute in a small instruction
interpreter against controlled hardware-channel and RAM doubles. This checks
port selection, pattern exchange, address wraps, sparse alias detection and
linear memory-access counts. It does not model electrical handshakes, process
scheduling across a network or real timing.

The four new headers were compiled with D7305 and converted with this project's
TCO2H. Regenerating the selected sources reproduces their payload bytes and
metadata; a second clean build reproduced all four headers exactly.

| Payload | Code bytes | Workspace words | Code plus workspace bytes |
|---------|------------|-----------------|---------------------------|
| `size16` | 768 | 87 | 942 |
| `size32` | 720 | 70 | 1000 |
| `ltest16` | 930 | 94 | 1118 |
| `ltest32` | 1056 | 73 | 1348 |

To regenerate only these payloads:

```sh
OCCAM_SOURCES='size16 size32 ltest16 ltest32' tools/occam-build.sh build/diagnostics
```

Two-processor hardware exports were accepted by D7214C `icconf` after adding
memory sizes, and by D7305 `occonf` after adding memory sizes and a minimal
application mapping. Both produced `.cfb` files.
Physical execution of the new sizing and link workers, their timing scale and
recovery on broken links still require hardware validation. The new native
commands have host C90 and export checks; they have not yet been built with
Acorn C or run on RISC OS hardware.
