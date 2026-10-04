#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Simulate the desktop text layer (textdisp_wide) with Verilator.
#
# The layer's whole contract is an address decode: output pixel (cx, cy) must
# show the font bit for cell (cx>>4, cy>>4) at glyph row cy[3:1] and column
# cx[3:1], in that cell's own foreground or background colour.  A bitstream
# build cannot tell that mapping right from plausible, so it is checked here
# instead, pixel by pixel, before anything is wired into the core.
#
#   tools/test_textdisp_wide.sh
#
# The module under test lives in the nestang checkout because it ships as part
# of that core's patch; this testbench is this project's, and belongs here.
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

nestang="${NESTANG_DIR:-}"
if [[ -z "$nestang" ]]; then
    for candidate in "$root/../tangcore/nestang" "/run/media/vash/GIT/tangcore/nestang"; do
        if [[ -d "$candidate" ]]; then
            nestang="$(cd -- "$candidate" && pwd)"
            break
        fi
    done
fi

src="$nestang/src/iosys/textdisp_wide.sv"
if [[ ! -f "$src" ]]; then
    echo "test_textdisp_wide: $src not found; is the patch applied?" >&2
    exit 1
fi

if ! command -v verilator >/dev/null 2>&1; then
    echo "test_textdisp_wide: verilator not found" >&2
    exit 1
fi

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

verilator --binary --timing -j 0 \
    -Wall -Wno-fatal -Wno-DECLFILENAME -Wno-UNUSEDSIGNAL -Wno-VARHIDDEN \
    -Wno-WIDTHEXPAND -Wno-WIDTHTRUNC -Wno-BLKSEQ \
    --top-module tb_textdisp_wide \
    -I"$nestang/src/assets" \
    --Mdir "$work" \
    -o tb \
    "$root/tools/tb_textdisp_wide.sv" "$src"

"$work/tb"
