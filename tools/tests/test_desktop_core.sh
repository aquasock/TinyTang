#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
desktop="$root/fpga/desktop"
verilator --binary --timing -j 4 -Wno-fatal -Wno-DECLFILENAME \
    -Wno-UNUSEDSIGNAL -Wno-UNDRIVEN -Wno-WIDTHEXPAND -Wno-WIDTHTRUNC \
    -Wno-PINMISSING -Wno-PINNOTFOUND -Wno-ASSIGNIN -Wno-CASEINCOMPLETE \
    --top-module tb_desktop_uart \
    --Mdir "$work/uart" -o tb \
    "$root/tools/tests/sim/tb_desktop_uart.sv" \
    "$desktop/desktop_link.v" "$desktop/uart_fixed.v" \
    "$desktop/desktop_regs.sv" "$desktop/clock_monitor.sv" > "$work/uart.log" 2>&1 || \
    { cat "$work/uart.log"; exit 1; }
c++ -std=gnu++17 -Wall -Wextra -Werror -I "$root/ports/bl616/phosphor" \
    "$root/tools/tests/sim/gen_desktop_block_frames.cpp" -o "$work/gen_frames"
"$work/gen_frames" "$work/uart"
(cd "$work/uart" && ./tb)
python3 - "$work/uart/desktop-replies.hex" <<'PY'
import binascii
import sys
from pathlib import Path
replies=Path(sys.argv[1]).read_text().splitlines()
assert len(replies)==66
for line in replies:
    p=bytes.fromhex(line)
    assert len(p)==15
    assert binascii.crc_hqx(b'\x10'+p[:-2],0xffff)==int.from_bytes(p[-2:],'big'),line
print('desktop responses: independent Python CRC check PASS')
PY
iverilog -g2012 -I "$root/fpga/desktop" -s tb_desktop_pmod -o "$work/pmod" \
    "$root/tools/tests/sim/tb_desktop_pmod.sv" "$root/fpga/desktop/desktop_pmod.sv" \
    "$root/fpga/desktop/desktop_oled.sv" "$root/fpga/desktop/oled_panel.sv" \
    "$root/fpga/desktop/oled_spi.sv"
vvp "$work/pmod"
bash "$root/tools/tests/test_desktop_link.sh" > "$work/link.log" 2>&1 || \
    { tail -80 "$work/link.log"; exit 1; }
tail -5 "$work/link.log"
bash "$root/tools/tests/test_composite.sh"
bash "$root/tools/tests/test_desktop_video.sh"
echo 'desktop core: regressions PASS'
