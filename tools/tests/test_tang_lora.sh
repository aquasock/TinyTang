#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# The LoRa transport on the host, against a PMOD UART model
# (tools/tests/tb_tang_lora.c).
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

cc -std=c11 -O1 -Wall -Wextra -Werror -fsanitize=address,undefined \
   -I "$root/tools/tests/stubs/tang_lora" \
   -I "$root/ports/bl616" \
   "$root/tools/tests/tb_tang_lora.c" \
   "$root/ports/bl616/tang_proto.c" -o "$work/tb"
"$work/tb"
