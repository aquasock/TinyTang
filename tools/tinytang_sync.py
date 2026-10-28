#!/usr/bin/env python3
"""TinyTang Sync: move files between this computer and a TinyTang board.

A small, deliberate take on the old Palm ActiveSync idea:

  1. asks you to put the board at its console and connect it to this PC's USB
     port (the board's OTG socket), then finds it by its USB identity;
  2. reads the board's folder tree and prints it;
  3. asks which folder to work with;
  4. compares that folder with a mirror kept beside this script, by name and
     size;
  5. shows what differs and asks which way to move the files.

Everything travels over the board's own console with the shell's `tangput` and
`tangget` commands, so the board needs no extra software and the card never has
to leave it.  Two files go one way and come back the same; anything else is a
bug.

Nothing is deleted.  A file present on one side only is copied to the other; a
file present on both with different sizes is overwritten, in the direction you
choose, and listed before it happens.

Only the standard library and pyserial are needed:

    pip install pyserial
    python tinytang_sync.py                 # your files live in ./tang
    python tinytang_sync.py --remote /root/media/nes
"""

from __future__ import annotations

import argparse
import os
import re
import sys
import time
from pathlib import Path

try:
    import serial
    from serial.tools import list_ports
except ImportError:  # pragma: no cover - the one dependency
    sys.exit("pyserial is needed:  pip install pyserial")

USB_VID = 0xFFFF          # the board enumerates as ffff:5454, "TinyTang CDC"
USB_PID = 0x5454
BAUD = 115200
MIRROR_DIRNAME = "tang"   # the local mirror sits beside this script
CHUNK = 8192
MAX_ENTRIES = 5000        # a tree scan stops here rather than crawling a card
MAX_DEPTH = 6


class ConsoleError(Exception):
    pass


# --------------------------------------------------------------------- console


