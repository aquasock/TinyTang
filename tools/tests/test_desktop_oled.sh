#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
verilator --binary --timing -j 4 -Wno-fatal -Wno-WIDTHEXPAND -Wno-WIDTHTRUNC \
    -I"$root/fpga/desktop" --top-module tb_desktop_oled --Mdir "$work/sim" -o tb \
    "$root/tools/tests/sim/tb_desktop_oled.sv" "$root/fpga/desktop/desktop_oled.sv" \
    "$root/fpga/desktop/oled_panel.sv" "$root/fpga/desktop/oled_spi.sv" \
    > "$work/build.log" 2>&1 || { cat "$work/build.log";exit 1; }
(cd "$work/sim" && ./tb) > "$work/result.log" 2>&1 || { cat "$work/result.log";exit 1; }
cat "$work/result.log"
python3 - "$work/sim/oled-spi.hex" "$work/result.log" <<'PY'
import re,sys,zlib
from pathlib import Path
pixels=bytes.fromhex(Path(sys.argv[1]).read_text())
assert len(pixels)==12288
reported=int(re.search(r'OLED_SPI_CRC ([0-9a-f]+)',Path(sys.argv[2]).read_text())[1],16)
assert zlib.crc32(pixels)==reported,(hex(zlib.crc32(pixels)),hex(reported))
print('OLED: independent Python CRC of actual serialized pixel bytes PASS')
PY
