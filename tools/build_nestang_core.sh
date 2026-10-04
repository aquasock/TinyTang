#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Build the nestang console138k core image, deterministically.
#
# The image on the card at /cores/console138k/nestang.bin is reproduced
# byte-for-byte by this script from an unpatched nestang checkout: 4,593,044
# bytes, MD5 808b1f14b19d2d2db670ba95b8fa9ebf (`.ai/core-reference.md`,
# DEV-002).  That baseline is what makes a later diff meaningful: when the
# desktop OSD layer changes the image, the change is this project's and
# nothing else's.
#
# Gowin 1.9.11.03 needs two things to run headless on this host, both taken
# from Tang-Phosphor's scripts/build.sh: an offscreen Qt platform, and the
# system libfreetype preloaded.  Without the preload gw_sh dies with
# "undefined symbol: FT_Done_MM_Var" before reading a single source file.
#
#   tools/build_nestang_core.sh            # apply patches, then build
#   tools/build_nestang_core.sh --check    # resolve toolchain and paths only
#
# The patches in third_party/patches/ are applied first, so what this builds is
# always what this project carries.  Erected as a script rather than done by
# hand because the artifact is a regression baseline the user runs.
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
check_only=0
[[ "${1:-}" == "--check" ]] && check_only=1

# ------------------------------------------------------------ the toolchain
gowin_sh="${GOWIN_SH:-}"
if [[ -z "$gowin_sh" ]]; then
    gowin_sh="$(command -v gw_sh || true)"
fi
if [[ -z "$gowin_sh" ]]; then
    for candidate in \
        /home/vash/tools/gowin-1.9.11.03/IDE/bin/gw_sh \
        /opt/Gowin/Gowin_V1.9.11.03/IDE/bin/gw_sh
    do
        [[ -x "$candidate" ]] && gowin_sh="$candidate" && break
    done
fi
if [[ -z "$gowin_sh" || ! -x "$gowin_sh" ]]; then
    echo "build_nestang_core: gw_sh not found; set GOWIN_SH to its full path." >&2
    exit 1
fi

# ------------------------------------------------------------- the checkout
nestang="${NESTANG_DIR:-}"
if [[ -z "$nestang" ]]; then
    for candidate in "$root/../tangcore/nestang" "/run/media/vash/GIT/tangcore/nestang"; do
        if [[ -d "$candidate/.git" || -f "$candidate/.git" ]]; then
            nestang="$(cd -- "$candidate" && pwd)"
            break
        fi
    done
fi
if [[ -z "$nestang" || ( ! -d "$nestang/.git" && ! -f "$nestang/.git" ) ]]; then
    echo "build_nestang_core: nestang checkout not found; set NESTANG_DIR." >&2
    exit 1
fi

printf 'build_nestang_core: gw_sh     %s\n' "$gowin_sh"
printf 'build_nestang_core: checkout  %s\n' "$nestang"
printf 'build_nestang_core: revision  %s\n' "$(git -C "$nestang" rev-parse HEAD)"

if (( check_only )); then
    NESTANG_DIR="$nestang" "$root/scripts/apply-nestang-patches.sh"
    printf 'build_nestang_core: --check only; not building\n'
    exit 0
fi

# --------------------------------------------------------------- the build
NESTANG_DIR="$nestang" "$root/scripts/apply-nestang-patches.sh"

export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}"
system_freetype=/usr/lib/x86_64-linux-gnu/libfreetype.so.6
if [[ -f "$system_freetype" ]]; then
    export LD_PRELOAD="$system_freetype${LD_PRELOAD:+:$LD_PRELOAD}"
fi

cd "$nestang"
"$gowin_sh" build.tcl console138k ds2

artifact="$nestang/impl/pnr/nestang_console138k_ds2.bin"
if [[ ! -f "$artifact" ]]; then
    echo "build_nestang_core: build finished but $artifact is missing" >&2
    exit 1
fi
printf 'build_nestang_core: %s\n' "$artifact"
printf 'build_nestang_core: %s bytes, MD5 %s\n' \
    "$(stat -c %s "$artifact")" "$(md5sum "$artifact" | cut -d' ' -f1)"
