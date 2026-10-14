# C004 validation

Run `make check` to exercise the C004 host code and compiled monitor helpers
without a link driver or physical switches. The checks use the embedded headers;
they do not need DOSBox or a target compiler.

For memory-safety checks, run:

```sh
C004_TEST_CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -no-pie' make check
```

The C harness includes the actual host implementation and replaces the device
adapter with an in-memory link. It checks all 256 C004B enquiry byte values,
C004A selection masking, invalid replies, fragmented transfers, programming
sequences, template parsing and complete-route reset tracking. Programming
checks include empty maps, reciprocal pairs, directed connections, fan-out,
self-connections and deterministic random maps.

`tests/c004_monitor.py` executes monitor and outgoing-router code from the
generated 16-bit and 32-bit blobs. It initializes link/timer registers to the
bootstrap's `MINT` value and requires each hardware transfer to use the expected
channel address. A synchronous link model supplies enquiry responses and
the timer/input choice used by the A/B probe. The checks cover identification,
restoration of every possible input selection on output 2, disconnected output
2, enquiries, reset and application-mode programming. They verify that probing
preserves the map and that configuration sequences end with command 3. Router
checks cover framed identification setup, forwarding a raw programming sequence
across multiple packet segments, and resetting the correct channels on halt.

This interpreter does not execute the returning router process or the host
bootstrap. It does not validate discovery across a changing network, physical
link handshakes, timeout margins, board reset latches or electrical behaviour.
Those remain integration and hardware checks. The C004A model represents
remembered selections; enquiries on that revision do not reliably establish
whether each output is connected.

## Fixed behaviours

- Helpers place individual hardware channels and construct channel-pointer
  arrays in workspace, preserving the selected boot and C004 links.
- Programming ends with configuration-complete command 3, including empty maps.
- The revision probe masks the saved enquiry byte to an input number before
  restoring output 2, then completes the configuration sequence. It preserves
  directed connections rather than creating a reciprocal pair.
- Invalid revision replies fail instead of being treated as a chip revision.
- Halting a helper resets the actual link channels rather than the workspace
  slots containing their references.
- Compact templates accept the spaces emitted after the thirty-second port.
  Malformed, incomplete and out-of-range connections are rejected.
- Successful fragmented reads no longer consume the timeout retry allowance.
  Packet headers and payloads tolerate partial writes.
- Reset tracking retains complete routes, including leading zero hops, and
  has no fixed 100-switch array. Helper loading and reset sends are checked.

## Long-format compatibility

Long connections now use the documented `INPUT>OUTPUT` direction. Earlier code
parsed and printed directed connections backwards. Reciprocal `x-y` entries and
compact maps retain their meaning: compact position selects the output, and the
character at that position selects its input.

Regenerate long-format snapshots from the switches, or use compact snapshots
saved before this change. Existing long-format files produced by the reversed
implementation need their directed endpoints swapped. The historical manual
describes the intended direction, although its example is inconsistent with
that description.

## Monitor source and rebuilding

`occam/c00416.occ` and `occam/c00432.occ` recover the monitor from the earlier
[`check16b.occ`](https://wotug.org/parallel/vendors/inmos/archive-server/checkocc/check16b.occ)
and [`check32b.occ`](https://wotug.org/parallel/vendors/inmos/archive-server/checkocc/check32b.occ)
sources, with the current bootstrap stop sequence and the probe/programming
fixes above. They replace opaque helper blobs with rebuildable source. The
recovered baseline is not claimed to reproduce the old helpers byte for byte.

The build uses `/U` for these helpers, matching the unchecked instruction
generation in the released monitors. Other diagnostic sources retain their
existing compiler flags. Regenerate all headers with `make occam`; compiler
errors include the saved compiler log. See the README for toolchain requirements.

The historical description of C004 configuration and its text formats is in
`ispytest.txt` within the [ISPY Linux archive](https://wotug.org/parallel/transputer/software/debuggers/ispy/ispy_lx.zip).
