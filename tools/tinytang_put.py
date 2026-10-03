#!/usr/bin/env python3
"""Put a file onto a TinyTang board's SD card over its USB CDC console.

Uses the shell's `tangput <size> <path>` command, which reads raw bytes from
the console and writes them to the card.  The path is the shell's, so it is
relative to the shell's root (the card), e.g. /roms/castlevania.nes.

    tools/tinytang_put.py "/home/vash/Desktop/Castlevania (USA) (Rev A).nes" /roms/castlevania.nes

The board must be in two-wire mode and running a TinyTang firmware that has
the tangput command.
"""

import argparse
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("pyserial is required: pip install pyserial")


def wait_for(port, needle, timeout, echo=True):
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
    ap.add_argument("local", help="file to send")
    ap.add_argument("remote", help="path on the card, e.g. /roms/game.nes")
    ap.add_argument("--port", default="/dev/ttyACM0")
    ap.add_argument("--baud", type=int, default=115200)
    args = ap.parse_args()

    with open(args.local, "rb") as fh:
        data = fh.read()
    if not data:
        sys.exit("refusing to send an empty file")

    print(f"opening {args.port}")
    port = serial.Serial(args.port, args.baud, timeout=1.0, write_timeout=30.0)
    try:
        port.write(b"\r")
        time.sleep(0.6)
        port.read(65536)

        print(f"sending {len(data)} bytes to {args.remote}")
        port.write(f"tangput {len(data)} {args.remote}\r".encode())
        wait_for(port, "tangput: ready for", 30)

        port.write(data)
        wait_for(port, "tangput: wrote", 180)
        print("\ntransfer complete")
    finally:
        port.close()

    print(f"placed {len(data)} bytes at {args.remote}")


if __name__ == "__main__":
    try:
        main()
    except TimeoutError as exc:
        sys.exit(str(exc))
