#!/usr/bin/env python3
"""Run a shell command on a TinyTang board over its USB CDC console.

The board's shell is the interface: this just opens the console, sends one
line, and streams whatever comes back for a fixed window.  Useful for driving
commands that take a while and print progress, such as `nesload`.

    tools/tinytang_run.py "nesload /roms/castlevania.nes" --seconds 30

The board must be in two-wire mode and running a TinyTang firmware.
"""

import argparse
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("pyserial is required: pip install pyserial")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", nargs="+", help="command line(s) to send, one per argument")
    ap.add_argument("--port", default="/dev/ttyACM0")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--seconds", type=float, default=8.0,
                    help="how long to keep streaming after the last command")
    args = ap.parse_args()

    print(f"opening {args.port}")
    port = serial.Serial(args.port, args.baud, timeout=0.3, write_timeout=30.0)
    try:
        port.write(b"\r")
        time.sleep(0.7)
        port.read(65536)

        for command in args.command:
            print(f"$ {command}")
            port.write(command.encode() + b"\r")
            time.sleep(0.5)
            sys.stdout.write(port.read(65536).decode("utf-8", "replace"))
            sys.stdout.flush()

        deadline = time.time() + args.seconds
        while time.time() < deadline:
            chunk = port.read(4096)
            if chunk:
                sys.stdout.write(chunk.decode("utf-8", "replace"))
                sys.stdout.flush()
    finally:
        port.close()
    print()


if __name__ == "__main__":
    main()
