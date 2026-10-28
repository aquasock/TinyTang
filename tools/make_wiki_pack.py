#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Convert a Kiwix/openZIM .zim archive into a TinyKiwix page pack for the SD card.

Why a conversion and not an on-board reader: a ZIM cluster is compressed as a
whole and the openZIM format document says clusters are "typically ... about
1 MB".  Decoding one needs its full back-reference window resident (zstd cannot
decode with truncated history), and this board has 320 KB of OCRAM, no PSRAM,
and a largest free heap block under 70 KB.  So the archive is read here, on a
host, and the board gets plain text it can page through.

The pack is three things in one folder:

  wiki.meta   key = value, from the archive's own M metadata
  wiki.idx    fixed-size records sorted by folded title: the title index
  wiki.NNN    article bodies, chunked so no file exceeds the FAT32 4 GiB limit

A record is WIKI_RECORD_BYTES (80) bytes, little-endian:

  title[64]  NUL-padded, display case
  u32 chunk  which wiki.NNN
  u32 offset byte offset of the body inside that chunk
  u32 length body length in bytes
  u32 lines  newline count of the body

Bodies are wrapped ASCII text, one display row per line, and never compressed:
the board has no decompressor and 119 GB of card in front of it.

Redirects are resolved rather than copied.  libzim writes most of them as small
HTML pages carrying <meta http-equiv="refresh" content="0;URL='./Target'">, so
they are detected, joined to the target's body, and indexed under their own
title -- "USA" opens the "United States" text without storing it twice.

Everything here streams: bodies are written as they are found, the index is
built by external sorts of fixed-size records, and memory is bounded by
--sort-records rather than by the archive.  That is what makes the 49 GB
Wikipedia archives a matter of hours rather than of RAM.

Only Python's standard library is used, including `compression.zstd` (3.14+), so
the conversion is reproducible from a stock interpreter with no pip install.

