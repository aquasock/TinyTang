#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Convert the English Wikipedia ZIM into a TinyKiwix pack, unattended.

Designed to be started before bed and read in the morning:

    tools/convert_wikipedia_overnight.py start          # run it (detached-safe)
    tools/convert_wikipedia_overnight.py status         # from another terminal
    tools/convert_wikipedia_overnight.py tail           # follow the log

What makes it usable overnight:

* **It verifies before it converts.**  The archive's own MD5 is checked against
  the stored value at the header's ``checksumPos``, not "everything but the last
  16 bytes" -- the English archive carries about 1.01 GB of junk past its
  trailer, so the old check would reject a complete download.  See
  ``tools/zim_verify.py``.
* **It refuses to run out of disk.**  The pack is roughly half the archive's
  content in text, so it needs tens of gigabytes; the free space is checked
  before the conversion starts and re-checked as it runs, and the job stops
  *before* filling the filesystem rather than dying in the middle of a write.
* **It says what it is doing, once every few seconds**, with the entries
  processed, the rate and an ETA.  The converter only reports every 5000
  entries and can be quiet for long stretches on a slow machine, so this adds a
  heartbeat that proves it is alive.
* **The state lives in files**, not in the terminal: ``status.json`` and
  ``progress.log`` beside the output, so a second terminal can watch without
  attaching to anything.
* **The converter has no resume.**  If the job dies, the next run clears the
  partial pack and starts again; it says so rather than pretending otherwise.

Exit status: 0 the pack is built and verified, 1 something failed, 2 usage.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import selectors
import shutil
import signal
import subprocess
import sys
import time
from datetime import datetime, timezone

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONVERT = os.path.join(ROOT, "tools", "make_wiki_pack.py")
CHECK = os.path.join(ROOT, "tools", "check_wiki_pack.py")
VERIFY = os.path.join(ROOT, "tools", "zim_verify.py")

SCRATCH = os.environ.get("SCRATCH", "/run/media/vash/GIT/tinytang-scratch")
DEFAULT_ZIM = os.path.join(SCRATCH, "wikipedia_en_all_nopic_2026-06.zim")
DEFAULT_OUT = os.path.join(SCRATCH, "wikipedia-en-all")

# The converter's progress line, as make_wiki_pack.py prints it:
#   "  12345/6789012 entries, 12000 kept, 340/s, elapsed 36s, eta 5h30m"
PROGRESS_RE = re.compile(
    r"(?P<done>\d+)/(?P<total>\d+) entries, "
    r"(?P<kept>\d+) kept, "
    r"(?P<rate>[\d.]+)/s, "
    r"elapsed (?P<elapsed>[\w.]+), "
    r"eta (?P<eta>[\w.]+)"
)
HEADER_RE = re.compile(r"(?P<count>\d+) entries\s+(?P<clusters>\d+) clusters")

# Stop if the filesystem falls below this, which leaves room for the checker.
MIN_FREE_BYTES = 8 << 30

# Do not start a conversion that cannot finish with room to spare.
ESTIMATE_PACK_FRACTION = 0.55   # pack bytes per content byte, nopic text


def _now() -> str:
    return datetime.now(timezone.utc).astimezone().isoformat(timespec="seconds")


def _human(n: float) -> str:
    for unit in ("B", "KB", "MB", "GB", "TB"):
        if abs(n) < 1024 or unit == "TB":
            return f"{n:.1f} {unit}" if unit != "B" else f"{int(n)} B"
        n /= 1024.0
    return f"{n:.1f} TB"


def _clock(seconds: float) -> str:
    s = int(max(seconds, 0))
    if s >= 3600:
        return f"{s // 3600}h{s % 3600 // 60:02d}m"
    if s >= 60:
        return f"{s // 60}m{s % 60:02d}s"
    return f"{s}s"


