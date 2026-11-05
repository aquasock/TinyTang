#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# The core link on the host (tools/tests/tb_tang_fpga_uart.c): frame format,
# the transmit lock, and the extended register endpoint, against a UART FIFO
# model.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

# _GNU_SOURCE: the module uses strnlen, as the SDK's newlib provides it.
cc -std=c11 -O1 -D_GNU_SOURCE -Wall -Wextra -Werror -fsanitize=address,undefined \
   -I "$root/tools/tests/stubs/tang_fpga_uart" \
   -I "$root/third_party/tinydesk-shell/include" \
   -I "$root/ports/bl616" \
   "$root/tools/tests/tb_tang_fpga_uart.c" \
   "$root/ports/bl616/fpga_frames.c" \
   "$root/ports/bl616/ae350_load.c" \
   "$root/ports/bl616/tang_nes_ini.c" \
   "$root/ports/bl616/tang_gbc_ini.c" \
   "$root/ports/bl616/tang_pipe_sniff.c" \
   "$root/ports/bl616/tang_settings_ini.c" -o "$work/tb"
"$work/tb"