class Console:
    """The board's console: one command at a time, text in, raw bytes for files."""

    def __init__(self, port: str):
        self.port = port
        self.ser = serial.Serial(port, BAUD, timeout=0.2, write_timeout=60.0)

    def close(self) -> None:
        try:
            self.ser.close()
        except Exception:
            pass

    # -- framing ---------------------------------------------------------

    def _write(self, data: bytes) -> None:
        self.ser.write(data)
        self.ser.flush()

    def _drain(self, quiet: float = 0.15) -> None:
        """Read until the line has been quiet for `quiet` seconds.

        The board's last bytes of one command can still be in flight when the
        next one starts; without this the next command's output begins with the
        previous one's tail.
        """
        end = time.time() + quiet
        while time.time() < end:
            if self.ser.read(256):
                end = time.time() + quiet

    def _slurp(self, seconds: float) -> bytes:
        out = b""
        end = time.time() + seconds
        while time.time() < end:
            out += self.ser.read(1024)
        return out

    def at_prompt(self) -> bool:
        """True only when the shell is sitting at its prompt.

        The console answers ESC [ ? 7 7 n only when nothing is running: 1 at the
        prompt, 2 with the desktop up, nothing at all while a command runs.
        """
        self.ser.reset_input_buffer()
        self._write(b"\x1b[?77n")
        end = time.time() + 3.0
        seen = b""
        while time.time() < end:
            seen += self.ser.read(64)
            m = re.search(rb"\x1b\[\?77;(\d)n", seen)
            if m:
                return m.group(1) == b"1"
        return False

    def command(self, line: str, timeout: float = 20.0) -> str:
        """Run one shell command and return its output, without the echo.

        A nonce is echoed after the command and read back, which is the only
        reliable end-of-output marker: the prompt carries colour codes and the
        commands themselves print whatever they like.
        """
        nonce = "tt%08x" % (int.from_bytes(os.urandom(4), "big"))
        self.ser.reset_input_buffer()
        self._write(line.encode("utf-8", "replace") + b"\r")
        time.sleep(0.05)
        self._write(("echo " + nonce).encode() + b"\r")
        buf = b""
        end = time.time() + timeout
        needle = nonce.encode()
        while time.time() < end:
            buf += self.ser.read(4096)
            if needle in buf:
                break
        else:
            raise ConsoleError(f"the board did not answer {line!r}")
        text = buf.decode("utf-8", "replace")
        # Everything from the echo of the nonce command onward is ours, not the
        # command's: cut there, then drop the echoed command line and any prompt.
        text = ANSI.sub("", text.split("echo " + nonce)[0])
        lines = text.replace("\r\n", "\n").replace("\r", "\n").split("\n")
        while lines and lines[0].strip() in (line.strip(), ""):
            lines.pop(0)
        while lines and (not lines[-1].strip() or re.search(r"[#$]\s*$", lines[-1])):
            lines.pop()
        self._drain()
        return "\n".join(lines).rstrip()

    # -- files -----------------------------------------------------------

    def make_dirs(self, remote_dir: str) -> None:
        """Create a folder one level at a time.

        The shell's `mkdir -p` reports success and creates nothing (measured on
        this build), so each level is made with a plain `mkdir`, whose "exists"
        answer is harmless and ignored.
        """
        current = ""
        for part in [p for p in remote_dir.split("/") if p]:
            current += "/" + part
            self.command(f'mkdir "{current}"')

    def put(self, local: Path, remote: str, size: int) -> None:
        """Send a file with the shell's tangput: header, then raw bytes."""
        nonce = None
        self.ser.reset_input_buffer()
        self._write(f'tangput {size} "{remote}"\r'.encode())
        buf = b""
        end = time.time() + 30
        while time.time() < end:
            buf += self.ser.read(256)
            if b"ready for" in buf:
                break
            if b"cannot create" in buf or b"bad path" in buf or b"run it from" in buf:
                raise ConsoleError(buf.decode("utf-8", "replace").strip().splitlines()[-1])
        else:
            raise ConsoleError("tangput did not answer")
        nonce = None
        with open(local, "rb") as fh:
            sent = 0
            while sent < size:
                block = fh.read(min(CHUNK, size - sent))
                if not block:
                    break
                self._write(block)
                sent += len(block)
        # tangput answers after the last byte.
        buf = b""
        end = time.time() + 120
        while time.time() < end:
            buf += self.ser.read(256)
            if b"tangput: wrote" in buf:
                return
            if b"write failed" in buf:
                raise ConsoleError("the card refused the write")
        raise ConsoleError("no confirmation after the upload")

    def get(self, remote: str, local: Path) -> int:
        """Fetch a file with tangget: one header line, then exactly that many bytes.

        The header's own line ending is not part of the file -- the mistake that
        made a first version of this reader lose two bytes per transfer.
        """
        self.ser.reset_input_buffer()
        self._write(f'tangget "{remote}"\r'.encode())
        head = b""
        end = time.time() + 30
        match = None
        pattern = re.compile(rb"tangget: (\d+) bytes at (\d+) of (\d+)\r\n")
        while time.time() < end:
            head += self.ser.read(256)
            match = pattern.search(head)
            if match:
                break
            if b"cannot open" in head or b"bad path" in head:
                raise ConsoleError(head.decode("utf-8", "replace").strip().splitlines()[-1])
        if match is None:
            raise ConsoleError("tangget did not answer")
        want = int(match.group(1))
        # The read that found the header can carry file bytes and trailer
        # together; keep exactly the announced number and let the rest go.
        body = head[match.end():][:want]
        local.parent.mkdir(parents=True, exist_ok=True)
        with open(local, "wb") as fh:
            fh.write(body)
            got = len(body)
            end = time.time() + 30
            while got < want and time.time() < end:
                block = self.ser.read(min(CHUNK, want - got))
                if block:
                    fh.write(block)
                    got += len(block)
                    end = time.time() + 30
            if got != want:
                raise ConsoleError(f"short read: {got} of {want} bytes")
        # Consume the trailer so the next command starts clean.
        self._slurp(0.2)
        return want


# ---------------------------------------------------------------------- board


def find_board(verbose: bool = True) -> str:
    """The board's serial port, found by USB identity rather than by name."""
    candidates = list(list_ports.comports())
    named = []
    for info in candidates:
        if info.vid == USB_VID and info.pid == USB_PID:
            return info.device
        named.append(info)
    # No VID match (some drivers hide it): offer to try the likely ones.
    likely = [i.device for i in named if re.search(r"ttyACM|ttyUSB|usbmodem|COM\d+", i.device)]
    for device in likely:
        try:
            console = Console(device)
        except Exception:
            continue
        try:
            if console.at_prompt():
                return device
        finally:
            console.close()
    listing = "\n".join(f"  {i.device}  {i.description}" for i in named) or "  (none found)"
    raise ConsoleError(
        "no TinyTang board found.\n"
        "  - connect the board's OTG socket to this computer with a data cable;\n"
        "  - let it boot to the console prompt (root@tinytang:~#), not the desktop.\n"
        f"serial ports seen:\n{listing}")


