#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Host checks for tools/tinytang_card_firmware.py.

The tool exists so that `bl616-firmware.bin` is never a silent downgrade, so
the parts worth testing without a board are the ones that decide that: which
identity an image carries, which identity a `platform` reply carries, and that
a disagreement refuses to write rather than writing and warning.

Nothing here touches a card or a board; every path is a temporary directory.
"""

import os
import re
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))

import tinytang_card_firmware as tcf  # noqa: E402

CHECKS = 0
FAILED = 0

GOOD = b"bl616/freertos, tinytang 3a31690-dirty.cd6a5d5"
OTHER = b"bl616/freertos, tinytang 5c58bd3-dirty.45fed98"


def check(ok, what):
    global CHECKS, FAILED
    CHECKS += 1
    if not ok:
        FAILED += 1
        print(f"  FAIL {what}", file=sys.stderr)


def image(path, payload):
    """An image is anything carrying the banner; padding makes it look real."""
    with open(path, "wb") as f:
        f.write(b"\x00" * 32 + payload + b"\x00" * 32)


def run(*argv):
    """main() with stderr folded into stdout, so a refusal can be inspected."""
    import contextlib
    import io
    out = io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(out):
        try:
            rc = tcf.main(list(argv))
        except SystemExit as exc:
            rc = exc.code if isinstance(exc.code, int) else 1
            if isinstance(exc.code, str):
                out.write(exc.code)
    return rc, out.getvalue()


def main():
    with tempfile.TemporaryDirectory() as work:
        card = os.path.join(work, "card")
        os.makedirs(card)
        built = os.path.join(work, "built.bin")
        stale = os.path.join(work, "stale.bin")
        blank = os.path.join(work, "blank.bin")

        image(built, GOOD)
        image(stale, OTHER)
        with open(blank, "wb") as f:
            f.write(b"\x00" * 64)

        # --- identity reading -------------------------------------------------
        check(tcf.read_image_id(built) == "3a31690-dirty.cd6a5d5",
              "the banner's identity is read")
        check(tcf.read_image_id(stale) == "5c58bd3-dirty.45fed98",
              "a different build reads as itself")
        check(tcf.read_image_id(blank) is None,
              "an image with no banner has no identity")

        # A clean build has no dirty component, and the pattern must still match.
        clean = os.path.join(work, "clean.bin")
        image(clean, b"bl616/freertos, tinytang cca1cfa")
        check(tcf.read_image_id(clean) == "cca1cfa",
              "a clean build's identity has no dirty suffix")

        # --- platform parsing -------------------------------------------------
        reply = ("\x1b[1;32mroot@tinytang\x1b[0m# platform\r\nplatform\r\n"
                 "platform: bl616/freertos, tinytang 3a31690-dirty.cd6a5d5\r\n"
                 "TinyDesk Shell: 0.1.6\r\n")
        check(tcf.reported_id(reply) == "3a31690-dirty.cd6a5d5",
              "the identity is taken from a real platform reply")
        check(tcf.reported_id("no such command") is None,
              "a reply without the phrase has no identity")

        # --- the copy ----------------------------------------------------------
        rc, out = run(card, "--image", built, "--no-board")
        dest = os.path.join(card, tcf.CARD_NAME)
        check(rc == 0, f"installing with no board succeeds (rc={rc}: {out})")
        check(os.path.isfile(dest), "the file is created at the card root")
        check(tcf.read_image_id(dest) == "3a31690-dirty.cd6a5d5",
              "the installed file carries the built identity")
        check(tcf.sha256(dest) == tcf.sha256(built),
              "the installed file is byte-identical to the image")

        # It says what it replaced, which is the whole point of the tool.
        rc, out = run(card, "--image", stale, "--no-board")
        check(rc == 0 and "replacing 3a31690-dirty.cd6a5d5" in out,
              f"the replaced identity is named ({out!r})")

        # --- refusals ----------------------------------------------------------
        rc, out = run(card, "--image", blank, "--no-board")
        check(rc != 0, "an image with no identity is refused")
        check(tcf.read_image_id(dest) == "5c58bd3-dirty.45fed98",
              "a refused image leaves the card file untouched")

        missing = os.path.join(work, "missing.bin")
        rc, out = run(card, "--image", missing, "--no-board")
        check(rc != 0, "a missing image is refused")

        rc, out = run(os.path.join(work, "no-card"), "--image", built, "--no-board")
        check(rc != 0, "a missing card is refused")

        # A board running something else must stop the install, not warn.
        before = tcf.sha256(dest)
        tcf.ask_board = lambda port, seconds=6.0: "0000000-dirty.deadbee"
        rc, out = run(card, "--image", built, "--port", "/dev/null")
        check(rc != 0, f"a board on another rev is refused (rc={rc})")
        check("refusing" in out, "the refusal says so plainly")
        check(tcf.sha256(dest) == before, "a mismatched board leaves the card untouched")

        # A board running this image proceeds.
        tcf.ask_board = lambda port, seconds=6.0: "3a31690-dirty.cd6a5d5"
        rc, out = run(card, "--image", built, "--port", "/dev/null")
        check(rc == 0, f"a matching board installs (rc={rc}: {out})")
        check("board:  3a31690-dirty.cd6a5d5" in out, "the board's identity is echoed")

        # A board that answers nothing is not treated as agreement.
        tcf.ask_board = lambda port, seconds=6.0: None
        before = tcf.sha256(dest)
        rc, out = run(card, "--image", stale, "--port", "/dev/null")
        check(rc != 0, "a silent board is refused rather than assumed")
        check(tcf.sha256(dest) == before, "a silent board leaves the card untouched")

    print(f"{CHECKS} checks, {FAILED} failed")
    return 1 if FAILED else 0


if __name__ == "__main__":
    sys.exit(main())
