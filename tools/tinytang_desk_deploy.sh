#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Put the desktop-layer core on the Tang and bring up a cartridge.
#
# This is the mechanical half of the hardware test: copy the patched core to the
# card, program the FPGA from it, prove the core answers on UART1, and stream a
# ROM.  Every step is the same three commands the firmware has always used, in
# the same order, for the same reason -- a ROM streamed into a core that is not
# listening vanishes with no error anywhere.
#
# The half that needs eyes is the desktop itself, and that needs the firmware in
# this tree flashed first: `osd desk` does not exist on an older one.  Once it
# does, the cycle is:
#
#   osd desk on      the desktop appears, full-screen, over the whole output
#   osd desk off     back to the game
#   L on controller 1, from a game, brings the desktop back
#
#   tools/tinytang_desk_deploy.sh [rom]
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

core="${CORE:-$root/../tangcore/nestang/impl/pnr/nestang_console138k_ds2.bin}"
port="${PORT:-/dev/ttyACM0}"
rom="${1:-/roms/castlevania.nes}"
remote="${REMOTE:-/cores/console138k/nestang-desk.bin}"

if [[ ! -f "$core" ]]; then
    echo "no core image at $core" >&2
    echo "build one with tools/build_nestang_core.sh" >&2
    exit 1
fi

printf 'core   %s\n' "$core"
printf 'size   %s bytes, MD5 %s\n' "$(stat -c %s "$core")" "$(md5sum "$core" | cut -d' ' -f1)"
printf 'port   %s\n' "$port"
printf 'to     %s\n' "$remote"
printf 'rom    %s\n\n' "$rom"

# 1. The image goes to the card under its own name, so the working nestang.bin
#    stays where it is and can be loaded again if this one misbehaves.
python3 "$root/tools/tinytang_put.py" "$core" "$remote" --port "$port"

# 2. Program the FPGA, prove the core answers, then stream.  The probe between
#    the two loads is the point: it is the only way to tell a live core from a
#    dead one without watching the output.
python3 "$root/tools/tinytang_run.py" \
    "tangload $remote" \
    "fpga" \
    "nesload $rom" \
    --port "$port" --seconds 20

echo
echo "If those three reported 'core loaded', 'core 1 answering' and 'the core is"
echo "running', the cartridge is up.  With the firmware from this tree flashed,"
echo "'osd desk on' puts the desktop over it -- and that is the part to look at."
