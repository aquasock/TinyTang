#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Write a deterministic test WAV for the Phosphor player.

Four rising notes, half a second each, then the same four in the right channel
only -- so a listener can tell at once that audio is playing, that it plays to
the end, and that left and right are not swapped.  Signed 16-bit stereo at
44.1 kHz, the core's CD-quality profile (PHOS-001), at about -12 dBFS so a
first test at an unknown volume is not loud.

The same arguments always produce the same bytes, so the file's size and CRC
are a fixed reference for what `phosphor play` should report.

    tools/make_test_wav.py build/test-tones.wav
"""

import math
import struct
import sys
import zlib

RATE = 44100
NOTES = (440.0, 554.37, 659.25, 880.0)   # A4, C#5, E5, A5
NOTE_SECONDS = 0.5
AMPLITUDE = 0.25                          # about -12 dBFS
FADE = 0.01                               # 10 ms ramps, so notes do not click


def note(freq, left, right):
    n = int(RATE * NOTE_SECONDS)
    fade = int(RATE * FADE)
    out = bytearray()
    for i in range(n):
        env = min(1.0, i / fade, (n - 1 - i) / fade)
        v = int(round(32767 * AMPLITUDE * env * math.sin(2 * math.pi * freq * i / RATE)))
        out += struct.pack("<hh", v if left else 0, v if right else 0)
    return out


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    pcm = bytearray()
    for f in NOTES:
        pcm += note(f, True, True)
    for f in NOTES:
        pcm += note(f, False, True)
    header = (b"RIFF" + struct.pack("<I", 36 + len(pcm)) + b"WAVE" +
              b"fmt " + struct.pack("<IHHIIHH", 16, 1, 2, RATE, RATE * 4, 4, 16) +
              b"data" + struct.pack("<I", len(pcm)))
    data = header + pcm
    with open(sys.argv[1], "wb") as f:
        f.write(data)
    print(f"{sys.argv[1]}: {len(data)} bytes, CRC-32 0x{zlib.crc32(data) & 0xffffffff:08x}, "
          f"{len(pcm) // 4 / RATE:.1f} s")


if __name__ == "__main__":
    main()