class Runner:
    def __init__(self, zim: str, out: str, interval: int, truncate: bool,
                 skip_verify: bool, limit: int = 0):
        self.zim = zim
        self.out = out
        self.interval = interval
        self.truncate = truncate
        self.skip_verify = skip_verify
        self.limit = limit
        self.state_path = os.path.join(out, "status.json")
        self.log_path = os.path.join(out, "progress.log")
        self.state: dict = {
            "started": _now(),
            "stage": "starting",
            "zim": zim,
            "out": out,
        }
        self.log_file = None

    # ------------------------------------------------------------- plumbing

    def say(self, text: str) -> None:
        line = f"{datetime.now().strftime('%H:%M:%S')}  {text}"
        print(line, flush=True)
        if self.log_file:
            self.log_file.write(line + "\n")
            self.log_file.flush()

    def save(self, **changes) -> None:
        self.state.update(changes)
        self.state["updated"] = _now()
        try:
            os.makedirs(self.out, exist_ok=True)
            tmp = self.state_path + ".tmp"
            with open(tmp, "w") as f:
                json.dump(self.state, f, indent=2)
            os.replace(tmp, self.state_path)
        except OSError:
            pass

    def fail(self, text: str) -> int:
        self.say("FAILED  " + text)
        self.save(stage="failed", error=text, finished=_now())
        return 1

    # -------------------------------------------------------------- preflight

    def check_space(self, need: int) -> bool:
        os.makedirs(self.out, exist_ok=True)
        free = shutil.disk_usage(self.out).free
        self.save(free_bytes=free, need_bytes=need)
        if free < need:
            self.say(f"not enough space: {_human(free)} free, "
                     f"{_human(need)} needed")
            return False
        return True

    def verify_archive(self) -> int | None:
        """Return the content length, or None on failure."""
        self.say(f"verifying {self.zim}")
        args = [sys.executable, VERIFY, self.zim]
        if self.truncate:
            args.insert(2, "--truncate")
        proc = subprocess.run(args, capture_output=True, text=True)
        for line in (proc.stdout + proc.stderr).splitlines():
            if line.strip():
                self.say("  " + line.strip())
        if proc.returncode != 0:
            self.say("the archive did not verify; stopping")
            return None
        # Ask the verifier for the numbers rather than scraping prose.
        info = subprocess.run(
            [sys.executable, "-c",
             "import sys,json;sys.path.insert(0,%r);import zim_verify;"
             "print(json.dumps(zim_verify.describe(%r)))" % (os.path.join(ROOT, "tools"), self.zim)],
            capture_output=True, text=True)
        if info.returncode != 0:
            self.say("could not read the archive's header; stopping")
            return None
        detail = json.loads(info.stdout)
        self.save(**detail)
        return detail["content_bytes"]

    # ------------------------------------------------------------- conversion

    def clear_stale(self) -> None:
        """The converter has no resume, so a partial pack must go."""
        if not os.path.isdir(self.out):
            return
        stale = [n for n in os.listdir(self.out)
                 if re.fullmatch(r"wiki\.(idx|parts|auth|meta|\d{3})", n)]
        if not stale:
            return
        total = sum(os.path.getsize(os.path.join(self.out, n))
                    for n in stale
                    if os.path.isfile(os.path.join(self.out, n)))
        self.say(f"clearing {len(stale)} partial pack file(s), {_human(total)} "
                 f"(the converter cannot resume)")
        for n in stale:
            try:
                os.unlink(os.path.join(self.out, n))
            except OSError as exc:
                self.say(f"  could not remove {n}: {exc}")

    def run_converter(self) -> int:
        cmd = [sys.executable, CONVERT, "--zim", self.zim,
               "--out", self.out, "--min-chars", "40"]
        if self.limit:
            cmd += ["--limit", str(self.limit)]
        self.say("converting: " + " ".join(cmd))
        started = time.monotonic()
        self.save(stage="converting", command=cmd)

        # Binary, not text: read1() is what lets the loop return between lines
        # instead of blocking until the child exits, and it lives on the binary
        # buffer.  Decoding is done per chunk.
        proc = subprocess.Popen(cmd, stderr=subprocess.PIPE,
                                stdout=subprocess.DEVNULL, bufsize=0)
        sel = selectors.DefaultSelector()
        sel.register(proc.stderr, selectors.EVENT_READ)

        done = total = kept = 0
        rate = 0.0
        last_report = time.monotonic()
        last_print = 0.0
        tail: list[str] = []

        def emit(force: bool = False) -> None:
            nonlocal last_print
            now = time.monotonic()
            if not force and now - last_print < self.interval:
                return
            last_print = now
            elapsed = now - started
            if done and total:
                pct = done * 100.0 / total
                remain = (total - done) / rate if rate > 0 else 0
                self.say(f"  {done:,}/{total:,} entries  {pct:5.1f}%  "
                         f"{kept:,} kept  {rate:.0f}/s  "
                         f"elapsed {_clock(elapsed)}  eta {_clock(remain)}")
                self.save(stage="converting", entries_done=done,
                          entries_total=total, kept=kept, rate=rate,
                          elapsed_s=int(elapsed), eta_s=int(remain))
            elif total:
                # The converter opens the archive and walks its index before it
                # processes an entry, which on this one is minutes with no
                # output.  Saying "0% eta 0s" there would read as a stall.
                self.say(f"  reading the archive index  ({total:,} entries, "
                         f"{_human(os.path.getsize(self.zim))})  "
                         f"elapsed {_clock(elapsed)}")
                self.save(stage="converting", entries_total=total,
                          elapsed_s=int(elapsed))
            else:
                self.say(f"  working, elapsed {_clock(elapsed)}")
                self.save(stage="converting", elapsed_s=int(elapsed))

        def consume(line: str) -> None:
            nonlocal done, total, kept, rate, last_report
            m = PROGRESS_RE.search(line)
            if m:
                done = int(m.group("done"))
                total = int(m.group("total"))
                kept = int(m.group("kept"))
                rate = float(m.group("rate"))
                last_report = time.monotonic()
                emit(force=True)
                return
            h = HEADER_RE.search(line)
            if h:
                total = int(h.group("count"))
                self.save(entries_total=total,
                          clusters=int(h.group("clusters")))
                return
            tail.append(line)
            if len(tail) > 20:
                tail.pop(0)

        buf = ""
        while True:
            events = sel.select(timeout=self.interval)
            for key, _ in events:
                # One os.read, not a stream read: a stream read waits for EOF
                # or fills the buffer, and either way the heartbeat would never
                # run during the conversion.
                try:
                    chunk = os.read(key.fileobj.fileno(), 65536)
                except BlockingIOError:
                    chunk = b""
                chunk = chunk.decode("utf-8", "replace")
                if not chunk:
                    # EOF: unregister and let the loop see the exit.
                    sel.unregister(proc.stderr)
                    break
                buf += chunk
                while "\n" in buf:
                    line, buf = buf.split("\n", 1)
                    if line.strip():
                        consume(line.rstrip())

            if proc.poll() is not None:
                # Drain whatever is still buffered, then stop.
                rest = b""
                try:
                    rest = proc.stderr.read() or b""
                except (ValueError, OSError):
                    pass
                if rest:
                    buf += rest.decode("utf-8", "replace")
                for line in buf.splitlines():
                    if line.strip():
                        consume(line.rstrip())
                break

            emit()
            free = shutil.disk_usage(self.out).free
            self.save(free_bytes=free)
            if free < MIN_FREE_BYTES:
                self.say(f"stopping: only {_human(free)} free left")
                proc.terminate()
                try:
                    proc.wait(timeout=30)
                except subprocess.TimeoutExpired:
                    proc.kill()
                return self.fail("the filesystem ran out of room")
            if time.monotonic() - last_report > 300:
                self.say("  (no progress line for 5 min; still running)")
                last_report = time.monotonic()

        sel.close()
        rc = proc.wait()
        emit(force=True)
        if rc != 0:
            shown = "\n".join("  " + t for t in tail[-6:])
            return self.fail(f"the converter exited {rc}\n{shown}")
        return 0

    # ----------------------------------------------------------------- finish

    def check_pack(self) -> int:
        self.say("checking the pack")
        self.save(stage="checking")
        proc = subprocess.run([sys.executable, CHECK, self.out],
                              capture_output=True, text=True)
        for line in (proc.stdout + proc.stderr).splitlines():
            if line.strip():
                self.say("  " + line.strip())
        if proc.returncode != 0:
            return self.fail("the pack did not pass its own check")

        total = 0
        files = 0
        for name in sorted(os.listdir(self.out)):
            if re.fullmatch(r"wiki\.(idx|parts|auth|meta|\d{3})", name):
                p = os.path.join(self.out, name)
                if os.path.isfile(p):
                    total += os.path.getsize(p)
                    files += 1
        self.say(f"pack: {files} file(s), {_human(total)}")
        self.save(stage="done", finished=_now(), pack_bytes=total,
                  pack_files=files)
        return 0


