#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Build all four placements, validate timing, then report before any deployment.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
export DESKTOP_BUILD_DIR="${DESKTOP_BUILD_DIR:-$root/build/desktop/sweep}"
mkdir -p "$DESKTOP_BUILD_DIR"
failed=0
for placement in 0 1 2 3; do
    echo "Building desktop placement $placement"
    if ! bash "$root/tools/build_desktop_core.sh" "$placement"; then
        echo "Desktop placement $placement failed" >&2
        failed=1
    fi
done
if (( failed )); then
    echo 'sweep failed; inspect each placement build.log' >&2
    exit 1
fi
python3 "$root/tools/report_desktop_sweep.py" "$DESKTOP_BUILD_DIR"
