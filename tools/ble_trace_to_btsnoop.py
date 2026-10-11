#!/usr/bin/env python3
"""Turn a board's `bletrace dump` into a btsnoop file that btmon can read.

The board records every HCI packet its Bluetooth host sends and receives
(ports/bl616/tang_ble_trace.c) and prints them as lines of

    BT <microseconds> <type> <length> <hex>

where <type> is the Linux monitor protocol's packet type (2 command, 3 event,
4 ACL out, 5 ACL in).  That is also btsnoop's "monitor" datalink (2001), so
the conversion is direct, and `btmon -r` then decodes the board's traffic the
same way it decodes a capture taken on a PC:

    tools/tinytang_run.py "bletrace dump" --seconds 5 > trace.txt
    tools/ble_trace_to_btsnoop.py trace.txt board.snoop
    btmon -r board.snoop

Packets the board cut short (it keeps at most 96 bytes of each) are written
with their true length and the bytes that were kept.  Any other text in the
input, such as the shell's prompt, is ignored.
"""

import argparse
import re
import struct
import sys
import time

DATALINK_MONITOR = 2001
# btsnoop timestamps are microseconds since midnight, 1 January 0 AD.
EPOCH_DELTA_US = 0x00DCDDB30F2F8000
NEW_INDEX = 0

LINE = re.compile(r"^BT (\d+) (\d+) (\d+) ([0-9a-f]*)\s*$")


def record(out, opcode, data, orig_len, ts_us):
    flags = (0 << 16) | opcode  # controller index 0
    out.write(struct.pack(">IIIIq", orig_len, len(data), flags, 0, ts_us))
    out.write(data)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("dump", help="text captured from `bletrace dump`")
    ap.add_argument("out", help="btsnoop file to write")
    args = ap.parse_args()

    packets = []
    with open(args.dump, errors="replace") as f:
        for raw in f:
            m = LINE.match(raw.strip())
            if m:
                t, op, length, hexdata = m.groups()
                packets.append((int(t), int(op), int(length), bytes.fromhex(hexdata)))
    if not packets:
        sys.exit("no BT lines found in %s" % args.dump)

    # Board times are a free-running 32-bit microsecond counter; anchor the
    # first packet at the time of conversion and unwrap.
    base = EPOCH_DELTA_US + int(time.time() * 1e6)
    first = packets[0][0]
    with open(args.out, "wb") as out:
        out.write(b"btsnoop\0" + struct.pack(">II", 1, DATALINK_MONITOR))
        # Name the controller so btmon labels it.
        new_index = struct.pack("<BB6s8s", 0, 0, bytes(6), b"tinytang")
        record(out, NEW_INDEX, new_index, len(new_index), base)
        wraps, last = 0, first
        for t, op, length, data in packets:
            if t < last:
                wraps += 1
            last = t
            ts = base + (t + wraps * (1 << 32) - first)
            record(out, op, data, length, ts)
    print("%d packet(s) written to %s" % (len(packets), args.out))


if __name__ == "__main__":
    main()