# The shell lists a directory on one line, entries separated by two spaces,
# with colour codes around directory names.  An entry is a kind, a size, then a
# name that runs to the next entry.
ANSI = re.compile(r"\x1b\[[0-9;]*m")
LS_ENTRY = re.compile(r"([d-])\s+(\d+)\s{2,}(.+?)(?=\s{2,}[d-]\s+\d+\s{2,}|\Z)", re.S)


def parse_ls(text: str) -> list[tuple[str, int, str]]:
    text = ANSI.sub("", text)
    return [(m.group(1), int(m.group(2)), m.group(3).strip())
            for m in LS_ENTRY.finditer(text)]


def scan_tree(console: Console, root: str) -> dict[str, int]:
    """Every file under root, as {absolute board path: size}."""
    files: dict[str, int] = {}
    dirs = [("", root)]
    seen = 0
    while dirs and seen < MAX_ENTRIES:
        rel, path = dirs.pop(0)
        out = console.command(f'ls -l "{path}"')
        for kind, size, name in parse_ls(out):
            if name in (".", "..", ""):
                continue
            child_rel = f"{rel}/{name}" if rel else name
            child_path = f"{path}/{name}"
            seen += 1
            if kind == "d":
                if child_rel.count("/") < MAX_DEPTH:
                    dirs.append((child_rel, child_path))
            else:
                files[child_path] = size
        if seen >= MAX_ENTRIES:
            break
    return files


def print_tree(files: dict[str, int], limit: int = 400, root: str = "/root") -> None:
    """The scan as an indented tree, directories grouped by their path."""
    prefix = root.rstrip("/") + "/"
    shown = 0
    last_dir = None
    for full in sorted(files, key=lambda p: (p.count("/"), p.lower())):
        rel = full[len(prefix):] if full.startswith(prefix) else full
        parts = rel.split("/")
        directory = "/".join(parts[:-1])
        if directory != last_dir:
            print(f"{'  ' * (len(parts) - 1)}{directory + '/' if directory else ''}")
            last_dir = directory
        print(f"{'  ' * len(parts)}{parts[-1]:<40s} {files[full]:>10d}")
        shown += 1
        if shown >= limit:
            print(f"  ... {len(files) - shown} more")
            break


# ----------------------------------------------------------------------- sync


def local_root_for(script: Path, remote: str) -> Path:
    """The mirror beside the script: ./tang/root/media/nes for /root/media/nes."""
    return script / MIRROR_DIRNAME / remote.strip("/")


def scan_local(root: Path) -> dict[str, int]:
    files: dict[str, int] = {}
    if not root.exists():
        return files
    for path in sorted(root.rglob("*")):
        if path.is_file():
            files[str(path.relative_to(root)).replace(os.sep, "/")] = path.stat().st_size
    return files


def human(n: int) -> str:
    for unit in ("B", "KB", "MB", "GB"):
        if n < 1024 or unit == "GB":
            return f"{n:.0f} {unit}" if unit == "B" else f"{n:.1f} {unit}"
        n /= 1024.0
    return f"{n:.1f} GB"


