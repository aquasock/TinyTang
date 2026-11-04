#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Build the desktop core with the open flow: yosys (read_slang), the
# nextpnr-himbaechel fork built against the regenerated GW5AST-138C database,
# and the apicula fork's gowin_pack.  No Gowin EDA is involved.
#
#   tools/build_desktop_core_open.sh [out-dir]
#   tools/build_desktop_core_open.sh --verify <provenance.txt>
#
#   DESKTOP_BUILD_DIR  where fpga/desktop is staged (build/desktop)
#   NEXTPNR_SEED       placement seed (default 3; see fpga/desktop/desktop.cst)
#   NEXTPNR_BUILD, APICULA_FORK, PY, PYDEPS   see tools/desktop_open/build.sh
#
# The design tree is fpga/desktop, staged by tools/build_desktop_core.sh
# --prepare-only.  The output directory receives desktop.fs, desktop.bin,
# copies stamped with their own sha256 prefix, and a provenance.txt naming the
# tuple that made them.
#
# `--verify` rebuilds from this tree with the toolchain named by the same
# environment, into a scratch directory, and compares the result against a
# provenance file recorded by an earlier build: every field but `built_at`,
# and both output digests, must match.  It is the check that a recorded
# bitstream can be reproduced here and now -- the thing that was missing when a
# core of record could not be rebuilt from its own recorded tuple.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

verify=0
if [[ "${1:-}" == "--verify" ]]; then
    verify=1
    recorded="${2:-}"
    if [[ -z $recorded || ! -f $recorded ]]; then
        echo "usage: $0 --verify <provenance.txt>" >&2
        exit 2
    fi
    recorded="$(cd "$(dirname "$recorded")" && pwd)/$(basename "$recorded")"
    scratch="$(mktemp -d "${TMPDIR:-/tmp}/desktop-verify.XXXXXX")"
    # The staging directory is per-invocation so a verify cannot disturb, or be
    # disturbed by, an ordinary build sitting in build/desktop.
    build="$scratch/desktop"
    out="$scratch/out"
    trap 'rm -rf "$scratch"' EXIT
else
    build="${DESKTOP_BUILD_DIR:-$root/build/desktop}"
    out="${1:-$build/open-s${NEXTPNR_SEED:-3}}"
fi

export NEXTPNR_SEED="${NEXTPNR_SEED:-3}"
DESKTOP_BUILD_DIR="$build" "$root/tools/build_desktop_core.sh" --prepare-only
tree="$(cat "$build/source-path.txt")"
"$root/tools/desktop_open/build.sh" "$tree" "$out"

if (( verify )); then
    echo
    echo "=== verifying against $recorded ==="
    # Not exec: the EXIT trap still has to remove the scratch directory.
    status=0
    "$root/tools/check_desktop_provenance.py" "$recorded" "$out/provenance.txt" || status=$?
    exit $status
fi
