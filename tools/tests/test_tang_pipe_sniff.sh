#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# The fpgapipe overlay sniffer on the host (tools/tests/tb_tang_pipe_sniff.c).
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

cc -std=c11 -O1 -Wall -Wextra -Werror -fsanitize=address,undefined \
   -I "$root/ports/bl616" \
   "$root/tools/tests/tb_tang_pipe_sniff.c" \
   "$root/ports/bl616/tang_pipe_sniff.c" -o "$work/tb"
"$work/tb"
