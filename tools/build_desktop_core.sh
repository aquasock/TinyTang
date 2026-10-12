#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Stage the desktop core's sources (all of them in fpga/desktop/) into a build
# directory, then build it with the Gowin IDE.  The open flow
# (tools/build_desktop_core_open.sh) stages the same way.
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
build="${DESKTOP_BUILD_DIR:-$root/build/desktop}"
mkdir -p "$build"
source_dir="$(mktemp -d "$build/stage.XXXXXX")"
mkdir -p "$source_dir/src/desktop"
cp -r "$root/fpga/desktop/." "$source_dir/src/desktop/"
printf '%s\n' "$source_dir" > "$build/source-path.txt"
(
    cd "$root"
    find fpga/desktop -type f -print0 | sort -z | xargs -0 sha256sum |
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
