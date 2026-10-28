#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check a TinyKiwix pack: every index record and every article body.

A pack is read on the board by binary search, so a single record out of order
or a single offset past the end of its chunk silently breaks a title lookup
rather than failing loudly.  This verifies the whole folder on the host, which
is cheap next to building a 20 GB pack and losing an evening on the board:

  * wiki.meta parses, names the format, and agrees with the index
  * wiki.idx is a whole number of records, sorted by folded title, no duplicates
  * every record's title is printable ASCII, its chunk exists, and its body
    lies inside that chunk
  * every body is printable ASCII, its line count matches the record, and no
    line exceeds the pack's column width

Usage: tools/check_wiki_pack.py /run/media/vash/TINYTANG/root/wiki
"""

import argparse
import os
import struct
import sys

TITLE_LIMIT = 64
RECORD_LIMIT = 80


def fail(message):
    print(f"check_wiki_pack: {message}", file=sys.stderr)
    return False


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("folder", help="pack folder, e.g. /root/wiki on the card")
    ap.add_argument("--sample", type=int, default=0,
                    help="check only every Nth body (0: every one)")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args(argv)

    folder = args.folder
    ok = True

    meta_path = os.path.join(folder, "wiki.meta")
    if not os.path.isfile(meta_path):
        return 0 if fail(f"{meta_path} is missing") else 1
    meta = {}
    with open(meta_path, encoding="ascii", errors="replace") as f:
        for line in f:
            if "=" in line:
                key, value = line.split("=", 1)
                meta[key.strip()] = value.strip()

    if meta.get("format") != "TWP1":
        ok = fail(f"wiki.meta says format {meta.get('format')!r}, not TWP1")
    title_bytes = int(meta.get("title_bytes", TITLE_LIMIT))
    record_bytes = int(meta.get("record_bytes", title_bytes + 16))
    chunks = int(meta.get("chunks", 0))
    articles = int(meta.get("articles", -1))
    columns = int(meta.get("line_columns", 78))
    if record_bytes != title_bytes + 16:
        ok = fail(f"record_bytes {record_bytes} is not title_bytes+16")
    if title_bytes > TITLE_LIMIT or record_bytes > RECORD_LIMIT:
        ok = fail(f"a {title_bytes}-byte title is wider than this checker knows")

    index_path = os.path.join(folder, "wiki.idx")
    if not os.path.isfile(index_path):
        return 0 if fail(f"{index_path} is missing") else 1
    size = os.path.getsize(index_path)
    if size % record_bytes:
        ok = fail(f"wiki.idx is {size} bytes, not a whole number of "
                  f"{record_bytes}-byte records")
        return 0 if ok else 1
    count = size // record_bytes
    if articles != count:
        ok = fail(f"wiki.meta claims {articles} articles, the index holds {count}")

    chunk_size = {}
    for c in range(chunks):
        path = os.path.join(folder, f"wiki.{c:03d}")
        if not os.path.isfile(path):
            ok = fail(f"wiki.meta claims {chunks} chunks but wiki.{c:03d} is missing")
            break
        chunk_size[c] = os.path.getsize(path)

    previous = None
    titles = set()
    bad = 0
    bodies = 0
    handles = {}
    try:
        with open(index_path, "rb") as index:
            for i in range(count):
                rec = index.read(record_bytes)
                title = rec[:title_bytes].split(b"\0")[0]
                chunk, offset, length, lines = struct.unpack(
                    "<IIII", rec[title_bytes:title_bytes + 16])

                def complain(what):
                    nonlocal ok, bad
                    bad += 1
                    ok = fail(f"record {i}: {what}") if bad <= 20 else False
                    return False

                if not title:
                    complain("empty title")
                    continue
                if any(b < 32 or b > 126 for b in title):
                    complain(f"title is not printable ASCII: {title[:40]!r}")
                    continue
                if title in titles:
                    complain(f"duplicate title {title.decode('ascii')!r}")
                titles.add(title)
                folded = title.lower()
                if previous is not None and folded < previous:
                    complain(f"out of order after {previous.decode('ascii')!r}")
                previous = folded
                if chunk not in chunk_size:
                    complain(f"chunk {chunk} does not exist")
                    continue
                if length == 0:
                    complain("zero-length body")
                    continue
                if offset + length > chunk_size[chunk]:
                    complain(f"body runs past the end of wiki.{chunk:03d} "
                             f"({offset}+{length} > {chunk_size[chunk]})")
                    continue

                if args.sample and i % args.sample:
                    continue
                handle = handles.get(chunk)
                if handle is None:
                    handle = handles[chunk] = open(
                        os.path.join(folder, f"wiki.{chunk:03d}"), "rb")
                handle.seek(offset)
                body = handle.read(length)
                bodies += 1
                if len(body) != length:
                    complain(f"short read ({len(body)} of {length})")
                    continue
                if body.count(b"\n") != lines:
                    complain(f"body has {body.count(chr(10).encode())} lines, "
                             f"the record says {lines}")
                printable = body.translate(None, b"\n")
                if any(b < 32 or b > 126 for b in printable):
                    complain("body is not printable ASCII")
                longest = max((len(line) for line in body.split(b"\n")), default=0)
                if longest > columns:
                    complain(f"a body line is {longest} columns, over {columns}")
    finally:
        for handle in handles.values():
            handle.close()

    if not args.quiet or not ok:
        print(f"check_wiki_pack: {count} records, {bodies} bodies checked in "
              f"{len(chunk_size)} chunk(s): {'OK' if ok else 'FAILED'}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
