#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Run the host test for the desktop layer's firmware side.
#
# The driver compiles inside the BL616 firmware, where a build can only prove it
# links.  The palette, the row-span diff, the framing and the size answer are
# all things that are wrong in ways a link does not notice, so they are checked
# on the host against the real source -- the test includes tang_osd_desk.c, not
# a copy of it.
#
#   tools/tests/test_osd_desk.sh
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"

if ! command -v cc >/dev/null 2>&1; then
    echo "test_osd_desk: no C compiler" >&2
    exit 1
fi

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

# The stub headers come first so FreeRTOS.h, task.h and tdsh.h resolve to the
# host stand-ins rather than to a device SDK.
cc -std=c11 -O1 -Wall -Wextra -Wno-unused-parameter \
   -I "$root/tools/tests/stubs" \
   -I "$root/ports/bl616" \
   -I "$root/third_party/tinydesk/include" \
   -o "$work/tb" \
   "$root/tools/tests/tb_osd_desk.c" \
   "$root/ports/bl616/tang_pad.c" \
   "$root/third_party/tinydesk/src/"*.c

"$work/tb"
