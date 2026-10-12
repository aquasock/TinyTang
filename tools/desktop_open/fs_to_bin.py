#!/usr/bin/env python3
"""Convert a Gowin text `.fs` bitstream into the vendor binary `.bin`.

    tools/fs-to-bin.py <input.fs> [-o output.bin]

Gowin ships the same configuration data in two encodings, and the difference is
only the encoding:

* `.fs` -- text. Each line is a run of ASCII `0` and `1`, one character per
  configuration bit, with an optional `//` comment header. This is what
  `gowin_pack` writes and what `openFPGALoader` accepts.
* `.bin` -- the same bits packed eight to a byte, most significant bit first,
  with no framing of its own. This is what TinyTang's in-firmware `tangload`
  writes to the FPGA's configuration port, so it is the form a core must be in
  to live on the SD card.

Nothing is added, prepended or appended: the `.bin` is the packed bit stream
and nothing else. That includes the device idcode, the user code and the CRC,
which are fields *inside* the configuration data rather than around it.

Measured rather than assumed, on 2026-10-08, against a design for which both
encodings exist (`TinyTang/build/desktop/source/impl/pnr/desktop.{fs,bin}`):
the `.fs` carries 36,192,256 data characters and the `.bin` is 4,524,032 bytes
-- exactly one eighth, with no remainder -- and packing the one reproduces the
other byte for byte (`sha256 c8406c7f...`, `cmp` clean).

The mapping was verified while the `.fs` header said `//Compress: OFF`; a
compressed `.fs` would not fit it.

SPDX-License-Identifier: MIT
"""

import argparse
import hashlib
import os
import sys


def fs_data_lines(path):
    """Yield each `.fs` data line as a string of `0`/`1`, skipping comments.

    Blank lines and the `//` header are dropped. A file whose data lines are
    not all binary is refused rather than half-converted.
    """
    with open(path) as fh:
        for lineno, line in enumerate(fh, 1):
            if line.startswith("//"):
                continue
            text = line.strip()
            if not text:
                continue
            if set(text) - {"0", "1"}:
                raise ValueError(
                    f"{path}: line {lineno} is not a binary run "
                    f"({text[:32]!r}); this is not an uncompressed .fs")
            yield line, text


def convert(src, dst):
    """Pack `src` into `dst`; return `(bits, bytes_written)`."""
    out = bytearray()
    acc = 0
    count = 0
    bits = 0
    for _line, text in fs_data_lines(src):
        bits += len(text)
        for ch in text:
            acc = (acc << 1) | (ch == "1")
            count += 1
            if count == 8:
                out.append(acc)
                acc = 0
                count = 0
    if count:
        raise ValueError(
            f"{src}: {bits} data bits is not a whole number of bytes "
            f"({count} left over)")
    with open(dst, "wb") as fh:
        fh.write(out)
    return bits, len(out)


def main():
    parser = argparse.ArgumentParser(
        description="Convert a Gowin text .fs bitstream to the vendor .bin")
    parser.add_argument("input", help="the .fs to read")
    parser.add_argument("-o", "--output", default=None,
                        help="the .bin to write (default: input with .bin)")
    args = parser.parse_args()

    dst = args.output
    if dst is None:
        root, ext = os.path.splitext(args.input)
        dst = root + ".bin"

    bits, size = convert(args.input, dst)
    digest = hashlib.sha256(open(dst, "rb").read()).hexdigest()
    print(f"{args.input}: {bits} bits -> {size} bytes")
    print(f"{dst}: sha256 {digest}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
