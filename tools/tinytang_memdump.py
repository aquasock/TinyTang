#!/usr/bin/env python3
"""Dump a memory region the board's `peek` allows, and find strings in it.

    tools/tinytang_memdump.py --region flash --out build/memdump/flash.bin
    tools/tinytang_memdump.py --region tzc_sec --out /tmp/tzc.bin --list-strings

Works from the region table the firmware itself reports (`peek -l`), so it can
only dump what the port has listed, and it checks every line of every reply
against the address it asked for: a dumper that quietly writes garbage is
worse than no dumper.  Widths come from the region's own entry.

The board must be in two-wire mode, running a firmware with the peek/poke
memory regions, and at a shell prompt (tools/tinytang_console.py).
SPDX-License-Identifier: MIT
"""

import argparse
import hashlib
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

ROW = re.compile(rb"^0x([0-9a-f]+):((?: [0-9a-f]{2})+)\s*$")
REGION = re.compile(rb"^(\S+)\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+([\d ]+)\s+(read|read/write)\s*$")


def send(port, line, settle=0.4, quiet=0.25):
    require_shell(port, timeout=20)
    port.write(b"\x15" + line.encode("ascii") + b"\r")
    port.flush()
    out, end = b"", time.time() + settle
    while time.time() < end:
        chunk = port.read(8192)
        if chunk:
            out += chunk
            end = time.time() + quiet
    return out


def regions(port):
    """Return {name: (start, end_inclusive, width_bits)} from the firmware's own `peek -l`.

    The WIDTHS column is in bits (8, 16, 32); the dump uses the smallest the
    region allows, for a byte-true image.  The listing's END is inclusive."""
    found = {}
    for raw in send(port, "peek -l", settle=1.0).replace(b"\r\n", b"\n").split(b"\n"):
        m = REGION.match(raw.strip())
        if not m:
            continue
        name, start, end = m.group(1).decode(), int(m.group(2), 16), int(m.group(3), 16)
        widths = [int(w) for w in m.group(4).split()]
        if not widths:
            continue
        found[name] = (start, end, min(widths))
    return found


def dump(port, start, end, width):
    """Read [start, end) at `width`, verifying every row's address."""
    per_row = 16 // width
    step = 2048                                  # bytes per command
    values = []
    at = start
    while at < end:
        chunk_end = min(at + step, end)
        count = (chunk_end - at) // width
        if count == 0:
            break
        out = send(port, f"peek -w {width * 8} 0x{at:08X} {count}", settle=0.5)
        got, expect = 0, at
        for raw in out.replace(b"\r\n", b"\n").split(b"\n"):
            m = ROW.match(raw.strip())
            if not m:
                continue
            row_at = int(m.group(1), 16)
            if row_at != expect:
                sys.exit(f"dump: expected 0x{expect:08X}, board printed 0x{row_at:08X}; aborting")
            data = bytes(int(b, 16) for b in m.group(2).split())
            values += data
            got += len(data)
            expect = row_at + per_row * width
        if got != count * width:
            sys.exit(f"dump: asked for {count * width} bytes at 0x{at:08X}, parsed {got}; aborting")
        at = chunk_end
        print(f"\r  {100 * (at - start) // max(1, end - start)}%  "
              f"{(at - start) // 1024} KiB", end="", flush=True)
    print()
    return bytes(values)


def strings(data, minimum):
    """Printable ASCII runs, with their offsets, for a quick look inside."""
    found = []
    run = b""
    for i, byte in enumerate(data):
        if 0x20 <= byte < 0x7F:
            run += bytes([byte])
        else:
            if len(run) >= minimum:
                found.append((i - len(run), run))
            run = b""
    if len(run) >= minimum:
        found.append((len(data) - len(run), run))
    return found


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--region", default="rom", help="a region name from `peek -l`")
    ap.add_argument("--out", required=True, help="file to write the bytes to")
    ap.add_argument("--start", help="start address (default: the region's start)")
    ap.add_argument("--length", type=lambda v: int(v, 0), help="bytes to dump (default: whole region)")
    ap.add_argument("--port", default="/dev/ttyACM0")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--min-string", type=int, default=6, help="shortest run reported by --list-strings")
    ap.add_argument("--list-strings", action="store_true", help="print printable runs found")
    args = ap.parse_args()

    port = serial.Serial(args.port, args.baud, timeout=0.2, write_timeout=30.0)
    try:
        known = regions(port)
        if not known:
            sys.exit("no memory regions: the firmware has no peek/poke table")
        if args.region not in known:
            sys.exit(f"no region named {args.region!r}; the board lists: "
                     + ", ".join(sorted(known)))
        start, end_inclusive, width_bits = known[args.region]
        end = end_inclusive + 1                    # the listing's END is inclusive
        if args.start:
            start = int(args.start, 0)
        if args.length:
            end = min(start + args.length, end)
        if not (known[args.region][0] <= start < end <= end_inclusive + 1):
            sys.exit(f"{args.region} covers 0x{known[args.region][0]:08X}-0x{end_inclusive:08X}")
        width = width_bits // 8                    # bytes per access
        print(f"{args.region}: 0x{start:08X}-0x{end - 1:08X}, {end - start} bytes, "
              f"{width_bits}-bit access")
        data = dump(port, start, end, width)
    finally:
        port.close()

    directory = os.path.dirname(os.path.abspath(args.out))
    os.makedirs(directory, exist_ok=True)
    with open(args.out, "wb") as fh:
        fh.write(data)
    print(f"wrote {args.out}: {len(data)} bytes, sha256 {hashlib.sha256(data).hexdigest()}")

    if args.list_strings:
        found = strings(data, args.min_string)
        print(f"{len(found)} printable runs of {args.min_string}+ characters:")
        for offset, run in found:
            print(f"  0x{start + offset:08X}  {run.decode('ascii', 'replace')}")


if __name__ == "__main__":
    try:
        main()
    except (TimeoutError, ConsoleNotReady) as exc:
        sys.exit(str(exc))
