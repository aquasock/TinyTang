#!/usr/bin/env python3
"""Check that the OLED shell's stdio output never reaches the USB console.

Starts the fire demo in the OLED Terminal's session (`oledterm run`), then
runs `cat /tang.ini` from the USB console again and again while it plays, and
counts the demo's colour codes in what comes back over USB.  Both commands
write through C stdio; if the two sessions share one buffered stdout, frame
bytes left in it are flushed through the console's route and show up here.

    tools/check_stdout_isolation.py [--rounds 40]

Needs /demos/fire.tdsh and its frames on the card (tools/make_oled_demo.py),
the desktop core loaded, and the console at a shell prompt; every command is
sent only after the board says so, with any half-typed line cleared first.
The OLED panel itself need not be attached.  Exit status 0 means no leak.
SPDX-License-Identifier: MIT
"""
import argparse
import os
import re
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("pyserial is required: pip install pyserial")

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tinytang_console import ConsoleNotReady, require_shell  # noqa: E402

# The demo frames set a foreground and a background colour together; nothing
# the console prints for `cat /tang.ini` does.
FRAME_SGR = re.compile(rb'\x1b\[(?:3[0-7]|9[0-7]);(?:4[0-7]|10[0-7])m')


def send(port, line, settle=0.6):
    require_shell(port, timeout=30)
    port.write(b'\x15' + line.encode() + b'\r')
    port.flush()
    out, end = b'', time.time() + settle
    while time.time() < end:
        chunk = port.read(4096)
        out += chunk
        if chunk:
            end = time.time() + 0.2
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--port', default='/dev/ttyACM0')
    ap.add_argument('--rounds', type=int, default=40)
    args = ap.parse_args()
    port = serial.Serial(args.port, 115200, timeout=0.1)
    try:
        send(port, 'oledterm run "tdsh run /demos/fire.tdsh"')
        time.sleep(1.0)
        leaked = 0
        for _ in range(args.rounds):
            leaked += len(FRAME_SGR.findall(send(port, 'cat /tang.ini')))
    finally:
        port.close()
    print(f'{args.rounds} console commands during the OLED demo: '
          f'{leaked} frame colour codes reached USB')
    return 1 if leaked else 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except ConsoleNotReady as exc:
        sys.exit(str(exc))