def cmd_start(args) -> int:
    if not os.path.isfile(args.zim):
        print(f"no archive at {args.zim}", file=sys.stderr)
        return 2

    runner = Runner(args.zim, args.out, args.interval, args.truncate,
                    args.skip_verify, args.limit)
    os.makedirs(args.out, exist_ok=True)
    runner.log_file = open(runner.log_path, "a")
    runner.say("=" * 60)
    runner.say(f"English Wikipedia -> {args.out}")

    # The convert stage is the long one; everything before it is minutes.
    if not args.skip_verify:
        content = runner.verify_archive()
        if content is None:
            return 1
    else:
        content = os.path.getsize(args.zim)

    need = int(content * ESTIMATE_PACK_FRACTION) + MIN_FREE_BYTES
    if not runner.check_space(need):
        return runner.fail("not enough disk space to start")

    runner.clear_stale()

    rc = runner.run_converter()
    if rc:
        return rc

    # The pack is only useful if it passes its own reader's check.
    return runner.check_pack()


def cmd_status(args) -> int:
    path = os.path.join(args.out, "status.json")
    if not os.path.isfile(path):
        print(f"no run recorded in {args.out}")
        return 1
    with open(path) as f:
        state = json.load(f)

    stage = state.get("stage", "?")
    print(f"stage        {stage}")
    print(f"started      {state.get('started', '?')}")
    print(f"updated      {state.get('updated', '?')}")
    if stage == "converting":
        done = state.get("entries_done", 0)
        total = state.get("entries_total", 0)
        if total:
            print(f"entries      {done:,} of {total:,} "
                  f"({done * 100.0 / total:.1f}%)")
        print(f"kept         {state.get('kept', 0):,}")
        print(f"rate         {state.get('rate', 0):.0f}/s")
        print(f"elapsed      {_clock(state.get('elapsed_s', 0))}")
        print(f"eta          {_clock(state.get('eta_s', 0))}")
    if "free_bytes" in state:
        print(f"free disk    {_human(state['free_bytes'])}")
    if stage == "done":
        print(f"pack         {state.get('pack_files', 0)} file(s), "
              f"{_human(state.get('pack_bytes', 0))}")
        print(f"finished     {state.get('finished', '?')}")
    if stage == "failed":
        print(f"error        {state.get('error', '?')}")
    if os.path.isfile(os.path.join(args.out, "progress.log")):
        print()
        print("last lines:")
        with open(os.path.join(args.out, "progress.log")) as f:
            lines = f.readlines()[-8:]
        for line in lines:
            print("  " + line.rstrip())
    return 0


