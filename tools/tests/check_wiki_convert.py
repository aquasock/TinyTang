#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Host check for tools/make_wiki_pack.py.

Builds a small ZIM archive in a temporary folder -- header, MIME list, one
uncompressed cluster, dir entries, the three pointer tables and an MD5 -- runs
the converter over it, and checks the pack the reader on the board will get.
The archive is synthetic so the expectations are exact: no download, no
network, and the same bytes every run.
"""

import hashlib
import os
import shutil
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
CONVERTER = os.path.join(ROOT, "tools", "make_wiki_pack.py")
CHECKER = os.path.join(ROOT, "tools", "check_wiki_pack.py")

CHECKS = 0
FAILED = 0


def check(ok, what):
    global CHECKS, FAILED
    CHECKS += 1
    if not ok:
        FAILED += 1
        print(f"  FAIL {what}", file=sys.stderr)


# --------------------------------------------------------------- ZIM builder


def build_zim(path, entries, mime_types, metadata):
    """Write a minimal but well-formed ZIM 6 archive.

    entries: list of dicts with ns, url, title, mime (index), blob (bytes),
             and optionally redirect_index (for a dirent redirect).
    metadata: dict of M-namespace name -> value, added as entries.
    """
    all_entries = list(entries)
    for name, value in metadata.items():
        all_entries.append({"ns": "M", "url": name, "title": name,
                            "mime": mime_types.index("text/plain"),
                            "blob": value.encode()})

    # One uncompressed cluster holding every blob, in entry order.
    blobs = [e["blob"] for e in all_entries]
    cluster = bytearray()
    cluster.append(1)                                   # info: no compression
    offset_size = 4
    table_size = (len(blobs) + 1) * offset_size
    offsets = [table_size]
    for blob in blobs:
        offsets.append(offsets[-1] + len(blob))
    for value in offsets:
        cluster += struct.pack("<I", value)
    for blob in blobs:
        cluster += blob

    mime_list = bytearray()
    for mime in mime_types:
        mime_list += mime.encode() + b"\0"
    mime_list += b"\0"

    # Dir entries, in URL order (namespace then url), which is what the format
    # requires of the pointer table.
    order = sorted(range(len(all_entries)),
                   key=lambda i: (all_entries[i]["ns"], all_entries[i]["url"]))
    dirents = bytearray()
    dirent_at = {}
    for i in order:
        e = all_entries[i]
        dirent_at[i] = 80 + len(mime_list) + len(dirents)
        entry = struct.pack("<HBBI", e["mime"], 0, ord(e["ns"]), 0)
        if e.get("redirect_index") is not None:
            entry = struct.pack("<HBBI", e["mime"], 0, ord(e["ns"]), 0xFFFFFFFF)
            entry += struct.pack("<I", e["redirect_index"])
        else:
            entry += struct.pack("<II", 0, i)           # cluster 0, blob i
        entry += e["url"].encode() + b"\0" + e["title"].encode() + b"\0"
        dirents += entry

    url_ptr = bytearray()
    for i in order:
        url_ptr += struct.pack("<Q", dirent_at[i])

    by_title = sorted(range(len(all_entries)),
                      key=lambda i: (all_entries[i]["ns"],
                                     all_entries[i]["title"].lower()))
    title_ptr = bytearray()
    for i in by_title:
        title_ptr += struct.pack("<I", order.index(i))

    # Clusters go after the pointer tables, so the last cluster ends at the
    # checksum position, which is how the reader bounds it.
    mime_pos = 80
    dirent_pos = mime_pos + len(mime_list)
    url_ptr_pos = dirent_pos + len(dirents)
    title_ptr_pos = url_ptr_pos + len(url_ptr)
    cluster_ptr_pos = title_ptr_pos + len(title_ptr)
    cluster_pos = cluster_ptr_pos + 8 * 1
    checksum_pos = cluster_pos + len(cluster)

    header = struct.pack(
        "<IHH16sIIQQQQIIQ",
        72173914, 6, 1, b"\0" * 16,
        len(all_entries), 1,
        url_ptr_pos, title_ptr_pos, cluster_ptr_pos, mime_pos,
        0, 0xFFFFFFFF, checksum_pos)

    body = bytes(header) + bytes(mime_list) + bytes(dirents) + bytes(url_ptr) + \
        bytes(title_ptr) + struct.pack("<Q", cluster_pos) + bytes(cluster)
    assert len(body) == checksum_pos, (len(body), checksum_pos)
    with open(path, "wb") as f:
        f.write(body)
        f.write(hashlib.md5(body).digest())


# --------------------------------------------------------------- pack reader


RECORD = 80


def read_pack(folder):
    meta = {}
    with open(os.path.join(folder, "wiki.meta"), encoding="ascii") as f:
        for line in f:
            if "=" in line:
                key, value = line.split("=", 1)
                meta[key.strip()] = value.strip()
    raw = open(os.path.join(folder, "wiki.idx"), "rb").read()
    records = []
    for at in range(0, len(raw), RECORD):
        rec = raw[at:at + RECORD]
        title = rec[:64].rstrip(b"\0").decode("ascii")
        chunk, offset, length, lines = struct.unpack("<IIII", rec[64:])
        records.append((title, chunk, offset, length, lines))
    return meta, records


def body_of(folder, record):
    _, chunk, offset, length, _ = record
    with open(os.path.join(folder, f"wiki.{chunk:03d}"), "rb") as f:
        f.seek(offset)
        return f.read(length).decode("ascii")


# --------------------------------------------------------------------- cases


def main():
    work = tempfile.mkdtemp(prefix="wiki-convert-")
    try:
        zim = os.path.join(work, "test.zim")
        pack = os.path.join(work, "pack")

        article = (b"<html><body>"
                   b"<h1>Albert Einstein</h1>"
                   b"<p>Albert Einstein was a German-born theoretical physicist.</p>"
                   b"<h2>Life</h2>"
                   b"<h3>A heading long enough that its marker must come out "
                   b"of the column budget rather than past it</h3>"
                   b"<p>His equation <math><semantics><mrow><mi>E</mi></mrow>"
                   b"<annotation encoding=\"application/x-tex\">"
                   b"{\\displaystyle E=mc^{2}}</annotation></semantics></math>"
                   b" is famous.</p>"
                   b"<p>Caf\xc3\xa9 notes \xe2\x80\x94 with an em dash and "
                   b"\xe4\xb8\xad\xe6\x96\x87 text.</p>"
                   b"<ul><li>Relativity</li><li>Photoelectric effect</li></ul>"
                   b"<table><tr><td>Born</td><td>1879</td></tr></table>"
                   b"</body></html>")
        second = (b"<html><body><h1>Physics</h1>"
                  b"<p>Physics is the natural science of matter and energy.</p>"
                  b"</body></html>")
        redirect_page = (b"<html><head>"
                         b"<meta http-equiv=\"refresh\" content=\"0;URL='./Physics'\" />"
                         b"</head><body><a href=\"./Physics\">Natural science</a>"
                         b"</body></html>")
        css = b"body { color: black; }"

        entries = [
            {"ns": "C", "url": "Albert_Einstein", "title": "Albert Einstein",
             "mime": 1, "blob": article},
            {"ns": "C", "url": "Physics", "title": "Physics", "mime": 1,
             "blob": second},
            {"ns": "C", "url": "Natural_science", "title": "Natural science",
             "mime": 1, "blob": redirect_page},
            {"ns": "C", "url": "style.css", "title": "style", "mime": 0,
             "blob": css},
            {"ns": "C", "url": "Aardvark", "title": "Aardvark", "mime": 1,
             "blob": redirect_page.replace(b"./Physics", b"./Missing")},
            # Two redirects whose targets sort the opposite way to their own
            # titles: the resolved records must be re-sorted by title before the
            # final merge, or the index comes out of order here.
            {"ns": "C", "url": "Alpha", "title": "Alpha", "mime": 1,
             "blob": redirect_page.replace(b"./Physics", b"./Physics")},
            {"ns": "C", "url": "Zeta", "title": "Zeta", "mime": 1,
             "blob": redirect_page.replace(b"./Physics", b"./Albert_Einstein")},
        ]
        build_zim(zim, entries, ["text/css", "text/html", "text/plain"],
                  {"Title": "Wikipedia (test)", "Language": "eng",
                   "Creator": "test", "Date": "2026-01-01",
                   "Description": "a synthetic archive"})

        result = subprocess.run(
            [sys.executable, CONVERTER, "--zim", zim, "--out", pack,
             "--main-page", "Physics"],
            capture_output=True, text=True)
        check(result.returncode == 0,
              f"the converter runs (stderr: {result.stderr.strip()})")
        if result.returncode != 0:
            return report()

        meta, records = read_pack(pack)
        titles = [r[0] for r in records]

        check(len(records) == 5, f"five records: two articles and three resolved "
                                 f"redirects (got {len(records)}: {titles})")
        check(titles == sorted(titles, key=str.lower),
              f"the index is sorted by folded title ({titles})")
        check("Albert Einstein" in titles and "Physics" in titles,
              "both articles are indexed")
        check("Natural science" in titles, "the redirect page became a title")
        check("style" not in titles, "a non-HTML entry is not an article")
        check("Aardvark" not in titles,
              "a redirect to a missing target is dropped rather than dangling")

        by_title = {r[0]: r for r in records}
        article_rec = by_title["Albert Einstein"]
        redirect_rec = by_title["Natural science"]
        target_rec = by_title["Physics"]
        check(article_rec[3] == len(body_of(pack, article_rec).encode()),
              "the record's length is the body's byte count")
        check(redirect_rec[1:] == target_rec[1:],
              "the redirect reuses the target's body instead of storing it twice")

        body = body_of(pack, article_rec)
        lines = body.splitlines()
        check(max(len(line) for line in lines) <= 78,
              "even a heading fits the 78 columns")
        check(all(len(line) <= 78 for line in lines),
              f"every line fits the reader's 78 columns (longest "
              f"{max(len(l) for l in lines)})")
        check(all(32 <= ord(c) < 127 or c == "\n" for c in body),
              "the body is printable ASCII")
        check("Cafe notes - with an em dash" in body,
              "accents and dashes are folded")
        check("Born" not in body, "tables are not turned into page text")
        check("displaystyle" not in body,
              "the hidden MathML LaTeX is not read as article text")
        check("- Relativity" in body, "list items keep a bullet")
        check("## Life" in body, "a section heading is kept as a heading")
        check(not body.startswith("## Albert Einstein"),
              "the article's repeated title heading is dropped")

        check(meta.get("format") == "TWP1", "the pack names its format")
        check(meta.get("articles") == "5", "the metadata counts index records")
        check(meta.get("chunks") == "1", "the metadata counts chunks")
        check(meta.get("main_page") == "Physics", "the main page is recorded")
        check(meta.get("title") == "Wikipedia (test)",
              "the title comes from the archive's own metadata")
        check(meta.get("language") == "eng", "the language comes from the archive")
        check("CC BY-SA" in meta.get("license", ""),
              "a Wikipedia archive with no License key still names its terms")
        check("Wikipedia" in meta.get("license", ""),
              "the licence names the project the archive came from")

        # The pack checker must accept what the converter wrote, and must be
        # the thing that would have caught a line over the reader's width.
        checked = subprocess.run([sys.executable, CHECKER, pack, "--quiet"],
                                 capture_output=True, text=True)
        check(checked.returncode == 0,
              f"the pack checker accepts the pack ({checked.stderr.strip()})")

        # A second run with a tight article cap must mark the truncation.
        pack2 = os.path.join(work, "pack2")
        subprocess.run([sys.executable, CONVERTER, "--zim", zim, "--out", pack2,
                        "--max-article-bytes", "120"], capture_output=True, text=True)
        _, records2 = read_pack(pack2)
        article2 = {r[0]: r for r in records2}["Albert Einstein"]
        body2 = body_of(pack2, article2)
        check(len(body2) <= 300 and "[truncated" in body2,
              "the article cap marks the cut instead of ending silently")
        check(all(len(line) <= 78 for line in body2.splitlines()),
              "a truncated body still fits the reader's columns")
    finally:
        shutil.rmtree(work, ignore_errors=True)
    return report()


def report():
    print(f"wiki convert: {CHECKS} checks, {FAILED} failed")
    return 0 if FAILED == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
