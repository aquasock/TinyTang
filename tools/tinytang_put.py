#!/usr/bin/env python3
"""Put a file onto a TinyTang board's SD card over its USB CDC console.

Uses the shell's `tangput <size> <path>` command, which reads raw bytes from
the console and writes them to the card.  The path is the shell's, so it is
relative to the shell's root (the card), e.g. /roms/castlevania.nes.

    tools/tinytang_put.py "/home/vash/Desktop/Castlevania (USA) (Rev A).nes" /roms/castlevania.nes

The board must be in two-wire mode and running a TinyTang firmware that has
the tangput command, and the console must be at a **shell prompt** -- not
running the desktop.  That check is not politeness: raw bytes and keystrokes
are the same thing on this wire, so a file sent while the desktop is up is
typed into whichever window has focus, silently.  That is how a 131 KB ROM
became a directory full of `new.txt` entries and took the card's FAT with it.
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


# Signs that the console is running the desktop rather than a shell.
DESKTOP_MARKERS = (b"[Start]", b"Terminal - tdsh", b"\x1b[?1049h")


def require_shell(port, timeout=8.0):
    """Refuse to send unless the console is at a shell prompt.

    The point is to fail *before* writing anything, because the failure this
    prevents is destructive and silent: the bytes go somewhere, no error comes
    back, and the damage shows up later on the card.  A prompt is the proof
    that the bytes will reach tangput rather than a focused window.
    """
    port.write(b"\r")
    time.sleep(0.8)
    deadline = time.time() + timeout
    seen = b""
    while time.time() < deadline and not seen:
        seen = port.read(65536)
    if not seen:
        raise TimeoutError("console gave no reply; refusing to send")
    for marker in DESKTOP_MARKERS:
        if marker in seen:
            raise TimeoutError(
                "the desktop is running on this console; exit it first "
                "(F10, End, Enter) or a raw send will be typed into a window")
    if b"root@tinytang" not in seen:
        raise TimeoutError(
            "no shell prompt on the console; refusing to send raw bytes "
            f"(saw: {seen[-120:]!r})")


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
        require_shell(port)
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