def cmd_tail(args) -> int:
    path = os.path.join(args.out, "progress.log")
    if not os.path.isfile(path):
        print(f"no log at {path}", file=sys.stderr)
        return 1
    with open(path) as f:
        lines = f.readlines()
        for line in lines[-args.lines:]:
            print(line.rstrip())
    return 0


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("command", nargs="?", default="start",
                    choices=("start", "status", "tail"))
    ap.add_argument("--zim", default=DEFAULT_ZIM)
    ap.add_argument("--out", default=DEFAULT_OUT)
    ap.add_argument("--interval", type=int, default=20,
                    help="seconds between progress lines (default 20)")
    ap.add_argument("--truncate", action="store_true",
                    help="cut the archive's junk tail before converting")
    ap.add_argument("--skip-verify", action="store_true",
                    help="do not re-hash the archive (use after a verified run)")
    ap.add_argument("--lines", type=int, default=40, help="lines for `tail`")
    ap.add_argument("--limit", type=int, default=0,
                    help="stop after N entries (for a smoke test)")
    args = ap.parse_args(argv)

    def on_sigint(signum, frame):
        print("\ninterrupted", file=sys.stderr)
        sys.exit(130)

    signal.signal(signal.SIGINT, on_sigint)

    if args.command == "status":
        return cmd_status(args)
    if args.command == "tail":
        return cmd_tail(args)
    return cmd_start(args)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
