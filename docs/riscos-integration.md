# RISC OS Wimp integration

The implementation is isolated on `integration/riscos-wimp`, based on current
GitHub `master` at `1775a3c`. The supported command-line tools stay on master;
the frontend is not ready to become a default build or release component.

## What is on this branch

`ro-wimp/` contains a Norcroft/RISC OS frontend, portable graph/layout and job
state, a callable discovery layer, native resources, export/staging helpers and
host checks. Shared `cklib.c` changes are gated by `ISPY_CALLABLE`: command-line
builds retain their existing error/allocation functions and empty-read policy.
The SIZE16/SIZE32 worker changes and regenerated headers add optional RAM-probe
anchor/timing behaviour; zero/legacy request behaviour needs explicit acceptance
before those shared payload changes land.

The historical local deployment notes, screenshots and progress diaries remain
local working material. They are not installation dependencies. Ordinary builds
continue to use checked-in payload headers; native compiler/toolchain inputs
are obtained separately as described in the main README.

## Proposed landing sequence

1. Review the returning-error/allocation and callable timeout interfaces as a
   separate shared-library change. Keep `ISPY_CALLABLE` opt-in and compare Linux
   CLI builds/output with master using the same fixtures and default flags.
2. Review SIZE16/SIZE32 protocol additions independently. Verify legacy/default
   requests on both word widths, alias detection, limits and optional timing;
   regenerate payloads from the tracked Occam sources with the documented toolset.
3. Land the frontend only after native stability is established. Keep its build,
   resources and exports under `ro-wimp/`; do not alter the default Linux or
   RISC OS command-line targets. Exports must preserve existing platform source
   selection, include paths, APCS mode and linker libraries.

The frontend's historical illegal-window-handle incident is tracked in
[issue #9](https://github.com/philpem/INMOS-ispy/issues/9). Template size preflight
and resource-directory fixes have separate evidence; neither is established as
its cause. Resolving that issue is required before frontend release/landing.

## Acceptance and current evidence

After fetching and pulling current GitHub master on 29 October 2026, all 116
existing host regressions passed with the local shared changes applied. All
seven Wimp host tests also passed:

```sh
make test-host
python3 -m unittest discover -s ro-wimp -p 'test_*.py' -v
```

These checks cover ordinary host builds and diagnostic payloads, plus portable
ownership/cancellation, RAM-result handling, topology, layout, sprites and
staging. They do not establish native Wimp handle lifetimes or desktop stability.

Before any shared changes merge, repeat the host suite and compare CLI behaviour
against master; build the existing RISC OS command-line export with Norcroft;
exercise its existing hardware acceptance cases; and separately compile/run the
frontend through its own export. Include any other platform build recipes being
kept supported in the shared-source review. No new native deployment or hardware
validation was performed during this maintenance pass.
