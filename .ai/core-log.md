## 1 COMMIT Unreleased 2026-10-10T14:20:43-07:00

#### Coming From:

Unreleased b0aea6f

#### Purpose:

Open the new active log with the live project state and the next build cycle's plan, after the user archived the previous log and cleared it.

#### Outcome:

The previous active log was archived and cleared on the user's direction, in commit `b0aea6f`, which added the archive and emptied `.ai/core-log.md` without an entry in the same commit; that log held 38 entries and its complete contents are preserved byte-identical in `.ai/archived_logs/core-log_2026-10-10T141714-0700.tar.gz`, a single-member `core-log.md` in the same gzip'd GNU-tar shape as the 2026-10-07 archive beside it, verified by extraction against both the working tree and the committed object before this entry was written. The archived log's last entry is the numbering correction the previous cycle made: the active log had run from 41 to 77 since the user-directed trim of 2026-10-07, the project's own `tools/check_core_log.py` rejected every entry, and the correction renumbered the headers to 1 to 37, carried 114 in-document citations with them, qualified five citations to sealed history as `archived entry 26`, `28`, `37`, `39` and `40`, and left 18 citations to the Tang-Phosphor and Tang-Build logs untouched after each was checked against those repositories' own logs, so the checker now passes. Its entries before that recorded the removal of the Sipeed schematics and the seven board photographs, and the PMOD socket-retry change in `ports/bl616/phosphor/pmod_sockets.cpp` that is committed but has never been built or flashed. The state this log opens on is otherwise unchanged and was verified this cycle: the repository is at `b0aea6f` and clean apart from a dirty `third_party/tinydesk` submodule, which is the carried patch `0001-settings-bluetooth-button.patch` applied in place by `scripts/apply-tinydesk-patches.sh` at every configure and is therefore a revert rather than unrecorded work; the board is attached in two-wire mode as `ffff:5454` on `/dev/ttyACM0`, so `/dev/ttyACM0` exists and the board is reachable with short single commands; and the toolchain the build needs is present, the Bouffalo SDK at `~/.cache/tangcore-dev/sdk`, `riscv64-unknown-elf-gcc` under `~/.cache/tangcore-dev/toolchain/bin`, and a nestang checkout at `../tangcore/nestang`. The desktop core runs with VGA and without HDMI, which is characterised and closed in the Tang-Build project and is not work to reopen here; an open-flow build carries no HDMI because the device database's clock model has no entry into any HCLK block, so no build here should be chosen in the expectation that one brings the display up. Nothing was built, deployed or tested, so all three statuses are not applicable, and the required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` is unchanged and that no settled entry was rewritten, and validated this entry as entry 1 of the new active log under the archival procedure, with six canonical sections, prose in Outcome and Next Steps, an allowed Status set and a count of one within the 100-entry limit.

#### Next Steps:

The next build cycle is the one this log opens on, and its work is the socket-register retry that has been committed since 2026-10-10 without ever running: build the BL616 firmware with `make CHIP=bl616 BOARD=bl616dk`, confirm against the last qualified image that the only source change is the retry in `ports/bl616/phosphor/pmod_sockets.cpp`, where `read_reg` and `write_reg` now attempt three times at 20 ms after a single missed read used to return `nullptr`, skip the socket declaration, send no `/tang.ini` and leave the screen dark with nothing printed; flash it with `tools/tinytang_flash.py`; have the user power-cycle; and check that `platform` prints the new build identity rather than the previous one. Acceptance is the part still open and it should be settled before the cycle starts rather than after: the retry fires only on a miss, so a clean first read passes with or without the fix, and the log's own wording asks that a socket declaration survive a first-miss read, which needs either a deliberate way to force the miss or an agreed bound such as a stated number of load cycles with no dark screen, because a result that could not have failed must not be recorded as a pass. The second item is the dirty `third_party/tinydesk` submodule, resolved by a `git checkout` inside it once the patch script is confirmed to reproduce the change at configure time. No other work in this repository is pending, and the entries that carry the detail behind all of this are in the archive named above, which `.ai/core.md` says is not to be inspected or cited without the user's approval, so anything needed from them should be asked for rather than assumed.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 2 COMMIT Unreleased 2026-10-10T15:07:00-07:00

#### Coming From:

Unreleased b0aea6f

#### Purpose:

Build the PMOD socket-declaration retry that previous cycles left as source only, deploy it, and confirm on hardware that a socket declaration survives a core load.

#### Outcome:

The retry is now built, deployed and accepted on hardware rather than recorded as source. The firmware was built with `make CHIP=bl616 BOARD=bl616dk`, giving identity `b0aea6f-dirty.945fd1e` in `build/build_out/tinytang_bl616.bin`, 655008 bytes, MD5 `d4773d1e57693b18b8e693731aa9058b`, with no warnings or errors in the build log; the firmware source difference since the previous binary's base commit `eb3fbe1` is the retry in `ports/bl616/phosphor/pmod_sockets.cpp` together with the About-window line the previous cycle already qualified and accepted, so the retry is the only change in this image that was not already accepted on hardware, and the build's `-dirty` hash reflects the uncommitted log entry this cycle writes. It was installed with `tools/tinytang_flash.py --yes`, which uploaded 655008 bytes to `/tinytang-upload.bin` and staged and committed it, and after the user's power cycle `platform` reported `b0aea6f-dirty.945fd1e` where the board had run `eb3fbe1-dirty.71886e5` before. The board returned from the loader as `ffff:5454` on `/dev/ttyACM0` exactly as FLS-001 describes, and the previous binary in `build/build_out` was overwritten by this build, so the pre-fix image is not retained as a file. The acceptance evidence is 14 core loads with the declaration checked on every one: eight `tangload` runs of `/cores/console138k/desktop.bin` and six of `/cores/console138k/phosphortang.bin`, the core the dark screen was reported on, and all 14 printed `tang.ini: pmod0 vga_j2, pmod1 vga_j1; word 0x0230` followed by `tang.ini: sockets declared (0xc0 = 0x0230)`. The line's absence is the old defect's signature, because `tang_ini_core_loaded()` returns silently when the ABI read at `0x04` misses, so a skipped declaration would have appeared as a load with no declaration line and no error, and none occurred. That is a bound and not a proof, and it is recorded as one: nothing in the firmware reports how often `read_reg` had to retry, so a recovered first-miss is indistinguishable from a load that never missed, and the instrumented build that would count retries was offered and declined in favour of accepting the 14-load bound. The user confirmed the screen and the PmodVGA output as correct, which is this cycle's hardware acceptance, and the board was left in its normal state with the desktop core loaded and `tangini` reporting `core 0x54 (ABI 1.1) has 0xc0 = 0x0230, as declared`. One process finding is recorded rather than fixed: `tools/tinytang_flash.py` blocks on its `install ... ? [y/N]` prompt when stdin is not a terminal, which hung the first flash attempt with `/dev/ttyACM0` held open until the process was killed and re-run with `--yes`. The required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` is unchanged and that no settled entry was rewritten, and validated this entry as number 2 of the new active log with six canonical sections, prose in Outcome and Next Steps, an allowed Status set and a count within the 100-entry limit.

#### Next Steps:

No engineering work remains on the retry. The one open item is the dirty `third_party/tinydesk` submodule, resolved by a `git checkout` inside it once `scripts/apply-tinydesk-patches.sh` is confirmed to reproduce the change at configure time, because the dirt is that patch applied in place and not unrecorded work. Two small follow-ups are named rather than done. `tools/tinytang_flash.py` should either pass `--yes` implicitly when stdin is not a terminal or say plainly in its usage that `--yes` is required without a TTY, since the prompt is invisible in an agent session and hangs while holding the console port. And if proof that the retry actually fires is ever wanted, the instrumented build that counts retries in `read_reg` and `write_reg` remains the way to get it, because the 14-load soak can only bound the miss rate rather than show a recovery. Nothing else in this repository is pending, and the detail behind all of it sits in the archived log, which is not to be inspected or cited without the user's approval.

#### Files Modified:

None.

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
