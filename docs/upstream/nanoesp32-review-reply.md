Published as a comment on
[tinydesk-project/tinydesk PR #15](https://github.com/tinydesk-project/tinydesk/pull/15),
replying to schikani's review. The user posted it after reviewing this text; the
posted comment is the markdown below the rule, with one difference — it kept the
bullet saying the UTF-8 BOM was removed, which the earlier draft for review had
dropped. That is accurate and the reviewer asked for it by name, so nothing
needs correcting.

The work it describes is on the fork branch `aquasock:nanoesp32-port` as commit
`0cd5fa6`, so the PR carries the change independently of the comment.

Everything below the rule is the reply as posted.

---

All of it is addressed in one added commit, `0cd5fa6`. Nothing in the original
port commit changed.

### The pin rules

You were right, and it is worse than a naming problem: `td_hw_pin_owner()`
decided from `chip` alone, so an ESP32-S3 reserved GPIO43/44 as "the desktop's
serial link" and left GPIO19/20 free. Here that is backwards — the desktop
runs on the USB Serial/JTAG peripheral, so `machine.Pin(19, Pin.OUT)` as root
severs the console mid-session, while D0/D1 (44/43) are free and were refused.

The port now defines `TD_LINK_USJ` project-wide, as you suggested, and the
rules treat an ESP32-S3 that defines it the way they already treat the C6:
19/20 are the link, and `td_hw_uart_kept()` stops calling UART0 the console.
Everything else is byte-for-byte the old behaviour — I reordered the branches
so the C6 case is tested first, and the ESP32 and plain-S3 answers are
unchanged and still asserted.

One trap worth recording, because it fails silently: setting the define with
`idf_build_set_property(COMPILE_DEFINITIONS TD_LINK_USJ APPEND)` after
`project()` compiles with no diagnostic and reaches no component. It has to be
`add_compile_definitions()` before `project()`. The absence of an error proves
nothing here; `compile_commands.json` is what showed it — 1,326 translation
units carry the define now, `td_py_hw_pins.c` among them.

`tests/test_py_pins.c` builds twice: `test_py_pins` as before, and
`test_py_pins_usj` with `TD_LINK_USJ TD_EXPECT_USJ`. The first version of that
test passed when I deleted the define, because without it the file simply
compiled its plain half and asserted the plain answers — so it now carries
`#error` guards and refuses to build a target whose define and expectations
disagree. Verified by compiling it by hand with `TD_EXPECT_USJ` alone, which
stops at the `#error`.

### The smaller ones

- `board.example.conf`: no longer three devices on GPIO21. `eth.cs` keeps 21
  (D10 is the bus's hardware SS, the natural select), `sd.cs` → 5 (D2),
  `rs485.2.de` → 4 (A3), with a note on why 21 is spoken for. No pin is
  assigned twice now.
- `README.md`: `read_flash` with the underscore, and `--before usb_reset`
  rather than `no_reset`, which the following section warns against. The
  generic `/fs` section is gone.
- `CONTRIBUTING.md` and `docs/RELEASE_CHECKLIST.md` know about the fifth port.
- Changelog line added, in a new `## Unreleased` section rather than into
  0.2.0, since `v0.2.0` is already tagged. Move it if you would rather it
  landed under the released heading.

`PROJECT_VER` in the port is still 0.2.0; I did not touch versioning, assuming
that is release-time.

### Verification

- `ctest`: 13/13 pass, including `test_py_pins_usj`.
- `idf.py fullclean && idf.py build` in `ports/nanoesp32`: rc=0, no errors, no
  warnings in the changed files. Firmware 2,096,192 bytes.
- `compile_commands.json` confirms the define reaches `td_py_hw_pins.c` in the
  real firmware build, not only in the host test.
- Merges cleanly onto current `main` (`fb07d1a`): tested, no conflicts.

I have not run CI on the merged result, only the branch itself.
