#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Find the SDRAM plane's read alignment on the board, from the PC.

The desktop core's 1280x720 plane (desktop ABI 1.9, fpga/desktop/fb_sdram.sv)
reads the dock's SDRAM with a read alignment that depends on the board's pin
and access delays, so the core lets it be set at run time (link frame 0x1B:
bits 2:0 clocks from READ to capture, bit 3 the falling edge, half a clock
earlier) and reports a checksum of every frame it scans out (registers
0x14/0x18, Fletcher-style: s2 += s1; s1 += p over the 921,600 visible RGB565
pixels in raster order) with its status (0x10).

This tool takes the link with the shell's `fpgapipe`, draws a known picture
into the plane, and for every alignment compares the core's checksum with the
one computed here.  A setting whose frames come back intact passes; the
passing settings should form one run in sample time, and the middle of it is
the one to build in.

    tools/plane_sweep.py                 # draw, sweep, show the picture at the best
    tools/plane_sweep.py --no-draw       # sweep what the plane already holds
    tools/plane_sweep.py --off           # just hide the plane again
    tools/plane_sweep.py --no-draw --soak 60   # check every frame for a minute
    tools/plane_sweep.py --baud 10000000 --fill --soak 60
                                         # negotiate 10 Mbaud (ABI 1.10), let the
                                         # BL616 draw the picture (`planefill`,
                                         # timed), then check every frame

With --book, the plane's buffers (desktop ABI 1.11) are checked against a
TinyReader book on the PC: buffer first+k must hold spread start+k, loaded
beforehand with the shell's `readerdraw <book> <spread> <buffer>`.  Each buffer
is shown in turn (frame 0x1D) and its frames' checksum compared with the
spread's, and the switch is timed in frames.

    tools/plane_sweep.py --book zelda.trb --buffers 1:0 --count 11

With --baud the link's rate is negotiated through the shell (`fpgabaud`) before
the pipe opens, so everything after runs at it: the frames the PC draws, and
the CRC-checked register reads that carry the checksums back.  A clean soak
therefore proves both directions at that rate.

