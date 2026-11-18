#!/usr/bin/env python3
"""Reflash a TinyTang board over its USB CDC console, without BOOT mode.

The image is uploaded to the card with the shell's `tangput` command and then
installed with `tangflash`, which stages it and commits it from TCM.  The tool
waits until the commit has finished -- the board resets into its loader and
shows up as the FT2232 -- because power removed during the commit leaves a
board that only starts again from BOOT mode.  Then the board needs a power
cycle to run it (FLS-001), after which it runs the new firmware; no BOOT
button, no card reader.

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
# Must match FW_APP_MAX_SIZE in ports/bl616/tang_fw_update.c (FLS-002). A board
# still running a build from before 2026-11-18 refuses anything over 0xE0000
# itself, so moving it across takes two installs: an image under 0xE0000 built
# with the new layout, then the large one.
FW_APP_MAX = 0x1C0000


CDC_ID = (0xFFFF, 0x5454)          # TinyTang's console
FT2232_ID = (0x0403, 0x6010)       # the vendor loader, after the commit's reset
COMMIT_TIMEOUT_S = 180


def wait_for_vendor_loader(timeout):
    """True once the FT2232 has enumerated: the commit loop has finished and
    reset.  Polls the host's USB serial ports; the console is gone by then."""
    from serial.tools import list_ports
    start = time.time()
    while time.time() - start < timeout:
        ids = {(p.vid, p.pid) for p in list_ports.comports() if p.vid is not None}
        if FT2232_ID in ids and CDC_ID not in ids:
            print(f"  loader up after {time.time() - start:.0f} s")
            return True
        time.sleep(0.5)
    return False


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
    finally:
        port.close()

    # "OK committing" comes BEFORE the commit: the board then erases and
    # rewrites the whole application slot from TCM with interrupts off, about
    # 200 sectors and many seconds, and only then soft-resets into the vendor
    # loader, which enumerates as the FT2232.  Power removed before that leaves
    # a half-written application that never starts again -- the board then
    # comes up one-wire on every power-up and needs the BOOT button.  That
    # happened on 2026-11-17: a readback showed 137 of 197 sectors rewritten.
    # So the commit is over only when the FT2232 appears.
    print("\nwriting the application slot -- do NOT power off the board")
    if not wait_for_vendor_loader(COMMIT_TIMEOUT_S):
        sys.exit(f"the board did not reset into its loader within {COMMIT_TIMEOUT_S} s. Do NOT "
                 "power it off yet: wait several more minutes for the FT2232 (0403:6010) to "
                 "appear, and only then power-cycle it.")
    print("committed: the board has reset into its loader (FT2232)")

    print("Power-cycle the board now to run the new firmware; until then it shows "
          "up as an FT2232. Afterwards `platform` reports the new build identity. "
          "If it does not come back, hold BOOT at power-up: the BL616 then presents "
          "its ROM bootloader as 349b:6160 on /dev/ttyACM0, and a recovery writes "
          "only the application at 0x40000 with BLFlashCommand --flash --write "
          "--start=0x40000 --len=0x<size in hex> --file=<image>, as the README "
          "gives it; without --flash and --len it writes nothing. Never run "
          "`make flash`, which would overwrite Sipeed's loader at 0x0 (BRD-009).")


if __name__ == "__main__":
    try:
        main()
    except (TimeoutError, ConsoleNotReady) as exc:
        sys.exit(str(exc))
