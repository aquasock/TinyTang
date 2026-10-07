Draft reply for [tinydesk-project/tinydesk issue #6](https://github.com/tinydesk-project/tinydesk/issues/6),
"Heavy RAM usage of Editor app", answering the author's comment of
2026-10-07T20:42Z. Not posted: it is for the user to review and post from
their own account (`.ai/core.md` forbids agents writing to repositories
outside `aquasock`). The results are core-log entry 55 and `TDESK-016`.

---

Thanks, that explained it. I set the limits you suggested in TinyTang's build, `-DTD_EDITOR_MAX=8192 -DTD_EDITOR_UNDO=2048`, and tested on the Tang (BL616, 132 KB heap):

- Free heap in System Monitor was 50 KB with Files open and 38 KB with the Editor open on a 7,000-byte file, so the Editor now takes 12 KB instead of 20–25 KB. It went back to 50 KB every time I closed it.
- The 7,000-byte file edited normally. I added three characters, saved, and the change was there when I reopened it.
- A 9,000-byte file opened read-only ("File too large: read-only") and wasn't changed on the card.

8 KB covers our scripts, playlists and config files, so this works for us. A buffer that grows with the file would still be nice later.

One thing for the tests: `test_history_limits` pastes 10,001 bytes, so it can only pass at the default 16 KB. To check our build, I run the rest of `tests/test_editor.c` at our limits and replace that test with checks at the 8 KB edge: a file one byte short grows to exactly 8 KB, typing at 8 KB says "The file is full", 8,193 bytes opens read-only, a paste past 8 KB is refused, and only an undo step over 2 KB is lost. If the paste were sized from `TD_EDITOR_MAX` and `TD_EDITOR_UNDO`, that test would pass under a port's overrides as well. Our harness is [tools/tests/test_editor_limits.sh](https://github.com/aquasock/TinyTang/blob/4aba99a/tools/tests/test_editor_limits.sh) if it's useful.
