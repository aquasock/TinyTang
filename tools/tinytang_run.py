#!/usr/bin/env python3
"""Run a shell command on a TinyTang board over its USB CDC console.

The board's shell is the interface: this just opens the console, sends one
line, and streams whatever comes back for a fixed window.  Useful for driving
commands that take a while and print progress, such as `nesload`.

    tools/tinytang_run.py "nesload /roms/castlevania.nes" --seconds 30

The board must be in two-wire mode and running a TinyTang firmware, and the
console must be at a shell prompt: each line is sent only after the board says
so (tinytang_console), so a line is never typed into the desktop or into a
command that is still running.
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


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", nargs="+", help="command line(s) to send, one per argument")
    ap.add_argument("--port", default="/dev/ttyACM0")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--seconds", type=float, default=8.0,
                    help="how long to keep streaming after the last command")
    ap.add_argument("--wait", type=float, default=10.0,
                    help="how long to wait for the prompt before each command")
    args = ap.parse_args()

    print(f"opening {args.port}")
    port = serial.Serial(args.port, args.baud, timeout=0.3, write_timeout=30.0)

    def show(data):
        sys.stdout.write(data.decode("utf-8", "replace"))
        sys.stdout.flush()

    try:
        for command in args.command:
            require_shell(port, timeout=args.wait, on_output=show)
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
    try:
        main()
    except ConsoleNotReady as exc:
        sys.exit(str(exc))