Licence: the source archive's own metadata is copied into wiki.meta, and where
it does not state a licence the terms of the identified source are named.  A
converted pack is a derivative of the archive's content and keeps those terms.
"""

from __future__ import annotations

import argparse
import heapq
import lzma
import os
import re
import struct
import sys
import tempfile
import textwrap
import unicodedata
from html.parser import HTMLParser

from compression import zstd

# --------------------------------------------------------------------- format

WIKI_MAGIC = "TWP1"
WIKI_VERSION = 1
WIKI_TITLE_BYTES = 64
WIKI_LINE_COLS = 78
DEFAULT_CHUNK_BYTES = 3 * 1024 * 1024 * 1024     # under FAT32's 4 GiB file cap
DEFAULT_MAX_ARTICLE = 256 * 1024
DEFAULT_SORT_RECORDS = 100_000

# ------------------------------------------------------------------ ZIM format

ZIM_MAGIC = 72173914
DIRENT_REDIRECT = 0xFFFFFFFF
CLUSTER_NONE = 1
CLUSTER_XZ = 4
CLUSTER_ZSTD = 5


class ZimError(Exception):
    pass


class Zim:
    """A minimal reader for the parts of a ZIM this conversion needs."""

    def __init__(self, path: str):
        self.f = open(path, "rb")
        head = self.f.read(80)
        if len(head) < 80:
            raise ZimError("file is shorter than a ZIM header")
        (magic, major, minor, _uuid) = struct.unpack_from("<IHH16s", head, 0)
        if magic != ZIM_MAGIC:
            raise ZimError(f"not a ZIM archive (magic 0x{magic:08x})")
        (self.article_count, self.cluster_count) = struct.unpack_from("<II", head, 24)
        (self.url_ptr_pos, self.title_ptr_pos, self.cluster_ptr_pos,
         self.mime_list_pos) = struct.unpack_from("<QQQQ", head, 32)
        self.main_page = struct.unpack_from("<I", head, 64)[0]
        self.checksum_pos = struct.unpack_from("<Q", head, 72)[0]
        self.version = (major, minor)
        self.mime_types = self._read_mime_list()
        self._cluster_ptr_cache: dict[int, int] = {}
        self._cluster_id = -1
        self._cluster_blobs: list[bytes] = []

    def close(self) -> None:
        self.f.close()

    def _read_mime_list(self) -> list[str]:
        self.f.seek(self.mime_list_pos)
        out: list[str] = []
        while True:
            chunk = bytearray()
            while True:
                c = self.f.read(1)
                if not c:
                    raise ZimError("MIME list runs off the end of the file")
                if c == b"\0":
                    break
                chunk += c
            if not chunk:
                return out
            out.append(chunk.decode("utf-8", "replace"))

    def _u64(self, at: int) -> int:
        self.f.seek(at)
        return struct.unpack("<Q", self.f.read(8))[0]

    def cluster_ptr(self, index: int) -> int:
        if index not in self._cluster_ptr_cache:
            self._cluster_ptr_cache[index] = self._u64(self.cluster_ptr_pos + 8 * index)
        return self._cluster_ptr_cache[index]

    def dirent(self, index: int):
        """(mimetype, namespace, cluster, blob, url, title, redirect_index)."""
        ptr = self._u64(self.url_ptr_pos + 8 * index)
        self.f.seek(ptr)
        head = self.f.read(16)
        mimetype, _param_len, namespace = struct.unpack_from("<HBB", head, 0)
        revision = struct.unpack_from("<I", head, 4)[0]
        redirect = None
        if revision == DIRENT_REDIRECT:
            redirect = struct.unpack_from("<I", head, 8)[0]
            cluster = blob_index = 0
            tail = self.f.read(8)
        else:
            cluster, blob_index = struct.unpack_from("<II", head, 8)
            tail = self.f.read(8)
        data = bytearray(tail)
        while data.count(b"\0") < 2:
            more = self.f.read(256)
            if not more:
                raise ZimError(f"dirent {index} is truncated")
            data += more
        first = data.index(b"\0")
        second = data.index(b"\0", first + 1)
        url = bytes(data[:first])
        title = bytes(data[first + 1:second])
        return (mimetype, chr(namespace), cluster, blob_index, url, title, redirect)

    def cluster_blob(self, cluster_index: int, blob_index: int) -> bytes:
        if cluster_index != self._cluster_id:
            self._load_cluster(cluster_index)
        return self._cluster_blobs[blob_index]

    def _load_cluster(self, index: int) -> None:
        start = self.cluster_ptr(index)
        end = (self.cluster_ptr(index + 1) if index + 1 < self.cluster_count
               else self.checksum_pos)
        self.f.seek(start)
        raw = self.f.read(end - start)
        if not raw:
            raise ZimError(f"cluster {index} is missing")
        info = raw[0]
        compression = info & 0x0F
        offset_size = 8 if info & 0x10 else 4
        payload = raw[1:]
        if compression == CLUSTER_NONE:
            data = payload
        elif compression == CLUSTER_ZSTD:
            data = zstd.decompress(payload)
        elif compression == CLUSTER_XZ:
            data = lzma.decompress(payload)
        else:
            raise ZimError(f"cluster {index}: compression {compression} is unknown")
        first = int.from_bytes(data[:offset_size], "little")
        count = first // offset_size - 1
        if first % offset_size or count < 0 or (count + 1) * offset_size > len(data):
            raise ZimError(f"cluster {index}: offset table does not fit")
        offsets = [int.from_bytes(data[i * offset_size:(i + 1) * offset_size], "little")
                   for i in range(count + 1)]
        if offsets[0] != (count + 1) * offset_size:
            raise ZimError(f"cluster {index}: first offset {offsets[0]} is not the table size")
        # The offsets are measured from the first byte after the info byte, which
        # is where the offset table starts; the check above pins that down.
        self._cluster_blobs = [data[offsets[i]:offsets[i + 1]] for i in range(count)]
        self._cluster_id = index


# --------------------------------------------------------------- HTML to text

_SKIP_TAGS = {"style", "script", "noscript", "table", "sup", "figure",
              "math", "annotation"}
_HEADINGS = {"h1", "h2", "h3", "h4", "h5", "h6"}
_REDIRECT_RE = re.compile(
    rb"""http-equiv=["']refresh["'][^>]*content=["'][^"']*URL=['"]?\.?/?([^"'&>#]+)""",
    re.IGNORECASE)

# Characters NFKD does not decompose but that an English reader expects folded
# rather than dropped.
_CHAR_MAP = {
    "\u00df": "ss", "\u00e6": "ae", "\u00c6": "AE", "\u0153": "oe", "\u0152": "OE",
    "\u00f8": "o", "\u00d8": "O", "\u0111": "d", "\u0110": "D", "\u0142": "l",
    "\u0141": "L", "\u00fe": "th", "\u00de": "Th", "\u00f0": "d", "\u00d0": "D",
    "\u2013": "-", "\u2014": "-", "\u2018": "'", "\u2019": "'", "\u201a": "'",
    "\u201c": '"', "\u201d": '"', "\u201e": '"', "\u2026": "...", "\u00a0": " ",
    "\u2022": "*", "\u00b7": ".", "\u2032": "'", "\u2033": '"', "\u2192": "->",
    "\u2190": "<-", "\u00b0": " deg", "\u00b1": "+/-", "\u00d7": "x", "\u00f7": "/",
    "\u2264": "<=", "\u2265": ">=", "\u2248": "~", "\u00bd": "1/2", "\u00bc": "1/4",
}


def to_ascii(text: str) -> str:
    """Fold text into the printable ASCII the desktop's font can draw."""
    text = "".join(_CHAR_MAP.get(c, c) for c in text)
    text = unicodedata.normalize("NFKD", text)
    out = []
    for ch in text:
        if unicodedata.combining(ch):
            continue
        code = ord(ch)
        if 32 <= code < 127:
            out.append(ch)
        elif code > 126:
            # Non-Latin scripts have no glyphs here; drop rather than litter the
            # page with '?'.
            if code < 0x2000 and ch.isalnum():
                out.append(ch)
            else:
                out.append(" ")
    return re.sub(r"[ \t]+", " ", "".join(out))


def clean_title(raw: bytes) -> bytes:
    """A display title, ASCII, whitespace-collapsed, with underscores expanded."""
    text = raw.decode("utf-8", "replace").replace("_", " ")
    text = " ".join(to_ascii(text).split())
    return text.encode("ascii", "ignore")


def redirect_target(raw: bytes) -> bytes | None:
    """The target path of a libzim redirect page, or None for a real article."""
    match = _REDIRECT_RE.search(raw[:1024])
    if not match:
        return None
    return match.group(1)


class ArticleText(HTMLParser):
    """Collect an article's visible text as blocks: headings, paragraphs, items."""

    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self.blocks: list[tuple[str, str]] = []
        self._skip: list[str] = []
        self._current: list[str] = []
        self._kind = "p"
        self._heading: str | None = None

    def _flush(self) -> None:
        text = " ".join("".join(self._current).split())
        if text:
            self.blocks.append((self._heading or self._kind, text))
        self._current = []
        self._kind = "p"
        self._heading = None

    @staticmethod
    def _hidden(tag, attrs):
        """MediaWiki's hidden MathML carries the LaTeX source as text."""
        if tag in _SKIP_TAGS:
            return True
        for name, value in attrs:
            if name == "class" and value and "mathml" in value:
                return True
        return False

    def handle_starttag(self, tag, attrs):
        if self._hidden(tag, attrs):
            self._skip.append(tag)
            return
        if self._skip:
            return
        if tag in _HEADINGS:
            self._flush()
            self._heading = tag
        elif tag in ("p", "div", "section", "article", "blockquote"):
            self._flush()
        elif tag == "li":
            self._flush()
            self._kind = "li"
        elif tag == "br":
            self._flush()

    def handle_endtag(self, tag):
        if self._skip:
            if self._skip[-1] == tag:
                self._skip.pop()
            return
        if tag in _HEADINGS or tag in ("p", "div", "section", "article",
                                      "blockquote", "li", "tr"):
            self._flush()

    def handle_data(self, data):
        if not self._skip:
            self._current.append(data)


def render_article(raw: bytes, max_bytes: int, title: bytes = b"") -> bytes:
    parser = ArticleText()
    parser.feed(raw.decode("utf-8", "replace"))
    parser.close()
    parser._flush()

    # MediaWiki's own pages repeat the title as the first heading; the reader
    # already shows the title in its header line, so drop the duplicate.
    lead = title.decode("ascii", "ignore").strip()
    blocks = parser.blocks
    if lead and blocks and blocks[0][1].strip().lower() == lead.lower():
        blocks = blocks[1:]

    lines: list[str] = []
    for kind, text in blocks:
        text = " ".join(to_ascii(text).split())
        if not text:
            continue
        if kind in _HEADINGS:
            # The "## " marker comes out of the column budget, or a heading can
            # run three columns past every other line.
            lines.append("## " + text[:WIKI_LINE_COLS - 3])
            lines.append("")
        elif kind == "li":
            wrapped = textwrap.wrap(text, width=WIKI_LINE_COLS - 2,
                                   break_long_words=True, break_on_hyphens=False)
            for i, piece in enumerate(wrapped or [""]):
                lines.append(("- " if i == 0 else "  ") + piece)
        else:
            lines.extend(textwrap.wrap(text, width=WIKI_LINE_COLS,
                                       break_long_words=True, break_on_hyphens=False))
            lines.append("")

    while lines and not lines[-1]:
        lines.pop()
    body = "\n".join(lines) + "\n" if lines else ""
    encoded = body.encode("ascii", "replace")
    if len(encoded) > max_bytes:
        cut = encoded[:max_bytes].rsplit(b"\n", 1)[0]
        encoded = cut + b"\n[truncated: this article is longer than the pack allows]\n"
    return encoded


# ------------------------------------------------------------------ pack write


def fold(title: bytes) -> bytes:
    return title.strip(b"\0").lower()


def _records(path: str, record_bytes: int):
    with open(path, "rb") as f:
        while True:
            rec = f.read(record_bytes)
            if len(rec) < record_bytes:
                return
            yield rec


def _sort_runs(path: str, record_bytes: int, key, sort_records: int) -> list[str]:
    """Split a record file into sorted runs and return their paths."""
    runs: list[str] = []
    with open(path, "rb") as f:
        while True:
            block = f.read(sort_records * record_bytes)
            if not block:
                break
            recs = [block[i:i + record_bytes] for i in range(0, len(block), record_bytes)]
            recs.sort(key=key)
            run = tempfile.NamedTemporaryFile(prefix="wiki-run-", delete=False)
            run.write(b"".join(recs))
            run.close()
            runs.append(run.name)
    return runs


def _merge_runs(runs: list[str], record_bytes: int, key):
    """A single iterator over several sorted runs; the caller must close it."""
    streams = [open(p, "rb") for p in runs]
    merged = heapq.merge(*[_records(p, record_bytes) for p in runs], key=key)
    return merged, streams


class PackWriter:
    """Writes bodies as they are found and builds the index by external sorts."""

    def __init__(self, out_dir: str, chunk_limit: int, title_bytes: int,
                 sort_records: int):
        self.out_dir = out_dir
        self.chunk_limit = chunk_limit
        self.title_bytes = title_bytes
        self.record_bytes = title_bytes + 16
        self.redirect_bytes = title_bytes * 2
        self.sort_records = sort_records
        self.chunk_index = -1
        self.chunk = None
        self.chunk_used = 0
        self.articles = 0
        self.redirects = 0
        self.articles_tmp = tempfile.NamedTemporaryFile(
            prefix="wiki-articles-", suffix=".bin", delete=False)
        self.redirects_tmp = tempfile.NamedTemporaryFile(
            prefix="wiki-redirects-", suffix=".bin", delete=False)

    # ---------------------------------------------------------------- writing

    def _open_chunk(self) -> None:
        if self.chunk:
            self.chunk.close()
        self.chunk_index += 1
        self.chunk = open(os.path.join(self.out_dir, f"wiki.{self.chunk_index:03d}"), "wb")
        self.chunk_used = 0

    def _record(self, title: bytes, chunk: int, offset: int, length: int,
                lines: int) -> bytes:
        name = title[: self.title_bytes].ljust(self.title_bytes, b"\0")
        return name + struct.pack("<IIII", chunk, offset, length, lines)

    def add_article(self, title: bytes, body: bytes, lines: int) -> None:
        if self.chunk is None or self.chunk_used + len(body) > self.chunk_limit:
            self._open_chunk()
        offset = self.chunk_used
        self.chunk.write(body)
        self.chunk_used += len(body)
        self.articles_tmp.write(self._record(title, self.chunk_index, offset,
                                             len(body), lines))
        self.articles += 1

    def add_redirect(self, title: bytes, target: bytes) -> None:
        name = title[: self.title_bytes].ljust(self.title_bytes, b"\0")
        dest = target[: self.title_bytes].ljust(self.title_bytes, b"\0")
        self.redirects_tmp.write(name + dest)
        self.redirects += 1

    # --------------------------------------------------------------- building

    def finalize(self) -> tuple[int, int]:
        """Sort articles, resolve redirects, and write wiki.idx. Returns counts."""
        if self.chunk:
            self.chunk.close()
            self.chunk = None
        self.articles_tmp.close()
        self.redirects_tmp.close()

        title_of = lambda rec: rec[: self.title_bytes]
        article_key = lambda rec: (fold(title_of(rec)), title_of(rec))

        runs = _sort_runs(self.articles_tmp.name, self.record_bytes, article_key,
                          self.sort_records)
        articles_path = tempfile.NamedTemporaryFile(prefix="wiki-sorted-",
                                                    delete=False).name
        with open(articles_path, "wb") as out:
            merged, streams = _merge_runs(runs, self.record_bytes, article_key)
            try:
                for rec in merged:
                    out.write(rec)
            finally:
                for s in streams:
                    s.close()
                for p in runs:
                    os.unlink(p)

        resolved_path = None
        resolved = 0
        if self.redirects:
            resolved_path, resolved = self._resolve_redirects(articles_path)

        index_path = os.path.join(self.out_dir, "wiki.idx")
        written = 0
        with open(index_path, "wb") as index:
            sources = [articles_path] + ([resolved_path] if resolved_path else [])
            merged, streams = _merge_runs(sources, self.record_bytes, article_key)
            previous = None
            try:
                for rec in merged:
                    key = article_key(rec)
                    if key == previous:      # the same title twice: keep the first
                        continue
                    previous = key
                    index.write(rec)
                    written += 1
            finally:
                for s in streams:
                    s.close()

        for path in (self.articles_tmp.name, self.redirects_tmp.name, articles_path):
            if path and os.path.exists(path):
                os.unlink(path)
        if resolved_path and os.path.exists(resolved_path):
            os.unlink(resolved_path)
        return written, resolved

    def _resolve_redirects(self, articles_path: str) -> tuple[str, int]:
        target_of = lambda rec: rec[self.title_bytes:]
        redirect_key = lambda rec: (fold(target_of(rec)), target_of(rec))
        runs = _sort_runs(self.redirects_tmp.name, self.redirect_bytes,
                          redirect_key, self.sort_records)
        resolved_tmp = tempfile.NamedTemporaryFile(prefix="wiki-resolved-",
                                                   delete=False)
        merged_redirects, redirect_streams = _merge_runs(runs, self.redirect_bytes,
                                                         redirect_key)
        articles = _records(articles_path, self.record_bytes)
        article_key = lambda rec: (fold(rec[: self.title_bytes]),
                                   rec[: self.title_bytes])
        current = next(articles, None)
        count = 0
        try:
            for redirect in merged_redirects:
                target_key = redirect_key(redirect)
                while current is not None and article_key(current) < target_key:
                    current = next(articles, None)
                if current is None:
                    break
                if article_key(current) != target_key:
                    continue
                # Reuse the target's body under the redirect's own title.
                title = redirect[: self.title_bytes]
                resolved_tmp.write(
                    self._record(title.rstrip(b"\0"),
                                 *struct.unpack("<IIII", current[self.title_bytes:])))
                count += 1
            resolved_tmp.close()
        finally:
            for s in redirect_streams:
                s.close()
            for p in runs:
                os.unlink(p)

        # The join walks its redirects in *target* order, so the resolved
        # records are not yet in their own title order -- and the final merge
        # assumes both streams are.  Sort them by their own title first.
        article_key = lambda rec: (fold(rec[: self.title_bytes]),
                                   rec[: self.title_bytes])
        title_runs = _sort_runs(resolved_tmp.name, self.record_bytes, article_key,
                                self.sort_records)
        sorted_path = tempfile.NamedTemporaryFile(prefix="wiki-redirects-sorted-",
                                                  delete=False).name
        with open(sorted_path, "wb") as out:
            merged, streams = _merge_runs(title_runs, self.record_bytes, article_key)
            try:
                for rec in merged:
                    out.write(rec)
            finally:
                for s in streams:
                    s.close()
                for p in title_runs:
                    os.unlink(p)
        os.unlink(resolved_tmp.name)
        return sorted_path, count


# ------------------------------------------------------------------------ main


# Wikimedia projects whose text is CC BY-SA 4.0, named so a pack without a
# License key of its own still states the terms it is under.
_WIKIMEDIA_PROJECTS = ("wikipedia", "wikivoyage", "wiktionary", "wikibooks",
                       "wikiquote", "wikisource", "wikinews", "wikiversity",
                       "wikispecies", "wikidata")


def _default_license(metadata: dict[str, str]) -> str:
    """Kiwix archives do not all carry a License key, so name the terms of the
    source the metadata does identify rather than leaving the pack silent."""
    title = (metadata.get("Title", "") + " " + metadata.get("Name", "")).lower()
    for project in _WIKIMEDIA_PROJECTS:
        if project in title:
            return (f"CC BY-SA 4.0 -- {project.capitalize()} text, "
                    "https://creativecommons.org/licenses/by-sa/4.0/ "
                    "(not stated in the archive metadata)")
    return "see the source archive"


def read_metadata(zim: Zim) -> dict[str, str]:
    """The archive's M namespace, read once: this is what the pack copies."""
    out: dict[str, str] = {}
    for i in range(zim.article_count):
        mimetype, ns, cluster, blob_index, url, title, redirect = zim.dirent(i)
        if ns == "M" and redirect is None:
            try:
                out[url.decode("ascii", "replace")] = zim.cluster_blob(
                    cluster, blob_index).decode("utf-8", "replace")
            except ZimError:
                pass
    return out


def write_meta(out_dir: str, metadata: dict[str, str], args, article_count: int,
               redirect_count: int, chunks: int) -> None:
    meta = {
        "format": WIKI_MAGIC,
        "format_version": str(WIKI_VERSION),
        "title": args.title or metadata.get("Title") or "Wikipedia",
        "language": metadata.get("Language") or "en",
        "creator": metadata.get("Creator", ""),
        "publisher": metadata.get("Publisher", ""),
        "source_date": metadata.get("Date", ""),
        "description": metadata.get("Description", ""),
        "license": (args.license or metadata.get("License")
                    or _default_license(metadata)),
        "source_archive": os.path.basename(args.zim),
        "articles": str(article_count),
        "redirects": str(redirect_count),
        "chunks": str(chunks),
        "title_bytes": str(args.title_bytes),
        "record_bytes": str(args.title_bytes + 16),
        "line_columns": str(WIKI_LINE_COLS),
        "main_page": args.main_page or "",
    }
    with open(os.path.join(out_dir, "wiki.meta"), "w", encoding="ascii") as f:
        for key in sorted(meta):
            if not str(meta[key]):
                continue
            f.write(f"{key} = {' '.join(str(meta[key]).split())}\n")


def convert(args) -> int:
    zim = Zim(args.zim)
    print(f"zim: {args.zim}  version {zim.version[0]}.{zim.version[1]}  "
          f"{zim.article_count} entries  {zim.cluster_count} clusters", file=sys.stderr)
    os.makedirs(args.out, exist_ok=True)
    for stale in os.listdir(args.out):
        if re.fullmatch(r"wiki\.(idx|\d{3})", stale):
            os.unlink(os.path.join(args.out, stale))

    html_mime = {i for i, m in enumerate(zim.mime_types) if m.startswith("text/html")}
    if not html_mime:
        raise ZimError("archive has no text/html entries")
    metadata = read_metadata(zim)

    writer = PackWriter(args.out, args.chunk_bytes, args.title_bytes,
                        args.sort_records)

    # Entries are visited in cluster order, not archive order.  Measured on the
    # simple-English archive, walking the archive's own order reached a new
    # cluster for 99.8% of entries -- each one a fresh ~1 MB decompression --
    # because clusters hold a handful of articles that are far apart in URL
    # order.  Sorting 12-byte references first means one decompression per
    # cluster, which is what makes a 49 GB archive a matter of hours.
    refs = tempfile.NamedTemporaryFile(prefix="wiki-refs-", suffix=".bin", delete=False)
    entries = 0
    for i in range(zim.article_count):
        mimetype, ns, cluster, blob_index, url, title, dirent_redirect = zim.dirent(i)
        if ns != "C" or mimetype not in html_mime or dirent_redirect is not None:
            continue
        refs.write(struct.pack("<III", cluster, blob_index, i))
        entries += 1
    refs.close()

    ref_key = lambda rec: (rec[0:4], rec[4:8])
    runs = _sort_runs(refs.name, 12, ref_key, args.sort_records)
    merged, streams = _merge_runs(runs, 12, ref_key)

    kept = skipped_short = 0
    try:
        for ref in merged:
            cluster, blob_index, index = struct.unpack("<III", ref)
            mimetype, ns, _c, _b, url, title, _r = zim.dirent(index)
            display = clean_title(title or url.rsplit(b"/", 1)[-1].replace(b"_", b" "))
            if not display:
                skipped_short += 1
                continue
            raw = zim.cluster_blob(cluster, blob_index)
            target = redirect_target(raw)
            if target is not None:
                # A redirect page: index its title against the target's body,
                # which the join after the sort resolves.  Underscores are the
                # URL form of a title.
                writer.add_redirect(display, clean_title(target))
            else:
                body = render_article(raw, args.max_article_bytes, display)
                if len(body.strip()) < args.min_chars:
                    skipped_short += 1
                    continue
                writer.add_article(display, body, max(body.count(b"\n"), 1))
            kept += 1
            if args.limit and kept >= args.limit:
                break
            if kept % 5000 == 0:
                print(f"  {kept} entries", file=sys.stderr)
    finally:
        for stream in streams:
            stream.close()
        for path in runs:
            os.unlink(path)
        os.unlink(refs.name)

    if writer.articles + writer.redirects == 0:
        raise ZimError("nothing was converted: the archive has no usable articles")

    chunks = writer.chunk_index + 1
    written, resolved = writer.finalize()
    write_meta(args.out, metadata, args, written, resolved, chunks)
    zim.close()
    print(f"wrote {written} index records ({resolved} redirects resolved) in "
          f"{chunks} chunk(s) to {args.out}; skipped {skipped_short} short entries",
          file=sys.stderr)
    return written


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--zim", required=True, help="source .zim archive")
    ap.add_argument("--out", required=True, help="pack folder, e.g. /root/wiki on the card")
    ap.add_argument("--limit", type=int, default=0, help="stop after N entries")
    ap.add_argument("--min-chars", type=int, default=0,
                    help="skip articles whose text is shorter than this")
    ap.add_argument("--max-article-bytes", type=int, default=DEFAULT_MAX_ARTICLE)
    ap.add_argument("--chunk-bytes", type=int, default=DEFAULT_CHUNK_BYTES,
                    help="body bytes per wiki.NNN file (FAT32 caps a file at 4 GiB)")
    ap.add_argument("--title-bytes", type=int, default=WIKI_TITLE_BYTES)
    ap.add_argument("--sort-records", type=int, default=DEFAULT_SORT_RECORDS,
                    help="records sorted in memory at a time")
    ap.add_argument("--title", default="", help="override the pack title")
    ap.add_argument("--license", default="", help="override the licence line")
    ap.add_argument("--main-page", default="", help="title the reader opens first")
    args = ap.parse_args(argv)
    if args.title_bytes < 8 or args.title_bytes > 255:
        ap.error("--title-bytes must be between 8 and 255")
    try:
        convert(args)
    except ZimError as exc:
        print(f"make_wiki_pack: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