def plan(board: dict[str, int], local: dict[str, int]):
    only_board = sorted(set(board) - set(local))
    only_local = sorted(set(local) - set(board))
    differing = sorted(p for p in set(board) & set(local) if board[p] != local[p])
    return only_board, only_local, differing


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--port", help="serial port; found by USB identity when omitted")
    ap.add_argument("--remote", help="folder on the board, e.g. /root/media/nes")
    ap.add_argument("--local", help="local mirror folder (default: ./tang beside this script)")
    ap.add_argument("--scan-only", action="store_true", help="print the tree and stop")
    args = ap.parse_args()

    script = Path(__file__).resolve().parent
    print("TinyTang Sync")
    print("-------------")
    print("Before we start:")
    print("  1. the board is powered and sitting at its console prompt")
    print("     (root@tinytang:~#), not in the desktop;")
    print("  2. its OTG socket is connected to this computer with a data cable.")
    input("Press Enter when that is true (Ctrl+C to stop) ... ")

    try:
        port = args.port or find_board()
    except ConsoleError as exc:
        print(f"\n{exc}")
        return 2
    print(f"\nboard on {port}")
    console = Console(port)
    try:
        if not console.at_prompt():
            print("The console did not answer as an idle shell. If the desktop is open, quit it")
            print("first -- the sync and the desktop cannot share the console.")
            return 2

        print("reading the folder tree ...")
        files = scan_tree(console, "/root")
        if not files:
            print("the board reported no files under /root")
            return 1
        print(f"\n{len(files)} files under /root\n")
        print_tree(files)
        if args.scan_only:
            return 0

        remote = args.remote
        if not remote:
            remote = input("\nFolder on the board to sync (e.g. /root/media/nes): ").strip()
        if not remote.startswith("/"):
            remote = "/" + remote
        remote = remote.rstrip("/") or "/root"

        known = any(p.startswith(remote.rstrip("/") + "/") for p in files)
        if not known:
            print(f"\nnote: {remote} is not in the tree just read. If that is a typo, stop")
            print("now; if it is a new folder, the files will be created on the board.")

        prefix = remote.rstrip("/") + "/"
        board = {p[len(prefix):]: size for p, size in files.items()
                 if p.startswith(prefix)}
        if remote == "/root":
            board = {p[len("/root/"):]: size for p, size in files.items()
                     if p.startswith("/root/")}

        local_root = Path(args.local).expanduser() if args.local else local_root_for(script, remote)
        local = scan_local(local_root)
        print(f"\nlocal mirror: {local_root}")
        print(f"  {len(local)} files here, {len(board)} files on the board")

        only_board, only_local, differing = plan(board, local)
        print("\nDifferences")
        print("-----------")
        if not (only_board or only_local or differing):
            print("  nothing: every file agrees in name and size")
            return 0
        for p in only_board:
            print(f"  board only   {p:<40s} {human(board[p])}")
        for p in only_local:
            print(f"  here only    {p:<40s} {human(local[p])}")
        for p in differing:
            print(f"  size differs {p:<40s} board {human(board[p])}   here {human(local[p])}")
        print("\nSizes are all this compares: two files of the same length are treated as equal,")
        print("because the card carries no real timestamps and hashing costs a transfer each.")

        print("\nWhich way?")
        print("  [1] this computer -> board   (send new and size-differing files up)")
        print("  [2] board -> this computer   (fetch new and size-differing files down)")
        print("  [3] nothing, leave it")
        choice = input("choose 1, 2 or 3: ").strip()
        if choice not in ("1", "2"):
            print("no changes made")
            return 0

        if choice == "1":
            moves = [(p, "up") for p in only_local + differing]
        else:
            moves = [(p, "down") for p in only_board + differing]
        if not moves:
            print("nothing to move in that direction")
            return 0

        total = sum((local[p] if d == "up" else board[p]) for p, d in moves)
        print(f"\n{len(moves)} file(s), {human(total)}")
        if input("proceed? [y/N] ").strip().lower() not in ("y", "yes"):
            print("no changes made")
            return 0

        failed = 0
        for i, (rel, direction) in enumerate(moves, 1):
            remote_path = f"{remote}/{rel}" if remote.strip("/") else "/" + rel
            local_path = local_root / rel
            started = time.time()
            try:
                if direction == "up":
                    local_path.parent.mkdir(parents=True, exist_ok=True)
                    size = local_path.stat().st_size
                    console.make_dirs(remote_path.rsplit("/", 1)[0])
                    console.put(local_path, remote_path, size)
                    print(f"  {i}/{len(moves)} up   {rel}  {human(size)}  {time.time()-started:.1f}s")
                else:
                    size = console.get(remote_path, local_path)
                    print(f"  {i}/{len(moves)} down {rel}  {human(size)}  {time.time()-started:.1f}s")
            except ConsoleError as exc:
                failed += 1
                print(f"  {i}/{len(moves)} FAILED {rel}: {exc}")
        print(f"\ndone: {len(moves) - failed} moved, {failed} failed")
        return 0 if failed == 0 else 1
    except KeyboardInterrupt:
        print("\nstopped")
        return 130
    except ConsoleError as exc:
        print(f"\n{exc}")
        return 1
    finally:
        console.close()


if __name__ == "__main__":
    sys.exit(main())
