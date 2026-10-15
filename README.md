# INMOS-ispy

A slightly updated version of the old INMOS Transputer `ispy`, `mtest`, and
`ftest` tools for configuration and diagnostics of INMOS Transputer hardware.
Host builds support Linux and an Acorn A5000 running RISC OS 3.1
with Acorn C 5.

This set of tools has been modified slightly from the last release by  Andy Rabagliati (http://www.wizzy.com/wizzy/ispy.html), in order to work with modern versions of GCC and the updated kernel driver on Linux 3.x.x as available in my other repository (https://github.com/megatron-uk/INMOS-Link-Driver).

## Usage

Build the host tools with GNU make and GCC:

```sh
make
```

This builds all five host programs using the checked-in target-code
headers. DOSBox and the Occam toolchain are needed only when you explicitly
regenerate those headers with `make occam`.

The tools retain their historical discovery and diagnostic interfaces. C004
configuration and template handling have correctness fixes; see
[C004 validation](docs/c004-validation.md) for compatibility details.

FTEST was recovered from the
[WoTUG INMOS archive](https://wotug.org/parallel/vendors/inmos/archive-server/).
The imported version is based on 3.0, dated 25 January 1991. Its original author
credits have been retained.

ISpy discovers the network, prints its processor/link map, and leaves
routing programs running on the transputers. FTEST and MTEST read that map
to select diagnostics and reconstruct the routes used to load them.
The pipe carries the map; diagnostic code and results travel over the
hardware link.

On Linux, start each diagnostic with a fresh network discovery:

```
./ispy | ./mtest
./ispy | ./ftest
./ftest -h
```

A pipe is optional. You can save the map and use it immediately afterwards:

```
./ispy > network.txt
./ftest < network.txt
```

`mtest -z` adds sparse memory sizing, `ltest` checks processor links in both
directions and measures latency and one-way bulk throughput. `netcfg` exports
INMOS C, occam and C004 switch descriptions. MTEST and FTEST remain separate programs.
See [network diagnostics and exports](docs/network-diagnostics.md) for commands,
coverage, timing limits and the hardware validation still required.

The saved map describes the live network ISpy prepared. Resetting the
processors, changing the topology, or replacing their routing programs
can make it unusable even if the file still describes the same hardware.

FTEST contains tests for M212/T2/T225, T414, T425, T800, T801, and T805.
Chip microcode tests are selected by silicon revision. T801B and T805D/G
use the replacement TEST85 payloads; T425B/C use TEST45. The recovered
TEST425A remains selected for T425A and TEST800A for T800C/D. Unknown
T425, T800, T801 and T805 revisions run common tests and report omitted
chip diagnostics. Legacy T2 and T414 selection is retained from FTEST. Full target
diagnostic coverage remains to be validated. Use `ftest -l` to report each
target payload and its subtest results on stderr. A transport or protocol
failure stops testing and prints partial results, with remaining processors
marked `NOT TESTED`; rerun ISpy before retrying. Diagnostic and transport
failures return a non-zero exit status.

TEST85 targets the T801B microcode family: T801B and T805D/G share the CPU
layout apart from `lddevid`, which these tests do not exercise. The
[microcode reference](https://www.transputer.net/iset/iset.asp) also identifies
T425B/C as a shared layout apart from `lddevid`. Unlisted later revisions
are not assumed compatible.

| CPU microcode family | Selected chip diagnostic |
|----------------------|--------------------------|
| T425A | Recovered TEST425A |
| T425B/C | Replacement TEST45 payloads |
| T800C/D | Recovered TEST800A |
| T801B, T805D/G | Replacement TEST85 payloads |

Chip revision letters are local to each product, not a common microcode
version sequence. Matching instruction behaviour does not establish matching
internal microstep addresses: closely related microprograms can have different
ROM layouts. T400B is not assumed compatible with the T425 address maps.
CPU and FPU compatibility must also be considered separately: T800C/D share
CPU microcode, but three FPU words differ. T800D, T801B and T805D/G share FPU
microcode. These ROM relationships do not prove identical physical cores or
hardware timing.

Use `ftest -n 1` to test only processor 1 through the existing routers.
The recovered TEST801A uses incompatible absolute microcode addresses on
T801B and T805D/G. It is retained as a reference and is no longer dispatched.
Use `ftest -l -s` to omit chip microcode tests and run the common diagnostics.
Results show `PARTIAL` and list omitted coverage; exit status is zero if
the tests that ran passed.

For the first replacement-payload hardware run, use `ftest -l -p -a`.
This runs only the priority/timer probe and restores routing after success.
It checks priority changes, arms a distant low-priority alarm and immediately
disables it. Probe-only results show `PARTIAL`. Once the probe passes, use
`-d cm`, `-d stack`, `-d const` or `-d inc` to select one replacement subtest
alongside the common diagnostics. Omit these options to run all four.
Use a fresh ISpy discovery before each validation run.

The replacement address maps are in `diagnostics/microsteps.json`, with named
Occam constants in `occam/u805.occ` and `occam/u425.occ`. Run the audit with
an externally supplied ROM archive:

```
python3 tools/ucode-audit.py /path/to/ucode.zip
```

The audit compares complete control words against the recovered test's
matching reference revision, excluding unused ROM feedback for single
microsteps. Normal microcode entries also require equivalent continuation
graphs. It covers named operations and symbolic `STEP` instructions. Use
`--write-includes` to regenerate the named constants after a successful
audit, then `make occam` to rebuild checked-in payload headers. ROM dumps
are not required by ordinary host builds.

The replacement TEST85/TEST45 and probe headers reproduce with the recovered
Occam compiler. Legacy FTEST and router headers retain the recovered binaries
built with Occam 2.00.04; rebuilding them with the recovered 2.02.10 compiler
changes code and workspace allocation. Validate regenerated legacy payloads
on hardware before relying on their results.


For a 32-bit root that stops responding, `ftest -l -a` attempts an Analyse
snapshot after the failed read. It uses the recovered CKMON bootstrap to
report the saved instruction pointer, workspace descriptor, flags and
queues. Analyse changes the network state; rerun ISpy before another test.

### RISC OS port

The Unix layout remains the source of truth. To create a separate Acorn
layout for transfer to RISC OS, run:

```
python3 tools/export-riscos.py
```

This creates a new `build/riscos` directory containing `c`, `h`, `s`, `o`,
and the native Acorn Makefile. Supply a different destination as an argument
when you need another snapshot. Existing destinations are refused. C sources and headers are
copied unchanged; Acorn C translates suffix-style include names such as
`"cklib.h"` to its native header layout. See the
[Acorn C filename conventions](https://www.riscos.com/support/developers/c/ccandcpp.html).
The Linux link module and its kernel-driver header are omitted.

`riscos/ro_link.c` and `riscos/tmblink.s` were recovered from
[riscos-transputer-nbody](https://github.com/philpem/riscos-transputer-nbody).
They provide the standalone TMB/TLink driver and timed read/write wrapper,
with the original Richard Crook credits retained. The wrapper uses OSLib;
the assembler uses its `OSLibInclude:` headers. Neither requires a Wimp
window or event loop.

After transferring the export, select its directory on RISC OS and run
Acorn `amu`. This builds `ispy`, `mtest`, `ftest`, `ltest`, and `netcfg` with `cc`, `objasm`,
and `link`. Set `C:`, `OSLib:`, and `OSLibInclude:` for your installed
compiler and libraries. The Makefile selects 26-bit APCS to match the
TMB assembly on RISC OS 3.1 and uses the checked-in transputer payloads.

ISpy uses Unix `isatty`/`fileno` to detect redirected input on Unix hosts.
The native RISC OS build instead discovers without an input template by
default. Use `ispy -f network` to read a named template, or `ispy -p` to
read a template from redirected stdin. `-x` ignores a template on either
host. MTEST and FTEST continue to read their network descriptions from
stdin. The RISC OS default link name is `TMB`; the supplied driver scans
for a supported podule and ignores this name.

The diagnostic C sources pass C90 syntax checks on the Unix host. ISPY, MTEST and FTEST
have compiled and linked with Norcroft C 5.09 on RISC OS, and
ISpy has discovered a network of two T805G processors. Both pass all 18
FTEST subtests: the 14 common integer and floating-point tests and the
replacement Conditional Mux, Stack drivers, Constants and Incrementer tests.
The full run ends with `OK` for both processors and no omitted tests.
PROBE85 also passes priority changes and timer cleanup on both T805Gs,
including routing through processor 0. Replacement TEST85 and TEST45
payloads compile with the recovered Occam toolchain and pass static ROM
audits. T801B, T805D and T425B/C remain hardware-unvalidated.
TEST85CM's detailed diagnostics
buffer the status, expected value and mask for each check, then report
failures after timer cleanup. Check IDs are listed in
`diagnostics/cm85-checks.json`. Alignment tracing isolated CM12: MOVEintr's
STATUS write preserves stale condition bits. The replacements now probe the
same condition through MOVEformWriteCount, which does not write STATUS;
the corrected test passes without changing its expected condition values.
MTEST completes on both processors,
reporting 4 KiB internal RAM and 4 MiB external RAM per processor, ending
on memory wrap.

On the tested native command line, `|` was passed to ISpy as an argument
rather than interpreted as a Unix pipe. Use file redirection:

```
ispy > net
ftest -l < net
```

Run `make test-host` on Unix to check the exported build dependencies and
ISpy input selection using a simulated link API. These checks require
Python 3 and GCC and do not access transputer hardware.

The T425 Verilog model is a possible additional validation target for the
T425 diagnostics and MTEST. Its existing Unix-socket link transport can
replace the Linux link module. The simulator currently exits when the host
disconnects, so handing a discovered network from ISpy to another
diagnostic needs a persistent connection or reconnect support. It does
not provide validation of T2 or T8 hardware diagnostics.

## Tools

`tools/tdis.py` is a dependency-free Transputer (T2/T4/T8) disassembler and
recursive-descent analyser, useful for inspecting the compiled occam code blobs
embedded in ISpy/mtest (`type16.h`, `check32.h`, `c00416.h`, ...).

```
python3 tools/tdis.py selftest                 # validate the decoder
python3 tools/tdis.py blobs ispy               # list embedded code blobs
python3 tools/tdis.py disasm ispy --offset 0xB4C0 --length 956 --recursive --stats
```

The blobs interleave code with constant/process tables, so `--recursive`
follows direct branches, calls and `ldpi`+`startp` process pointers and renders
unreachable bytes as `db`; `--stats` reports code coverage and an opcode
histogram.

`tools/tco2h.py` converts an `ilist /C` TCOFF listing into a code-blob header
(`type16.h`, `check32.h`, ...). It is a dependency-free replacement for the lost
INMOS `tco2h.pl`.

### Rebuilding the occam code blobs

`tools/occam-build.sh` compiles `occam/*.occ` with the original INMOS occam 2
DOS toolset under DOSBox, runs `ilist /C`, and regenerates the headers with
`tco2h.py`:

```
tools/occam-build.sh build/occam
```

To regenerate the repository headers and rebuild all five host tools in one command:

```sh
make occam all
```

It needs `dosbox`, `unzip` and `python3`, plus the D7305 toolset archive at
`tools/inmos/d7305a.zip` (not distributed here; see `.gitignore`). The D7x05
archives are from
<http://transputer.classiccmp.org/software/languages/occam/compilers/inmos/commercial/>.

The generated headers for `type16/32`, `check16/32` and `mtest16/32` are
byte-for-byte identical to the blobs in the released 3.23 binaries. The C004
helpers are rebuilt from recovered monitor sources with protocol fixes.
[Header validation and CHECK payload selection](docs/occam-header-validation.md)
records the differences from the older repository headers and explains why the
regenerated CHECK headers are retained.

Run `make check` (or `make test-host`) for host-build, disassembler, FTEST,
Acorn export and hardware-free C004 regression checks. See
[C004 validation](docs/c004-validation.md) for coverage, remaining integration
checks and the correction to directed connections in long-format templates.

## Notes

Supplied with a copy of link-driver.h as bundled with the kernel driver above. I will attempt to keep the two header files in sync, but if in doubt, install the latest kernel driver from my repo and symlink it here instead.
