#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Build the open-flow desktop core at several placement seeds in parallel.
#
#   tools/sweep_desktop_core_open.sh [seed ...]     default: 0 1 2 3 4 5 6 7
#
# Each seed gets its own staging tree and output directory under
# build/desktop, because the open flow stages the sources and writes
# .open-pnr/ into the tree it is given; two seeds sharing one tree would
# overwrite each other.  The build's own gates still apply per seed: setup and
# hold, the clock-topology regression, and the SPINE17 hop check.  No override
# is passed, so a seed that fails a gate produces no bitstream.
#
# Exit status is 0 only when every seed built and passed its gates.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
seeds=("$@")
((${#seeds[@]})) || seeds=(0 1 2 3 4 5 6 7)
jobs="${DESKTOP_SWEEP_JOBS:-8}"
base="${DESKTOP_BUILD_DIR:-$root/build/desktop}"
mkdir -p "$base"

run_seed() {
    local seed="$1" dir="$base/seed$1"
    mkdir -p "$dir"
    if DESKTOP_BUILD_DIR="$dir" NEXTPNR_SEED="$seed" \
        "$root/tools/build_desktop_core_open.sh" "$dir/open-s$seed" \
        > "$dir/build.log" 2>&1; then
        echo pass > "$dir/status"
    else
        echo fail > "$dir/status"
    fi
}
export -f run_seed
export root base

echo "building ${#seeds[@]} seed(s) with $jobs in parallel"
if ! printf '%s\n' "${seeds[@]}" | xargs -P "$jobs" -I{} bash -c 'run_seed "$@"' _ {}; then
    :
fi

failed=0
for seed in "${seeds[@]}"; do
    dir="$base/seed$seed"
    if [[ "$(cat "$dir/status" 2>/dev/null)" == pass ]]; then
        echo "seed $seed: PASS"
    else
        echo "seed $seed: FAIL (see $dir/build.log)"
        failed=1
    fi
done

echo
echo "seed  bin                                          bytes"
for seed in "${seeds[@]}"; do
    bin="$base/seed$seed/open-s$seed/desktop.bin"
    if [[ -f $bin ]]; then
        printf '%4s  %-44s %s\n' "$seed" "$(sha256sum "$bin" | cut -d' ' -f1)" \
            "$(stat -c %s "$bin")"
    else
        printf '%4s  (no bitstream)\n' "$seed"
    fi
done
exit "$failed"
