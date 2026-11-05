#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Host test for tools/zim_verify.py.

The behaviour that matters is the one the old "hash everything but the last 16
bytes" check got wrong: an archive whose content is complete but which carries
junk past its trailer.  Class Q carried 246 MB that way and the English
Wikipedia archive about 1.01 GB, so the tail is not hypothetical.

A fake ZIM is built here rather than a real one being carried: the 80-byte
header, some content and the MD5 trailer are all that the verifier reads, and a
synthetic file lets each case put the checksum where it wants.
"""

import hashlib
import os
import struct
import subprocess
import sys
import tempfile

# This file is tools/tests/check_zim_verify.py, so the repository root is three
# levels up: tests -> tools -> root.
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TOOL = os.path.join(ROOT, "tools", "zim_verify.py")

ZIM_MAGIC = 72173914
HEADER = 80
CHK_OFF = 72

failures = 0


def check(cond, what):
    global failures
    if not cond:
        print("FAIL:", what)
        failures += 1


def build(path, content=b"", tail=b"", *, magic=ZIM_MAGIC, chk=None,
          stored_md5=None):
    """Write a fake ZIM: header, content, MD5 trailer, then `tail` junk."""
    if chk is None:
        chk = HEADER + len(content)
    head = bytearray(HEADER)
    struct.pack_into("<I", head, 0, magic)
    struct.pack_into("<Q", head, CHK_OFF, chk)
    body = bytes(head) + content
    if stored_md5 is None:
        stored_md5 = hashlib.md5(body[:chk]).digest()
    with open(path, "wb") as f:
        f.write(body)
        f.write(stored_md5)
        f.write(tail)
    return path


def run(*args):
    r = subprocess.run([sys.executable, TOOL] + list(args),
                       capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


def human_old_check(path):
    """The logic the shelf script used to use, for the negative control."""
    size = os.path.getsize(path)
    with open(path, "rb") as f:
        f.seek(size - 16)
        tail = f.read(16)
        f.seek(0)
        d = hashlib.md5()
        left = size - 16
        while left:
            block = f.read(min(1 << 20, left))
            if not block:
                break
            d.update(block)
            left -= len(block)
    return d.digest() == tail


def main():
    with tempfile.TemporaryDirectory() as tmp:
        # ---------------------------------------------------- a clean file --
        ok = build(os.path.join(tmp, "clean.zim"), content=b"x" * 4096)
        rc, out = run(ok)
        check(rc == 0, "a clean archive verifies")
        check("matches" in out, "a clean archive says it matches")
        check("tail         0 bytes" in out, "a clean archive reports no tail")
        check(human_old_check(ok), "the old check also accepts a clean archive")

        # ------------------------------- complete content with a junk tail --
        # This is the class Q shape: 4096 bytes of content, 1000 bytes of junk.
        # The verifier must pass.  The old check must fail -- that is the whole
        # reason the verifier exists, so the negative control is asserted.
        junky = build(os.path.join(tmp, "junky.zim"), content=b"y" * 4096,
                      tail=b"\x00" * 1000)
        rc, out = run(junky)
        check(rc == 0, "complete content with a junk tail verifies")
        check("tail         1000 bytes" in out, "the tail is reported")
        check(not human_old_check(junky),
              "the old check REJECTS the same archive (the defect being fixed)")

        # ------------------------------------------ --truncate removes it ---
        cut = build(os.path.join(tmp, "cut.zim"), content=b"z" * 4096,
                    tail=b"\x00" * 500)
        rc, out = run("--truncate", cut)
        check(rc == 0, "truncate succeeds on a verifying archive")
        check(os.path.getsize(cut) == HEADER + 4096 + 16,
              "truncate leaves exactly header+content+trailer")
        rc, out = run(cut)
        check(rc == 0, "the truncated archive still verifies")
        check("tail         0 bytes" in out, "and reports no tail")

        # ---------------------- a corrupted body is refused, not truncated --
        bad = build(os.path.join(tmp, "bad.zim"), content=b"w" * 4096,
                    tail=b"\x00" * 100)
        # Flip one content byte without touching the stored MD5.
        with open(bad, "r+b") as f:
            f.seek(HEADER + 10)
            f.write(b"\xff")
        rc, out = run(bad)
        check(rc == 1, "a corrupted body fails verification")
        before = os.path.getsize(bad)
        rc, out = run("--truncate", bad)
        check(rc == 1, "truncate refuses a corrupted body")
        check(os.path.getsize(bad) == before,
              "and the evidence is not destroyed")

        # ----------------------------------------- a short download --------
        # checksumPos points past the end: the download lost bytes.  This is a
        # different failure from a padded one and must say so.
        short = os.path.join(tmp, "short.zim")
        # Built by hand with a checksumPos past the end of the file.
        with open(short, "wb") as f:
            head = bytearray(HEADER)
            struct.pack_into("<I", head, 0, ZIM_MAGIC)
            struct.pack_into("<Q", head, CHK_OFF, 4096)
            f.write(bytes(head))
            f.write(b"s" * 100)
        rc, out = run(short)
        check(rc == 1, "a file shorter than its checksumPos fails")
        check("short" in out.lower(), "and says the download is short")

        # ---------------------------------------------- not a ZIM at all ---
        notzim = build(os.path.join(tmp, "notzim.zim"), content=b"n" * 64,
                       magic=1234567)
        rc, out = run(notzim)
        check(rc == 1, "a wrong magic is refused")
        check("not a ZIM" in out, "and named as such")

        # ------------------------------------------ checksumPos nonsense ----
        zero = os.path.join(tmp, "zero.zim")
        with open(zero, "wb") as f:
            head = bytearray(HEADER)
            struct.pack_into("<I", head, 0, ZIM_MAGIC)
            struct.pack_into("<Q", head, CHK_OFF, 4)   # inside the header
            f.write(bytes(head) + b"\x00" * 64)
        rc, out = run(zero)
        check(rc == 1, "a checksumPos inside the header is refused")

        # ------------------------------------------------ a stored MD5 that
        # is simply wrong: the content may be fine but the trailer disagrees.
        wrong = build(os.path.join(tmp, "wrong.zim"), content=b"q" * 256,
                      stored_md5=b"\x00" * 16)
        rc, out = run(wrong)
        check(rc == 1, "a wrong stored MD5 fails")
        check("DOES NOT MATCH" in out, "and says so")

        # ---------------------------------------------------- usage errors --
        rc, out = run()
        check(rc == 2, "no argument is a usage error")
        rc, out = run(os.path.join(tmp, "absent.zim"))
        check(rc == 2, "a missing file is a usage error")

        # --quiet suppresses the summary but keeps the status.
        rc, out = run("--quiet", ok)
        check(rc == 0, "--quiet still succeeds on a clean archive")
        check(out.strip() == "", "--quiet prints nothing on success")
        rc, out = run("--quiet", wrong)
        check(rc == 1, "--quiet still fails on a wrong MD5")

    if failures:
        print(f"check_zim_verify: {failures} failure(s)")
        return 1
    print("check_zim_verify: all checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