The board is found by its USB identity (ffff:5454), never by a fixed ttyACM
name, and the console must be at a shell prompt (tinytang_console): nothing is
sent otherwise.  Closing the port ends the pipe; the plane keeps what it was
last told, so it stays on until --off or an app hides it.
"""

import argparse
import os
import struct
import sys
import time

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    sys.exit("pyserial is required: pip install pyserial")

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tinytang_console import ConsoleNotReady, require_shell  # noqa: E402

USB_VID, USB_PID = 0xFFFF, 0x5454
W, H = 1280, 720
ABI_19 = 0x00010009
ABI_110 = 0x0001000A
ABI_111 = 0x0001000B


def find_port():
    ports = [p.device for p in list_ports.comports()
             if p.vid == USB_VID and p.pid == USB_PID]
    if len(ports) != 1:
        sys.exit(f"expected one TinyTang console (ffff:5454), found {ports or 'none'}")
    return ports[0]


def shell(ser, line, until, timeout=60.0):
    """One shell command; the text up to `until` (or the timeout)."""
    ser.reset_input_buffer()
    ser.write(line.encode() + b"\r")
    seen, deadline = b"", time.time() + timeout
    while until.encode() not in seen and time.time() < deadline:
        seen += ser.read(256)
    return seen.decode(errors="replace")


def crc16(data, crc=0xFFFF):
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if crc & 0x8000 else (crc << 1)
            crc &= 0xFFFF
    return crc


class Link:
    """The core's UART frames, through the shell's fpgapipe."""

    def __init__(self, ser):
        self.ser = ser
        self.rx = b""
        self.seq = 0

    def frame(self, typ, payload=b""):
        n = len(payload) + 1
        self.ser.write(bytes([0xAA, n >> 8, n & 0xFF, typ]) + payload)

    def read32(self, addr, timeout=1.0):
        self.seq = (self.seq + 1) & 0xFFFF
        req = bytes([1, 1, self.seq >> 8, self.seq & 0xFF]) + struct.pack(">II", addr, 0)
        req += struct.pack(">H", crc16(bytes([0x10]) + req))
        self.ser.flush()
        self.frame(0x10, req)
        deadline = time.time() + timeout
        while time.time() < deadline:
            self.rx += self.ser.read(max(1, self.ser.in_waiting))
            while True:
                i = self.rx.find(b"\xAA")
                if i < 0:
                    self.rx = b""
                    break
                self.rx = self.rx[i:]
                if len(self.rx) < 4:
                    break
                n = (self.rx[1] << 8) | self.rx[2]
                if len(self.rx) < 3 + n:
                    break
                typ, body = self.rx[3], self.rx[4:3 + n]
                self.rx = self.rx[3 + n:]
                if typ == 0x10 and len(body) == 15 and ((body[3] << 8) | body[4]) == self.seq:
                    if crc16(bytes([0x10]) + body[:-2]) != (body[13] << 8 | body[14]):
                        raise IOError(f"reply to {addr:#x} has a bad CRC")
                    if body[2] != 0:
                        raise IOError(f"register {addr:#x} answered status {body[2]}")
                    return struct.unpack(">I", body[9:13])[0]
        raise TimeoutError(f"no reply reading register {addr:#x}")


def book_sums(path, spread):
    """The plane checksum of one spread of a .trb, as the core computes it."""
    import numpy as np
    with open(path, "rb") as f:
        head = f.read(128)
        w, h = struct.unpack_from("<HH", head, 8)
        off, sb = struct.unpack_from("<II", head, 16)
        if head[:4] != b"TRB1" or (w, h) != (W, H):
            sys.exit(f"{path}: not a 1280x720 TinyReader book")
        f.seek(off + spread * sb)
        p = np.frombuffer(f.read(sb), ">u2").astype(np.int64)
    n = p.size
    s1 = int(p.sum()) & 0xFFFFFFFF
    s2 = int((p * (n - 1 - np.arange(n, dtype=np.int64))).sum()) & 0xFFFFFFFF
    return s1, s2


def check_buffers(link, book, first_buffer, first_spread, count):
    """Show each buffer and compare its frames with the spread it should hold."""
    bad = 0
    for k in range(count):
        b, sp = first_buffer + k, first_spread + k
        want = book_sums(book, sp)
        seen0 = link.read32(0x10) >> 16 & 0xFF
        link.frame(0x1D, bytes([b]))
        link.ser.flush()
        t0 = time.time()
        frames, got, st = 0, None, 0
        # The switch is taken at a frame boundary: within two frames the
        # checksum must be the spread's, and stay it.
        while time.time() - t0 < 1.0:
            st = link.read32(0x10)
            seen = st >> 16 & 0xFF
            if seen != seen0:
                frames += (seen - seen0) & 0xFF
                seen0 = seen
                got = (link.read32(0x14), link.read32(0x18))
                if got == want and (st >> 12 & 15) == b:
                    break
        ok = got == want and (st >> 12 & 15) == b
        bad += not ok
        print(f"  buffer {b:2d}: spread {sp:3d}  {'intact' if ok else 'WRONG '}  "
              f"shown after {frames} frame{'s' if frames != 1 else ''}  "
              f"{got[0] if got else 0:08x} {got[1] if got else 0:08x}")
    return bad


def pattern(x, y):
    """RGB565.  A gradient in red and green, blue from x ^ y so neighbouring
    pixels differ in every bit position over a row, a white grid every 64
    pixels, and a one-pixel checkerboard block that only a 1:1 plane shows."""
    if x % 64 == 0 or y % 64 == 0:
        return 0xFFFF
    if 576 <= x < 704 and 296 <= y < 424:
        return 0xFFFF if (x ^ y) & 1 else 0x0000
    r = x * 31 // (W - 1)
    g = y * 63 // (H - 1)
    b = (x ^ y) & 31
    return (r << 11) | (g << 5) | b


def checksum(pix):
    s1 = s2 = 0
    for p in pix:
        s2 = (s2 + s1) & 0xFFFFFFFF
        s1 = (s1 + p) & 0xFFFFFFFF
    return s1, s2


def draw(link, rows):
    t0 = time.time()
    for y in range(H):
        link.frame(0x18, struct.pack(">HH", 0, y))
        row = rows[y]
        for half in (row[:640], row[640:]):
            link.frame(0x19, b"".join(struct.pack(">H", p) for p in half))
        if y % 90 == 89:
            print(f"  drawn {y + 1} of {H} rows, {time.time() - t0:.1f} s", flush=True)
    link.ser.flush()


def link_text(link):
    counts = link.read32(0x20)
    return (f"link at {link.read32(0x1C)} baud; {counts >> 16} fallbacks, "
            f"{counts & 0xFFFF} framing errors since configuration")


def status_text(st):
    return (f"ready {st >> 31 & 1} underrun {st >> 30 & 1} dropped {st >> 29 & 1} "
            f"cfg {st >> 24 & 15:#x} shown {st >> 12 & 15} fetch {st & 0x7FF}")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", help="console device (default: found by USB identity)")
    ap.add_argument("--no-draw", action="store_true", help="do not draw the picture first")
    ap.add_argument("--off", action="store_true", help="hide the plane and stop")
    ap.add_argument("--set", type=lambda v: int(v, 0), help="set this alignment and stop")
    ap.add_argument("--soak", type=float, metavar="SECONDS",
                    help="check every frame at the current alignment for this long, then stop")
    ap.add_argument("--baud", type=int, metavar="RATE",
                    help="negotiate the link to 2000000, 5000000 or 10000000 first (ABI 1.10)")
    ap.add_argument("--fill", action="store_true",
                    help="have the BL616 draw the picture (shell `planefill`, timed); implies --no-draw")
    ap.add_argument("--book", help="check buffers against this .trb (ABI 1.11)")
    ap.add_argument("--buffers", default="1:0", metavar="BUF:SPREAD",
                    help="with --book: the first buffer and the spread it holds (default 1:0)")
    ap.add_argument("--count", type=int, default=11, help="with --book: buffers to check")
    ap.add_argument("--frames", type=int, default=3,
                    help="frames that must match at each setting (default 3)")
    args = ap.parse_args()

    port = args.port or find_port()
    ser = serial.Serial(port, 115200, timeout=0.05, write_timeout=120)
    try:
        require_shell(ser)
    except ConsoleNotReady as e:
        sys.exit(f"{port}: {e}")
    if args.baud:
        out = shell(ser, f"fpgabaud {args.baud}", "fpgabaud: ", 10)
        time.sleep(0.3)
        out += ser.read(4096).decode(errors="replace")
        print(" ".join(l.strip() for l in out.splitlines() if l.startswith("fpgabaud")))
        if "both ends at" not in out:
            sys.exit("the rate was not negotiated")
    if args.fill:
        out = shell(ser, "planefill", "planefill: ", 120)
        time.sleep(0.3)
        out += ser.read(4096).decode(errors="replace")
        line = [l.strip() for l in out.splitlines() if l.startswith("planefill:")]
        print(line[-1] if line else out[-300:])
        if not line or "bytes in" not in line[-1]:
            sys.exit("planefill did not draw")
        args.no_draw = True
    ser.write(b"fpgapipe\r")
    seen, deadline = b"", time.time() + 3
    while b"fpgapipe: open" not in seen and time.time() < deadline:
        seen += ser.read(256)
    if b"fpgapipe: open" not in seen:
        sys.exit(f"fpgapipe did not open: {seen[-200:]!r}")
    time.sleep(0.2)
    ser.reset_input_buffer()
    link = Link(ser)

    abi = link.read32(0x04)
    geo = link.read32(0x0C)
    print(f"core ABI {abi:#010x}, plane {geo >> 16}x{geo & 0xFFFF}, "
          f"status {status_text(link.read32(0x10))}")
    if abi < ABI_19:
        sys.exit("the loaded core has no SDRAM plane (desktop ABI 1.9 or later)")
    if abi >= ABI_110:
        print(link_text(link))

    if args.book:
        if abi < ABI_111:
            sys.exit("the loaded core has one plane buffer (desktop ABI 1.11 has twelve)")
        b0, s0 = (int(v) for v in args.buffers.split(":"))
        link.frame(0x1A, b"\x01")
        print(f"buffers {b0} to {b0 + args.count - 1} against {args.book}, spreads {s0} on")
        bad = check_buffers(link, args.book, b0, s0, args.count)
        print(f"{args.count - bad} of {args.count} buffers intact; status {status_text(link.read32(0x10))}")
        sys.exit(1 if bad else 0)

    if args.off:
        link.frame(0x1A, b"\x00")
        ser.flush()
        print("plane hidden")
        return
    if args.set is not None:
        link.frame(0x1B, bytes([args.set & 15]))
        ser.flush()
        time.sleep(0.1)
        print(f"alignment {args.set:#x}: status {status_text(link.read32(0x10))}")
        return

    rows = [[pattern(x, y) for x in range(W)] for y in range(H)]
    want = checksum(p for row in rows for p in row)
    print(f"expected checksum {want[0]:08x} {want[1]:08x}")
    if not args.no_draw:
        print("drawing 1280x720 through the link")
        t0 = time.time()
        draw(link, rows)
        dt = time.time() - t0
        print(f"drawn in {dt:.1f} s, {W * H * 2 / dt / 1024:.0f} KB/s through the pipe")
    link.frame(0x1A, b"\x01")

    if args.soak:
        # Every latched frame's checksum, at whatever alignment is set.  A
        # frame can be missed between reads; the count of those is reported.
        end = time.time() + args.soak
        st = link.read32(0x10)
        seen0, frames, bad, missed = st >> 16 & 0xFF, 0, 0, 0
        while time.time() < end:
            st = link.read32(0x10)
            seen = st >> 16 & 0xFF
            if seen == seen0:
                continue
            missed += ((seen - seen0) & 0xFF) - 1
            got = (link.read32(0x14), link.read32(0x18))
            if (link.read32(0x10) >> 16 & 0xFF) != seen:
                missed += 1          # a newer frame landed mid-read: discard
                seen0 = seen
                continue
            seen0 = seen
            frames += 1
            if got != want:
                bad += 1
                print(f"  frame {frames}: {got[0]:08x} {got[1]:08x}")
        st = link.read32(0x10)
        print(f"soak at alignment {st >> 24 & 15:#x}: {frames} frames checked, {bad} bad, "
              f"{missed} not read; status {status_text(st)}")
        if abi >= ABI_110:
            print(link_text(link))
        sys.exit(1 if bad or st >> 29 & 3 else 0)

    results = []
    for cfg in [d for d in range(1, 8)] + [8 | d for d in range(1, 8)]:
        link.frame(0x1B, bytes([cfg]))
        ser.flush()
        time.sleep(0.06)                     # the old setting's frame drains
        seen0 = link.read32(0x10) >> 16 & 0xFF
        good = 0
        sums = []
        for _ in range(args.frames):
            # wait for the next latched frame
            deadline = time.time() + 0.5
            while (link.read32(0x10) >> 16 & 0xFF) == seen0 and time.time() < deadline:
                time.sleep(0.005)
            st = link.read32(0x10)
            seen0 = st >> 16 & 0xFF
            got = (link.read32(0x14), link.read32(0x18))
            sums.append(got)
            good += got == want
        when = (cfg & 7) - (0.5 if cfg & 8 else 0)
        results.append((when, cfg, good, sums[-1], st))
    results.sort()
    print("\nsample time  setting        frames intact  last checksum       status")
    for when, cfg, good, s, st in results:
        edge = "falling" if cfg & 8 else "rising "
        print(f"  t+{when:<4}     {cfg & 7} {edge}    {good}/{args.frames}"
              f"            {s[0]:08x} {s[1]:08x}   {status_text(st)}")

    passing = [r for r in results if r[2] == args.frames]
    if not passing:
        link.frame(0x1B, b"\x03")
        ser.flush()
        print("\nno setting passed; the alignment is back at the built-in 3, rising edge")
        sys.exit(1)
    # The longest run of consecutive passing sample times; its middle.
    times = [r[0] for r in results]
    best, run = [], []
    for r in results:
        if r[2] == args.frames and (not run or times.index(r[0]) == times.index(run[-1][0]) + 1):
            run.append(r)
        elif r[2] == args.frames:
            run = [r]
        else:
            run = []
        if len(run) > len(best):
            best = list(run)
    mid = best[(len(best) - 1) // 2]
    link.frame(0x1B, bytes([mid[1]]))
    ser.flush()
    time.sleep(0.1)
    print(f"\npassing run: t+{best[0][0]} to t+{best[-1][0]} ({len(best)} settings); "
          f"set the middle, {mid[1] & 7} on the {'falling' if mid[1] & 8 else 'rising'} edge "
          f"(frame 0x1B {mid[1]:#04x}); status {status_text(link.read32(0x10))}")
    print("the plane is left showing the picture; --off hides it")


if __name__ == "__main__":
    main()
