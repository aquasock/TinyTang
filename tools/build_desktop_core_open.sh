#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Build the desktop core with the open flow: yosys (read_slang), the
# nextpnr-himbaechel fork built against the regenerated GW5AST-138C database,
# and the apicula fork's gowin_pack.  No Gowin EDA is involved.
#
#   tools/build_desktop_core_open.sh [out-dir]
#
#   NESTANG_DIR        nestang checkout (default ../tangcore/nestang)
#   DESKTOP_BUILD_DIR  where the design tree is reconstructed (build/desktop)
#   NEXTPNR_SEED       placement seed (default 17, the hardware-proven core)
#   NEXTPNR_BUILD, APICULA_FORK, PY, PYDEPS   see tools/desktop_open/build.sh
#
# The design tree is the one tools/build_desktop_core.sh --prepare-only
# reconstructs, plus the four source-portability edits the open front end
# needs (third_party/patches/desktop/open/).  The output directory receives
# desktop.fs, desktop.bin and copies stamped with their own sha256 prefix.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build="${DESKTOP_BUILD_DIR:-$root/build/desktop}"
export NEXTPNR_SEED="${NEXTPNR_SEED:-17}"
DESKTOP_BUILD_DIR="$build" "$root/tools/build_desktop_core.sh" --prepare-only
tree="$(cat "$build/source-path.txt")"
git -C "$tree" apply --whitespace=nowarn \
    "$root/third_party/patches/desktop/open/0001-open-toolchain-portability.patch"
out="${1:-$build/open-s$NEXTPNR_SEED}"
exec "$root/tools/desktop_open/build.sh" "$tree" "$out"
