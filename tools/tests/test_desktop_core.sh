#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
DESKTOP_BUILD_DIR="$work/source" bash "$root/tools/build_desktop_core.sh" --prepare-only
source_dir="$(cat "$work/source/source-path.txt")"
verilator --binary --timing -j 4 -Wno-fatal -Wno-DECLFILENAME \
    -Wno-UNUSEDSIGNAL -Wno-UNDRIVEN -Wno-WIDTHEXPAND -Wno-WIDTHTRUNC \
    -Wno-PINMISSING -Wno-PINNOTFOUND -Wno-ASSIGNIN -Wno-CASEINCOMPLETE \
    -DMENU_CORE -DDESKTOP_CORE --top-module tb_desktop_uart \
    --Mdir "$work/uart" -o tb \
    "$root/tools/tests/sim/tb_desktop_uart.sv" \
    "$source_dir/src/iosys/iosys_bl616.v" "$source_dir/src/iosys/uart_fixed.v" \
    "$root/fpga/desktop/desktop_regs.sv" > "$work/uart.log" 2>&1 || \
    { cat "$work/uart.log"; exit 1; }
(cd "$work/uart" && ./tb)
python3 - "$work/uart/desktop-replies.hex" <<'PY'
import binascii
import sys
from pathlib import Path
replies=Path(sys.argv[1]).read_text().splitlines()
assert len(replies)==17
for line in replies:
    p=bytes.fromhex(line)
    assert len(p)==15
    assert binascii.crc_hqx(b'\x10'+p[:-2],0xffff)==int.from_bytes(p[-2:],'big'),line
print('desktop responses: independent Python CRC check PASS')
PY
iverilog -g2012 -s tb_desktop_pmod -o "$work/pmod" \
    "$root/tools/tests/sim/tb_desktop_pmod.sv" "$root/fpga/desktop/desktop_pmod.sv"
vvp "$work/pmod"
# Validate the desktop-layer legacy path on the reconstructed patched source.
NESTANG_DIR="$source_dir" bash "$root/tools/tests/test_iosys_desk.sh" > "$work/legacy.log" 2>&1 || \
    { tail -80 "$work/legacy.log"; exit 1; }
tail -5 "$work/legacy.log"
NESTANG_DIR="$source_dir" bash "$root/tools/tests/test_desktop_video.sh"
echo 'desktop core: reconstruction and affected regressions PASS'
