#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Play every format in the Phosphor corpus on the board and check each one.

The corpus is tools/make_codec_corpus.sh's twelve files in /music on the card,
the same set Tang-Phosphor qualified in its entry 43.  With the Phosphor core
loaded and the desk layer off, each file is played through the resident AE350
Rockbox player with `phosphor play`, and a file passes when the player reports
it complete with zero underruns.  The sample count is compared with what entry
43 recorded for the same file on the same player, and a difference is reported
for review rather than waved through; the output rate must be the file's own.
The order puts Opus, the one 48 kHz file, after WMA at 44.1 kHz, and the sweep
ends by playing the WAV again, so the rate is seen to change both ways.

    tools/phosphor_format_sweep.py [--only mp3,flac] [--port /dev/ttyACM0]

Nothing is sent unless the console is first shown to be a plain shell prompt
(tinytang_put.require_shell), because bytes sent while the desktop is up are
typed into whichever window has focus.
"""

import argparse
import re
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("pyserial is required: pip install pyserial")

sys.path.insert(0, __import__("os").path.dirname(__file__))
from tinytang_put import require_shell  # noqa: E402

# Samples presented per file in Tang-Phosphor entry 43 (2026-10-02), the last
# full sweep of this corpus.  FLAC's 444240 against a nominal 441000 is that
# entry's own unexplained observation, carried as-is.
EXPECTED = [
    ("wav", 441000, 44100), ("flac", 444240, 44100), ("mp2", 440735, 44100),
    ("mp3", 441000, 44100), ("ogg", 441000, 44100), ("mp4", 441000, 44100),
    ("m4a", 441000, 44100), ("wv", 441000, 44100), ("ac3", 442368, 44100),
    ("tta", 441000, 44100), ("wma", 442368, 44100), ("opus", 479688, 48000),
]

PROMPT = b"root@tinytang"
DONE = re.compile(rb"playback complete: (\d+) samples, (\d+) underruns, (\d+) Hz")


def run(port, command, timeout):
    """Send one command line and return everything up to the next prompt."""
    port.reset_input_buffer()
    port.write(command.encode() + b"\r")
    seen = b""
    deadline = time.time() + timeout
    while time.time() < deadline:
        chunk = port.read(4096)
        if chunk:
            seen += chunk
            # The echo carries no prompt; the prompt after the output does.
            if seen.count(PROMPT) >= 1 and seen.rstrip().endswith(b"#"):
                break
    return re.sub(rb"\x1b\[[0-9;]*[A-Za-z]", b"", seen).decode("utf-8", "replace")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", default="/dev/ttyACM0")
    ap.add_argument("--only", help="comma-separated extensions to play")
    ap.add_argument("--timeout", type=float, default=60.0,
                    help="seconds to allow each file")
    args = ap.parse_args()

    only = set(args.only.split(",")) if args.only else None
    port = serial.Serial(args.port, 115200, timeout=0.3, write_timeout=30.0)
    failures = 0
    try:
        require_shell(port)
        plan = [(ext, n, rate, "") for ext, n, rate in EXPECTED
                if not only or ext in only]
        if not only:
            plan.append(("wav", 441000, 44100, "  after Opus: 48 -> 44.1 kHz"))
        for ext, expected, expected_rate, label in plan:
            path = f"/music/test.{ext}"
            out = run(port, f"phosphor play {path}", args.timeout)
            m = DONE.search(out.encode())
            if not m:
                failures += 1
                last = [l for l in out.splitlines() if l.startswith("phosphor:")][-1:]
                print(f"FAIL  {ext:5s} {last[0] if last else 'no result before timeout'}{label}")
                continue
            samples, underruns, rate = (int(v) for v in m.groups())
            ok = underruns == 0 and rate == expected_rate
            note = "" if samples == expected else f"  (entry 43: {expected})"
            failures += 0 if ok else 1
            print(f"{'PASS' if ok else 'FAIL'}  {ext:5s} {samples:7d} samples  "
                  f"{underruns} underruns  {rate} Hz{note}{label}")
            time.sleep(0.5)
    finally:
        port.close()

    print("sweep:", "PASS" if failures == 0 else f"FAIL ({failures})")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
