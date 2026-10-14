#!/usr/bin/env bash
#
# Rebuild the ISpy occam code-blob headers using the original INMOS DOS
# toolchain under DOSBox, then convert the TCOFF listings with tools/tco2h.py.
#
# The INMOS occam 2 toolset (D7305) is not distributed with this repo (see
# .gitignore); provide it at tools/inmos/d7305a.zip or via OCCAM_ZIP.  The
# D7x05 archives come from:
#   http://transputer.classiccmp.org/software/languages/occam/compilers/inmos/commercial/
#
# Requires: dosbox, unzip, python3.
#
# Usage:
#   tools/occam-build.sh [output-dir]
#
# Environment:
#   OCCAM_ZIP   toolset archive   (default tools/inmos/d7305a.zip)
#   OCCAM_WORK  scratch directory (default ${TMPDIR:-/tmp}/ispy-occam)
#
# Every occam/*.occ is compiled except checklib.occ (include-only) and
# peekpoke.occ (needs router.occ).  16-bit sources (filename ending "16")
# target the T2; the rest target the TA (32-bit) class.

set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
zip=${OCCAM_ZIP:-$root/tools/inmos/d7305a.zip}
out=${1:-$root/build/occam}
work=${OCCAM_WORK:-${TMPDIR:-/tmp}/ispy-occam}

for tool in dosbox unzip python3; do
    command -v "$tool" >/dev/null || { echo "error: $tool not found" >&2; exit 1; }
done
[ -f "$zip" ] || "$root/tools/fetch-toolchain.sh"

echo "==> extracting toolset"
rm -rf "$work"
mkdir -p "$work/disks" "$work/dos"
unzip -q "$zip" -d "$work/disks"
shopt -s nullglob
for exe in "$work"/disks/Disk.*/DISK*.EXE; do
    unzip -oq "$exe" -d "$work/dos"
done
shopt -u nullglob

mkdir -p "$work/dos/WORK"
cp "$root"/occam/*.occ "$work/dos/WORK/"

bases=()
for src in "$root"/occam/*.occ; do
    base=$(basename "$src" .occ)
    case "$base" in
        checklib|peekpoke|pass16|pass32) continue ;;   # include-only / examples not used here
    esac
    bases+=("$base")
done

echo "==> generating build batch (${#bases[@]} sources)"
{
    printf '@echo off\r\n'
    printf 'set PATH=C:\\TOOLS\r\ncd \\WORK\r\n'
    for base in "${bases[@]}"; do
        case "$base" in
            *16) target=T2 ;;
            *)   target=TA ;;
        esac
        # Released C004 helpers also disabled run-time error checking. Keep
        # that mode for their small router/monitor code; other blobs use /H.
        unchecked=
        case "$base" in c00416|c00432) unchecked=/U ;; esac
        printf 'oc %s.occ /Y /A /N /K /V /E /W /H %s /%s /O %s.tco > %s.log\r\n' \
               "$base" "$unchecked" "$target" "$base" "$base"
        printf 'ilist /C %s.tco > %s.lst\r\n' "$base" "$base"
    done
    printf 'dir *.tco\r\n'
} > "$work/dos/BUILD.BAT"

echo "==> running DOSBox toolchain"
SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-dummy} SDL_AUDIODRIVER=${SDL_AUDIODRIVER:-dummy} \
    dosbox -c "mount c $work/dos" -c "c:" -c "call C:\BUILD.BAT" \
           -c "exit" >/dev/null 2>&1

shopt -s nullglob
for base in "${bases[@]}"; do
    matches=("$work"/dos/WORK/"$base".tco "$work"/dos/WORK/"${base^^}".TCO)
    if [ ! -e "${matches[0]}" ] && [ ! -e "${matches[1]}" ]; then
        echo "error: $base did not compile" >&2
        for log in "$work/dos/WORK/$base.log" "$work/dos/WORK/${base^^}.LOG"; do
            [ ! -f "$log" ] || cat "$log" >&2
        done
        exit 1
    fi
done
shopt -u nullglob

echo "==> converting TCOFF listings to headers"
mkdir -p "$out"
count=0
for base in "${bases[@]}"; do
    matches=("$work"/dos/WORK/"$base".lst "$work"/dos/WORK/"${base^^}".LST)
    lst=${matches[0]}
    [ -e "$lst" ] || lst=${matches[1]}
    if [ ! -e "$lst" ]; then
        echo "error: no listing for $base" >&2
        exit 1
    fi
    python3 "$root/tools/tco2h.py" "$lst" -o "$out/$base.h"
    echo "    $out/$base.h"
    count=$((count + 1))
done

echo "==> wrote $count header(s) to $out"
