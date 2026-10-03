#!/usr/bin/env python3
"""Reflash a TinyTang board over its USB CDC console, without BOOT mode.

The image is uploaded to the card with the shell's `tangput` command and then
installed with `tangflash`, which stages it, commits it from TCM and resets.
The board comes back running the new firmware; no BOOT button, no card reader.

    tools/tinytang_flash.py build/build_out/tinytang_bl616.bin

The board must be in two-wire mode (the CDC is exposed) and running a TinyTang
firmware that has these commands.
"""

import argparse
import os
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("pyserial is required: pip install pyserial")

REMOTE_PATH = "/tinytang-upload.bin"
FW_APP_MAX = 0x80000


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
        # Wake the shell and get to a prompt.
        port.write(b"\r")
        time.sleep(0.6)
        port.read(65536)

        print(f"uploading {len(data)} bytes to {REMOTE_PATH}")
        port.write(f"tangput {len(data)} {REMOTE_PATH}\r".encode())
        wait_for(port, "tangput: ready for", 30)

        port.write(data)
        wait_for(port, "tangput: wrote", 120, echo=False)
        print("\nupload complete")

        print("staging and committing")
        port.write(f"tangflash {REMOTE_PATH}\r".encode())
        wait_for(port, "OK committing", 180)

        # The board erases, copies and resets; it will disappear and come back.
        print("\ncommitting; the board is resetting")
    finally:
        port.close()

    print("The board should now be running the new firmware. If it does not "
          "reappear, put it in ROM-bootloader mode and reflash with "
          "`make flash CHIP=bl616 BOARD=bl616dk COMX=...`.")


if __name__ == "__main__":
    try:
        main()
    except TimeoutError as exc:
        sys.exit(str(exc))
