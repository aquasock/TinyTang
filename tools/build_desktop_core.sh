#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Reconstruct the desktop core from the pinned nestang and carried patches.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
prepare=0
placement=2
if [[ "${1:-}" == --prepare-only ]]; then
    prepare=1
elif [[ -n "${1:-}" ]]; then
    placement="$1"
fi
[[ "$placement" =~ ^[0-4]$ ]] || { echo "placement must be 0..4" >&2; exit 1; }
nestang="${NESTANG_DIR:-$root/../tangcore/nestang}"
build="${DESKTOP_BUILD_DIR:-$root/build/desktop}"
mkdir -p "$build"
source_dir="$(mktemp -d "$build/reconstruct.XXXXXX")"
git -c advice.detachedHead=false clone --quiet --no-hardlinks "$nestang" "$source_dir"
git -C "$source_dir" checkout --quiet --detach c2450818e1f0c858e13c5dd16746ee5221a5c760
for patch in "$root/third_party/patches/"*.patch \
             "$root/third_party/patches/menu/0001-menu-core.patch" \
             "$root/third_party/patches/desktop/0001-desktop-host.patch"; do
    git -C "$source_dir" apply --whitespace=nowarn "$patch"
done
mkdir -p "$source_dir/src/desktop"
cp "$root/fpga/desktop/"* "$source_dir/src/desktop/"
printf '%s\n' "$source_dir" > "$build/source-path.txt"
(
    cd "$root"
    sha256sum third_party/patches/*.patch third_party/patches/menu/0001-menu-core.patch \
        third_party/patches/desktop/0001-desktop-host.patch fpga/desktop/* |
        sha256sum > "$build/source.sha256"
)
echo "desktop source: $source_dir"
if (( prepare )); then exit 0; fi
gowin_sh="${GOWIN_SH:-/home/vash/tools/gowin-1.9.11.03/IDE/bin/gw_sh}"
[[ -x "$gowin_sh" ]] || { echo "Gowin gw_sh missing; set GOWIN_SH" >&2; exit 1; }
out="$build/place$placement"
mkdir -p "$out"
# A failed rerun must not leave an earlier binary looking like its result.
rm -f "$out/desktop.bin"
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}"
if [[ -f /usr/lib/x86_64-linux-gnu/libfreetype.so.6 ]]; then
    export LD_PRELOAD="/usr/lib/x86_64-linux-gnu/libfreetype.so.6${LD_PRELOAD:+:$LD_PRELOAD}"
fi
(
    cd "$source_dir"
    "$gowin_sh" src/desktop/build.tcl "$placement"
) > "$out/build.log" 2>&1
cp "$source_dir/impl/pnr/desktop_tr_content.html" "$out/desktop_tr_content.html"
cp "$source_dir/impl/pnr/desktop.rpt.txt" "$out/desktop.rpt.txt"
cp "$build/source.sha256" "$out/source.sha256"
python3 "$root/tools/check_gowin_timing.py" "$out/desktop_tr_content.html"
install -m 644 "$source_dir/impl/pnr/desktop.bin" "$out/desktop.bin"
sha256sum "$out/desktop.bin"
