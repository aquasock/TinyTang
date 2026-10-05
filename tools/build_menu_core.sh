#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Build the TinyTang menu core: the console138k board with nestang's NES machine
# left out, so TinyDesk has a first-class host with no emulator behind it.
#
#   tools/build_menu_core.sh            # apply both series, then build
#   tools/build_menu_core.sh --check    # resolve toolchain and paths only
#
# The carried series is applied first by scripts/apply-nestang-patches.sh, the
# same way tools/build_nestang_core.sh does it; then
# third_party/patches/menu/0001-menu-core.patch is applied on top.  That patch
# adds the board file that carries MENU_CORE, gates the emulator in
# nestang_top.sv behind it, and asks build.tcl for the `menu` variant.
#
# The NES core is unaffected: the same series with no menu patch still builds
# impl/pnr/nestang_console138k_ds2.bin, and the menu patch is the only thing the
# menu core needs beyond it.  Reconstruction -- a fresh clone plus these two
# series reproducing the tree byte-for-byte -- is the guarantee, as it is for
# the NES series (TOOL-010).
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
check_only=0
[[ "${1:-}" == "--check" ]] && check_only=1

# ------------------------------------------------------------ the toolchain
gowin_sh="${GOWIN_SH:-}"
if [[ -z "$gowin_sh" ]]; then
    for candidate in \
        /home/vash/tools/gowin-1.9.11.03/IDE/bin/gw_sh \
        /opt/Gowin/Gowin_V1.9.11.03/IDE/bin/gw_sh
    do
        [[ -x "$candidate" ]] && gowin_sh="$candidate" && break
    done
fi
if [[ -z "$gowin_sh" || ! -x "$gowin_sh" ]]; then
    echo "build_menu_core: gw_sh not found; set GOWIN_SH to its full path." >&2
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
    echo "build_menu_core: nestang checkout not found; set NESTANG_DIR." >&2
    exit 1
fi

printf 'build_menu_core: gw_sh     %s\n' "$gowin_sh"
printf 'build_menu_core: checkout  %s\n' "$nestang"
printf 'build_menu_core: revision  %s\n' "$(git -C "$nestang" rev-parse HEAD)"

# --------------------------------------------------------------- the patches
NESTANG_DIR="$nestang" "$root/scripts/apply-nestang-patches.sh"

menu_patch="$root/third_party/patches/menu/0001-menu-core.patch"
if git -C "$nestang" apply --check "$menu_patch" 2>/dev/null; then
    git -C "$nestang" apply "$menu_patch"
    printf 'build_menu_core: applied %s\n' "$(basename "$menu_patch")"
else
    # The menu patch stops applying once the tree it produces is edited further,
    # for the same reason the carried series does (TOOL-010).  Presume it is
    # already there rather than refusing, and let the build judge.
    printf 'build_menu_core: %s not applied cleanly; presuming it is already applied\n' \
        "$(basename "$menu_patch")"
fi

if (( check_only )); then
    printf 'build_menu_core: --check only; not building\n'
    exit 0
fi

# --------------------------------------------------------------- the build
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}"
system_freetype=/usr/lib/x86_64-linux-gnu/libfreetype.so.6
if [[ -f "$system_freetype" ]]; then
    export LD_PRELOAD="$system_freetype${LD_PRELOAD:+:$LD_PRELOAD}"
fi

cd "$nestang"
"$gowin_sh" build.tcl console138k ds2 menu

artifact="$nestang/impl/pnr/nestang_console138k_ds2_menu.bin"
if [[ ! -f "$artifact" ]]; then
    echo "build_menu_core: build finished but $artifact is missing" >&2
    exit 1
fi
printf 'build_menu_core: %s\n' "$artifact"
printf 'build_menu_core: %s bytes, MD5 %s\n' \
    "$(stat -c %s "$artifact")" "$(md5sum "$artifact" | cut -d' ' -f1)"
