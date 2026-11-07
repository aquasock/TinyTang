#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Install the firmware the board runs as the card's `bl616-firmware.bin`.

`bl616-firmware.bin` at the card root is the one image the desktop's Software
Update window installs (td_update_app.c hardcodes it), so it must be the build
the board is actually running.  When it is not, installing from the desktop is
a silent downgrade: entry 15 found the card carrying a twelve-day-old image and
replaced it, and entry 24 found the same hazard again.

This makes that rule a check rather than a habit.  It reads the build identity
out of the image, optionally asks the board what it is running, refuses to
install when the two disagree, then copies the image and verifies the copy by
SHA-256.

    # the image the tree just built, into a mounted card
    tools/tinytang_card_firmware.py /run/media/vash/TINYTEST

    # require the board's own answer to match before installing
    tools/tinytang_card_firmware.py /run/media/vash/TINYTEST --port /dev/ttyACM0

    # no board attached: state out loud which rev is being installed
    tools/tinytang_card_firmware.py /run/media/vash/TINYTEST --no-board

The identity is the `tinytang <commit>[-dirty.<hash>]` string the build stamps
into the image (cmake/tinytang_build_id.cmake) and that `platform` prints.  It
is not a digest: the firmware digest is commit-scoped and a dirty tree cannot
be reproduced from a commit, which is why the card file is identified by this
string plus the SHA-256 this tool prints and records nowhere else.
"""

import argparse
import hashlib
import os
import re
import shutil
import sys
import time

DEFAULT_IMAGE = "build/build_out/tinytang_bl616.bin"
CARD_NAME = "bl616-firmware.bin"

# `platform` prints `bl616/freertos, tinytang <id>`; the startup banner carries
# the same string.  Bare ids also appear in the image, so anchor on the phrase.
BANNER = re.compile(rb"bl616/freertos, tinytang ([0-9a-zA-Z._-]+)")
REPORTED = re.compile(r"tinytang\s+([0-9a-zA-Z._-]+)")


def read_image_id(path):
    """The identity the image was built with, or None when it carries none."""
    with open(path, "rb") as f:
        blob = f.read()
    m = BANNER.search(blob)
    return m.group(1).decode() if m else None


def reported_id(text):
    """The identity in a `platform` reply, or None."""
    m = REPORTED.search(text)
    return m.group(1) if m else None


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def ask_board(port, seconds=6.0):
    """`platform`'s identity from the board, or raise RuntimeError."""
    try:
        import serial
    except ImportError as exc:  # pragma: no cover - depends on the host
        raise RuntimeError("pyserial is required to read the board: pip install pyserial") from exc
    try:
        with serial.Serial(port, 115200, timeout=0.2) as ser:
            ser.reset_input_buffer()
            ser.write(b"platform\r")
            deadline = time.time() + seconds
            text = ""
            while time.time() < deadline:
                chunk = ser.read(256)
                if chunk:
                    text += chunk.decode("utf-8", "replace")
                    if "platform:" in text:
                        break
            return reported_id(text)
    except OSError as exc:
        raise RuntimeError(f"cannot open {port}: {exc}") from exc


def main(argv=None):
    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ap = argparse.ArgumentParser(
        description="Install the running board's firmware as the card's bl616-firmware.bin.",
        epilog="The card must be mounted; this writes one file to its root.",
    )
    ap.add_argument("card", help="mounted card root, e.g. /run/media/vash/TINYTEST")
    ap.add_argument("--image", default=os.path.join(here, DEFAULT_IMAGE),
                    help=f"firmware image to install (default: {DEFAULT_IMAGE})")
    ap.add_argument("--port", help="board console, e.g. /dev/ttyACM0: require it to be running this image")
    ap.add_argument("--no-board", action="store_true",
                    help="do not contact a board; install and state the identity")
    ap.add_argument("--dry-run", action="store_true", help="check and report, write nothing")
    args = ap.parse_args(argv)

    if not os.path.isfile(args.image):
        sys.exit(f"no image at {args.image}: build it first (make CHIP=bl616 BOARD=bl616dk)")
    if not os.path.isdir(args.card):
        sys.exit(f"no card mounted at {args.card}")

    image_id = read_image_id(args.image)
    if not image_id:
        sys.exit(f"{args.image} carries no build identity; it is not a TinyTang firmware image")

    if args.port and not args.no_board:
        try:
            board_id = ask_board(args.port)
        except RuntimeError as exc:
            sys.exit(f"{exc}\nrefusing to install without the board's answer; pass --no-board to skip the check")
        if not board_id:
            sys.exit(f"the board on {args.port} did not report an identity; is it at a shell prompt?")
        if board_id != image_id:
            sys.exit(f"refusing: the board runs {board_id} and {args.image} is {image_id}.\n"
                     f"Install and power-cycle the board first, or pass --image for the running build.")
        print(f"board:  {board_id}")
    elif not args.no_board:
        print("board:  not contacted (no --port); the check is the image's own identity")

    print(f"image:  {image_id}  {sha256(args.image)}")

    dest = os.path.join(args.card, CARD_NAME)
    if os.path.isfile(dest):
        old_id = read_image_id(dest) or "no identity"
        print(f"card:   replacing {old_id}")
    else:
        print("card:   creating the file")

    if args.dry_run:
        print("dry run: nothing written")
        return 0

    shutil.copyfile(args.image, dest)
    # vfat keeps writes in the page cache; a card pulled without the flush has
    # the old size in its directory entry.  Opened for writing so fsync is
    # portable rather than relying on Linux's tolerance of a read-only fd.
    with open(dest, "r+b") as f:
        os.fsync(f.fileno())

    want = sha256(args.image)
    got = sha256(dest)
    if got != want:
        sys.exit(f"copy damaged: {dest} is {got}, the image is {want}")
    installed = read_image_id(dest)
    if installed != image_id:
        sys.exit(f"copy damaged: {dest} carries {installed}, the image is {image_id}")

    print(f"wrote:  {dest}  {got}  {os.path.getsize(dest)} bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
