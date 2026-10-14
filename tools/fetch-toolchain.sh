#!/usr/bin/env bash
#
# Download and verify the INMOS D7305 occam 2 toolset archive.
#
# The toolset is third-party and is not distributed with this repo (see
# .gitignore).  tools/occam-build.sh calls this automatically when the archive
# is missing.
#
# Source:
#   http://transputer.classiccmp.org/software/languages/occam/compilers/inmos/commercial/
#
# Environment:
#   OCCAM_TOOLCHAIN_DIR  destination directory (default tools/inmos)
#   OCCAM_URL            download URL
#   OCCAM_SHA256         expected SHA-256 of the archive

set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
dir=${OCCAM_TOOLCHAIN_DIR:-$root/tools/inmos}
url=${OCCAM_URL:-http://transputer.classiccmp.org/software/languages/occam/compilers/inmos/commercial/d7305a.zip}
sha=${OCCAM_SHA256:-040e9246423e5472704294a297385e00fa78fade805b53cfb42301ea6106f91e}
zip=$dir/d7305a.zip

mkdir -p "$dir"

if [ -f "$zip" ] && printf '%s  %s\n' "$sha" "$zip" | sha256sum -c --quiet 2>/dev/null; then
    echo "toolchain archive present and verified: $zip"
    exit 0
fi

echo "downloading $url"
if command -v curl >/dev/null 2>&1; then
    curl -fL --retry 3 -o "$zip.part" "$url"
elif command -v wget >/dev/null 2>&1; then
    wget -O "$zip.part" "$url"
else
    echo "error: need curl or wget to download the toolset" >&2
    exit 1
fi
mv "$zip.part" "$zip"

printf '%s  %s\n' "$sha" "$zip" | sha256sum -c - >/dev/null || {
    echo "error: checksum mismatch for $zip" >&2
    exit 1
}
echo "downloaded and verified: $zip"
