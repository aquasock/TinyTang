#!/usr/bin/env python3
"""Reflash a TinyTang board over its USB CDC console, without BOOT mode.

The image is uploaded to the card with the shell's `tangput` command and then
installed with `tangflash`, which stages it and commits it from TCM; the board
then needs a power cycle to run it (FLS-001).
After the power cycle it runs the new firmware; no BOOT button, no card reader.

    tools/tinytang_flash.py build/build_out/tinytang_bl616.bin

The board must be in two-wire mode (the CDC is exposed), running a TinyTang
firmware that has these commands, and at a shell prompt: the board is asked
first (tinytang_console), and nothing is sent unless it says so.
"""

import argparse
import os
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("pyserial is required: pip install pyserial")

# Nothing is sent until the board says its console is at a shell prompt
# (tinytang_console).
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tinytang_console import ConsoleNotReady, require_shell  # noqa: E402

REMOTE_PATH = "/tinytang-upload.bin"
# Must match FW_APP_MAX_SIZE in ports/bl616/tdsh_tang_flash.c (FLS-002). A
# board still running a pre-FLS-002 build refuses anything over 0x80000 itself.
FW_APP_MAX = 0xE0000


def wait_for(port, needle, timeout, echo=True):
    """Read until `needle` appears in the stream, or raise on timeout."""
    deadline = time.time() + timeout
    buf = b""
    while time.time() < deadline:
        chunk = port.read(4096)
        if chunk:
            buf += chunk
            if echo:
                sys.stdout.write(chunk.decode("utf-8", "replace"))
                sys.stdout.flush()
            if needle.encode() in buf:
                return buf
    raise TimeoutError(f"timed out waiting for {needle!r}; saw: {buf[-300:]!r}")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("image", help="the .bin to install on the board")
    ap.add_argument("--port", default="/dev/ttyACM0")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--yes", action="store_true", help="skip the confirmation prompt")
    args = ap.parse_args()

    with open(args.image, "rb") as fh:
        data = fh.read()
    if len(data) > FW_APP_MAX:
        sys.exit(f"image is {len(data)} bytes; the application slot is {FW_APP_MAX}")
    if not args.yes:
        reply = input(f"install {args.image} ({len(data)} bytes) on {args.port}? [y/N] ")
        if reply.strip().lower() not in ("y", "yes"):
            sys.exit("aborted")

    print(f"opening {args.port}")
    port = serial.Serial(args.port, args.baud, timeout=1.0)
    try:
        require_shell(port)

        print(f"uploading {len(data)} bytes to {REMOTE_PATH}")
        port.write(f"tangput {len(data)} {REMOTE_PATH}\r".encode())
        wait_for(port, "tangput: ready for", 30)

        port.write(data)
        wait_for(port, "tangput: wrote", 120, echo=False)
        print("\nupload complete")

        print("staging and committing")
        port.write(f"tangflash {REMOTE_PATH}\r".encode())
        wait_for(port, "OK committing", 180)

        # The board erases, copies and soft-resets into the vendor loader, where
        # it shows up as an FT2232 until it is power-cycled (FLS-001).
        print("\ncommitted")
    finally:
        port.close()

    print("Power-cycle the board now to run the new firmware; until then it shows "
          "up as an FT2232. Afterwards `platform` reports the new build identity. "
          "If it does not come back, put it in ROM-bootloader mode and reflash with "
          "`make flash CHIP=bl616 BOARD=bl616dk COMX=...`.")


if __name__ == "__main__":
    try:
        main()
    except (TimeoutError, ConsoleNotReady) as exc:
        sys.exit(str(exc))
