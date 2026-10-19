#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Build the desktop core with the open flow: yosys (read_slang), the
# nextpnr-himbaechel fork built against the regenerated GW5AST-138C database,
# and the apicula fork's gowin_pack.  No Gowin EDA is involved.
#
#   tools/build_desktop_core_open.sh [out-dir]
#
#   DESKTOP_BUILD_DIR  where fpga/desktop is staged (build/desktop)
#   NEXTPNR_SEED       placement seed (default 3, the hardware-proven core,
#                      desktop.24a11e81a46c.bin; see fpga/desktop/desktop.cst)
#   NEXTPNR_BUILD, APICULA_FORK, PY, PYDEPS   see tools/desktop_open/build.sh
#
# The design tree is fpga/desktop, staged by tools/build_desktop_core.sh
# --prepare-only.  The output directory receives
# desktop.fs, desktop.bin and copies stamped with their own sha256 prefix.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build="${DESKTOP_BUILD_DIR:-$root/build/desktop}"
export NEXTPNR_SEED="${NEXTPNR_SEED:-3}"
DESKTOP_BUILD_DIR="$build" "$root/tools/build_desktop_core.sh" --prepare-only
tree="$(cat "$build/source-path.txt")"
out="${1:-$build/open-s$NEXTPNR_SEED}"
exec "$root/tools/desktop_open/build.sh" "$tree" "$out"
