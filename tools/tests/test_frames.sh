#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Run the host test for the reply-frame decoder.
#
#   tools/tests/test_frames.sh
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"

if ! command -v cc >/dev/null 2>&1; then
    echo "test_frames: no C compiler" >&2
    exit 1
fi

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

cc -std=c11 -O1 -Wall -Wextra -Werror \
   -I "$root/ports/bl616" \
   -o "$work/tb" \
   "$root/tools/tests/tb_frames.c" \
   "$root/ports/bl616/fpga_frames.c"

"$work/tb"
