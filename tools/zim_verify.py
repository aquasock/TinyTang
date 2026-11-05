#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Verify a ZIM archive against the MD5 it carries.

An openZIM file ends with a 16-byte MD5 of everything before it -- but only if
the file ends there.  The header's ``checksumPos`` field says where the
checksum actually starts, and a download can carry junk past that point while
the content itself is complete.  Kiwix's class Q archive was 17,970,908,446
bytes with a ``checksumPos`` of 17,724,714,254: 246 MB of tail, and the MD5
over the first 17,724,714,254 bytes matched the stored value exactly.  The
English Wikipedia archive carries about 1.01 GB the same way.

The obvious check -- "hash everything but the last 16 bytes, compare with the
last 16" -- is therefore wrong on exactly the archives that need checking: it
fails on complete content, and it can never verify the archive with the largest
tail.  This reads ``checksumPos`` from the 80-byte header instead, hashes
exactly those bytes, and compares against the 16 bytes *at* that position.

    tools/zim_verify.py <archive.zim>          # verify, print a summary
    tools/zim_verify.py --quiet <archive.zim>  # exit status only
    tools/zim_verify.py --truncate <archive>   # cut the tail off, then verify

Exit status: 0 verified, 1 mismatch or malformed, 2 usage error.

The truncate mode exists because a shelf script that re-downloads will fetch
the junk again.  It removes only bytes past ``checksumPos + 16``, and refuses
to run unless those bytes are the junk it thinks they are -- that is, unless
the content verifies first.
"""

from __future__ import annotations

import argparse
import hashlib
import os
import struct
import sys

ZIM_MAGIC = 72173914          # 0x044D495A, "ZIM" little-endian with a 0x04
HEADER_BYTES = 80
# checksumPos is a u64 at offset 72 of the 80-byte header.
CHECKSUM_POS_OFFSET = 72
MD5_BYTES = 16
CHUNK = 8 << 20


def _read_header(f) -> bytes:
    f.seek(0)
    head = f.read(HEADER_BYTES)
    if len(head) != HEADER_BYTES:
        raise ValueError(f"file is {len(head)} bytes, shorter than the {HEADER_BYTES}-byte header")
    return head


def describe(path: str) -> dict:
    """The facts a caller needs, without hashing."""
    size = os.path.getsize(path)
    with open(path, "rb") as f:
        head = _read_header(f)
        magic = struct.unpack_from("<I", head, 0)[0]
        if magic != ZIM_MAGIC:
            raise ValueError(f"not a ZIM archive: magic {magic} is not {ZIM_MAGIC}")
        checksum_pos = struct.unpack_from("<Q", head, CHECKSUM_POS_OFFSET)[0]

    if checksum_pos < HEADER_BYTES:
        raise ValueError(
            f"checksumPos {checksum_pos} is inside the header; the file is not a valid ZIM"
        )
    needed = checksum_pos + MD5_BYTES
    if needed > size:
        raise ValueError(
            f"checksumPos {checksum_pos} plus its {MD5_BYTES}-byte trailer needs "
            f"{needed} bytes but the file is {size}: the download is short, "
            f"not merely padded"
        )
    return {
        "path": path,
        "size": size,
        "checksum_pos": checksum_pos,
        "content_bytes": checksum_pos,
        "tail_bytes": size - needed,
        "needed": needed,
    }


def _hash(path: str, length: int, progress=None) -> bytes:
    """MD5 over the first `length` bytes."""
    digest = hashlib.md5()
    done = 0
    with open(path, "rb") as f:
        while done < length:
            block = f.read(min(CHUNK, length - done))
            if not block:
                raise ValueError(f"file ended after {done} of {length} bytes")
            digest.update(block)
            done += len(block)
            if progress:
                progress(done, length)
    return digest.digest()


def verify(path: str, progress=None) -> dict:
    """Hash the content and compare it with the stored trailer."""
    info = describe(path)
    computed = _hash(path, info["checksum_pos"], progress)
    with open(path, "rb") as f:
        f.seek(info["checksum_pos"])
        stored = f.read(MD5_BYTES)
    info["computed"] = computed.hex()
    info["stored"] = stored.hex()
    info["ok"] = computed == stored
    return info


def truncate(path: str) -> dict:
    """Remove bytes past the trailer, after proving the content verifies.

    Refuses if the archive does not verify: cutting a file whose content is
    already wrong would destroy the evidence of what went wrong.
    """
    info = verify(path)
    if not info["ok"]:
        raise ValueError(
            "refusing to truncate: the archive does not verify, so the bytes past "
            "checksumPos+16 are not known to be junk"
        )
    if info["tail_bytes"] == 0:
        info["truncated"] = 0
        return info
    os.truncate(path, info["needed"])
    info["truncated"] = info["tail_bytes"]
    return info


def _human(n: int) -> str:
    for unit in ("B", "KB", "MB", "GB", "TB"):
        if n < 1024 or unit == "TB":
            return f"{n:.1f} {unit}" if unit != "B" else f"{n} B"
        n /= 1024.0
    return f"{n} B"


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(
        description="Verify a ZIM against the MD5 at its checksumPos.",
    )
    ap.add_argument("archive")
    ap.add_argument("--quiet", action="store_true", help="no output, exit status only")
    ap.add_argument(
        "--truncate",
        action="store_true",
        help="cut bytes past checksumPos+16 after verifying, in place",
    )
    ap.add_argument("--progress", action="store_true", help="print hashing progress")
    args = ap.parse_args(argv)

    if not os.path.isfile(args.archive):
        print(f"zim_verify: no such file: {args.archive}", file=sys.stderr)
        return 2

    def progress(done: int, total: int) -> None:
        pct = done * 100 // max(total, 1)
        print(f"  {pct:3d}%  {_human(done)} of {_human(total)}", flush=True)

    try:
        if args.truncate:
            info = truncate(args.archive)
        else:
            info = verify(args.archive, progress if args.progress else None)
    except ValueError as exc:
        if not args.quiet:
            print(f"zim_verify: {exc}", file=sys.stderr)
        return 1

    if not args.quiet:
        print(f"  file         {_human(info['size'])}")
        print(f"  content      {_human(info['content_bytes'])} (checksumPos {info['checksum_pos']})")
        print(f"  tail         {info['tail_bytes']} bytes past the trailer")
        print(f"  md5 stored   {info['stored']}")
        print(f"  md5 computed {info['computed']}")
        print(f"  result       {'matches' if info['ok'] else 'DOES NOT MATCH'}")
        if args.truncate and info.get("truncated"):
            print(f"  truncated    {info['truncated']} bytes removed, now {_human(os.path.getsize(args.archive))}")
    return 0 if info["ok"] else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
