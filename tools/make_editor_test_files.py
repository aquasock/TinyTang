#!/usr/bin/env python3
"""Write the two text files that check the Editor's 8 KB limit on the board.

    tools/make_editor_test_files.py <out-dir>

editor-7k.txt is 7,000 bytes and opens editable; editor-9k.txt is 9,000 bytes,
past TD_EDITOR_MAX (8,192, CMakeLists.txt), and opens read-only.  Each is
numbered lines of the same width, so the content is the same on every run and
any line shows how far into the file it is.  Copy them to the card with
tools/tinytang_put.py.
"""
import pathlib
import sys

def text(size):
    lines = []
    n = 0
    while sum(len(l) for l in lines) < size:
        n += 1
        lines.append(f"line {n:04d}: the quick brown fox jumps over the lazy dog\n")
    return "".join(lines)[:size - 1] + "\n"

def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__.strip().splitlines()[2].strip())
    out = pathlib.Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    for name, size in (("editor-7k.txt", 7000), ("editor-9k.txt", 9000)):
        data = text(size).encode("ascii")
        assert len(data) == size
        (out / name).write_bytes(data)
        print(f"{out / name}: {size} bytes")

if __name__ == "__main__":
    main()
