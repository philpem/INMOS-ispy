# INMOS-ispy

A slightly updated version of the old INMOS Transputer 'ispy' and 'mtest' tools for basic configuration and diagnostics of INMOS Transputer hardware on Linux systems.

This set of tools has been modified slightly from the last release by  Andy Rabagliati (http://www.wizzy.com/wizzy/ispy.html), in order to work with modern versions of GCC and the updated kernel driver on Linux 3.x.x as available in my other repository (https://github.com/megatron-uk/INMOS-Link-Driver).

## Usage

Build the host tools with GNU make and GCC:

```sh
make
```

This uses the checked-in target-code headers. DOSBox and the Occam toolchain
are needed only when you explicitly regenerate those headers.

The tools retain their historical discovery and diagnostic interfaces. C004
configuration and template handling have correctness fixes; see
[C004 validation](docs/c004-validation.md) for compatibility details.

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

To regenerate the repository headers and rebuild both host tools in one command:

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

Run `make check` for host-build, disassembler and hardware-free C004 regression
checks. See
[C004 validation](docs/c004-validation.md) for coverage, remaining integration
checks and the correction to directed connections in long-format templates.

## Notes

Supplied with a copy of link-driver.h as bundled with the kernel driver above. I will attempt to keep the two header files in sync, but if in doubt, install the latest kernel driver from my repo and symlink it here instead.
