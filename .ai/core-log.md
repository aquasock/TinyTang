## 1 COMMIT Unreleased 2026-11-04T21:08:00-07:00

#### Coming From:

Unreleased 7dbaafc

#### Purpose:

Roll the active log over at ninety-eight entries so the next cycle starts on a clean buffer, archiving the completed log as `core-log_2026-11-04T210755-0700.tar.gz` and recording here the live state a successor inherits.

#### Outcome:

The log was archived to `.ai/archived_logs/core-log_2026-11-04T210755-0700.tar.gz` at 123,731 bytes and verified by extracting it again and comparing with the original byte for byte, so the archive is the file that existed rather than a re-serialisation of it. The rollover was directed by the operator at ninety-eight entries rather than at the hundred the ring buffer allows, which discards no history and simply leaves the two unused slots unused. The state a successor inherits is this. The board runs firmware 682,256 bytes at SHA-256 `a44fa98478accc875c792416cadf9fa834ae94a3e8b626f25497dc941ba486c2`, build `7c44fb3-dirty.1491160`, which is entry 98's image carrying the pad-toggle and overlay-sniff work and entry 96's extracted HID report routing; the desktop core is `00d8b48cac8e54b27cdbdacf2adfe8e67331bd3ff95c05e6fd671192f46582cf` at 4,526,774 bytes at seed 3, reproduced bit for bit from its own recorded tuple. `SOURCE_DATE_EPOCH` is baked into the `Makefile` at 1793750400, so the firmware's header stamp is constant and a clean build is reproducible, but the image digest is commit-scoped because the build id embeds the commit hash and a hash of uncommitted changes, so a release must name the firmware by its SHA-256 together with the submodule state; the dirty component hashes only the `Subproject commit ...-dirty` line and therefore cannot distinguish one `third_party/tinydesk` patch set from another. The host gate is 42 of 42, being 41 scripts under `tools/tests` plus `check_help_width.py`, and `tools/check_test_coverage.py` reports 28 of 57 port and Phosphor modules built by a host test with the remaining 29 listed in section 6.1 of `docs/qualification.md` together with the reason each is deferred. The release ship set is TinyNES, TinyGBC, Tiny89 and TinyPhosphor, with Bluetooth, Software Update and TinyNode as platform features; TinySNES is out of scope as a single core that reintegrates later, and TinyCreature, TinyGBA, TinyVCMI and TinyMaps are not shipping. The operator has decided the personality 8 flip-bit deviation is to be closed in the next core revision rather than waived, and that the final firmware and `tinytang.bin` will be rebuilt before release, so that fix belongs in that cycle along with flipping `tb_desktop_pmod`'s flip-bit assertion from "the deviation was fixed" to the refusal it should then expect. Work from a concurrent session landed in this repository and is committed as entries 96 and 98, which is worth knowing before assuming the tree is one person's. The DS3231's backup supply was settled on 2026-11-04: set, then a full power cycle with both inputs removed, left OSF clear and the time within the round trip of the host, so the earlier invalid state was an event and not a flat cell. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the `.ai/` diff and confirmed it is this new archive together with this cleared log, that `.ai/core.md` was not changed and no settled entry was rewritten since the archived file is unchanged and reproducible from the archive, and validated this entry as number 1 of 100 in the new buffer with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and `None.` in Files Modified.

#### Next Steps:

Phase 2's remaining silicon rows are the LoRa link end to end including its retry budget under a busy node, since `node status` failed all ten attempts twice before answering on the sixth with 375 bytes of the node's own Reticulum traffic in the FIFO at -16 and -19 dBm; the `mkdir -p` measurement, which an earlier handoff carries as an unverified claim that reading `mkdir_parents` and `_mkdir_r` could not reproduce; and the first multi-chunk pack read on hardware, which has never been done and which the English Wikipedia conversion is about to supply a subject for. The Wikipedia conversion is the operator's to start with `tools/convert_wikipedia_overnight.py start`, measured at about 320 entries a second so roughly seven and a quarter hours for the archive in the scratch directory, and it verifies that archive at its `checksumPos` before converting because about 1.01 GB of junk follows the trailer. Three Bluetooth behaviours became testable on the board once entry 96 landed and should be re-run: the reconnect after a disconnect (BLE-011), the `-EALREADY` on re-subscribing a bonded device (BLE-010), and the notify callback's NULL report (BLE-004). The Classic Controller failing intermittently after a game, recorded across the archived entries 86 to 88 with two candidates named and no instrumentation, remains the one unexplained defect, and entry 98's pad work may or may not have touched it. One release question is still unanswered: what a release firmware identity should be, given that the build id cannot distinguish patch sets.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 2 COMMIT Unreleased 2026-11-04T22:12:13-07:00

#### Coming From:

Unreleased 4d9ee7f

#### Purpose:

Give TinySNES a desktop launcher in the mould of TinyNES and TinyGBC, so a cartridge on the card can be browsed, launched and stopped from TinyDesk with the core's colour bars on screen while it streams, and fix the Classic Controller that was dead on the desktop after returning from a game.

#### Outcome:

The firmware gains `ports/bl616/tang_snes_load.c`, which does on the BL616 what TinySNES's `tools/load_snes_rom.py` does over `fpgapipe`: it skips a 512-byte copier header, scores the internal header at 0x7FC0 and 0xFFC0, refuses images over 8 MiB, a closed link or a link the pipe owns, then holds the core with `SET_LOAD 1`, sends the 64-byte header frame and the image in 1 KiB frames each under the transmit lock, waits 500 ms, releases with `SET_LOAD 0` and hides the overlay; the shell's new `snesload` and the new `ports/bl616/tinysnes_app.cpp` both call it, the app being a 54 by 20 window rooted at `/root/media/snes` that lists `.sfc` and `.smc` files and offers Launch, Up, Refresh and Stop, registered in `td_desktop_bl616.c` and `CMakeLists.txt`. The first board run launched `zelda.smc` correctly but showed the desktop again between the core load and the game, because the desk's re-arm after `tang_core_load` restores the recorded overlay state and that record still said shown; the loader therefore now hides the overlay through the OSD module before it holds the core and puts the prior state back on any failure after that point, and the app records the overlay hidden, without sending, before loading the SNES core and records it shown before every return to `tinytang.bin`, so the desktop stays up until the FPGA is reconfigured, the bars show through the stream, and Stop brings the desktop back visible. After Stop the 8BitDo receiver on the Wii adapter was dead on the desktop while the Bluetooth keyboard worked: `wiipad` reported the part unsupported, and `tangi2c read 52 fa 6` showed identity `00 00 a4 20 00 01` with 0xFE reading 0x00, a data format `tang_wii_identify` refused because it accepted a Classic only in format 1 or 3, so the driver never sent the format-3 write that would have repaired it; writing 0x03 to 0xFE by hand brought it straight back as a ready Classic, which confirmed the diagnosis, and `tang_wii_identify` now treats any format byte with model byte 0x01 as a Classic, as TinySNES's own `wii_pad.sv` already does, while still telling a nunchuck by `00 00` and refusing a guitar by `03`. This is a likely cause of the intermittent Classic failure after a game that entry 1 carried as the one unexplained defect, though what writes 0x00 to 0xFE, the receiver itself or the reconfiguration, was not established. `tools/tests/tb_snes_load.c` with `test_snes_load.sh` covers the header scoring, the refusals, the frame sequence and the overlay order, `tb_wiipad.c` gains the format-0 identity and its adoption, `test_tang_fpga_uart.sh` links the loader, and planted faults were caught in every case, 18 in the loader, 9 in its overlay handling and 4 in the identity; the gate is 43 of 43, and `docs/qualification.md` now carries that count, the TinySNES launcher among the deferred app wrappers and the coverage tool's 30 of 59 uncovered modules. The firmware builds clean in the changed files at 686,496 bytes, SHA-256 `af4ea73d7182077508d93d0e3dff9fad8deac0b6a80967a725d4013f337572f8`, build `4d9ee7f-dirty.7353cba`, flashed with `tangflash` and confirmed by `platform` after the power cycle, with TinySNES's desk-overlay bitstream at `/root/cores/console138k/tinysnes.bin`; the operator reported the launch with colour bars, HOME over the game, Stop back to the desktop and the controller working afterwards as a pass. `third_party/tinydesk` carries uncommitted changes that are not this cycle's and are left out of the commit. The core-syntax audit re-read `.ai/core-syntax.md`, confirmed that `.ai/core.md` and the archive are unchanged and that the only `.ai/` change is this entry, and validated it as number 2 of 100 with exactly six sections, one-sentence Purpose, prose in Outcome and Next Steps, repository paths in Files Modified and allowed Status values.

#### Next Steps:

TinySNES's launcher still lacks `.zip` cartridges, battery saves and the Input, Video and Audio settings pages TinyNES and TinyGBC have, and PAL is deferred; which of these comes next is the operator's call. The receiver's drop to format 0 should be watched for on TinyNES and TinyGBC exits too, and if it is ever seen with this firmware the source of the 0x00 write is the thing to find. Entry 1's remaining work stands: LoRa end to end, the `mkdir -p` measurement, the first multi-chunk pack read, the three Bluetooth re-runs and the release firmware identity question.

#### Files Modified:

- CMakeLists.txt
- docs/qualification.md
- ports/bl616/tang_fpga_uart.c
- ports/bl616/tang_snes_load.c
- ports/bl616/tang_snes_load.h
- ports/bl616/tang_wiipad.c
- ports/bl616/td_desktop_bl616.c
- ports/bl616/tinysnes_app.cpp
- tools/tests/tb_snes_load.c
- tools/tests/tb_wiipad.c
- tools/tests/test_snes_load.sh
- tools/tests/test_tang_fpga_uart.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 3 COMMIT Unreleased 2026-11-05T14:31:00-07:00

#### Coming From:

Unreleased 83d6058

#### Purpose:

Record the English Wikipedia conversion that ran overnight, the pack it produced, and the one converter defect the finished pack exposed.

#### Outcome:

The conversion finished on its own, in 7h25m from 20:43:04 to 04:08:54, against the 7h15m its own estimate predicted at about 320 entries a second, and `check_wiki_pack.py` re-run by hand afterwards reported 8,425,137 records with 16,851 bodies sampled across 11 chunks and no failure. The pack is 32.93 GiB in 14 files: `wiki.000` to `wiki.010` at 3.00 GiB each with the last at 2.93 GiB, `wiki.idx` at 674,010,960 bytes, `wiki.parts` holding 1,097 parts and `wiki.meta` naming the source `wikipedia_en_all_nopic_2026-06.zim`, its date and CC BY-SA 4.0. There is no `wiki.auth` because an articles pack has no authors, which is correct rather than missing. Two properties were checked that the automated pass does not state outright. Every chunk's last record ends exactly at the file size, so the gap at the end of each chunk is zero bytes and the writer cut only on an article boundary, which is KIWI-004's rule and the reason a body can never straddle two files; the final article in chunk 0 closes with Wikipedia's licence footer rather than mid-word. And the reader imposes no chunk cap at all — `td_wiki_app.cpp` opens `wiki.%03u` from the record's own chunk field, and `TD_PACK_MAX` bounds how many packs a folder holds, not how many chunks a pack has — so eleven chunks is not a limit case. Two articles were read out to confirm the text is real: Arizona at 91,031 bytes and 1,713 lines, and a 631-byte stub, both correctly rendered. The finished pack also exposed a real converter defect, found by noticing an article titled `null` whose body was a MathML namespace notice. The archive keeps its own machinery beside the articles under URL prefixes beginning with an underscore, and exactly one of them — `_mw_/MathML` — carries a `text/html` mimetype and the literal string `null` as its ZIM title, so it passed the namespace, mimetype and length filters and became an encyclopedia entry called `null`. Measured over the whole archive there are 8,516,992 namespace-C text/html entries and exactly one with an underscore-prefixed URL; the 515,748 other `null`-titled entries are SVG assets and were already excluded by mimetype. `tools/make_wiki_pack.py` now skips underscore-prefixed URLs and treats a `null` title as absent, and `tools/tests/check_wiki_convert.py` builds that exact entry plus one under `_res_` and fails if either reaches the pack, which was proven by reverting the filter and watching four checks fail before restoring it, taking that suite from 57 to 60 checks. The built pack was deliberately not rebuilt for that: seven and a quarter hours to remove one bogus article from 8.4 million is not worth it, and the pack's structure is unaffected, but the operator can rebuild if a clean index matters more than the time. The pack is **not on the card** — no SD card is mounted on this machine — so the multi-chunk read on hardware that has never been done is still not done, and remains Phase 2's item. `docs/qualification.md` gained section 12 recording all of the above so the numbers survive the log rollover. The host gate is 43 of 43. The board was not touched: it runs entry 2's image at 686,496 bytes, SHA-256 `af4ea73d7182077508d93d0e3dff9fad8deac0b6a80967a725d4013f337572f8`, build `4d9ee7f-dirty.7353cba`, which supersedes the firmware entry 1 recorded. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff and confirmed it touches only this log, that `.ai/core.md` was not changed and no settled entry was rewritten, and validated this entry as number 3 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The pack is complete and verified but waiting on a card: mounting it and copying `wikipedia-en-all` to `/root/wiki` is the next step, and reading one article that crosses a chunk boundary is the hardware test this project has never run — `Zebra` in chunk 7 and `Arizona` in chunk 5 are known record positions, and an article in `wiki.010` exercises the last file. The filter fix means a rebuilt pack would have 8,425,136 records instead of 8,425,137; that is a choice for the operator, not a defect. Entry 2 left its own work: `.zip` cartridges and the battery-save and settings pages TinySNES's launcher still lacks, and watching for the receiver's drop to format 0 on TinyNES and TinyGBC exits. Entry 1's remaining items stand unchanged — the LoRa link end to end with its retry budget under a busy node, the `mkdir -p` measurement, the three Bluetooth re-runs that entry 96 unblocked, and the release firmware identity question that the build id cannot answer.

#### Files Modified:

- docs/qualification.md
- tools/make_wiki_pack.py
- tools/tests/check_wiki_convert.py

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 4 COMMIT Unreleased 2026-11-05T18:27:57-07:00

#### Coming From:

Unreleased 0d13893

#### Purpose:

Put the English Wikipedia pack on the card, settle what the four BL616 firmware images on it were and reduce them to the one the Software Update window installs, and record the first multi-chunk pack read on hardware.

#### Outcome:

The card carried four BL616 firmware images under three names and the operator's rule is one file with one name, so they were reduced to two roles. `bl616-firmware.bin` at the root is the only image the Software Update window installs — `td_update_app.c` hardcodes `/sd/bl616-firmware.bin` as its single path — and it held build `6c20dcf-dirty.5c5154b` from 2026-10-23, twelve days and many commits behind what the board runs, so installing it would have been a silent downgrade past the LoRa transport, the MPR121 driver and the Classic Controller fix. It was replaced with the running image, 686,496 bytes at SHA-256 `af4ea73d7182077508d93d0e3dff9fad8deac0b6a80967a725d4013f337572f8`. Two stale copies under `/root/` — `tinytang_bl616.bin` from `dda50fe` on 2026-10-28 and `tinytang-bl616.bin` from `dfc7bdc` on 2026-10-27 — were removed; nothing in the tree references either name, and both were copied to `tinytang-scratch/firmware-backup/` first. `tinytang-upload.bin` was deliberately kept: it is `REMOTE_PATH` in `tools/tinytang_flash.py`, a separate name so a development flash cannot clobber the image the GUI installs, and the operator confirmed that separation is intentional. The rule the operator stated is no revision control on purpose, and the app showing only a byte count rather than a build id is likewise by design, so neither was changed. The pack was then copied to `/root/wiki/wikipedia`: 14 files, 34 GB, every one compared with `cmp` against its source and byte-identical, and `check_wiki_pack.py` run against the card copy reported 8,425,137 records with 4,213 bodies sampled across 11 chunks and OK. Three copy attempts were needed because `cp` died with the shell running it twice, leaving `wiki.000` then `wiki.002` as the last complete file; the third ran detached under `setsid nohup` and finished, and every file was verified by content rather than by exit status. The user then read the whole of **Arizona** on the board, which closes the multi-chunk gap carried since the pack format was designed. That article is a stronger test than its size suggests: `read_page()` in `td_wiki_app.cpp` opens the chunk file afresh on every page turn — `f_open("wiki.%03u")` then `f_lseek` to `record->offset + cursor` — and Arizona is 91,031 bytes against a 12,288-byte page, so reading it end to end is eight separate open-seek-read cycles into `wiki.005` from offset 1,009,142,055. That establishes that the record's chunk field routes to the right file, that seeking about a gigabyte into a 3 GiB file works on FAT32 through the port's syscall layer, that paging across eight pages works with each turn a fresh seek, and that the body was not truncated. Two boundaries that read did not cross are named rather than glossed: the last chunk `wiki.010`, which is 2.93 GiB rather than 3.00 so its offset space ends differently, and any offset above 2 GiB, since Arizona sits near 1.0 GB while each 3 GiB chunk has about a third of its space above `0x80000000` where a 32-bit signed offset would wrap. `A & E Home Video` in chunk 10 at offset 311,802,683 and `A battuta` in chunk 9 at offset 2,500,311,686 are recorded as the targets for those, so the honest claim is that multi-chunk reading is proven for chunks 0 to 9 below 2 GiB rather than for the whole pack. Separately, `/root/gutenberg.pos` was examined: it holds the shelf, the book's record number and the page, and `position_save()` writes it on leaving a book and on closing the window, but nothing reads it — `POS_REAL` appears only at its definition and one `f_open(..., FA_WRITE | FA_CREATE_ALWAYS)`, `git log -S'POS_REAL, FA_READ'` returns nothing, and `launch()` goes straight to `show_library()`. Both `manual/gutenberg.md` and `README.md` claim the app resumes where you stopped, which it does not, and that discrepancy is recorded rather than fixed because restoring the position is a firmware feature and firmware work is held. The host gate is 43 of 43 and the board was not modified. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff and confirmed it touches only this log, that `.ai/core.md` was not changed and no settled entry was rewritten, and validated this entry as number 4 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

Two boundaries remain untried and are cheap to settle whenever the card is back in the board: open `A & E Home Video` to exercise the last chunk file, and `A battuta` to cross the 2 GiB offset mark, which would move the multi-chunk claim from chunks 0 to 9 below 2 GiB to the whole pack. TinyGutenberg's reading position is written and never read, and both the manual and the README claim otherwise; implementing the restore is about thirty lines beside the existing save and belongs in the release firmware cycle, alongside the personality 8 flip-bit fix the operator has decided to close there. Entry 1's remaining items stand: the LoRa link end to end with its retry budget under a busy node, the `mkdir -p` measurement, the three Bluetooth re-runs that entry 96 unblocked, and the release firmware identity question the build id cannot answer. Entry 2's TinySNES launcher still lacks `.zip` cartridges and the battery-save and settings pages.

#### Files Modified:

- docs/qualification.md

#### Status:

- Build: N/A
- Deployment: PASS
- User Test: PASS

---

## 5 COMMIT Unreleased 2026-11-05T20:10:06-07:00

#### Coming From:

Unreleased 871f099

#### Purpose:

Make the board come up in TinyDesk rather than at a shell prompt, by the operator's decision, and record why the desktop has to be started from the shell task rather than from the boot script.

#### Outcome:

The board now brings TinyDesk up after its boot script returns, unconditionally, which is the operator's choice: the first-boot advice a new card needs is moving into the desktop itself, so there is no longer a reason to stop at a prompt to deliver it. The call is `tdsh_execute_line(&s_session, "desktop")` placed in `tdsh_bl616_run_until()` after `tang_ble_boot()` and before the readline loop, so the desktop starts in the shell task and by exactly the path a typed `desktop` takes — which is the whole design constraint. It could not be done by adding `desktop` to `boot.tdsh`: a script runs in a worker whose stack is capped at `BL616_WORKER_STACK_MAX` of 16 KB, the interpreter's frames would sit under the desktop inside that same 16 KB, and 16 KB is the budget the desktop alone consumes in the shell task, so the console build would have overflowed the stack into the crash recorder and reset the board. `tang_ble_boot()` is called first because it returns as soon as it has created its task, so the radio finishes starting while the desktop comes up; ordering it after would leave the desktop with no keyboard. Exit from the start menu still returns to the prompt, because `cmd_desktop` returns after `td_run()` and the readline loop is immediately below it. The escape hatch is kept as a file rather than a reflash, preserving the property `boot.tdsh` has always had: an empty `/root/scripts/no-desktop` leaves the prompt in place, which is the state a card needs while it is being brought up or before a keyboard is paired. That decision lives in a new `ports/bl616/tang_boot_mode.c` and its header rather than inline in the platform file, because the platform file needs the shell, the console, the radio and the card to link and so cannot be host-tested at all, while the decision on its own can. `tools/tests/tb_boot_mode.c` covers it: no marker means the desktop, an empty marker means the prompt, a marker with text in it behaves the same because the file's existence is never read, a path in a missing directory is absent rather than an error, and a NULL or empty path still means the desktop so a caller cannot accidentally hold the board at a prompt. Two planted faults were caught, one making it always choose the desktop and one always the prompt. One claim in the first draft of that test was wrong and was corrected rather than worked around: it asserted that a directory named `no-desktop` is not a marker, which is true on the board's FatFS where `f_open` returns FR_DENIED for a directory, and false under glibc where `fopen` on a directory succeeds — so the two platforms genuinely differ and the assertion was pinning a host accident rather than the firmware's promise. The module's comment and header now say that plainly, and the test asserts only the pair that holds on both. `tang_ble.c` also needed one fix that this build exposed: entry 96's static assertions compared two different anonymous enums, which `-Wenum-compare` flags, so they now cast through `int` and the build is warning-free apart from the vendored JTAG programmer. `scripts/boot.tdsh` was corrected to match: its comments said the board stops at a prompt and that starting the desktop from there would take the console before the user asked, which is no longer what happens, and its closing line now reads "core loaded, starting TinyDesk" while the Bluetooth pairing advice is described as the last thing written to the tap rather than a stopping point. `README.md` and `manual/getting-started.md` were updated for the same reason — both told the user to type `desktop` to get the desktop, and the first says so in the Quick start — and both now describe the desktop as what comes up, the prompt as what an Exit or a `no-desktop` marker gives, and pairing as something the desktop's Bluetooth window does. The firmware builds at 690,624 bytes; it was **not deployed**, at the operator's instruction, so the board still runs entry 2's image and nothing on the card changed. The host gate is 44 of 44. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff and confirmed it touches only this log, that `.ai/core.md` was not changed and no settled entry was rewritten, and validated this entry as number 5 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The change is built but unproven: when the operator frees the board, flashing this image and confirming that a normal card reaches TinyDesk, that Exit returns to the prompt, and that an empty `/root/scripts/no-desktop` on the card stops at the prompt is the test it needs. It belongs in the release firmware cycle with the personality 8 flip-bit fix and the TinyGutenberg reading-position restore, all three being small and all three already on that list. The other agents' work in this repository continues independently and was not touched. The rest of the open list is unchanged: the two untried pack boundaries (`A & E Home Video` in chunk 10 and `A battuta` above 2 GiB), the LoRa link end to end with its retry budget under a busy node, the `mkdir -p` measurement, the three Bluetooth re-runs, and the release firmware identity question the build id cannot answer.

#### Files Modified:

- README.md
- manual/getting-started.md
- ports/bl616/tang_ble.c
- ports/bl616/tang_boot_mode.c
- ports/bl616/tang_boot_mode.h
- ports/bl616/tdsh_platform_bl616.c
- scripts/boot.tdsh
- tools/tests/tb_boot_mode.c
- tools/tests/test_boot_mode.sh

#### Status:

- Build: PASS
- Deployment: N/A
- User Test: N/A

---

## 6 COMMIT Unreleased 2026-11-05T20:23:47-07:00

#### Coming From:

Unreleased 4c3a2bc

#### Purpose:

Revert the desktop-first boot at the operator's change of mind, after the build had been deployed and the board shown to reach the prompt.

#### Outcome:

Entry 5's change was deployed and worked: the operator power-cycled and `platform` reported `4c3a2bc-dirty.f646e08`, and the operator then decided against it, because the Bluetooth setup and first-boot work are moving into the GUI where the advice belongs. The commit was reverted with `git revert`, which restores the platform file to `run_boot_script()` then `tang_ble_boot()` then the readline loop and removes `ports/bl616/tang_boot_mode.{c,h}`, `tools/tests/tb_boot_mode.c`, `tools/tests/test_boot_mode.sh` and the marker-file escape hatch, and puts `README.md`, `manual/getting-started.md` and `scripts/boot.tdsh` back to describing a console boot where the desktop is a command the user types. One part of the revert was deliberately not kept: entry 5 also carried the `-Wenum-compare` fix to the three static assertions entry 96 added in `tang_ble.c`, and reverting that would have put the warnings back, so the cast through `int` was restored by hand; the fix is unrelated to boot behaviour and outlives the feature it was found beside. The revert also erased entry 5 from `.ai/core-log.md`, since the commit's diff included adding it and `git revert` removes what a commit added; that is wrong for a log, because the cycle did happen and the record of it is not undone by the code being undone, so the log was restored from the reverted commit and this entry records the reversal instead. That is worth carrying as a rule: a revert of a cycle must not revert that cycle's own entry. The reverted firmware was rebuilt from the reverted tree at 690,624 bytes, SHA-256 `15f8548203412d79db11f3e66ec61274fc67d828f6a69a3133bdf4f7a45aeeeb`, build `4c3a2bc-dirty.d82a2f5` -- the same commit id with a different tree hash, which is the build-id scheme doing exactly what it is designed to do -- and deployed with `tools/tinytang_flash.py`; after the operator's power cycle `platform` reported that build and `tangini` reported `layout lora` with `core 0x54 (ABI 1.6) has 0xc0 = 0x0080, as declared`, so the card path and the core load are unaffected. The host gate is 43 of 43, one lower than entry 5's 44 because the boot-mode suite left with the feature it covered. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff and confirmed it adds this entry and restores entry 5 without rewriting it, that `.ai/core.md` was not changed, and validated this entry as number 6 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The board is back on console boot, and the desktop-first idea returns only if the GUI work it was waiting on makes it unnecessary -- the operator's reasoning was that the first-boot advice belongs in the desktop rather than in a prompt, which makes the automatic start pointless rather than wrong. The rest of the open list is unchanged: the two untried pack boundaries (`A & E Home Video` in chunk 10 and `A battuta` above 2 GiB), the LoRa link end to end with its retry budget under a busy node, the `mkdir -p` measurement, the three Bluetooth re-runs, the TinyGutenberg reading-position restore, the personality 8 flip-bit fix, and the release firmware identity question the build id cannot answer.

#### Files Modified:

- README.md
- manual/getting-started.md
- ports/bl616/tang_ble.c
- ports/bl616/tang_boot_mode.c
- ports/bl616/tang_boot_mode.h
- ports/bl616/tdsh_platform_bl616.c
- scripts/boot.tdsh
- tools/tests/tb_boot_mode.c
- tools/tests/test_boot_mode.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 7 COMMIT Unreleased 2026-11-05T20:32:41-07:00

#### Coming From:

Unreleased 871f099

#### Purpose:

Give TinySNES the battery saves it has never had, by storing the cartridge SRAM to the card on a HOME plus SELECT exit and putting it back when the cartridge loads.

#### Outcome:

A SNES game now keeps its save across a power cycle, which it could not do before: the mechanic worked inside a session and nothing ever wrote the SRAM out. The core half lives in the TinySNES repository as this cycle's entry 17 and is the reason this entry cannot be read on its own -- `iosys_bl616.v` gained frame 0x18 to store a run of bytes into the cartridge array from a 24-bit address and frame 0x19 to read one back, both refused unless the core is held in reset, and `snestang_top.v` muxes the 128 KiB BSRAM port to that window while `loading` is set, so the loader and the game never drive the array at once and no block RAM was added to a design already at 340 of 340. Here, `tang_snes_load.c` gained the whole firmware half. `tang_snes_save_size()` reads the battery out of the internal header: chip nibble 2, 5, 6, 9 or A means the cartridge has one, the size is 1 KiB shifted by byte 0x18, a SuperFX cartridge that leaves 0x18 zero takes the expansion-RAM byte three bytes before the header instead, and the result is capped at the core's 128 KiB window. Checked against the real cartridges in the operator's library the rule gives Zelda 8 KiB, Mario Kart 2 KiB, Donkey Kong Country 2 KiB, Chrono Trigger 8 KiB, Super Metroid 8 KiB, F-Zero 2 KiB and Yoshi's Island 32 KiB through the expansion byte, which are the sizes those cartridges are documented to carry. `tang_snes_save_path()` names the file `/sd/root/saves/snes/<stem>.sav`, the convention TinyNES and TinyGBC already use. `tang_snes_save_write()` holds the core in reset, reads the array out in 250-byte blocks with a per-block CRC-16, writes them to a `.part` file, reads the whole file back and checks its CRC-32 against what was sent, and only then unlinks the previous save and renames the new one into place -- so a failed save leaves the old one untouched, and the caller is told which stage failed rather than being told the save worked. `tang_snes_save_restore()` runs inside a load, after the ROM has drained and before reset is released, which is the only window the core accepts it; a missing file returns a distinct code because a first run is not a failure, and a file shorter than the cartridge's SRAM is padded with 0xFF rather than left holding the previous cartridge's bytes. `tang_snes_save_for()` sizes a cartridge from its file on the way out, so the exit costs one card read instead of holding the header for a whole session. `tinysnes_app.cpp` loses its Stop button and `stop_task` entirely: a game now ends by the pad, and `play_task` polls `tang_fpga_joypad` every 50 ms for `TANG_SNES_PAD_EXIT`, which is bit 15 of the pad word, set by the core while HOME and SELECT are both held. The task that watches is also the task that saves, because the watch is a loop and the exit is what ends it, and a second task would have bought a race and another 8 KiB of stack. A launch is refused while a game plays, since two launches would give the exit gesture two watchers. The firmware builds at 690,624 bytes, SHA-256 `da5633b7b3b86c355e5343f561a1ac02b85ddce41247e8d4ccf917e7bb0337b0`, identity `871f099-dirty.f646e08`, and was installed by `tools/tinytang_flash.py` and run after a power cycle; `tang` then reported exactly that identity, which is the check that it was the new image and not the old one. The host gate is 43 of 43, and `tools/tests/tb_snes_load.c` grew from five cases to eleven: the sizing rule including the SuperFX expansion byte and the cap, the path naming, a restore that sends its blocks between the drain and the release in order with the right addresses and an 0xFF pad on a short save, a cartridge with no battery sending nothing, a readout that writes, verifies and renames, four refusals that must each leave the previous save in place, and the resume that gives a held game back. Two defects were found and both were mine. The first was in the core, and it is the reason this cycle took three builds: `sav_addr` was assigned in two `always` blocks, which yosys does not reject -- it keeps one assignment and drops the other -- so on the board the read address never advanced and every 250-byte block of a save read back as one byte repeated. The symptom was carefully misleading: the reply length was right, the CRC was right, the bench passed, and the host test passed, because all three check framing rather than the one thing that was broken. The operator's own test is what exposed it, and it exposed it in a way worth recording: the saves were written with correct names and correct sizes, 8,192 bytes for Zelda and 32,768 for Eye of the Beholder, so the firmware half had worked and only the content was garbage. The second was in this cycle's own bench, which derived its expected reply length from the frame header instead of from the request, so a wrong header agreed with a wrong expectation; it now checks `frame_len` against what was asked for and fails on the old sender. The operator confirmed the result on the board: a save written from the desktop survived a reboot of the Tang. Read back off the card afterwards, the file is 8,192 bytes with twelve distinct byte values and real data at the offsets where Zelda's three save slots live, against the one repeated byte the broken build wrote. The desktop-first boot that another agent committed as entry 5 and reverted as entry 6 is not part of this image; the operator decided against it and the board runs this firmware, which is why the tree base above is 871f099. The build identity is worth one note: the second field, `f646e08`, is a hash of the working-tree diff, and a rebuild after the revert produced the same field and the same 690,624 bytes, differing only in the commit prefix, so the source that is committed reproduces the image that was tested. `third_party/tinydesk` has uncommitted changes that are not this project's and were left out of the commit. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed it touches only this log, confirmed `.ai/core.md` was not changed and no settled entry was rewritten, and validated this entry as number 9 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The SuperFX save size is the one part of the sizing rule that has not met a real cartridge: Yoshi's Island is predicted at 32 KiB through the expansion byte at 0x7FBD, and while the rule is right on the seven cartridges that were checked, none of those goes down that path, so a save on Yoshi's Island is the test it needs and it belongs with the next SuperFX cycle rather than on its own. Two saves written by the broken core are still on the card, renamed `Zelda-bad.sav` and `ADnD-bad.sav` so nothing loads them, and they can be deleted whenever the operator wants the space. The same trap that caught this cycle is worth carrying into the next one: a protocol test that checks framing -- length, CRC, byte count -- cannot see a data path that is internally consistent but wrong, and the evidence for this defect was already sitting in the build's synthesis log as 34 `multiple conflicting drivers for iosys.sav_addr` lines, one per bit, unread. That log is now a gate in the TinySNES repository, `TINYSNES_STAGE=lint`, which fails on any multiple driver that is not a known function local, so the class of fault is caught before a bitstream exists rather than after the operator has tested it. Nothing else in this repository was touched: the release firmware identity question the build id cannot answer, the pack boundaries, the LoRa link and the Bluetooth re-runs are all carried unchanged, and the reverted desktop-first boot is settled as against.

#### Files Modified:

- ports/bl616/tang_snes_load.c
- ports/bl616/tang_snes_load.h
- ports/bl616/tinysnes_app.cpp
- tools/tests/tb_snes_load.c
- tools/tests/test_snes_load.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 8 COMMIT Unreleased 2026-11-05T21:46:12-07:00

#### Coming From:

Unreleased 9ad6f09

#### Purpose:

Give the TinySNES window the five tabs TinyNES and TinyGBC carry, with the Saves page built the way theirs are, and stamp the save path into the log its Saves page reads.

#### Outcome:

The window is no longer a single Library page. It now has `Library`, `Input`, `Video`, `Audio` and `Saves`, in that order and with the tab row spaced by caption so "Library" does not run into the next label, which is how the other two apps lay theirs out. The page machinery is theirs too: per-page widget arrays, a `show_tab` that hides every other page and accents the current tab, and one status label per page because a shared widget takes the visibility of whichever page is shown last. Library is unchanged. Saves is built. Input, Video and Audio are placeholders by the operator's instruction, and they are honest ones: each carries a list of three rows rendered in the theme's dim colour under the heading "Not built yet: nothing here is set.", and the rows say the same in words — "Controller mapping: not built yet", "Aspect ratio: not set", "Volume: not set". Plausible-looking dummy values were rejected for the reason the TinyNES pages were built rather than faked: a row reading "Aspect: 4:3" would be read as a setting the core had taken, and nothing on those three pages is read by anything. The Saves page is the battery save of the cartridge selected in the Library and nothing else, because TinySNES has no save states and the operator deferred them: it shows the cartridge's name, `<stem>.sav` under `/root/saves/snes`, the file's size against the size the cartridge's own header asks for with a line saying so when they differ, whether the cartridge has a battery at all when its header says it does not, and when the save was last written. That date cannot come from the file, because FatFS gives a file no clock on this board, so it comes from `/sd/root/saves/last.log`, the same record TinyNES's and TinyGBC's pages read through `td_saves_log.c` — and nothing in the SNES save path was writing to it. `tang_snes_save_write()` therefore now leaves the two lines that parser expects, an `arm` before the readout and a `finish` after it, both stamped by `tang_ae350_note` with the RTC-kept clock, and a save that stops early logs `failed`, `unverified` or `unplaced` rather than vanishing: the arm line names the destination and the byte count, and the finish line names what arrived against what was wanted and the two CRC-32s. The parser itself is untouched, which is the point of it living in one module. `tools/tests/tb_snes_load.c` now asserts those lines — two notes on a good save, arm then finish naming the file and reading `verified`, and a refusal logged as `failed` — with a stub for `tang_ae350_note` that records them. The firmware builds at 694,752 bytes, SHA-256 `83f6965c473cf8b769a19c9fa8a8602733d2bfe9f9125b42cc5121e273191eeb`, identity `9ad6f09-dirty.385055a`, and was installed by `tools/tinytang_flash.py` and run after a power cycle, which `tang` confirmed by reporting that identity. The host gate is 43 of 43. The operator confirmed the window on the board: the five tabs are there, and after saving in Zelda the Saves page showed the new date, which is the part that could not work before the log lines existed. Read back afterwards, the save log holds exactly one arm-and-finish pair for that cartridge, both stamped `2026-11-05T21:36:53-07:00` and both `verified`, and the file on the card is 8,192 bytes with fifteen distinct byte values whose CRC-32 is `b19aa99e`, the same number the finish line recorded at write time — so the firmware's verify and the card's contents agree independently of each other. The core was not rebuilt this cycle; the TinySNES repository's entry 18 covers the SuperFX save size this entry would otherwise have carried forward as unverified. `third_party/tinydesk` has uncommitted changes that are not this project's and were left out of the commit. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed it touches only this log, confirmed `.ai/core.md` was not changed and no settled entry was rewritten, and validated this entry as number 8 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The three placeholder pages are the obvious next cycle and each needs a decision rather than only code: Input would take the pad mapping the Wii Classic Controller already delivers, Video would take the aspect and scanline settings the HDMI path can act on, and Audio would take a master volume the S-DSP mixer could apply, but which of those the core can honour without a rebuild is not yet known and should be established before any of them is drawn as a control that works. The Saves page could grow a way to delete a save, which the two files left by the broken core make less academic than it sounds, but nothing needs it yet. Save states stay deferred by the operator. One limit of the Saves page is worth carrying: it reads the log only when the page is shown, so a save written while the window is open on another tab appears when Saves is next opened rather than immediately, which is deliberate because the log read costs card time and the tick runs on the desktop's own task. Nothing else in this repository was touched: the release firmware identity question, the pack boundaries, the LoRa link and the Bluetooth re-runs are carried unchanged.

#### Files Modified:

- ports/bl616/tang_snes_load.c
- ports/bl616/tinysnes_app.cpp
- tools/tests/tb_snes_load.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 9 COMMIT Unreleased 2026-11-05T22:08:57-07:00

#### Coming From:

Unreleased 06becaa

#### Purpose:

Open TinyWiki in a maximised, non-movable window and narrow both the pack's wrap width and the reader's clip width to 70 columns, as an experiment in reading comfort, after finding that the two widths had been disagreeing at 78 and 76.

#### Outcome:

The wiki window is created with `TD_WIN_MAXIMIZED | TD_WIN_CLOSABLE | TD_WIN_RESIZABLE` and without `MOVABLE`, so it opens filling the screen and cannot be dragged. Dropping `RESIZABLE` instead would have been the obvious move and is wrong: `td_win_toggle_maximize()` in `third_party/tinydesk/src/wm.c` returns immediately for a window without that flag, so the window would have opened at its default rect rather than maximised. Keeping `RESIZABLE` and setting `MAXIMIZED` gets the lock the window manager already implements — drag-move refused, the resize grip neither drawn nor accepted — while leaving double-click and F11 able to restore it, which is treated as a feature rather than a leak. The width change was prompted by the experiment and turned out to fix a defect that predated it. The converter wrapped a pack to `WIKI_LINE_COLS` 78 while the reader clipped every line to `WINDOW_COLS` 76, and the reader clips rather than re-wrapping, so any line between 76 and 78 characters lost its tail with nothing to show for it: the manual pack as it stood had 23 such lines, the longest exactly 78. Both are now 70, chosen so text does not run to the window's edge, and the rebuilt manual pack measures a longest line of exactly 70 with none over. The two constants are now bound together by the test rather than by a comment: `tools/tests/check_wiki_convert.py` reads `WIKI_LINE_COLS` from the converter's own source and `WINDOW_COLS` from `td_wiki_app.cpp`, asserts they are equal, and names the consequence when they are not; drifting the reader back to 76 was tried and produced `FAIL the converter wraps at 70 and the reader clips at 76; they must be long lines lose their tail on screen`, which is the check working. That suite grew from 57 checks to 62. Only TinyWiki was changed, at the operator's direction: `td_gutenberg_app.cpp` still clips at 76, so a book and an article wrap differently in the same window, which is a known inconsistency recorded here rather than fixed. The firmware built at 694,816 bytes, SHA-256 `beb17fc11d1ace2dcba1cc3cfa7930dc528ce66ea9cb3bbdbbb6a8c938d29551`, build `06becaa-dirty.71f0034`, and was deployed with `tools/tinytang_flash.py`; after the operator's power cycle `platform` reported that build and the operator reports the result passing, so a maximised wiki at 70 columns reads better than the previous arrangement. The 70-column manual pack was built to `/tmp/manual70` and passed `check_wiki_pack.py` with 10 records in one chunk, but it is **not on the card**: no SD card is mounted on this machine, so the board is still reading the 78-column manual built earlier and will show clipped lines until the pack is rebuilt onto the card. The host gate reports 43 passing and one failing, and the failure is not this cycle's: `test_tang_fpga_uart` links `ports/bl616/tang_snes_load.c`, which a concurrent session is midway through editing, and reports `undefined reference to 'tang_snes_settings_load'`; stashing this cycle's three files and re-running reproduced it identically, so it predates this work and belongs to that session. Their files were left untouched and nothing of theirs was staged. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff and confirmed it touches only this log, that `.ai/core.md` was not changed and no settled entry was rewritten, and validated this entry as number 9 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The manual pack has to be rebuilt onto the card, because the board is running a reader that clips at 70 against a pack wrapped at 78 and will cut the tail off any line between those widths: with the card mounted, `python3 tools/make_wiki_pack.py --markdown manual --out <card>/root/wiki/manual` and the pack at `/tmp/manual70` is a ready-made copy. Whether 70 columns is the final number is the operator's judgement, and if it moves both constants must move together or the new cross-check fails, which is the point of it. `td_gutenberg_app.cpp` still clips at 76 and should follow to 70 if a book and an article are to wrap alike. The concurrent session's SNES work is mid-edit and breaks `test_tang_fpga_uart` until it lands; that gate entry should be re-run after their commit. The rest of the open list is unchanged: the two untried pack boundaries (`A & E Home Video` in chunk 10 and `A battuta` above 2 GiB), the LoRa link end to end with its retry budget under a busy node, the `mkdir -p` measurement, the three Bluetooth re-runs, the TinyGutenberg reading-position restore, the personality 8 flip-bit fix, and the release firmware identity question the build id cannot answer.

#### Files Modified:

- ports/bl616/td_wiki_app.cpp
- tools/make_wiki_pack.py
- tools/tests/check_wiki_convert.py

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 10 COMMIT Unreleased 2026-11-05T22:43:05-07:00

#### Coming From:

Unreleased d1e45e3

#### Purpose:

Build the TinySNES window's Input page, storing the pad mapping in the card-root settings file beside the other two systems' own, and fix the host gate this work broke.

#### Outcome:

The Input page is real rather than a placeholder, in TinyNES's shape: a Player 1/Player 2 toggle, a row per SNES button reading `<button> <- <source>`, a Change button that cycles the selected row through every controller button and then `none`, a Restore that returns a player to the default map, and a Save that writes the file. A row always exists for every button and an absent binding reads `none` rather than the row disappearing, so the list never changes height under the player, and an edited row is marked `*` until Save. A binding of `none` is a real state, not a missing one: it is how a player removes a button rather than moving it, and the core's frame carries it as selectors 12 to 15. The file is `/sd/tinysnes.ini` at the card root, beside `/tinynes.ini` and `/tinygbc.ini`, read through the same `tang_settings_ini.c` walk they use. Its `[input]` section holds `p1_<button> = <source>` and `p2_<button> = <source>`, the same shape as TinyNES's, and `[tinysnes] version` matches the other two files' convention. `ports/bl616/tang_snes_ini.c` and its header are new: the defaults are the identity map, so an absent file leaves a pad the player cannot tell apart from every build before this one, and an unknown key or value is ignored and not counted, so a misspelt binding shows in the key count rather than as a button that silently does nothing. The write goes through a `.tmp` with the old file moved aside first and removed only once the new one is in place, because FatFS has no atomic replace and a card that fills up must not cost a player their bindings; a failure at any step puts the old file back. `tang_snes_apply_pad_map()` in `tang_snes_load.c` sends the core's frame 0x1A, and a load calls it once the link is up and before the ROM goes out. The core half is the TinySNES repository's entry 19, and this entry cannot be read on its own without it. The firmware builds at 694,816 bytes, SHA-256 `1394f3d87af0c3912f0ceabda5170bf7bd085d872df8a48da6b66fce36fa804f`, identity `d1e45e3-dirty.888a330`, deployed with `tools/tinytang_flash.py` and run after a power cycle. The operator confirmed the page and the mapping on the board: the pad's behaviour was checked before the first save and again after, with the buttons exchanged in the game, and `/tinysnes.ini` on the card shows the edit. That file is 408 bytes with `p1_a = y` and `p1_y = x` against `p2_` still at identity, so the write and the read-back agree. Two tests are new and one gate is repaired. `tools/tests/tb_snes_ini.c` with `tools/tests/test_snes_ini.sh` drives the real parser: identity defaults, an absent file, several pairs on one line, `r` against `rt` in both directions because a prefix compare would bind the shoulder to the wrong button, `none`, player 2 kept apart from player 1, comments and unknown sections skipped, the frame word's packing at identity, at an A/B swap and at all-unbound, and a save-then-reload round trip, 34 checks. `tb_snes_load.c` gained the pad map frame: identity when no file exists, and the exact six bytes for a pushed map and for all-unbound. The gate this work broke is `test_tang_fpga_uart`, which links `tang_snes_load.c` and had no module for the `tang_snes_settings_load` and `tang_snes_map_word` symbols that file now calls; another agent found it, established it predated their own cycle by stashing their files and reproducing it, and left this session's files untouched, which is recorded because it was the right call. The fix adds `tang_snes_ini.c` to that link line, and the host gate is 44 of 44. Player 2 is deliberately not functional and the page does not pretend otherwise in the code's comments, though it does in its layout: `snestang_top.v` ties player 2's pad inputs to zero and the core's frame carries one player's selectors, so the toggle edits keys that are stored and read back and can change nothing. The operator's decision was to leave it for now; it is recorded here and in the core's entry so the page is not later read as working. No settled entry was rewritten: entry 9's note that this session's SNES work was mid-edit and breaking that gate is now closed by this entry rather than edited. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff and confirmed it touches only this log, that `.ai/core.md` was not changed and no settled entry was rewritten, and validated this entry as number 10 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

Player 2 is the one drawn control that cannot work, and the operator left it for a later cycle deliberately: the options are to hide the toggle while this core has one pad, to keep it with a caption saying so, or to wire a second pad, which is the only one that makes it real and needs a second `wii_pad` on another adapter plus a wider frame at the core end. It should be settled before the Video and Audio pages are built, because the same question -- whether a control is honest when the core cannot honour it -- will come up on both, and the placeholder treatment those two pages currently use is the answer this session preferred. The remap is proven on player 1 with the Wii Classic Controller only; the other pad sources pass through the same stage in the core and none was exercised. Entry 9's note about the manual pack still stands and is not this cycle's: the board reads a pack wrapped at 78 against a reader that clips at 70 until that pack is rebuilt onto the card. The rest of the open list is unchanged: the two untried pack boundaries, the LoRa link end to end, the `mkdir -p` measurement, the three Bluetooth re-runs, the TinyGutenberg reading-position restore, the personality 8 flip-bit fix and the release firmware identity question.

#### Files Modified:

- ports/bl616/tang_snes_ini.c
- ports/bl616/tang_snes_ini.h
- ports/bl616/tang_snes_load.c
- ports/bl616/tang_snes_load.h
- ports/bl616/tinysnes_app.cpp
- tools/tests/tb_snes_ini.c
- tools/tests/test_snes_ini.sh
- tools/tests/tb_snes_load.c
- tools/tests/test_snes_load.sh
- tools/tests/test_tang_fpga_uart.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 11 COMMIT Unreleased 2026-11-05T23:33:08-07:00

#### Coming From:

Unreleased 9b4d026

#### Purpose:

Build the TinySNES window's Video page with the two settings this console can honour, and store them in the settings file beside the input map.

#### Outcome:

The Video page is real rather than a placeholder, with two rows and a Save: `Aspect`, cycling 4:3, 8:7 and 1:1, and `Extra lines`, toggling between shown and cropped. Both are written to `/tinysnes.ini` under a new `[video]` section, `aspect` and `overscan = show|crop`, read through the same `tang_settings_ini.c` walk as `[input]`. The defaults are 4:3 with the extra lines shown, which is the geometry and the line count this core had before the setting existed, so a card with no `[video]` section behaves exactly as it did. `tang_snes_apply_video()` sends the core's frame 0x1B and a load calls it beside the pad map, before the ROM goes out; the core half is the TinySNES repository's entry 20. The Save on this page writes the whole file rather than only its own section, so an Input edit and a Video edit saved from different pages both persist -- the Input page's Save does the same, because `tang_snes_settings_save()` serialises the struct rather than patching one section. One setting the other two systems have was deliberately left out: there is no palette row. The NES needs one because its composite output leaves the colours ambiguous and its emulators ship competing interpretations of that artifact, while the SNES drives digital BGR555 from CGRAM and this core's path from it is a bit shift, so there is one correct rendering and a palette selector would invent an ambiguity the hardware does not have. That reasoning is in `tang_snes_ini.h` beside the aspect enum, not only here, so a later cycle adding a palette row meets the argument first. The page also says so on screen. The firmware builds at 698,928 bytes, SHA-256 `b1be6bedf721edfa17263d18a7a092227427c597de983670b034bc1cbcb444a5`, identity `9b4d026-dirty.4ec8ed0`, and was deployed with `tools/tinytang_flash.py`. It is **not confirmed on the board**: the operator deployed at the end of the session and did not power-cycle before stopping, so the page layout and both settings are unproven on hardware. The host gate is 44 of 44 and `tb_snes_ini.c` grew from 34 checks to 40: the `[video]` keys read and written, an aspect this build does not know keeping the default and not counting as understood, and a save-then-reload round trip whose key count now includes the two video keys. `tb_snes_load.c` was updated for the frame order the load now has -- pad map, video, hold, header -- and its overlay-index assertion moved from 1 to 2, because two settings frames now precede the hide. Three expectations in the loader test and one in the ini test were wrong on the first run and the tests caught them, all of them indices or counts that shifted by one when a frame was added. One older bench defect surfaced in the core repository and is recorded there: a pad chord test that depended on a tick phase, failing once the new frames shifted it, and fixed by holding the chord long enough for a report to finish rather than by widening a retry count. `test_tang_fpga_uart` needed no change this cycle, unlike entry 10, because `tang_snes_apply_video()` lives in `tang_snes_load.c` itself and `tang_snes_ini.c` is already on that link line. Two untracked scripts in `tools/` named for previewing bitmap and cell layers are not this project's work and were left out of the commit. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff and confirmed it touches only this log, that `.ai/core.md` was not changed and no settled entry was rewritten, and validated this entry as number 11 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The board is flashed and unconfirmed, and settling that is the first task: power-cycle, open the Video page, and check the picture is unchanged before anything is saved, since the defaults are meant to reproduce the previous geometry exactly. Then step the aspect to 8:7 and 1:1, both of which must be visibly and proportionally narrower, and save so that a launch picks the setting up. The extra-lines row cannot be tested on demand -- it needs a cartridge that sets SETINI bit 2, and a row that changes nothing is as likely to mean no such cartridge here as to mean the row is broken -- so the useful observation is a title whose bottom fifteen lines are missing today, recorded by name if one is found. `Audio` is the last placeholder and it is the harder of the two remaining pages, because a master volume cannot be a frame applied after the mixer: the S-DSP sums its voices internally and a volume applied downstream of that would scale the output rather than the mix, so the setting would have to live inside `dsp.v` and that is a core cycle with a listening test, not a page. It should not be started with the assumption that it is the same job as Video was. Entry 19's open question about Player 2 -- whether a page may draw a control the core cannot honour -- is now the only such case left on the window, and the Audio page is where the answer will be tested again. Entry 9's manual pack still needs rebuilding onto the card for the 70-column reader. The rest of the open list is unchanged: the two untried pack boundaries, the LoRa link end to end, the `mkdir -p` measurement, the three Bluetooth re-runs, the TinyGutenberg reading-position restore, the personality 8 flip-bit fix and the release firmware identity question.

#### Files Modified:

- ports/bl616/tang_snes_ini.c
- ports/bl616/tang_snes_ini.h
- ports/bl616/tang_snes_load.c
- ports/bl616/tang_snes_load.h
- ports/bl616/tinysnes_app.cpp
- tools/tests/tb_snes_ini.c
- tools/tests/tb_snes_load.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 12 COMMIT Unreleased 2026-11-05T23:33:20-07:00

#### Coming From:

Unreleased 81ca971

#### Purpose:

Establish what image support TinyWiki could actually have while an emulator core keeps running, and what picture quality each block-RAM budget buys.

#### Outcome:

Two deterministic preview tools now render what the hardware could put on a 1280 by 720 screen, from facts in this tree rather than from assumptions. The first question was whether an image can appear over a game at all, and the answer needed no new mechanism: the desk layer replaces the whole picture while the core keeps running, which is why F12 is a single byte, and `desk_link.sv` says so in its own header. That layer exists in TinyNES, TinyGBC and Tiny89 as the same `desk_link.sv` plus `desk_font.hex`, in TinyPhosphor as `textdisp_wide.sv`, and in TinySNES not at all, which carries `desk_font.hex` but no `desk_link.sv` and so has the glyph table and none of the mechanism, which means anything drawn over a game is a per-core bitstream change and TinySNES has no room for one: it sits at 340 of 340 BSRAM. The existing layer is 80 by 45 cells of 16 by 16 pixels carrying 5 bytes each on the wire, a 7-bit character, a 15-bit foreground and a 15-bit background, so a cell holds one of 128 glyphs in two RGB555 colours. `tools/preview_cell_layer.py` renders that faithfully, taking the glyph table from `fpga/desktop/font.vh`, treating bit 0 as the leftmost column per PROT-004, and quantising every colour to RGB555; it produced a walkthrough page whose text is exact, since the text is the font, over a dungeon figure, and it confirms that prose in this layer is as readable as it is today only because it is not rasterised. The same tool shows that the 16 quadrant patterns would give a 160 by 90 two-colour image grid, and the glyph slots for them are free: `font.vh` has exactly 34 blank glyphs, `0x00` to `0x1F` and `0x20` and `0x7F`, measured rather than assumed. A real bitmap layer is a different order of quality, and it is bounded by block RAM that was measured rather than guessed: TinyNES builds to 66 of 340 BSRAM and 14,898 of 138,240 LUTs, read from `build/ae350_soc/nextpnr.log` in the TinyNES tree, which leaves 274 blocks of 2,304 bytes, 616 KB. Against a 1280 by 720 frame that makes 1 bpp 50 blocks, 4 bpp 200 blocks and 8 bpp 400 blocks, so full resolution at 16 levels fits and full resolution at 256 does not, while 960 by 540 at 8 bpp takes 225. `tools/preview_bitmap_layer.py` renders each of those settings as its own complete 1280 by 720 frame, so no combination panel has to be interpreted, and the operator supplied a 1280 by 720 scan of a magazine page as the subject because it is close to the worst realistic case: dense body text inside an existing halftone. That test produced the cycle's one real finding, which runs against the obvious instinct. 1 bpp dither destroys such a scan, because screening an image that is already screened is destructive interference rather than dithering. 2 and 3 bpp climb to usable, 4 bpp is comfortably readable, and 8 bpp at full resolution is clean but 126 blocks over budget. Held against each other at equal block cost, native 4 bpp at 1280 by 720 is visibly sharper on letterforms than 8 bpp at 960 by 540 scaled up, so for text resolution beats colour depth and the extra four bits buy little that the pixels were not already giving. Sixteen colours rather than sixteen greys matters for the same bytes, since a palette keeps a magazine's logo and a map's coding that greyscale discards. One error of this cycle was caught and fixed before commit: the first side-by-side labelled 8 bpp at 960 by 540 as 180 blocks, and re-deriving the arithmetic gave 225, which is what the committed tool now prints. The operator reviewed the renders and deferred the choice of depth to the next session, naming setting e, which is 4 bpp at 1280 by 720 in sixteen greys for 200 blocks. No firmware was built and the board was not touched, so the image on it is unchanged; the host gate is 44 of 44, and the two failing entries that entry 9 recorded were cleared by the concurrent session's TinySNES cycles, which landed as 9b4d026 and 81ca971. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff and confirmed it adds this entry and nothing else, that `.ai/core.md` was not changed and no settled entry was rewritten, and validated this entry as number 12 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The depth is the operator's to settle, with setting e held for that decision, and the renders are reproducible with `python3 tools/preview_bitmap_layer.py` or `--image <any picture>` for a different subject. Three things stand between a decision and a design. Block RAM is measured for TinyNES alone, so TinyGBC, Tiny89 and TinyPhosphor need the same reading from their own `nextpnr.log` before 200 blocks can be promised anywhere, and Phosphor carries audio buffers that may leave less. A frame at 4 bpp is 460,800 bytes, which is 2.3 s over the 2 Mbaud wire and about 5.4 s at the 85 KB/s the link shows under load, so a page turn is a slideshow unless the host diffs rectangles the way the desk layer already diffs cells. And the bitstream work is the expensive half rather than the RTL: a changed netlist in five designs needs a seed sweep and a board test, which is the cycle the reference records as taking 2026-10-11 to 10-14, and TOOL-021's spine findings mean the build gates alone do not clear a core. TinySNES can take no framebuffer at all until BSRAM is freed or the dock's SDRAM grows a controller. Carried unchanged from entry 9: the 70-column manual pack is built and verified but still not on the card, so the board reads a 78-column pack through a 70-column reader, and `td_gutenberg_app.cpp` still clips at 76; `SOURCE_DATE_EPOCH` is a `?=` default rather than a lock, which the release rebuild should settle. The open list beyond this cycle is as entry 11 left it.

#### Files Modified:

- tools/preview_bitmap_layer.py
- tools/preview_cell_layer.py

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 13 COMMIT Unreleased 2026-11-06T09:56:36-07:00

#### Coming From:

Unreleased b73a7dd

#### Purpose:

Build the TinySNES desktop window's Audio page, which was the last placeholder, as one master volume and a mute in the shape TinyNES and TinyGBC already ship.

#### Outcome:

The page is TinyNES's, and so is the arithmetic behind it. `tang_settings_ini.h` gained `tang_audio_gain`, which squares the percentage out of `TANG_VOLUME_UNITY` 256 with rounding rather than truncation, so each 5 percent step moves the level about as much as the last instead of the top half of the slider doing almost nothing. It lives in the shared settings module rather than in the app or in fabric because the curve is a property of the slider a person drags, and a copy in C beside a lookup table in RTL would eventually disagree; the core takes the number this returns and nothing else. `tang_snes_settings_t` grew a `tang_audio_t`, `[audio]` is read through the same `tang_audio_apply` that `/tinynes.ini` and `/tinygbc.ini` use, which is what stops the three players from drifting on what `volume` or `mute` means, and the writer emits the section after `[video]`. `tang_snes_apply_audio` turns the pair into the two little-endian bytes of frame `0x1C` and is called beside the pad map and the picture's shape when a game starts, so all three settings arrive before the cartridge streams and a core that hears none of them keeps unity, identity and 4:3. The page itself is a 5 percent slider, a `%3u%%` label that appends "muted", a Mute checkbox, Save, and the note that the value is taken when the next game starts; the placeholder list and its `DUMMY_ROWS` table are gone, and the tab-set comment no longer calls anything unbuilt. Adding a third settings frame broke `tb_snes_load.c`, which asserts an exact frame sequence: the settings frames are now three before the hold rather than two, so `frame_count`, `osd_first_index`, `restored()` and the three `frame_fail_at` indices moved by one, and those are expectation changes with no change to what the test proves. `tb_snes_ini.c` gained the `[audio]` round trip, the clamping of a volume past the top, the four named gain values including that 100 percent is exactly unity, and the two new keys in the key count. The firmware is `build/build_out/tinytang_bl616.bin`, SHA-256 `77a3432ad9213cc7033c09ebcfcae7ea9db817eef353bfc313e6b4d554907058` at 698,944 bytes, identity `b73a7dd-dirty.30d9470`, with `tang_audio_gain` and `tang_snes_apply_audio` present in the ELF rather than assumed linked; the host gate passes 44 of 44. It was installed by `tools/tinytang_flash.py`, which reported `tangflash: OK committing`, and a power cycle through the operator's relay script brought it back reporting the new identity. The core half is the TinySNES repository's entry 22 and this entry cannot be read on its own without it. The operator tested the page on the board and reports the volume working perfectly, and the card corroborates the path independently of the listening test: `/tinysnes.ini` now carries `[audio]` with `volume = 20` and `mute = no`, a value neither the defaults nor the cycle's own test wrote, so the slider, the label, Save and the atomic rename all ran on hardware and a gain of 10 out of 256 reached the core. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff and confirmed it adds this entry and nothing else, confirmed `.ai/core.md` was not changed, confirmed entries 1 to 12 were not rewritten, and validated this entry as number 13 of 100 with six sections in order, one-sentence Purpose, prose Outcome and Next Steps, permitted Status values, only eligible paths in Files Modified and no self-referencing hash.

#### Next Steps:

The Audio page has one property still unproven and it cannot be settled by ear: that a mute silences both outputs and that unity is bit-identical to the previous build, which a capture through the TinySNES tree's `tools/ab_capture.sh` would answer. The Input page's Player 2 toggle is still drawn and inert by the operator's earlier decision, and the same question it raises has now been answered twice the same way -- Video and Audio both carry only controls the core honours -- so it is worth settling rather than carrying. The `tmp.*.test_*.log` files that `tools/tests/run_all.sh` leaves in the repository root are still there and are nobody's intent; the harness should clean them up or write them under `build/`. `third_party/tinydesk` still carries uncommitted changes that are not this repository's and stayed out of this commit. Everything else open is as entry 12 left it.

#### Files Modified:

- ports/bl616/tang_settings_ini.c
- ports/bl616/tang_settings_ini.h
- ports/bl616/tang_snes_ini.c
- ports/bl616/tang_snes_ini.h
- ports/bl616/tang_snes_load.c
- ports/bl616/tang_snes_load.h
- ports/bl616/tinysnes_app.cpp
- tools/tests/tb_snes_ini.c
- tools/tests/tb_snes_load.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 14 COMMIT Unreleased 2026-11-06T10:42:32-07:00

#### Coming From:

Unreleased b70eca4

#### Purpose:

Make `tools/make_wiki_pack.py` keep drawn ASCII art intact, so a hard-wrapped plain-text walkthrough can be renamed to `.md`, converted, and read on the board without its maps being flattened.

#### Outcome:

The converter collapsed space runs and re-wrapped every line, which is right for HTML prose and destroys ASCII art, because in a map every run of spaces is a column. Measured on a 215,697-byte GameFAQs walkthrough of The Legend of Zelda that the operator supplied, that turned the overworld grid's `A   |   |   |   |   |   |D9 |` into `A | | | | | |D9 |`, read a 79-character banner of asterisks as a Markdown bullet list and prefixed each line with `- `, and split a rule mid-run into two lines because it was re-wrapped at the page width. The fix is one predicate and one branch in `markdown_body()`: `is_drawing()` classifies a line as drawing when it carries a wall, is a run of box and rule characters only, or is a digits-and-spaces axis row, and a run of such lines is emitted verbatim with its own common indent removed, so a right-aligned box still fits the page while every column keeps its relative position. A drawn line wider than the page is trimmed only when it holds fewer than two walls, which distinguishes a separator or a boxed byline from a map row: the first loses ornament and keeps its meaning, the second would lose a room, so it is kept whole and reported through the existing `wide_lines` channel. The rewrite also had to stop using the loop variable as the list index, since the drawn path advances by a run rather than a line, and the per-line branches now advance by one. On the operator's file the pack now reads `A   |   |   |   |   |   |D9 |   |   |   |   |WS |D5 |   |   | L |   |` with all sixteen columns aligned under `1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16`, and the dungeon grids in all nine dungeons keep their walls. The pack is 1 record, 213,881 bytes, 8,985 lines and 17.4 pages at the reader's 12,288-byte page, and its widest line is exactly 70 columns, so `WINDOW_COLS` in `td_wiki_app.cpp` clips nothing. `tools/check_wiki_convert.py` grew from 62 checks to 68: map columns preserved, every row of a map kept and not only the first, a banner staying one drawn line, a banner not read as a list, no drawn line over the page, and prose beside a drawing still wrapped. Forcing `is_drawing()` false failed three of them and restoring it passed all six, so the guard bites rather than describing itself. The host gate is 44 of 44. The pack was uploaded to `/root/wiki/zelda` on the card as `wiki.meta`, `wiki.idx` and `wiki.000`, `ls` confirmed all three, and `check_wiki_pack.py` against the built pack reported OK; the operator opened it and passed it. No firmware was built or flashed and no firmware source changed, so the board still runs the image entry 11 deployed; `platform` could not be re-read to quote its identity because the desktop was up and the console guard refuses to send while the console is held, which is TOOL-011 working rather than a gap. Two things the operator will see are not this cycle's and were not changed: a markdown pack still carries `parts = 0` so a 17-page article has no contents list, and a legend entry hard-wrapped in the source still breaks mid-sentence because each source line is its own paragraph. Two untracked tools from this session's earlier detours were left out of the commit rather than deleted, `tools/preview_tile_vs_frame.py` and `tools/make_walkthrough_pack.py`, because deleting the operator's files is their call and the second was called overbuilt before it was finished. One correction is owed from entry 9 and is recorded here rather than edited there: the failure string `tools/tests/check_wiki_convert.py` prints is "they must be equal or long lines lose their tail on screen", and entry 9 quoted it as "they must be long lines lose their tail on screen", dropping three words. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff and confirmed it adds this entry and nothing else, that `.ai/core.md` was not changed and no settled entry was rewritten, and validated this entry as number 14 of 100 with exactly six sections, a one-sentence Purpose, prose in Outcome and Next Steps, only eligible paths in Files Modified, allowed Status values and no self-referencing hash.

#### Next Steps:

The reading problem this cycle exposes is that a long document has no navigable structure: `convert_markdown` passes `parts_count = 0` to `write_meta`, so the 17-page pack scrolls linearly with no chapter jumps, and the board's `manual` pack has the same shape. Giving a markdown pack a parts table is the next useful cycle, and it needs a heading convention rather than a new splitter, since the source already marks its sections. The legend raggedness is the smaller half of the same question: a source line that continues a previous one should rejoin it, and the rule to distinguish a wrapped continuation from a real line break in a hard-wrapped file is the open decision. The two untracked tools are the operator's to delete or keep. Nothing else in this repository was touched: the release firmware identity question the build id cannot answer, the pack boundaries `A & E Home Video` and `A battuta`, the LoRa link end to end, the `mkdir -p` measurement and the three Bluetooth re-runs are all carried unchanged, as are the 70-column manual pack that is still not on the card and the TinyGutenberg width that still clips at 76.

#### Files Modified:

- tools/make_wiki_pack.py
- tools/tests/check_wiki_convert.py

#### Status:

- Build: N/A
- Deployment: PASS
- User Test: PASS

---

## 15 COMMIT Unreleased 2026-11-06T13:29:54-07:00

#### Coming From:

Unreleased 5c58bd3

#### Purpose:

Assemble a first-boot SD card for a new user, and give the pack converter's format sweep a corpus directory it can actually find.

#### Outcome:

A complete card was built at `/run/media/vash/TINYTEST` (outside this repository) from the release ship set, and the two repository-side defects it exposed were fixed here. The card carries the BL616 firmware built at this commit, 698,944 bytes at SHA-256 `c84fa6a575e602f69c9c7dca04d38545793486707b0ddeade47aeab2d4b7db49`, identity `5c58bd3-dirty.45fed98`; the desktop core rebuilt with `tools/build_desktop_core_open.sh` at its default seed 3 and reproduced the core of record bit for bit, 4,526,774 bytes at `00d8b48cac8e54b27cdbdacf2adfe8e67331bd3ff95c05e6fd671192f46582cf` with `desktop.fs` at `60dd20032095f6d1b7e14dab9e77ca57973e5d494051d0acf2955dd929e45fda` and `tinytang.fs.gz` at exactly the 401,300 bytes `BUILDING.md` records; the four game cores from their own repositories with a `.fs.gz` beside every `.bin`, all five pairs proven by decompressing the `.fs`, converting it back with `tools/desktop_open/fs_to_bin.py` and comparing digests; the four AE350 players; the 70-column manual pack rebuilt from `manual/`; twelve codec tones generated by `tools/make_codec_corpus.sh`, verified deterministic by a second run compared byte for byte; seven NES homebrew cartridges zipped and checked through TinyNES's own `tests/test_cart_zip.sh`; five Retro Regen hardware test suites with SNES and GBA extracted beside their archives because TinySNES and any future GBA window read only a raw image; and `tinytang.ini` shipped with no active layout line, because a card whose file says anything else declares modules that are not seated. Two corrections came out of the work. The Tiny89 core first placed on the card was `build/deploy/baseline-tiny89_soc-0ec60b4f.bin`, an older build whose `.fs` no longer exists and which the Tiny89 repository's own log does not name; the card now carries `build/tiny89_soc_kbd/tiny89_soc.bin` at `ec9b3027050c49791110eefd84e8390b8a0b21b7892fec29f0bfcc8025c77131`, the image that repository records as the one deployed, whose `.fs` was available so the pair is consistent. And `tools/phosphor_format_sweep.py` built its paths as `/root/media/music/test.<ext>`, which stopped being right once the corpus was filed under a `diagnostics` folder; it now takes `--dir` and defaults to `/root/media/music/diagnostics`, so a sweep needs no arguments beyond the port, and its docstring and usage line were corrected to match. `manual/files.md` gained `gb`, `gbc` and `snes` in the card layout, moved the manual's own path from the retired `/root/manual` to `/root/wiki/manual`, and records that the game windows descend into subfolders; `manual/tinynes.md` gained a section naming the eight cartridges that ship, their authors and licences, and the reason each is zipped. The host gate is 44 of 44. No firmware source changed: the image was rebuilt only to establish the identity this card carries, and the board was not touched, so nothing was deployed to it and the operator has not tested the card, both at the operator's instruction for this cycle. Three limits are carried rather than hidden. `tinysnes.bin` is not on the card, so the TinySNES window lists its eleven homebrew games and the Retro Regen test but every launch will report that the core did not load. No GBA app is registered in this build, so the GBA test ROM waits in `media/gba/diagnostics` for one. And `root/media/ti89/TI89Titanium_OS.89u` is Texas Instruments' copyrighted operating system, present because this is a test card and marked in the card README as something that must be removed before the layout is published. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff and confirmed it adds this entry and nothing else, that `.ai/core.md` was not changed and no settled entry was rewritten, and validated this entry as number 15 of 100 with exactly six sections, a one-sentence Purpose, prose in Outcome and Next Steps, only eligible paths in Files Modified, allowed Status values and no self-referencing hash.

#### Next Steps:

The card is written and untested, so the first thing it needs is the operator's power-up: whether a fresh card reaches the prompt, whether `desktop` comes up, and whether each window lists its folder. The TinySNES decision is open and blocks that library from being usable: the candidate core is `build/video/tinysnes.bin` in the TinySNES repository at 5,238,252 bytes, built 2026-11-05, and copying it in with its `.fs.gz` would make those eleven SNES games and the Retro Regen test launchable, but `docs/qualification.md` still records TinySNES as out of scope for this release, so it is the operator's call to overturn and not an omission. Before the layout is published, `TI89Titanium_OS.89u` must be removed, and the card should gain whatever the operator wants a new user to have that is currently absent: cartridges beyond the bundled homebrew, music beyond the test tones, and the book and wiki packs that a real card would carry. The three per-system `.ini` files now documented in the card README are at their defaults and have not been exercised by a save from either window.

#### Files Modified:

- manual/files.md
- manual/tinynes.md
- tools/phosphor_format_sweep.py

#### Status:

- Build: PASS
- Deployment: N/A
- User Test: NOT RUN

---

## 16 COMMIT Unreleased 2026-11-06T14:27:59-07:00

#### Coming From:

Unreleased 26e92ac

#### Purpose:

Record the first boot of the release card by the operator, and what reading the running board afterwards established about it.

#### Outcome:

The card assembled in entry 15 boots, and the operator reports a pass through the Bluetooth setup to the desktop, which supersedes entry 15's `User Test: NOT RUN` without rewriting it. The board was then read over the CDC console rather than taken on report, and every claim below is its own output. `platform` answered `bl616/freertos, tinytang b73a7dd-dirty.30d9470`, TinyDesk Shell 0.1.6. `fpga` answered core 84 answering on UART1 at 2 Mbaud, and 0x54 is the desktop core, so `boot.tdsh` did its whole job: `tangload` of `tinytang.bin`, `osd clear`, `osd desk on`, and then the prompt. `tangini` reported no layout line with both sockets released and core 0x54 at ABI 1.6 holding 0xc0 = 0x0000 as declared, which is the missing-layout path working rather than a file that failed to parse. `crash` reported no record from a previous run, watchdog on, zero resets since power-up and reset status 0x7d with the watchdog bit high, which is the clean power-up the recorder is meant to distinguish from a fault. `ble` reported the radio up, the stack taking 16,852 bytes of heap at start, heap 86,016 free of 137,560 with a 42,508-byte largest block, and the last script task using 11,088 of its 16,384 bytes of stack, and it named the two devices that work: a Logi M750 mouse and a Rii K06 combo keyboard, both paired and reconnecting by themselves, with `bonds.bin` written to the card. Every folder the card carries reads back from the board as built: six NES zips in `homebrew` with AccuracyCoin and the Retro Regen test in `diagnostics`; three Game Boy and six Game Boy Color zips; eleven `.sfc` under `media/snes/homebrew` and the extracted Retro Regen test beside its archive; twelve codec tones in `music/diagnostics`; four AE350 players; five cores with five `.fs.gz`; and `wiki.meta` reporting ten articles at 70 columns, so TinyWiki will find the manual. Two things the reading turned up are recorded rather than corrected. The board is running `b73a7dd-dirty.30d9470`, which is entry 13's firmware, while the card's own `bl616-firmware.bin` is `5c58bd3-dirty.45fed98` at 698,944 bytes and `c84fa6a5…`, so what passed was not the release candidate; the difference between them is the TinySNES Audio page committed as `b70eca4` and nothing this card exercises, and installing it is a Software Update from the desktop followed by a power cycle, which was not done because the operator directed that the card and the firmware both be left alone. And `/root/media/gba/homebrew` exists and is empty, created by the operator rather than by the card build, so it lists as a folder that opens to nothing. One claim was deliberately not tested: the zipped-cartridge path is still proven only on the host, through TinyNES's own `cart_zip.c` under sanitizers, because `ae350run` waits out a program that does not exit and a shell-driven launch would hold the console for its 300-second bound and leave a cartridge running. The operator's own launch of a zipped cartridge from the TinyNES window is still the thing that would close it. `blekbd` reported idle with 871 reports and not paired, which is a leftover count in a slot whose bond is gone, not a failure: the working keyboard is the K06 in the combo slot. No source file changed in this cycle, so the host gate carries at 44 of 44 from entry 15 and was not re-run, and the `.ai/` diff is this entry alone. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff and confirmed it adds this entry and nothing else, that `.ai/core.md` was not changed and no settled entry was rewritten, and validated this entry as number 16 of 100 with exactly six sections, a one-sentence Purpose, prose in Outcome and Next Steps, `None.` in Files Modified, allowed Status values and no self-referencing hash.

#### Next Steps:

The release candidate has not been booted: the board runs `b73a7dd` while the card carries `5c58bd3`, so the first thing the next cycle should do is install the card's `bl616-firmware.bin` through the desktop's Software Update app, power-cycle, and confirm `platform` reports `5c58bd3-dirty.45fed98` before anything else is qualified against it. Two hardware claims remain open for the operator's hand rather than an agent's: launching a zipped cartridge from the TinyNES window, which would close the last gap in the zip path, and the same for a Game Boy zip in the TinyGBC window. The TinySNES decision is unchanged and still blocks that library: `tinysnes.bin` is absent, the candidate is `build/video/tinysnes.bin` at 5,238,252 bytes in the TinySNES repository, and `docs/qualification.md` records the core as out of scope for this release. Before the layout is published, `root/media/ti89/TI89Titanium_OS.89u` must be removed, and the empty `root/media/gba/homebrew` should be either filled or deleted. Everything else carried from entry 15 stands: the empty GBA folder, the three per-system `.ini` files unexercised by a save, the pack contents list that `convert_markdown` still leaves at `parts = 0`, and the carried release qualification rows.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: PASS
- User Test: PASS

---

## 17 COMMIT Unreleased 2026-11-06T16:05:18-07:00

#### Coming From:

Unreleased 3a31690

#### Purpose:

Record Tiny89's save stall in the release qualification matrix as a known issue the operator accepted, so the release carries the evidence rather than a memory of it.

#### Outcome:

The calculator runs and its save does not finish, which the operator reported and then accepted as a known issue rather than a cycle to fix now. Three Ctrl+S attempts were read out of `/root/saves/last.log` with the board on firmware `3a31690-dirty.45fed98` and the Tiny89 player the card carries, which is the build Tiny89's own log records for its entry 21 at SHA-256 `1f19f86e…` and 5,701,736 bytes. The third attempt wrote the state correctly: `arm ...state: 263316 bytes, crc eb590206`, then `finish ...: 263316 of 263316 bytes, crc eb590206 of eb590206, whole`, 529 ms of card time, and `/root/states/ti89/TI89Titanium_OS.state` on the card, so the state path works and the thing a launch resumes from now exists. What stalls is the 4 MiB flash image the same save goes on to send, because `save_flash()` runs when the OS has changed its flash: the log shows `arm ...flash: 4194304 bytes, crc d1fcdbf9` and `first byte 00`, and then no progress line and no finish line more than twenty minutes later. Progress is logged at every 32 KiB boundary and the sender paces itself at 62 ms per 512 bytes, so the whole image is about 8.5 minutes and the first progress line is due four seconds in; under 32 KiB of 4 MiB moved. The session never returns, which is the part the operator sees: `fpga` still answered core 0x4E (Tiny89) long afterwards, the picture stayed frozen and there was no second attempt because the app is inside the run. The first two attempts are the transport loss already recorded, where a lost frame shifts a byte payload and only the CRC can see it, but the second has a different signature worth carrying: its finish line reads `161792 of 0` with `crc cebaacfa of 00000000`, and exactly one line of code zeroes that field, the refusal branch of `ae350_save_arm()`, which also clears `s_ae350_save_left` and leaves `s_ae350_save_active` set so the next payload byte ends the save and hands the rest of a raw RAM dump to the text parser. That is recorded as a lead and not a finding, because how a second arm would arise mid-payload is not established and the flash stall shows no finish line at all, so it is not that path; whether the calculator is blocked in `t89_put_char` on `T89_UART_TX_READY` or the board stopped draining UART1 has not been told apart, and `s_rx_drops` exists but is printed only inside a failure message, so a failure that never prints one leaves no evidence behind and exposing it is the first step of any fix. The section also records that the fault spans two repositories, the sender being the Tiny89 ELF on the card and the receiver `ports/bl616/tang_fpga_uart.c`, that the card carries one entry older than Tiny89's HEAD, and that Tiny89's tree has uncommitted work — the archive attribute for the variable-send header — which touches the install path and not the save path, so the newer work does not fix this. One correction is owed against what this session reported out loud before it checked: a `flash.part` reading as zero bytes was offered as evidence of the stall, and it is not, because FatFS does not commit a file's directory size until `f_sync` or `f_close`; the missing 32 KiB progress line is the evidence, and the section says so. The matrix row now reads `untested | Tiny89 | no host test in this tree. Its save stalls after the state lands — see 5.1`, so the cross-reference is there from the top. No code changed. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff and confirmed it adds this entry and nothing else, that `.ai/core.md` was not changed and no settled entry was rewritten, and validated this entry as number 17 of 100 with exactly six sections, a one-sentence Purpose, prose in Outcome and Next Steps, only `docs/qualification.md` in Files Modified, allowed Status values and no self-referencing hash.

#### Next Steps:

The first step of any fix is to make the failure legible, because it currently is not: `s_rx_drops` should be readable outside a failure message, and the flash save should log progress where a stalled one is visible. Only then is the right question answerable — whether the calculator is blocked in `t89_put_char` waiting for `T89_UART_TX_READY`, or the board stopped draining UART1 — since the two need opposite fixes and neither has been observed. A second question sits behind that: 4 MiB on the Ctrl+S key may simply be the wrong design, and a flash image that only travels when the OS has changed might belong behind its own command rather than lengthening every save. Nothing here blocks the release, since TinyNES and TinyGBC both passed on this card and Tiny89 is not in the ship set's read path. The pad fix is this session's next entry, and cycle two after it puts `tinysnes.bin` on the card.

#### Files Modified:

- docs/qualification.md

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: PASS

---

## 18 COMMIT Unreleased 2026-11-06T16:05:36-07:00

#### Coming From:

Unreleased 660edc7

#### Purpose:

Stop a core load that programs nothing from leaving the Wii pad and the clock dead over a core that never moved.

#### Outcome:

Launching a cartridge whose core image is not on the card killed the Classic Controller, and the cause was a belief nothing put back. `tang_core_load()` calls `tang_ini_core_replacing()` before programming, which clears `s_i2c_bus`, and only `tang_ini_core_loaded()` sets it again, on the success branch; `fpga_program()` returns before touching the FPGA when `f_open` fails, so a missing file left the flag false over a desktop core still sitting there with an I2C engine on the USB-A port. Three things read that flag and all three went dark: `tang_wiipad_hw_ready()`, which is `tang_core_i2c_bus() && tang_fpga_link_open() == 0 && !tang_fpga_pipe_open()`, so `tang_wiipad_poll()` marked the pad `TANG_WIIPAD_OFF` and returned early; the RTC through `tang_clock.c`; and `tangi2c`. The operator met it as "the core did not load, then my controller stops working", which is the TinySNES window launching a cartridge whose `tinysnes.bin` entry 15 deliberately left off the card, but the fault is any failed load and not TinySNES. The fix keeps what the flag was and offers a way back: `tang_ini_core_replacing()` records `s_i2c_bus_was` before clearing, a new `tang_ini_core_load_failed()` restores it and asks the clock to resync, and `tdsh_tang_flash.c` calls it on the failure branch. Restoring is the right answer rather than latching off even when a load failed part-way through programming, because the pad's next probe discovers a core that really is gone and that is a state `tang_wiipad_poll()` already handles, while a flag pinned false is the one answer that cannot recover without another load. Two host tests cover it and both were proven to bite. `tools/tests/tb_pmod_sockets.cpp` with `test_pmod_sockets.sh` compiles the real `ports/bl616/phosphor/pmod_sockets.cpp` and `tang_ini.c`, with the FPGA stubbed at the two seams the module uses — the link frames for the core-ID probe and `fpga_debug_request` for the register reads — so the module's own retry loops and its socket-word path run for real; it holds 11 checks across a desktop core at ABI 1.6 setting the bus, replacing clearing it, a failed load restoring it, a failed load over a socketless core leaving it off, a successful load of TinyNES clearing it, a desktop core below ABI 1.5 not claiming it, the socket word still being sent, a missing `/tinytang.ini` still being reported, and a core that does not answer leaving it off; emptying `tang_ini_core_load_failed()` fails two of them. `tools/tests/tb_tang_flash.c` gained the caller's half, so the failure path must now ask for the restore and its hook sequence reads `program ini-load-failed`, and removing that call from `tdsh_tang_flash.c` fails two checks there. The host gate is 45 of 45, one more than entry 17's 44 because the new suite is the addition. The firmware builds at 698,944 bytes, SHA-256 `88ce23b2cdd003e64f7d7672530f3ba3f7299e5af9ee0ffdc202ce487232e6a3`, identity `3a31690-dirty.cd6a5d5`, and was installed with `tools/tinytang_flash.py`; after the operator's power cycle `platform` reported exactly that identity. The board test is the one that failed before the fix and passes after: `wiipad status` read `classic ready` with live bytes `7a 7c 79 76 00 00 ff ff`, `tangload /root/cores/console138k/nosuchfile.bin` then failed with `open fail, res:4` and `tangload: failed`, and `wiipad status` still read `classic ready` with the same live bytes rather than `off`. The other two consumers were checked in the same state: `tangini` reported core 0x54 at ABI 1.6 holding 0xc0 = 0x0000 as declared, and `tangi2c rtc` read the DS3231 at `2026-11-06 23:04:57`, which is itself proof the flag was true because that read is gated on it. One build observation worth carrying: the same source produced `1a1108ee…` and then `88ce23b2…` because the build id embeds a hash of the working tree and the two new test files appeared between the two builds, with the dirty component moving `cbc026a` to `cd6a5d5`; the identity of the deployed image is the one quoted above. `pmod_sockets.cpp` had no host coverage at all before this cycle and `check_test_coverage.py` still lists it as untested, because the new suite drives it from `tb_pmod_sockets.cpp` rather than naming the module in a `test_*.sh` build line the tool recognises; that bookkeeping is left for a cycle that touches the tool. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff and confirmed it adds this entry and nothing else, that `.ai/core.md` was not changed and no settled entry was rewritten, and validated this entry as number 18 of 100 with exactly six sections, a one-sentence Purpose, prose in Outcome and Next Steps, only eligible paths in Files Modified, allowed Status values and no self-referencing hash.

#### Next Steps:

Cycle two is next and is already scoped: `tinysnes.bin` on the card, from `build/video/tinysnes.bin` in the TinySNES repository at 5,238,252 bytes, with its `.fs.gz` beside it, which makes the eleven SNES homebrew games and the Retro Regen test launch and removes the operator's reason for meeting this fault at all. It overturns the out-of-scope note for TinySNES in `docs/qualification.md`, so that row has to move rather than be deleted, and the card README's "there is no tinysnes.bin" paragraph has to go. Two smaller things came out of this cycle and belong to whoever next touches the coverage tool: `check_test_coverage.py` does not see `pmod_sockets.cpp` as covered even though a real test now builds it, and the `1a1108ee` versus `88ce23b2` pair is a second worked example for the build-id caveat in section 1 of `docs/qualification.md`, which entry 15 already warns about but does not illustrate with a digest pair. Everything else carried from entry 17 stands unchanged, and the Tiny89 save stall stays a known issue with no fix scheduled.

#### Files Modified:

- ports/bl616/phosphor/pmod_sockets.cpp
- ports/bl616/tdsh_tang_flash.c
- tools/tests/tb_pmod_sockets.cpp
- tools/tests/tb_tang_flash.c
- tools/tests/test_pmod_sockets.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 19 COMMIT Unreleased 2026-11-06T16:18:27-07:00

#### Coming From:

Unreleased 4284991

#### Purpose:

Put the TinySNES core on the release card and move TinySNES from out of scope to in, so the eleven SNES homebrew games and the Retro Regen test that the card already carries can be launched.

#### Outcome:

The release card now carries `tinysnes.bin` at 5,238,252 bytes, SHA-256 `6b7489d022b65cc97b51b1a014e59cc2029f900501e134bd3e2cd43bcadfb26e`, with `tinysnes.fs.gz` beside it at 1,773,023 bytes from a `gzip -9 -n` of the 41,942,501-byte `tinysnes.fs` at SHA-256 `6bb911d033dc9792a4fe78b185bb606153b8c201d5adcf5ed2a3c470948baf28`, both uploaded over the CDC console with `tools/tinytang_put.py` and both landing at exactly their source sizes, checked with `ls -l` on the board. Choosing the build took reading rather than picking the newest directory: `build/video/tinysnes.bin` at `ce199fa6…` is the Video-settings core, but `build/audio1/tinysnes.bin` at `6b7489d0…` is newer and is the image TinySNES's own entry 22 deployed and had accepted with the desktop's Audio page, so it carries the pad remap, the aspect and extra-lines frames and the volume frames together, which the firmware's own entries 11 and 13 support; `build/hyg1/tinysnes.bin` is a hygiene rebuild that is byte-identical to it and was used as the source, with both the `.bin` and the `.fs` compared rather than assumed. The core was then verified on the board rather than left as a file transfer: `tangload /root/cores/console138k/tinysnes.bin` programmed it and reported `ID=0001081b`, `tangload: core loaded`, and `fpga` answered `core 2 answering on UART1 at 2000000 baud`, core 2 being SNES in TangCore's own table; the desktop core was then loaded back, which reported `tinytang.ini: sockets declared, 0xc0 = 0x0000` and answered as core 84, so the socket declaration ran and the I2C bus belief was restored, which is entry 18's fix doing its job on a real pair of loads. That establishes the file is a valid bitstream for this device, that the JTAG load path takes it, and that the link answers afterwards; it does not establish a cartridge streamed through the window, and the entry says so rather than claiming the library works. The card README was updated by fetch, edit and verified return rather than by rewriting it blind: `tangget` pulled the 27,196-byte file down through `tools/tinytang_sync.py`'s own `Console.get()`, the result was checked for transfer damage (no CR, no NUL, 105 non-ASCII bytes, 543 lines, valid UTF-8), three edits were made, and the 27,693-byte file was uploaded to `/README.new`, fetched back and compared — the round trip is byte-identical at SHA-256 `fcc41d12546721389adc7aa5122e5e52898968d30b1f7d2532dc926685a0408e` — before `/README.md` was replaced and `/README.new` renamed into place. The edits add `tinysnes.bin` and `tinysnes.fs.gz` to the card tree, add the core to the list with what its three settings pages and its HOME gesture do, replace the paragraph that said there was no `tinysnes.bin` on the card with one saying what is and is not proven about it, and move the build stamp from `5c58bd3` to `4284991`. In this repository `docs/qualification.md` moves TinySNES from out of scope to in with a `partial` row in the applications table: the loader is covered by `test_snes_load` and the settings by `tb_snes_ini`, the window is not covered, and no cartridge has been launched through it yet. The host gate is 45 of 45. No firmware was built and nothing was flashed, so the board still runs entry 18's `3a31690-dirty.cd6a5d5`; the only things that changed on the card are the two new core files and the README, and the one thing the cycle did not change is the out-of-scope sentence's other half, that TinyCreature, TinyGBA, TinyVCMI and TinyMaps are not shipping, which still holds. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff and confirmed it adds this entry and nothing else, that `.ai/core.md` was not changed and no settled entry was rewritten, and validated this entry as number 19 of 100 with exactly six sections, a one-sentence Purpose, prose in Outcome and Next Steps, only `docs/qualification.md` in Files Modified, allowed Status values and no self-referencing hash.

#### Next Steps:

One launch closes this cycle and it is the operator's: open the TinySNES window, pick a cartridge from `homebrew` — the eleven `.sfc` files are raw images because that window has no inflater — and see the game. If it plays, the `partial` row becomes silicon and the README's caveat can go; if it does not, the loader's own message names the stage it reached, and the console shows it because a cartridge stream is printed as it runs. The Retro Regen SNES test in `diagnostics` is a better second test than a game because it reports what it finds rather than merely drawing. Two smaller things are carried. The manual has no TinySNES page, which is consistent with it having no TinyGBC or Tiny89 page either, so bringing the game cores into the manual is one cycle rather than a gap this one opened. And `tinytang-upload.bin` is now at the card root, left by the entry-18 flash; it is `tinytang_flash.py`'s staging path by design and should be left alone, but a published card should not carry a stray 683 KB firmware image, so removing it belongs with the release-layout pass alongside `TI89Titanium_OS.89u`.

#### Files Modified:

- docs/qualification.md

#### Status:

- Build: N/A
- Deployment: PASS
- User Test: NOT RUN

---

## 20 COMMIT Unreleased 2026-11-06T17:39:50-07:00

#### Coming From:

Unreleased a42b641

#### Purpose:

Record that audio works from all three emulator cores through HDMI and the PMOD I2S2, and record the two defects the false alarm about it exposed.

#### Outcome:

The operator reported no HDMI audio from any emulator on the release card, and the card's settings were the first suspect: `tinytang.ini` there declared no PMOD layout while the working card declared `lora`, so the four `.ini` files were copied from the working card to the release card over the CDC console with `tangput` and fetched back with `tangget` for comparison, all four byte-identical at `f73740e0…` for `tinynes.ini`, `fe51d572…` for `tinygbc.ini`, `0abb5b02…` for `tinysnes.ini` and `fa5d55f1…` for `tinytang.ini`, and `tangini apply` put the `lora` word on the loaded desktop core. That appeared to fix it and did not. The board was then asked directly and contradicted the theory: `tangload` of `tinysnes.bin` followed by `tangini` answered "core 0x02 has no PMOD sockets", and TinyNES answered nothing at all because its AE350 boot ROM speaks the `TinyNES LOAD` protocol rather than the 0xAA frames (PROT-010), so `pmod_sockets.cpp`'s two-entry table — Phosphor 0x50 and the desktop core 0x54 — means no emulator core has ever been sent a socket word on either card. The pin map agrees with that rather than merely permitting it: in `desktop_pmod.sv` a released socket is `1'bz` and personality 8 drives PMOD0, so under no layout and under `lora` the PMOD1 pins carrying `pmod1_dac`'s MCLK, LRCK, SCLK and SDIN are identical, and `tangload` reconfigures the FPGA so no previous core's pin state survives. The real explanation was the content: the first cartridges tried were homebrew, and AccuracyCoin, FamiDash and Snake have sparse or deliberately minimal sound, so with forty retail games on the card the operator played Super Mario Bros. and heard it, and reported sound from TinyNES, TinyGBC and TinySNES on both HDMI and the PMOD I2S2. That is the first time audio was heard from these cores through this board's HDMI, and it is now a silicon row in the desktop core's video matrix. The forty games were added to the release card in four `commercial` folders, ten per system, taken from this machine's MiSTer image and chosen by Metacritic rank: NES and both Game Boy sets stay zipped because their players inflate a `.zip` themselves, and the SNES ten are extracted to raw `.sfc` because TinySNES has no inflater. Two things were checked before they were left there — every archive unpacks through the player's own reader, the NES ten through TinyNES's `test_cart_zip.sh` with 0 bad and the twenty Game Boy ones through TinyGBC's real `cart_zip.c` under sanitizers, and every SNES ROM carries a valid LoROM or HiROM header — and one thing was caught that would have failed silently: the Game Boy library skips any file whose name reaches its 56-byte `LIB_NAME_BYTES` rather than truncating it, and seven of the twenty had names of 58 to 99 characters, among them a 87-character Pokemon Yellow, so all twenty were renamed to short form. The card README gained the four folders in its tree and a rewritten "Not shippable" section naming all forty games and saying to delete the four folders before the layout is published, beside the TI-89 operating system that was already there. Two real defects came out of the false alarm and are recorded in `docs/qualification.md` §5.2 rather than fixed. The volume slider on the NES and TinyGBC windows does nothing: `tang_audio_gain()` has exactly one caller, `tang_snes_load.c:461`, which sends frame 0x1C to the SNES core, while `tinynes_app.cpp` and `tinygbc_app.cpp` read, store, save and draw the value as a percentage with a mute checkbox and never carry it to their cores, whose players use their own `SOUND_VOLUME`. And `tangini` cannot inspect an emulator core, reporting "core 0x02 has no PMOD sockets" against SNES and "no core answering" against TinyNES, both true and both inviting the wrong conclusion this cycle spent hours on. The TinySNES row moved from `partial` to `silicon` because the cartridge that had never been launched now has been. The host gate is 45 of 45. No firmware was built or flashed, so the board still runs entry 18's `3a31690-dirty.cd6a5d5`, confirmed by `platform` after the operator's reboot with core 84 answering and `0xc0 = 0x0080` as declared. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff and confirmed it adds this entry and nothing else, that `.ai/core.md` was not changed and no settled entry was rewritten, and validated this entry as number 20 of 100 with exactly six sections, a one-sentence Purpose, prose in Outcome and Next Steps, only `docs/qualification.md` in Files Modified, allowed Status values and no self-referencing hash.

#### Next Steps:

The volume slider is the next cycle worth doing and it is small: `tinynes_app.cpp` and `tinygbc_app.cpp` need a frame that carries `tang_audio_gain()`'s result to their cores, and each core needs to apply it to the player it runs, which is the same shape as SNES frame 0x1C that `tang_snes_load.c` already sends. Doing it for one system first would show the contract before the second repeats it. `tangini`'s two messages should also be made to distinguish "this core has no sockets" from "this core does not answer", since the first is normal for an emulator and the second is not. The card still carries forty retail games and the TI-89 operating system, both to be removed before the layout is published, and `tinytang-upload.bin` at its root from entry 18's flash. Nothing else changed: the Tiny89 save stall stays a known issue, the pack contents list is still unwritten, and the carried release qualification rows are as entries 17 to 19 left them.

#### Files Modified:

- docs/qualification.md

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: PASS

---

## 21 COMMIT Unreleased 2026-11-06T18:12:54-07:00

#### Coming From:

Unreleased 4d7c592

#### Purpose:

Withdraw entry 20's claim that the TinyNES and TinyGBC volume sliders do nothing, after the operator showed they work and the code was read again.

#### Outcome:

Entry 20 recorded as fact that the volume sliders on the TinyNES and TinyGBC windows do nothing, on the evidence that `tang_audio_gain()` has exactly one caller and that caller is the SNES frame 0x1C. The operator tested all three emulators, set each to about 30 percent, and heard the volume drop on HDMI and on the PMOD I2S2, which is what a working control sounds like. The claim was wrong, and the reading that produced it was wrong in the way this file's own section 5.2 warns about: an absence in one file is not an absence in the system. The sliders do reach their cores, by a different route than SNES's. Each player asks the host for its settings at startup rather than being sent them: `receive_settings()` in TinyNES's `firmware/apps/nes_bench/main.cpp` prints `TinyNES SETTINGS WANT <address> <length>` and then takes the reply into `host_settings`, and `ae350run`'s line handler in `tang_fpga_uart.c` answers it with a blob whose last four bytes are player 1's mapping, player 2's, the aspect and the palette followed by the volume and mute from `/tinynes.ini`, printing `volume=%u` in its own log line; TinyGBC's player does the same and its reply puts volume and mute in `blob[0]` and `blob[1]`. So the slider is a launch-time setting rather than a live one, and that is the only real distinction: dragging it during a game changes nothing until the next cartridge is launched, where SNES's frame 0x1C takes effect as it is moved. The correction is recorded in `docs/qualification.md` section 5.2 in place of the false claim, which now states what actually happens, names both mechanisms and their file and symbol, and says plainly that the claim was wrong and why the first reading missed it. The two application rows are corrected to match: TinyNES and TinyGBC now read that volume and mute are launch-time and sent in the settings reply, rather than that the slider does not reach the core. Nothing else in section 5.2 changed, so the `tangini` ambiguity stands as a real finding and the silicon row for audio on HDMI and the PMOD I2S2 stands as proven. The board was read to confirm the state the operator described: `/tinygbc.ini` holds `volume = 30, mute = no`, written by the window's own Save, which is the control working end to end. No source changed, so the host gate carries at 45 of 45 from entry 20 and was not re-run, and the board still runs `3a31690-dirty.cd6a5d5`. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff and confirmed it adds this entry and nothing else, that `.ai/core.md` was not changed and that entry 20 was not edited — the correction is recorded here, where a settled entry's error belongs — and validated this entry as number 21 of 100 with exactly six sections, a one-sentence Purpose, prose in Outcome and Next Steps, only `docs/qualification.md` in Files Modified, allowed Status values and no self-referencing hash.

#### Next Steps:

The only real question the slider leaves is whether launch-time is good enough. It is for a setting a player changes between games and wrong for one they reach for because a passage is too loud, and SNES alone has the live path. Making the other two live would mean a frame in each core's `desk_link.sv` and a matching send from `tang_fpga_uart.c`, which is two core cycles with a seed sweep and a board test each, so it belongs with the next core rebuild that also closes the personality-8 flip-bit deviation rather than on its own. The `tangini` message ambiguity is the remaining finding from this investigation and is small enough to be its own cycle. Everything else carried from entry 20 stands unchanged: the card still holds forty retail games, the TI-89 operating system and `tinytang-upload.bin` to be removed before publication, the Tiny89 save stall stays a known issue, and the pack contents list is still unwritten.

#### Files Modified:

- docs/qualification.md

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: PASS

---

## 22 COMMIT Unreleased 2026-11-07T07:27:16-07:00

#### Coming From:

Unreleased 5ed64cc

#### Purpose:

Make the release matrix's own gate and coverage figures match what the tools measure, since three of them had been stale for many cycles and one of them was the document contradicting every log entry beside it.

#### Outcome:

Three figures in `docs/qualification.md` were false and are now measured. Section 2 said "43 of 43 pass (42 scripts plus the width gate)" while entries 18 through 21 each reported 45 of 45 in their own prose; running `tools/tests/run_all.sh` on 2026-11-07 returns "45 of 45 passed", and `--list` names 44 scripts plus `check_help_width.py`, so the two suites the matrix had not counted are `test_snes_ini` and `test_pmod_sockets`. Section 6's first bullet said 39 host regressions where it now says 44, the same error one line further down. Section 6's second bullet said 30 of 59 port modules were built by no host test and section 6.1 was headed "The other 30"; `tools/check_test_coverage.py` reports 31 of 60 covered and 29 not, and the module that moved is `pmod_sockets.cpp`, which the tool lists as `COVERED pmod_sockets.cpp test_pmod_sockets.sh` because that script names the module's path on its compiler line at line 27. Entry 18 recorded the expectation that the tool could not see it, because the suite drives the module from `tb_pmod_sockets.cpp` rather than a `test_*.sh` build line the tool recognises; that expectation was wrong and the script has named the path since it landed, so the entry is corrected here rather than edited, and the matrix now says so. Section 6.1's Phosphor row loses `pmod_sockets` and reads eight files at 2,276 lines rather than nine at 2,587. Four more counts in that same table were stale by measurement and were corrected with it, since leaving them beside a corrected one would be a half-true table: `tang_ble.c` is 2,748 lines rather than 2,681, the eight app wrappers total 6,840 rather than 6,269 (the TinySNES window grew through entries 8, 10, 11 and 13), `tang_osd.c` is 326 rather than 321 and `td_desktop_bl616.c` is 434 rather than 432. Every other figure in the table was checked and stands. The board was read first and is healthy on entry 18's image: `platform` answered `bl616/freertos, tinytang 3a31690-dirty.cd6a5d5`, `fpga` answered core 84 on UART1 at 2 Mbaud, and `tangini` reported `layout lora` with `core 0x54 (ABI 1.6) has 0xc0 = 0x0080, as declared`; the desktop then took the console and the guard refused further sends, so `crash` and `ble` were not re-read, which is TOOL-011 working. The running image is source-equivalent to `HEAD` because `git diff --stat 3a31690..HEAD` touches no `ports/` file except entry 18's own fix, so no firmware was built, flashed or needed. `.codewhale/handoff.md` was rewritten because it was actively misleading a successor: it named entry 20 as the last cycle when entry 21 had superseded it, and it carried entry 20's withdrawn volume-slider claim as the immediate next step alongside the coverage-tool note this cycle disproves. No source, test, tool or build file changed, so the gate was run rather than modified and passes 45 of 45 both before and after. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff and confirmed it adds this entry and nothing else, that `.ai/core.md` was not changed and no settled entry was rewritten — entry 18's wrong expectation is corrected here, where a settled entry's error belongs — and validated this entry as number 22 of 100 with exactly six sections, a one-sentence Purpose, prose in Outcome and Next Steps, only eligible paths in Files Modified, allowed Status values and no self-referencing hash.

#### Next Steps:

The `tangini` ambiguity is the next cycle and is the one entry 21 named: `pmod_sockets.cpp` cannot distinguish a core that legitimately has no socket word, which is every emulator core and printed as "core 0x02 has no PMOD sockets", from a core that does not answer at all, which for TinyNES is a different protocol and printed as "no core answering"; both are true and both read as faults, which is what cost entry 20 hours. The distinction is host-testable against `tools/tests/tb_pmod_sockets.cpp`, whose two stubs already separate the core-ID probe from the register reads, and it needs a build, a flash and one board test to close. Two smaller things stand where they did: the launch-time versus live volume question for TinyNES and TinyGBC belongs with the next core rebuild that closes the personality-8 flip-bit deviation rather than on its own, and `check_test_coverage.py` is still not a gate, which is deliberate while 29 modules are legitimately deferred. Everything else open is unchanged: the forty retail games, `TI89Titanium_OS.89u` and `tinytang-upload.bin` to strip from the release card before the layout is published, the two untried pack boundaries, the LoRa link end to end, the `mkdir -p` measurement, the three Bluetooth re-runs, the Tiny89 save stall as an accepted known issue, the pack contents list, and the TinyGutenberg widths and reading position.

#### Files Modified:

- docs/qualification.md

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 23 COMMIT Unreleased 2026-11-07T07:36:37-07:00

#### Coming From:

Unreleased cca1cfa

#### Purpose:

Settle `tangini`'s two "no sockets" and "no answer" messages as correct diagnostic output rather than a defect to fix, by the operator's decision, and record the reason they are this way so no later cycle reads them as a fault again.

#### Outcome:

The operator's instruction was to drop the item and the observation that there is a reason it is this way, and reading the code confirms it. Both messages are the literal answer to the same probe. `cmd_tangini` asks the loaded core with `core_id()`, which sends `FPGA_CMD_CORE_ID` and waits for `FPGA_RESP_CORE_ID` — the 0xAA frame protocol of PROT-001 — and reports what came back: a core that answers but is not in `s_socket_cores` prints "core 0x%02x has no PMOD sockets", and a core that does not answer prints "no core answering". Against SNES the first is true and normal, because that table holds exactly two entries, Phosphor 0x50 and the desktop core 0x54, and no emulator core is ever sent a socket word; against TinyNES the second is equally true about the link, because its AE350 boot ROM speaks the `TinyNES LOAD` protocol rather than the 0xAA frames (PROT-010). Neither message implies an action, because the module's own contract at the top of `pmod_sockets.cpp` is that other cores are left alone and nothing is written to a core without the sockets, so both branches return without touching the core. The module cannot tell the two apart without keeping a record of what it loaded, and the firmware deliberately has no such record: `tang_ae350_run()` blocks for the program's lifetime and clears its own note when the run ends, so there is no persistent "an AE350 program is running" state to consult. A circumlocution that explained the second case would therefore have to be printed from a stored belief about the core rather than from the core, and a belief that says something the core does not is precisely the defect entry 18 recorded and fixed: a flag cleared on the way into a load and restored only on success, left false over a core that never moved, which killed the Wii pad and the RTC. So the current behaviour is the design, and changing it would trade a literal message for a stale one. `docs/qualification.md` section 5.2's paragraph was rewritten from a finding into the settled position: it now states both messages as the link's literal answer, quotes the module's contract, names `tang_ae350_run()`'s lifetime as the reason no such state exists, cites entry 18's failure class, and records the decision with its date. Nothing else in the file changed, so the `tangini` ambiguity stops being carried as an open item in §5.2 while the silicon row for the `lora` layout stands. No source, test, tool or build file changed and no firmware was built or flashed, so the board still runs `3a31690-dirty.cd6a5d5` as entry 22 read it, and the host gate was re-run after the change and passes 45 of 45, as it did before, because no test builds from the document and nothing the gate compiles was touched. `.codewhale/handoff.md` was updated to match: the item moved out of "Immediate next step" into the corrections list, and the next step now says plainly that nothing in the tree needs a firmware cycle and that the remaining work is card-side and host-side. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff and confirmed it adds this entry and nothing else, that `.ai/core.md` was not changed and no settled entry was rewritten — entry 20's framing is corrected here and in §5.2 rather than edited — and validated this entry as number 23 of 100 with exactly six sections, a one-sentence Purpose, prose in Outcome and Next Steps, only eligible paths in Files Modified, allowed Status values and no self-referencing hash.

#### Next Steps:

No code work is owed on this. The open list is now card-side and host-side, and the operator chooses from it: stripping the release card before the layout is published — `root/media/ti89/TI89Titanium_OS.89u`, the four `commercial` folders holding forty retail games, and `tinytang-upload.bin` at the root — is the item with a reason to happen before anything else, since it is the only one that blocks publishing; the two untried pack boundaries `A & E Home Video` in chunk 10 and `A battuta` above 2 GiB are cheap board reads that would extend the multi-chunk claim to the whole pack; a markdown pack still carries `parts = 0` so a long article has no chapter jumps; and TinyGutenberg writes `POS_REAL` through `position_save()` and never reads it while `manual/gutenberg.md` and `README.md` both claim the app resumes, with the app also clipping at 76 against the wiki's 70. The personality-8 flip-bit deviation still belongs to the final core rebuild, and a live NES/GBC volume path would ride along there if it is ever wanted, the launch-time sliders themselves being proven. The Tiny89 save stall stays an accepted known issue with no fix scheduled.

#### Files Modified:

- docs/qualification.md

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 24 COMMIT Unreleased 2026-11-07T09:29:42-07:00

#### Coming From:

Unreleased 737d03f

#### Purpose:

Remove the copyrighted material from the release card before its layout is published, and correct the two documents that described it as still being there.

#### Outcome:

The card at `/run/media/vash/TINYTEST` went from 101 MB to 61 MB by losing every byte that could not be redistributed, and nothing else. Forty retail games left with the four `commercial` folders holding them, ten each for NES, Game Boy, Game Boy Color and SNES, 39 MB together; `root/media/ti89/TI89Titanium_OS.89u` left, the 1.4 MB Texas Instruments operating system for the Tiny89 app. Two classes of derivative went with them because they are copies of the same work rather than data about it: `root/states/ti89/TI89Titanium_OS.state`, a 263 KB memory image of the TI OS, `root/states/gbc/Wario Land 3.state`, a save state of a retail Game Boy Color title, and six battery saves named for retail cartridges — `Dragon Warrior III.sav`, `Pokemon Gold.sav` with its `.rtc`, `Wario Land 3.sav`, `Legend of Zelda, The (USA) (Rev 1).sav`, `Donkey Kong Country (USA) (Rev 2).sav` and `Super Mario All-Stars + Super Mario World (USA).sav` — plus a zero-byte `TI89Titanium_OS.flash.part` left by a stalled save. The one save that stayed is `Nova the Squirrel.sav`, which belongs to a homebrew title. Nothing was deleted outright: all fifteen items were moved to `/run/media/vash/GIT/tinytang-scratch/card-ip-holding-2026-11-07/` with a `MANIFEST.txt` naming each one and its size, so the removal is reversible with a single move and the operator can delete the holding copy when satisfied, which is the right order for a step that cannot be undone on the card. What remains is what the card documents: six NES, three Game Boy, six Game Boy Color homebrew zips, eleven SNES homebrew `.sfc` files, the five Retro Regen tests and AccuracyCoin, twelve generated codec tones, five cores with their `.fs.gz` pairs, four AE350 players, the manual pack and the `.ini` files. The card's `README.md` was corrected rather than only trimmed, because four parts of it had become false: the tree listing lost the four `commercial` rows and the `ti89/TI89Titanium_OS.89u` row, a stale `tiny89-diag.elf` row came out since that file had already been removed by hand, the paragraph that called this "the test card exception" carrying forty retail games was replaced, and the whole "Not shippable, though it is on this card" section became "Supplying your own cartridges and the calculator's OS", which says that nothing commercial is present by design, how to add your own cartridges and in which formats, and that Tiny89 cannot run because `root/media/ti89/` is now present and empty and the operator must supply an image from a calculator they own. The final bullet under the notes was inverted to match: it used to say the TI-89 image must be removed before release, and now says the empty folder is why Tiny89 will not start. `docs/qualification.md` section 5.2 gained a paragraph recording the removal, because its audio row was measured on 2026-11-06 with those games present and a later reader would otherwise go looking for them; the measurement itself is untouched, as a settled fact, and the note says a future audio test has to supply its own cartridge. The removal was verified by asking the card rather than by trusting the moves: `find` returns zero directories named `commercial` and zero paths matching `*.89u` or `*Titanium*`, and `du` reports 61 MB against 101 MB before. Nothing was built, flashed or deployed and the board was not touched, so the firmware it runs and every core on the card are as entry 23 left them. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff and confirmed it adds this entry and nothing else, that `.ai/core.md` was not changed and no settled entry was rewritten, and validated this entry as number 24 of 100 with exactly six sections, a one-sentence Purpose, prose Outcome and Next Steps, only `docs/qualification.md` in Files Modified, allowed Status values and no self-referencing hash.

#### Next Steps:

Three items on the card are not copyrighted material and were therefore left in place, each needing the operator's word rather than an assumption: `tinytang-upload.bin` at the root, 698,944 bytes at SHA-256 `88ce23b2…`, which is `tools/tinytang_flash.py`'s staging path and is the firmware the board actually runs, while `bl616-firmware.bin` beside it is the older `c84fa6a5…` that the Software Update window installs — so the card can install a build the board is not running, which is worth settling separately from this cycle; `root/ble/bonds.bin`, 252 bytes of the operator's own Bluetooth pairing keys, which belong to a person rather than a release and should probably not ship; and `.Trash-1000/`, 5.8 MB of files deleted through the desktop's file manager, which are still on the card and are included in any copy of it. The rest of the release checklist is otherwise cleared: the layout no longer carries retail games, an operating system, or saves derived from either, and the card README describes what is actually there. The one hardware consequence is that Tiny89 cannot start from this card until an operating system image is supplied, which is the intended state and is now documented rather than presented as a defect.

#### Files Modified:

- docs/qualification.md

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: PASS

---

## 25 COMMIT Unreleased 2026-11-07T09:38:25-07:00

#### Coming From:

Unreleased 20317f4

#### Purpose:

Make the card's `bl616-firmware.bin` the build the board runs, and make that a check rather than a habit, because an out-of-date copy there turns the desktop's Software Update into a silent downgrade.

#### Outcome:

`bl616-firmware.bin` on the card held `bl616/freertos, tinytang 5c58bd3-dirty.45fed98`, which is entry 15's build, while the board runs `3a31690-dirty.cd6a5d5`; `td_update_app.c` hardcodes that one path, so installing from the desktop would have replaced the running firmware with a twelve-day-older one and reported success. It now holds the running build: 698,944 bytes at SHA-256 `88ce23b2cdd003e64f7d7672530f3ba3f7299e5af9ee0ffdc202ce487232e6a3`, which is `build/build_out/tinytang_bl616.bin` and, independently, the `tinytang-upload.bin` that entry 18's flash left at the card root; the identity stamped inside the installed file reads `3a31690-dirty.cd6a5d5`, so the evidence is the binary's own banner rather than a filename or a date. That is the same pair entry 24 found by comparing digests, and it is the second time the file drifted, which is why the fix is a tool and not another copy. `tools/tinytang_card_firmware.py` refills the card file from `build/build_out/tinytang_bl616.bin`, reads the build identity out of the image's `bl616/freertos, tinytang <id>` banner, names the identity it is replacing, and verifies the written copy by SHA-256 and by re-reading its banner. With `--port /dev/ttyACM0` it also asks the board and refuses to write when `platform`'s answer disagrees with the image, which is the half that makes the rule enforceable: without it the tool would only report which revision it happened to copy. A board that answers nothing is treated as no agreement rather than as consent, and `--no-board` exists so the operator can install deliberately while saying out loud, in the tool's own output, which revision is going in. One latent defect in the first draft was fixed before commit: the post-copy flush opened the destination read-only and called `os.fsync` on it, which is portable on Linux only by tolerance, while on the vfat card the flush is precisely what stops a pulled card from keeping the old directory entry — the handle is now opened for writing. `tools/tests/check_card_firmware.py` with `test_card_firmware.sh` covers it in 22 checks with no board and no card, every path a temporary directory: the banner read, a clean build with no dirty suffix, an image with no banner, a real `platform` reply with its colour codes, the copy and its digest, that the replaced identity is named, and five refusals, four of which must leave the card file untouched — a blank image, a missing image, a missing card, a board running another revision, and a board that answers nothing, the last two spelled `--port` so a real board is never needed. Two faults were planted and both were caught, at three checks and one check respectively; the second is worth carrying because the check that caught it was not the obvious one, the blank-image refusal appearing to work only because a later identity comparison failed after the file had already been overwritten, which is exactly the ordering the untouched-file assertion exists to detect. `manual/files.md` and `README.md` now state the rule where each audience looks, the manual beside the flash command and the README beside the quick start's mention of the file, and the card's own `README.md` gained the rule with the shipped identity and digest written down so a reader can compare them without a build tree. The host gate is 46 of 46, one more than entry 24 because the new suite is the addition. Nothing was flashed and no firmware source changed, so the board still runs the image entry 18 deployed; the board was not reachable during this cycle and the check therefore rested on the build output's own banner, which matches the identity `platform` reported on 2026-11-07 earlier in the session and the digest entry 18 recorded after its power cycle. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff and confirmed it adds this entry and nothing else, that `.ai/core.md` was not changed and no settled entry was rewritten, and validated this entry as number 25 of 100 with exactly six sections, a one-sentence Purpose, prose Outcome and Next Steps, only eligible paths in Files Modified, allowed Status values and no self-referencing hash.

#### Next Steps:

`tinytang-upload.bin` is still at the card root, byte-identical to `bl616-firmware.bin` at `88ce23b2…` because entry 18's flash staged it there; it is not in the card README's tree, and now that the two agree it is clutter rather than a hazard, so it is the operator's call whether to keep the staging file on a published card or let the next flash recreate it. Two other items from entry 24 remain open on the card and neither was touched: `root/ble/bonds.bin`, 252 bytes of the operator's own pairing keys, which belong to a person rather than a release, and `.Trash-1000/`, 5.8 MB of files deleted through the desktop that are still present and would travel with any copy. The rule this cycle encodes only holds if it is used: the natural moment is after every flash, since `tools/tinytang_flash.py` already stages the image on the card and a second command keeps the installable copy in step, and a future cycle could fold the call into the flash tool so the two cannot drift at all, which is the same shape of guarantee as this tool but with nothing left to remember. Nothing else is open on the firmware side: the board is on the current rev and the card now installs that rev.

#### Files Modified:

- README.md
- manual/files.md
- tools/tinytang_card_firmware.py
- tools/tests/check_card_firmware.py
- tools/tests/test_card_firmware.sh

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: PASS

---

## 26 COMMIT Unreleased 2026-11-07T09:46:04-07:00

#### Coming From:

Unreleased 58eec40

#### Purpose:

Close the one gap entry 25 left open, by verifying against the running board that the card's firmware file is the build the board runs, rather than inferring it from the build output.

#### Outcome:

Entry 25 installed the running image on the card and recorded that the board was not reachable, so its evidence was the image's own banner. With the card back in the Tang and the board enumerating again as `ffff:5454` on `/dev/ttyACM0`, all three links in the chain were read on hardware instead. `platform` answered `bl616/freertos, tinytang 3a31690-dirty.cd6a5d5`, with `fpga` reporting core 84 answering on UART1 at 2 Mbaud and `tangini` reporting `layout lora` and `core 0x54 (ABI 1.6) has 0xc0 = 0x0080, as declared`, so the board is on the same image and in the same state entries 23 and 25 describe. `ls -l /` from the board showed `bl616-firmware.bin` at 698,944 bytes and `tinytang-upload.bin` at the same size, which is a size and not a content. The content was then fetched back over the console with `tangget`, the way entry 19 verified the card README: 698,944 bytes, SHA-256 `88ce23b2cdd003e64f7d7672530f3ba3f7299e5af9ee0ffdc202ce487232e6a3`, and the `bl616/freertos, tinytang 3a31690-dirty.cd6a5d5` banner present inside the returned bytes. That digest is the one `build/build_out/tinytang_bl616.bin` produces, so the file the board reads is byte-for-byte the file the tree builds, and it is the image the board is running. This matters more than a redundant check would, because the copy travelled from a host mount to the card and then into the board, and a truncated or stale write is exactly what the cycle exists to prevent. The tool's board-side path, which entry 25 had tested only against a stub, was then exercised against the real Tang: with a scratch directory standing in for the card, `--port /dev/ttyACM0 --dry-run` read `3a31690-dirty.cd6a5d5` off the board, matched it to the image, and proceeded, while a synthetic image carrying `deadbee-dirty.feedfac` produced `refusing: the board runs 3a31690-dirty.cd6a5d5 and <image> is deadbee-dirty.feedfac` with exit status 1 and nothing written. That is the refusal the tool exists for, and it now has two ways of failing that were shown to bite rather than described: planted faults in entry 25's host suite, and a real board disagreeing with a real image here. One behaviour was confirmed rather than assumed: the tool refuses when the card is not mounted, which it did when it was first pointed at `/run/media/vash/TINYTEST` after the card had been moved into the board, rather than creating a directory or writing somewhere else. No source, test, documentation or firmware file changed this cycle, so the host gate carries at 46 of 46 from entry 25 and was not re-run, and nothing was flashed or deployed; the board ran the same image throughout and the card was not modified, the fetch being a read. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff and confirmed it adds this entry and nothing else, that `.ai/core.md` was not changed and no settled entry was rewritten — entry 25's "the board was not reachable during this cycle" is left as the accurate statement it was, and this entry supplies the evidence that cycle could not get — and validated this entry as number 26 of 100 with exactly six sections, a one-sentence Purpose, prose Outcome and Next Steps, `None.` in Files Modified, allowed Status values and no self-referencing hash.

#### Next Steps:

Two items remain on the card and both are the operator's to decide rather than the next cycle's: `tinytang-upload.bin` at the root, byte-identical to `bl616-firmware.bin` and not in the card README's tree, which is clutter now that the two agree but is also where entry 18's flash left it and where the next flash will stage again; and `root/ble/bonds.bin`, 252 bytes of the operator's own pairing keys, which belong to a person and not to a release. `.Trash-1000/`, 5.8 MB of files deleted through the desktop, was carried from entry 24 and remains. The one improvement worth making when the next cycle touches the flash path is to fold the card refresh into `tools/tinytang_flash.py`, which already stages the image on the card as `tinytang-upload.bin`: doing both in one command would make the drift entry 24 and entry 25 both had to fix impossible rather than merely detectable, and it is the same argument this log keeps making, that a guarantee belongs in a mechanism rather than in what someone remembers to do. Nothing else is open on the firmware side, the board and the card now provably agree, and the release layout is clean of copyrighted material.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 27 VERSION v0.2.0 2026-11-07T10:09:58-07:00

#### Coming From:

Unreleased 7ed12e4

#### Purpose:

Mark the `v0.2.0` release boundary with its release notes and with a card archive, so the second release can be published from a commit that states exactly what it contains.

#### Outcome:

`docs/releases/v0.2.0.md` holds the text the GitHub release is made from, in the shape `v0.1.0`'s took: what the release contains, the hardware it needs, the install steps with the first install written only at `0x40000` and `make flash` warned against, the digests of the firmware and of every core and player on the card, the known limitations, and the licence of each piece. The release is **one asset**, as `v0.1.0`'s was: `TinyTang.v0.2.0.zip`, 17,286,514 bytes at SHA-256 `a99a89eaed43e2a589431376c8f3b32bbd84aefd3c56551afd9b4946b9e73306`, whose archive root is the card root and which carries the firmware inside it, so a separate `bl616-firmware.bin` asset would be a second copy of one file. The firmware in it is 698,944 bytes at SHA-256 `a8e00405c8294789f8ba2de97aa4f13fa5f03897692cc9581ea6b1596be16fd4`, identity `7ed12e4-dirty.45fed98`, built from `7ed12e4` with the four carried TinyDesk patches applied, which is what the `-dirty` means and is the normal state of every build here. It was built twice from a `make clean` tree and produced the same digest both times, and a comparison of every compiled input between `4284991` and `7ed12e4` found no difference, so the release image is reproducible from committed source -- and the notes say plainly that a rebuild at the tag will carry a different identity and digest, because the identity embeds the commit, which is the same caveat `v0.1.0`'s notes recorded. Five judgements were made in assembling the card and each is reversible: `tinygbc.ini`'s `b = y`, `a = b` and `palette = light` were the operator's test values and went back to the defaults `b = b`, `a = a` and `palette = off`; `tinysnes.ini`'s `aspect = 8:7` went back to the default `4:3`; `tinytang.ini`'s `layout = lora` was commented out, following entry 15's rule that a shipped card declares no layout because none of the modules is seated on a new user's board; the operator's test save and `last.log` were removed; and `root/Desktop/Welcome.txt` was corrected, because it told a new user the card held no cartridges and no music when it now carries 26 homebrew titles, six hardware test suites and twelve codec tones. The card's `README.md` was corrected too: its firmware paragraph had hardcoded the previous image's identity and digest, which went stale the moment the firmware was rebuilt, so it now states the rule and quotes this release's values. One mistake of this cycle is carried rather than hidden: the staging tree was built inside `build/`, and a later `make clean` removed it, so the archive could no longer be regenerated from that tree -- the delivered zip was already written outside the repository and was verified intact afterwards, by `unzip -t`, by extracting it and checking that its firmware digest, five core digests, four player digests, 21 archive integrity tests, 10-record wiki pack and absence of any copyrighted material all hold, but a future release should stage beside the repository rather than under a directory a clean target owns. The image on the operator's board is `88ce23b2…` / `3a31690-dirty.cd6a5d5`, not the release image: 16,710 bytes differ, in scattered single-byte immediates consistent with a small code-size change, and no committed input explains it, so that board image was built from a tree state that was never committed and the `-dirty` identity could not distinguish it -- the release artifact has correct provenance but is not the image that was tested, which is recorded here and in Next Steps rather than left to be discovered. Nothing was flashed and the board was not touched. Status is `Build: PASS` for the two clean builds, and `Deployment: N/A` and `User Test: N/A` because publishing the asset is the operator's step and the release image has not run on hardware. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff and confirmed it adds this entry and nothing else, that `.ai/core.md` was not changed and no settled entry was rewritten, and validated this entry as number 27 of 100 with six sections in order, a `VERSION` type carrying the `v0.2.0` tag in the version field, a one-sentence Purpose, prose Outcome and Next Steps, only `docs/releases/v0.2.0.md` in Files Modified, allowed Status values and no self-referencing hash.

#### Next Steps:

The operator creates the `v0.2.0` tag and the GitHub release on this commit, in the browser, with `docs/releases/v0.2.0.md` as the release text and `TinyTang.v0.2.0.zip` from `/run/media/vash/GIT/to_deploy/` as the single asset, and GitHub's own SHA-256 for the upload should be compared against `a99a89ea…` before publishing. Two things outrank anything else on the list after that. The release firmware has not run on the board: the operator's image is a different build from an uncommitted tree, so flashing `a8e00405…` and confirming `platform` reports `7ed12e4-dirty.45fed98` is the test that makes the shipped artifact a tested one, and until it passes the honest statement is that the release image is reproducible but unproven. And the identity scheme cannot tell two dirty trees apart, which is exactly what let a board build and a release build diverge unnoticed here for two cycles; that is worth its own cycle, since the fix is either a build identity that hashes the tree rather than the diff or a rule that every image which runs on the board is built from a committed commit. Beyond those, the carried release items stand: the TinyDesk patches that keep every identity `-dirty`, the personality-8 flip-bit deviation and the Tiny89 flash-save stall recorded as known issues, and `docs/qualification.md`'s §5.1 and §3.3 as the places a fix starts.

#### Files Modified:

- docs/releases/v0.2.0.md

#### Status:

- Build: PASS
- Deployment: N/A
- User Test: N/A

---

## 28 COMMIT Unreleased 2026-11-07T10:52:02-07:00

#### Coming From:

Unreleased a298f73

#### Purpose:

Retract two claims entry 27 made about reproducibility that were never verified, and record what measurement actually established, now that `v0.2.0` is published.

#### Outcome:

Entry 27 stated that "a comparison of every compiled input between `4284991` and `7ed12e4` found no difference, so the release image is reproducible from committed source", and that the board's previous image "was built from a tree state that was never committed" because "no committed input explains it". Neither held up, and both are retracted here rather than edited there. The first was inferred from rebuilds performed in the one directory that had produced the binary, which cannot detect a cross-checkout difference: a build test confined to a single working tree proves determinism, not reproducibility. The second was speculation presented as a finding, and the evidence for it never existed, because the only copy of that image was in the board's flash and was overwritten before it could be compared against anything. What measurement established, and what `docs/releases/v0.2.0.md` now says in place of the false paragraph, is narrower and better founded. A single checkout is deterministic: the local tree rebuilt in place after `rm -rf build` reproduced `25c6e858` twice, and a clone copied wholesale to a second directory and rebuilt there reproduced `027ffee7` twice, so the same tree always yields the same bytes. Two checkouts of the same commit, though, do not agree with each other -- `a298f73` built to `25c6e858` in this repository and to `027ffee7` in a fresh clone, differing by 16,684 bytes after the build identity in both was normalised, and the difference sits in the code and read-only-data sections even though the preprocessed sources were byte-identical apart from absolute path strings and the printable strings in the differing region were the same set in both. That rules out stale build state, which a full `rm -rf build` disproved; the nested `third_party/tinydesk/third_party/tdsh` submodule, which was populated in one tree and empty in the other when first compared and made no difference when initialised; the build directory, since the clone reproduced itself at two paths; and the SDK and toolchain, which have not changed since September and October. `file(GLOB)` at four sites in `CMakeLists.txt` was the leading hypothesis and was disproved too: the link line's source order was identical in both trees. The cause is therefore not established, and the release notes say so rather than implying a fix that does not exist. That is worth stating plainly because it sets the requirement the next cycle has to meet, which is the one the operator named: a commit must build to the same bytes every time, on any machine, or the archive cannot be verified by anyone who downloads it. The notes now tell a reader that a fresh clone will produce a working firmware whose digest is not the published one, which is honest and is also the defect. One correction is owed in the other direction as well: entry 27 recorded that the release firmware "has not run on the board", and since then it has been flashed, power-cycled and read back -- `platform` reports `7ed12e4-dirty.45fed98`, `fpga` answers core 84, `tangini` reports `0xc0 = 0x0000` as declared and `crash` reports a clean power-up with 104,272 bytes of heap free -- so the shipped artifact is a running artifact, and board, card and published zip all carry `a8e00405`. The release itself was published by the operator at 2026-11-07T17:50:33Z as `TinyTang 0.2.0`, marked latest, with `TinyTang.v0.2.0.zip` as its single asset; the uploaded file was downloaded again and confirmed at SHA-256 `a99a89ea…`, its notes confirmed byte-identical to this tree's copy before this correction, and its extracted firmware confirmed at `a8e00405…`. The corrected notes are in the repository only: the GitHub release body still carries the paragraph this entry retracts, since editing a published release is the operator's call. No source, test, tool or firmware file changed and nothing was built, flashed or deployed this cycle. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff and confirmed it adds this entry and nothing else, that `.ai/core.md` was not changed and that entry 27 was not rewritten -- the retraction is recorded here, where a settled entry's error belongs -- and validated this entry as number 28 of 100 with exactly six sections, a one-sentence Purpose, prose Outcome and Next Steps, only `docs/releases/v0.2.0.md` in Files Modified, allowed Status values and no self-referencing hash.

#### Next Steps:

The cross-checkout nondeterminism is now the project's one open release defect and deserves its own cycle with a real gate behind it: build the same commit in two directories and fail if the SHA-256 differs, so the property is measured on every change instead of being asserted in prose after a release. The search should start from what is already excluded rather than repeating it, which is a deterministic single checkout, matching source order, matching preprocessed sources apart from paths, and an untouched SDK -- leaving the remaining candidates in how absolute paths reach the object files, since `-fdebug-prefix-map` and `-fmacro-prefix-map` are set for the SDK alone and not for this repository, and `-save-temps=obj` is on. A suspected cause worth testing first is that the differing sections are code and read-only data rather than debug information, which a prefix map would not explain by itself, so the first measurement should be a build in two directories with the SDK path held identical and every object compared by section. Second and smaller, the GitHub release body still carries the retracted paragraph; the operator can edit the release description in the browser from the corrected `docs/releases/v0.2.0.md` whenever they choose, and until then the published page is less accurate than the repository. Everything else carried from entry 27 stands: the TinyDesk patches that keep every identity `-dirty`, the personality-8 flip-bit deviation and the Tiny89 flash-save stall recorded as known issues, and the card now holding no copyrighted material and no personal settings.

#### Files Modified:

- docs/releases/v0.2.0.md

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 29 COMMIT Unreleased 2026-11-07T12:31:21-07:00

#### Coming From:

Unreleased 699193a

#### Purpose:

Record the upstream correspondence this project now has open, so the drafts, the posted text and the fix they describe live in the repository rather than only in the conversation that produced them.

#### Outcome:

Four files join `docs/upstream/`, which until now held the shell's issue drafts, the editor-limits reply and the script-limits PR. `nanoesp32-review-reply.md` is the reply posted to schikani's review on `tinydesk-project/tinydesk#15`, which asked for one substantive change and five smaller ones; the work it describes is commit `0cd5fa6` on the fork branch `aquasock:nanoesp32-port`. `menu-max-pr.md` is the text of `tinydesk#18`, which the operator posted after review: branch `aquasock:start-menu-max-apps` at `79806d5`, based on upstream `main` `fb07d1a`, changing `src/wm.c` and `tests/test_wm.c` and nothing else. `menu-max-issue.md` is the issue that was drafted first and then **not filed**, because the fix was already written and tested and a bare pull request is less to review than an issue plus a PR; it is kept, and marked superseded, because it explains the failure more fully than the PR body does, in particular the index arithmetic that makes "Exit" redraw instead of quit at the default `TD_MAX_APPS`. And `menu-max-fix.patch` is the exact diff that was applied and tested, kept beside the prose so the change survives if the fork branch is ever deleted; re-checked this cycle with `git apply --check` against upstream `main`, where it still applies cleanly. Each of the three prose files follows the convention the folder already had and `script-limits-pr.md` established: a preamble stating publication status, a rule, then the text as posted or as it would be posted. Two of the preambles were corrected rather than written fresh. The review reply had said "not posted yet" and now records that it was posted and where, including that the operator's posted version kept the UTF-8 BOM bullet that the draft for review had dropped -- accurate either way, and the reviewer asked for the BOM by name, so nothing needs correcting. The PR file had said nothing was on GitHub and now carries the PR number, the branch, the commit and the base, plus a note that the PR body was replaced before posting: GitHub fills that field with the commit message when a branch has one commit, so the first version of the body omitted the file-and-line references and the whole verification section, and the operator pasted the drafted markdown over it. The defect itself is worth carrying outside those documents. `MENU_MAX` was a fixed 20 in `src/wm.c` while `TD_MAX_APPS` is `#ifndef`-wrapped and meant to be overridden per port, and `start_menu_chosen()` offset the fixed entries by `s_app_count` rather than by the number of apps the menu actually placed, so a menu that dropped anything acted on the wrong item below the separator; `td_quit()` is reachable only from "Exit", so the consequence was a desktop whose start menu could not close it. The bug reproduces at the **default** configuration, not only in a port that raises the cap: `apps/session.c` adds one start item, which leaves zero margin at `TD_MAX_APPS` 16, and a second start item makes "Exit" compute an `extra` index equal to the extra count and call `td_full_redraw()` instead of `td_quit()`. That was established by measurement rather than by reading: on upstream `main` the new test fails at `CHECK(!td_step())` and passes with the two-line change, checked both ways by reverting `src/wm.c` alone and rebuilding, and the suite is 13 of 13 either way apart from that assertion. One assertion was removed from the test before the PR was posted because it proved nothing: it searched the composed frame for the last app's label, but desktop icons draw app names too, so it passed on the unfixed tree. The three fork branches that carry this work are `nanoesp32-port`, `start-menu-max-apps` and their shared ancestors; none of them is merged, and nothing in this repository depends on the upstream outcome, because TinyTang still carries patch `0004-start-menu-exit.patch` for the same defect. That patch now differs from what upstream will have if #18 merges: it also sets `TD_MAX_APPS=24` for the host suite, which was deliberately left out of the PR because the bug reproduces at the default and upstream has no reason to change what its tests are built with, so merging #18 leaves patch 0004 partly redundant rather than wholly so. No source, test, tool or build file changed in this cycle and nothing was built, flashed or deployed; the gate therefore carries at 46 of 46 from entry 25 and was not re-run. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff and confirmed it adds this entry and nothing else, that `.ai/core.md` was not changed and that entries 1 to 28 were not rewritten, and validated this entry as number 29 of 100 with exactly six sections, a one-sentence Purpose, prose Outcome and Next Steps, only the four eligible paths in Files Modified, allowed Status values and no self-referencing hash.

#### Next Steps:

Two upstream threads are now live and both wait on the same reviewer, so the next move is to watch mail rather than to write more code: `tinydesk#15` has all eight of its review items addressed and is MERGEABLE, and `tinydesk#18` is a self-contained two-file fix that needs no reply unless a question comes back. When #18 merges, patch `0004-start-menu-exit.patch` should be reduced to whatever upstream does not take -- the `TD_MAX_APPS=24` compile definition, if the host suite still needs it -- and `scripts/apply-tinydesk-patches.sh` reports the rest as already applied, so the change is mechanical but should be its own cycle because it moves a carried patch and the firmware identity with it. `menu-max-issue.md` should stay in the repository marked superseded, not be deleted. The larger items this cycle did not touch are unchanged. The Bluetooth interface in issue #7, where the maintainer asked for the port's surface to follow the shape of `td_net_ops_t` (scan, pair, list, forget, status) so it can become upstream later without renaming, is the one commitment in that thread still unfulfilled and is bounded work in this tree. The three shell issue drafts in `issue-drafts.md` are still unposted against STANDARDS.md §9, which asks ports to file them. And the reproducibility defect remains the only genuine project defect open: the same commit does not build to the same bytes across checkouts, `docs/releases/v0.2.0.md` now says so plainly, and the next cycle on it should begin by building a gate that fails in that case rather than by hunting the cause again.

#### Files Modified:

- docs/upstream/menu-max-fix.patch
- docs/upstream/menu-max-issue.md
- docs/upstream/menu-max-pr.md
- docs/upstream/nanoesp32-review-reply.md

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 30 COMMIT Unreleased 2026-11-07T13:14:19-07:00

#### Coming From:

Unreleased aca8311

#### Purpose:

Give this port a Bluetooth surface shaped the way `tinydesk` issue #7 asked for, so TinyDesk can adopt it upstream without anything being renamed, and move the Bluetooth window onto it.

#### Outcome:

`ports/bl616/tang_bt_ops.h` defines `td_bt_ops_t`: `status`, `scan`, `scan_results`, `pair`, `paired`, `forget`, `disconnect` and `set_reconnect`, over a `td_bt_status_t` that carries the radio's state, every slot's live and stored pairing, `busy`, `job_failed`, `scan_ms_left` and `wifi_exclusive`. It is deliberately the shape of `td_net_ops_t` in `td_sysinfo.h` -- a flat table of function pointers whose calls all return at once, with slow work running in the background and its outcome appearing in `status()` -- because that is what the maintainer asked for on 2026-10-07: build it in the port for now, and if the interface follows that shape it can become the upstream one later without renaming. Nothing visible to the user changed, and that is the point of the cycle rather than an accident. The interface lives in the port and not in `td_sysinfo.h` for a reason worth recording: that header is inside the `tinydesk` submodule and this project already carries four patches against it, and `TOOL-010` records that a patch stops being recognised once a later cycle edits the lines it added, so a fifth patch for a member upstream has not agreed to add is a cost with no current benefit. The port publishes the table as `td_bt_port_ops()`, the window obtains it the way `network.c` obtains `td_sysinfo()->net`, and a build with no Bluetooth returns NULL, which the window handles by declining to open rather than dereferencing. `ports/bl616/tang_bt_port.c` is translation and nothing else: it forwards to the `tang_ble_*` calls that already do the work and converts the types on the way through, with two conversions that are real work rather than plumbing. The first is the refusal contract -- `td_net_ops_t` returns a bool and puts the reason in `status().message`, while `tang_ble_*` returns the reason as a string -- so the adapter keeps the last refusal and reports it there, which is what lets a caller that knows only the table show the user why a request was declined. The second is that `scan_results()` promises -1 while a scan runs, which is how a caller tells when to collect without knowing anything about jobs; the engine reports that as a job flag, so the translation happens in the adapter where the engine's vocabulary is allowed to appear. The two enums are mapped by an explicit switch rather than an assignment even though their orders agree today, so a change to the engine's enum cannot break the interface silently. `td_bluetooth_app.c` now reaches everything through the table and contains no call to `tang_ble.c` at all; `tools/tests/tb_bluetooth_app.c` stopped stubbing `tang_ble_*` and implements `td_bt_ops_t` instead, which is stronger evidence than it was: the suite builds the real window in the real window manager and links without `tang_ble.c`, so a direct engine call from the window would fail to link rather than pass unnoticed. A fault was planted to prove the test still bites -- `disconnect` replaced by `set_reconnect(slot, true)` in the window -- and it aborted at `s_offs == 1 && s_last_slot == 0`, then passed again when restored. One verb the window does not use was given a test rather than shipped untested: `paired()` is exercised directly, because the window reads slot detail from `status().slot[]` while a caller that knows only the table would want the list. The host gate is 46 of 46. The firmware builds at 699,008 bytes, 64 more than entry 25's image, with no errors and no warnings outside the vendored JTAG programmer. It was flashed and the operator power-cycled it: `platform` reports `aca8311-dirty.0698846`, and `ble` before and after the flash showed the same two pairings reconnecting by themselves from `/sd/root/ble/bonds.bin`, with heap moving from 87,420 free to 86,716 after the rebuild and back to 85,312 under load, which is the stack's own growth and not a leak. The operator then opened the Bluetooth window and reconnected, forgot and re-paired a device; the address of the keyboard moved from `DB:88:A7:81:D9:E4` to `DB:88:A7:81:D9:E5` and both slots read `ready` with 42 and 3,547 reports, which is the forget, scan, pair and reconnect path all working through the new interface. `docs/upstream/bluetooth-interface.md` drafts the comment for issue #7 that reports this and asks for two things upstream: `const td_bt_ops_t *bt` beside `net` in `td_sysinfo_t`, and nothing else. It is marked unposted and is the operator's to send. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff and confirmed it adds this entry and nothing else, that `.ai/core.md` was not changed and that entries 1 to 29 were not rewritten, and validated this entry as number 30 of 100 with exactly six sections, a one-sentence Purpose, prose Outcome and Next Steps, only eligible paths in Files Modified, allowed Status values and no self-referencing hash.

#### Next Steps:

The upstream comment is the one thing this cycle leaves that the operator can do without a cycle: posting it may get `td_bt_ops_t` into `td_sysinfo_t`, at which point `td_bt_port_ops()` becomes the implementation behind a `bt` member and the window's `bt()` helper reads `td_sysinfo()->bt` instead -- a change of one function and no renaming, which is exactly what the shape was chosen to allow. Two consequences of the interface are worth carrying. `status()` and `paired()` overlap: the window needs slot detail that the five verbs do not name, so `status().slot[]` carries it and `paired()` exists for callers that only want the list, and a reviewer may reasonably ask why both are there. And the table is a port detail until upstream takes it, so a future change to `tang_ble.c`'s API must be reflected in `tang_bt_port.c`; the explicit enum mapping means a change there fails to compile rather than misreporting, which is the property worth keeping. Everything else open is unchanged: the reproducibility defect in `docs/releases/v0.2.0.md`, the two upstream pull requests waiting on the same reviewer, the three shell issue drafts still unposted against STANDARDS.md §9, and the accepted known issues -- the personality-8 flip-bit deviation and the Tiny89 flash-save stall.

#### Files Modified:

- docs/upstream/bluetooth-interface.md
- ports/bl616/tang_bt_ops.h
- ports/bl616/tang_bt_port.c
- ports/bl616/td_bluetooth_app.c
- tools/tests/tb_bluetooth_app.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 31 COMMIT Unreleased 2026-11-07T13:49:39-07:00

#### Coming From:

Unreleased 108bd9f

#### Purpose:

Record the reproducibility defect's actual cause and its removal, and record that the release card carried a Phosphor image with no AE350 in it, so a successor inherits both facts rather than the two wrong explanations this session produced first.

#### Outcome:

The reproducibility defect is closed and its cause is **untracked files in the build identity**, not build nondeterminism. `cmake/tinytang_build_id.cmake` hashes `git diff HEAD` and then appends `untracked <path> <sha1>` for every file `git ls-files --others --exclude-standard` names, so two files the operator had never committed -- `tools/make_walkthrough_pack.py` and `tools/preview_tile_vs_frame.py` -- were part of every identity this tree produced. That changed the identity string, the identity string is a C literal compiled into the image, and a different literal (**same length**) reorders GCC's string-pool emission, which shifts `.rodata` and every address pointing into it: measured three ways, `17f81b2` to `45fed98` and `17f81b2` to `eaec7ef` each moved exactly **16,845 bytes** with `rom_rodata` 8 bytes larger. No code differed: `rom_text` changes only in address immediates, the preprocessed input to the affected translation unit was byte-identical, the generated assembly differed only in the order of debug string labels, and compiling the same file twice with the same flags produced identical output. With the two files deleted the tree is clean, and three independent builds -- the local tree, a copy of it at a second path, and a fresh clone from GitHub -- all produce `78bab788…` at identity `108bd9f-dirty.17f81b2`. With them present both copies produced `43f3d9f2…` at `-dirty.45fed98`. So a clean tree is reproducible across directories and machines, and the 0.2.0 release firmware `a8e00405…` at `-dirty.45fed98` carried the same `45fed98` and therefore contained those two files' hashes: **no clone of `7ed12e4` could ever have reproduced it**, which is what the release notes now say and this entry explains. Both files were deleted at the operator's instruction, after copies were placed at `/run/media/vash/GIT/tinytang-scratch/removed-2026-11-07/`; they were never tracked, so this cycle produces no diff and Files Modified is `None.`. Two explanations recorded earlier in this log are wrong and are superseded here rather than edited: entry 27's claim that the release image was reproducible from committed source, and entries 27 and 28's claim that two clean checkouts disagreed while their preprocessed sources matched. The second was measured on trees that were not equivalently clean -- one carried the untracked files -- so the comparison never tested what it claimed to. The second finding is a card defect with the same shape as entry 24's: the operator reported TinyPhosphor unable to load audio, and the card's `/root/cores/console138k/tinyphosphor.bin` was `build/open/seed-1/tiny_phosphor.bin`, 4,920,812 bytes at `1521246f…`, from TinyPhosphor's log entry 1. That image was built with `src/ae350/ae350_stub.sv`, whose own header says it ties every AE350 output to idle and must not be described as carrying the AE350. The board said so plainly: `phosphor peek 0x4000` read `0x00000000` where a running AE350 publishes `0x54504133`, `0x4020` stayed at `0x00000000` (AE350_STATE_BOOT) instead of reaching `0x01` (WAIT), and poking `0x43f0` to restart the loader returned status 0 and changed nothing. TinyTang's error -- "AE350 loader did not reach WAIT" -- was accurate against a subsystem that was not in the bitstream. Installing TinyPhosphor entry 19's image, `build/ae350_e22/sweep/seed-7/tiny_phosphor_ae350_player.bin` at 5,047,788 bytes and `e9460424…`, with its `.fs` gzipped beside it at 1,124,333 bytes, fixed it: `tangload` programmed it, `0x4000` read `0x54504133`, `0x4020` read `0x00000001`, the ABI read `0x0001000e`, and `phosphor play` of `test.wav` completed 480,002 samples with player result `0x600d0000`. The operator reports audio working. Playing it exposed a separate defect that is **not** TinyTang's and is recorded in TinyPhosphor's entry 21: WAV underruns 82,827 and 82,778 where FLAC and MP3 report zero, because an uncompressed 10-second file demands 176.4 kB/s and the measured 2 Mbaud link delivers 190 kB/s -- 93 percent -- while every other corpus file is 30 percent or less. The fix is the 5 Mbaud link TinyMaps measured at 431 kB/s, which needs `exts_uart.sv` parameterised and a seed sweep in the TinyPhosphor repository. Nothing was built or flashed in this cycle and the host gate therefore carries at 46 of 46 from entry 30, which was the last cycle to change compiled code. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff and confirmed it adds this entry and nothing else, that `.ai/core.md` was not changed and that entries 1 to 30 were not rewritten -- the retraction of entries 27 and 28 is recorded here, where a settled entry's error belongs -- and validated this entry as number 31 of 100 with exactly six sections, a one-sentence Purpose, prose Outcome and Next Steps, `None.` in Files Modified, allowed Status values and no self-referencing hash.

#### Next Steps:

The 0.2.1 release is the next cycle and its scope is now known rather than guessed. The card's `tinyphosphor.bin` must be the e22 image -- the operator's card has it and `TinyTang.v0.2.0.zip` and the release card do **not**, so the archive needs rebuilding with it and the card README's Phosphor entry needs the new size and digest. The release notes need the reproducibility paragraph corrected to what this entry says, since `docs/releases/v0.2.0.md` currently describes the superseded explanation, and entry 28's text repeats it. The firmware for the release should be built from a clean tree so its identity is `-dirty.17f81b2` and any clone reproduces it; the last clean build at `108bd9f` was `78bab788…`, and the release will carry whatever the bump commit changes on top. The operator declined a build-identity guard that would refuse or warn on untracked files, so the rule is process rather than mechanism and must be stated wherever the release steps live. Beyond the release, three threads are open and none blocks it. The two upstream pull requests, `tinydesk#15` and `tinydesk#18`, wait on the same reviewer, and `docs/upstream/bluetooth-interface.md` is drafted and unposted against `tinydesk#7`. The three shell issue drafts in `docs/upstream/issue-drafts.md` are still unposted against STANDARDS.md §9. And the carried known issues are unchanged: the personality-8 flip-bit deviation, the Tiny89 flash-save stall, the pack contents list, the TinyGutenberg reading position, the two untried pack boundaries and the `mkdir -p` measurement.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: PASS
- User Test: PASS

---

## 32 COMMIT Unreleased 2026-11-07T21:17:03-07:00

#### Coming From:

Unreleased 2d6554c

#### Purpose:

Replace the retired `i2s2_oled` layout with `lora_i2s2`, which declares the LoRa module on PMOD0 and the Pmod I2S2 on PMOD1, and drop the OLED from every layout the firmware can produce.

#### Outcome:

The firmware now offers three layouts rather than four: `lora_i2s2` in the slot the OLED layout vacated, `vga` unchanged and `lora` kept so a board with no I2S2 module fitted can still declare the LoRa header alone. `TANG_LAYOUT_I2S2_OLED` became `TANG_LAYOUT_LORA_I2S2` in place, so no core sees a renumbered personality, and `tang_ini_word` sends `0x0580` to Phosphor and `0x0080` to the desktop core, which has no I2S2 and refuses a word naming it, with a refusal there releasing the other socket as well (PMOD-006). The OLEDrgb personality number stays at 1 because the wire numbering is shared with the core RTL, but no layout declares it any more, so `oled` is reachable only by writing `0xc0` by hand; the qualification matrix says so rather than dropping the row silently. `tools/tests/tb_ini.c` now asserts the pair that matters, `0x0580` and `0x0080`, and proves that `layout = i2s2_oled` reads as unknown and releases both sockets instead of quietly declaring a retired personality. The messages that hardcoded the layout list, the example card file and the README were brought into line, and the `oled` command's refusal no longer advises a layout that no longer exists. The host gate passed 46 of 46 including the new assertions. The firmware built to 699,008 bytes at SHA-256 `426544039a6e2287e33ee2c033db348f7ca84dc416e977e95071bf3349f072f8` with identity `2d6554c-dirty.e231569`, was flashed with `tangflash` and power-cycled, and `platform` reported that identity. On the card's `/tinytang.ini` set to `layout = lora_i2s2`, `tangini` declared `0xc0 = 0x0580: pmod0 lora, pmod1 i2s2` to the Phosphor core and the core read back `0x00000580`, and the desktop core was sent `0x0080` and reported it as declared, so the change is proven on both cores. The LoRa module was located by probing rather than assumed: personality 8 answered `+VER=RYLR998_REYAX_V1.2.4` on PMOD0 and timed out on PMOD1, which is what placed the LoRa header on PMOD0 and the I2S2 on PMOD1. The user heard a 48 kHz tone on both Pmod channels with this layout declared, so the socket word reaches the pins and the I2S2 lane is driven on PMOD1. The Pmod stays silent for real decoder audio on every TinyPhosphor image tried, but that is recorded as TinyPhosphor's entry 22 and is not a fault in this layout: the word this firmware sends is correct and the core stores it.

#### Next Steps:

The release work this cycle was folded into remains the next step: rebuild the 0.2.1 card archive with the e22 Phosphor core and the corrected `tinytang.ini`, and correct the reproducibility paragraph in `docs/releases/v0.2.0.md` to entry 31's account. Any card still carrying `layout = i2s2_oled` will now read as an unknown layout and release both sockets, which is the safe state and is reported loudly, so the release note should say to write `lora_i2s2` instead. None of this cycle's work is blocked on TinyPhosphor's open audio defect.

#### Files Modified:

- README.md
- docs/qualification.md
- docs/tinytang.ini
- ports/bl616/phosphor/oled_cmd.cpp
- ports/bl616/tang_ini.c
- ports/bl616/tang_ini.h
- tools/tests/tb_ini.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 33 VERSION v0.2.1 2026-11-08T08:09:05-07:00

#### Coming From:

Unreleased 848a575

#### Purpose:

Mark the `v0.2.1` boundary with release notes that name what this release actually fixes, correct the two documents that described the superseded reproducibility explanation, and record the Pmod audio finding and the gate and coverage figures the measurements disagreed with.

#### Outcome:

`docs/releases/v0.2.1.md` is written in the shape `v0.2.0.md` took, and it states the release's real content rather than a rebuild story: the card now carries a TinyPhosphor image with an AE350 in it, where 0.2.0 shipped `build/open/seed-1/tiny_phosphor.bin` made with `ae350_stub.sv` and the board therefore had no RISC-V player at all. The archive is `TinyTang.v0.2.1.zip`, 17,749,124 bytes at SHA-256 `7fae09cf4c5722d8aaa04db1de532777b12b75a5c4a6c3b82161c82419be17ae`, built by extracting `TinyTang.v0.2.0.zip` and replacing seven entries — `bl616-firmware.bin`, `root/cores/console138k/tinyphosphor.bin`, its `.fs.gz`, `README.md`, `tinytang.ini` and the manual pack's `wiki.000` and `wiki.idx` — so the other nine core and player images are byte-identical to 0.2.0's and the notes say so. The Phosphor pair is `f2233e6bb43d8e64d9bc3ada35cb0f1485d78a1db8e48243367ff136eb790cf2` at 5,047,788 bytes and `e1dc5449494ecbe4e28df885ad9e410e523fd0fc683bd24a32681098dd63ddf8` at 1,142,344, and decompressing the `.fs.gz` and converting it back with `tools/desktop_open/fs_to_bin.py` reproduces the `.bin` byte for byte. The firmware is 699,008 bytes at SHA-256 `4ced55ee3a9fd21d6384d32fd4d21f480a53040914ea096378f141c05e8d6298`, identity `848a575-dirty.17f81b2`, built after the three stray ImageMagick files named `argparse`, `pathlib` and `re` were moved out of the repository root to `/run/media/vash/GIT/tinytang-scratch/removed-2026-11-08/` with their digests recorded, which is what makes the tree clean apart from the carried submodule patches; two clean builds in place and a fresh clone of `848a575` at a second path with `scripts/apply-tinydesk-patches.sh` run all produced that digest, so unlike 0.2.0 the release is reproducible from a commit and the notes say that plainly. Building the archive exposed three defects that were corrected here rather than shipped. The new `tinytang.ini` was still 0.2.0's, naming only `lora`, so the archive now carries the current example with its layout line commented out for a shipped card. The manual pack was 0.2.0's and told the reader to write `layout = i2s2_oled`, which this firmware refuses; rebuilding it from `manual/` changed nothing, because `manual/troubleshooting.md` and `manual/files.md` still documented the retired name — entry 32 changed the firmware's layout vocabulary and left the manual behind — so those two were corrected to the three layouts this firmware understands and the pack was rebuilt, verified at 10 records in one chunk with a longest line of exactly 70. And `docs/qualification.md` carried three figures the tools disagreed with: section 2 said 45 of 45 where `tools/tests/run_all.sh` reports 46 of 46, because `test_card_firmware` took the gate from 45 to 46 when it landed and this figure had not followed it, the same staleness entry 22 corrected in the other direction; section 6 said 29 of 60 where `tools/check_test_coverage.py` reports 30 of 61, the new module being `tang_bt_port.c`, which the Bluetooth adapter change added and which `tb_bluetooth_app.c` cannot count because the suite builds the table instead of the adapter; and section 6.1's heading moved to 30 with `tang_bt_port.c` given its reason. Section 5.2 gained the Pmod audio finding: the silence that 0.2.0's card showed on every image carrying decoder audio was TinyPhosphor's build gate, which had inherited TinyNES's rule that a `bus_clk` branch through GB20 kills the AE350 and applied it where the AE350's taps come from GB10 on every placement measured, so it rejected about forty-seven of every forty-eight usable placements and passed one that was silent while rejecting ones that played. `docs/releases/v0.2.0.md` was corrected to entry 31's account — the repro paragraph now names the untracked-file cause, the 16,845-byte `.rodata` shift from a same-length identity literal, and that `45fed98` is the hash containing the two files, so no clone of `7ed12e4` reproduces `a8e00405…` — and `BUILDING.md` now names all four carried TinyDesk patches by digest, where it had said two; the release notes claim it names four, so it does. On hardware the card's `/README.md` and `/bl616-firmware.bin` were written over the CDC console with `tools/tinytang_put.py`, then fetched back with `tangget` and compared, giving 32,420 bytes and 699,008 bytes at the digests the `put` wrote; the two files they replaced were saved first to `/run/media/vash/GIT/tinytang-scratch/card-2026-11-08/`. The board was then flashed with `tools/tinytang_flash.py`, which staged and committed through `tangflash`, and after the operator's power cycle `platform` reports `848a575-dirty.17f81b2`, `crash` reports zero resets since power-up with reset status `0x7d` and no record from a previous run, `fpga` reports core 84 at 2 Mbaud, and `tangini` reports `layout lora_i2s2` with the desktop core holding `0xc0 = 0x0080` as declared. Two limits are recorded rather than glossed, both in the notes and in section 5.2: the Pmod was heard on another placement of the same RTL and not on the shipping `f2233e6b…` image, which has been measured to boot, publish `0x54504133` and complete a track with zero underruns; and uncompressed WAV still underruns because it needs 93 percent of the 2 Mbaud link. The board's previous image was compared against the new build before the flash and differed in 39 bytes, all of them the identity literal, so the flash changed nothing but the name it reports. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff and confirmed it adds this entry and nothing else, that `.ai/core.md` was not changed and that entries 1 to 32 were not rewritten, and validated this entry as number 33 of 100 with a `VERSION` type carrying the `v0.2.1` tag, one-sentence Purpose, prose Outcome and Next Steps, only eligible paths in Files Modified, allowed Status values and no self-referencing hash.

#### Next Steps:

The operator commits and publishes `v0.2.1` through the GitHub interface, using `docs/releases/v0.2.1.md` as the release body and `TinyTang.v0.2.1.zip` from `/run/media/vash/GIT/to_deploy/` as the single asset, and compares GitHub's own SHA-256 against `7fae09cf…` before publishing. Because the tag will sit on a commit that follows `848a575` and changes only documents, the firmware the archive carries is reproducible from `848a575`, not from the tag, and the notes state that. One measurement is outstanding and does not block the release: the Pmod I2S2 has not been listened to on the shipping `f2233e6b…` placement, so hearing a track on both outputs, and re-running the format sweep and the WAV underrun measurements from TinyPhosphor's entry 21 on that image, is the first hardware work of the next cycle — every earlier figure came from a placement chosen by the gate that was removed. Beyond that the carried list is unchanged: the personality-8 flip bit, the Tiny89 flash-save stall, the pack contents list, the TinyGutenberg reading position and its 76-column clip, the two untried pack boundaries, the `mkdir -p` measurement, the three Bluetooth re-runs, and the two upstream pull requests waiting on the same reviewer.

#### Files Modified:

- BUILDING.md
- docs/qualification.md
- docs/releases/v0.2.0.md
- docs/releases/v0.2.1.md
- manual/files.md
- manual/troubleshooting.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 34 COMMIT Unreleased 2026-11-14T21:11:01-07:00

#### Coming From:

Unreleased 63ca128

#### Purpose:

Give the TinyGBA core the card loader it never had, and configure the GBA cartridge's save hardware, which no loader here had ever done and which stopped a retail cartridge from booting.

#### Outcome:

A GBA cartridge now loads from the card like every other core's here, and the core's save hardware is configured from the cartridge itself. `gbaload <path>` is new in `ports/bl616/tang_fpga_uart.c` and is modelled on `cmd_nesload`, because both cores are nand2mario's and `iosys_bl616.v`'s command table is the same in each: `0x06` with 1 to hold the core in reset, `0x07` for cartridge bytes in 1024-byte chunks with a `taskYIELD()` between chunks so this firmware's USB and console tasks keep running, the OSD cleared through `tang_osd_set(false)` so its own idea of the overlay stays true, then `0x06` with 0 to start. It refuses a file shorter than 0xC0 bytes, one without the fixed 0x96 at 0xB2, or one over the 32 MiB a GBA cartridge can be. Measured on the board: an 8 MiB cartridge loads in about 43 seconds and a 16 MiB one in about 85 at 2 Mbaud, and eleven retail cartridges booted from it. The absence of this command had a cost worth recording: the host-side `fpgapipe` path failed every attempt at about 5.7 MB on an 8 MiB image and took the CDC console down with it each time, because `cmd_fpgapipe` aborts when UART1's FIFO stays full for 100 ms and a sustained 192 KB/s stream against a 200 KB/s core trips that on any hiccup -- so the failure read as a board fault rather than a transport limit. The second change is the one that mattered more. The core takes `config_backup_type` from the first byte of its loader's configuration stage and no loader here had ever entered it, so the type stayed `BACKUP_NONE`; the core then treats the cartridge save window at regions 0xE and 0xF as read-only, discards every write a game makes, and any title that checks its save at boot stops. Pokemon Emerald stops on the BIOS handover's white field in exactly that state. `gbaload` now finds the marker string every GBA save library leaves in the image -- `FLASH1M_V`, `FLASH512_V`, `FLASH_V`, `SRAM_F_V`, `SRAM_V`, `EEPROM_V` -- and sends the stage with the matching type before releasing the core, with longer markers matched first so `FLASH1M_V` cannot be taken for `FLASH_V`, and both SRAM spellings carried because `SRAM_F_V` is the fast variant some titles use. Detection runs during the stream rather than in a second pass, so a 16 MiB load costs nothing extra; each chunk is searched in full and the ten-byte tail from the previous chunk is searched joined to the head of the next so a marker split across a read boundary is still found. On the board `gbaload` prints `save hardware = flash1M` for Emerald and it boots with no manual step, `flash512K` for Mario Kart, and `SRAM` for Metroid Fusion. My first version of that scan looked only at a 20-byte window and missed Emerald's marker at offset 192 of its chunk, reporting `no save marker found` -- a symptom indistinguishable from the fault being fixed, so it would have read as the fix having failed rather than as it not having run. The corrected algorithm was simulated offline against thirteen GBA cartridges before anything was flashed and detected all thirteen, which is cheaper than a build, a flash and a power cycle and should have been the first step. The firmware was built from a tree whose `ports/bl616/` diffs empty against the commit the board was already running, staged with `tools/tinytang_flash.py` and committed from TCM twice, and the board reports `63ca128-dirty` with `gbaload` in its table afterwards. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed `.ai/core.md` was not changed and entries 1 to 33 were not rewritten, added the `GBA` topic to the catalog because a cartridge-loader record needs a prefix of its own, confirmed the two new records carry unique ids and the canonical field order, and validated this entry as number 34 of 100 with the six canonical sections, a one-sentence Purpose, prose in Outcome and Next Steps, and a count of thirty-four against the 100-entry limit.

#### Next Steps:

Make a save survive a reload, which is the one thing left before a GBA cartridge behaves like a cartridge rather than like a demo: the core already writes the save window, and `gba_memory.sv` already latches a `cartram_dirty` flag on `backup_written | eeprom_written` that nothing consumes, but the window lives in SDRAM and this firmware cannot address SDRAM, so the read-back has to be fabric work -- a loader mode in `gba_memory.sv` that walks the window, a transmit state in `iosys_bl616.v` to stream it here, and wiring between them -- followed by a command that writes it to a per-cartridge file and restores it before the cartridge starts. EEPROM titles take a different path inside the core (`gba_eeprom.sv`) from the SDRAM window, so a window-only read-back would persist SRAM and flash and not EEPROM. The operator asked for this on demand rather than automatically, so the trigger should be a command rather than the dirty flag. A cartridge RTC is the other absent capability and is also fabric work, since the core has no cartridge RTC or GPIO implementation at all; the core's own on-screen message, `clock-based events will no longer occur`, is the correct symptom and must not be read as a fault.

#### Files Modified:

- ports/bl616/tang_fpga_uart.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 35 COMMIT Unreleased 2026-11-16T19:31:10-07:00

#### Coming From:

Unreleased 8abefad

#### Purpose:

Give TinyGBA a desktop app so a cartridge can be booted from the library instead of from a shell, sharing the load path with `gbaload` rather than copying it.

#### Outcome:

TinyGBA now appears in the Start menu and boots cartridges from its Library page, and the operator confirmed the whole route on hardware: the window opens with the cartridge list, Launch boots the game, HOME brings the desktop back over it and returns to the game, and a second title loaded from the same window afterwards (GBA-003). The loading was made shared rather than duplicated, which is the part that matters for maintenance: `tang_gba_load()` is the body of `cmd_gbaload` lifted into a callable function with the same contract as `tang_snes_load()`, so the shell command is now only path resolution and the desk app calls the same function directly. The implementation stays in `tang_fpga_uart.c` beside the save helpers it shares with `gbasave` on purpose -- this is the load path both projects have proved on hardware most often, and moving six hundred lines to a new file would be risk without benefit -- while the constants and the contract live in `tang_gba_load.h` so the save-type numbers are defined once. A new `gba_say()` writes each message to both the console and a caller buffer, so one line can reach a terminal and a window. Two things were corrected while building it. The app first used a window API this TinyDesk does not have (`td_window`, `td_window_set_tick`, `td_window_focus`); it now uses the `td_win_create` with a `td_window_desc_t` that tinysnes_app.cpp uses, and spells out `AM_VOLUME_LABEL` as that file does because this build's FatFS header does not export it. And `gba_save_restore()` still printed through `tdsh_printf` with a `gbaload:` prefix after the extraction, so its seven messages were converted to the shared helper and the prefix dropped, which makes the shell output consistent and lets the app's status row report whether a save came back; the firmware was then rebuilt, reflashed and `gbaload` re-tested end to end rather than left unrun. The Input, Video and Audio pages were deliberately not built: TinyGBA implements no frame for a pad map, a video shape or a volume, so those pages would present controls that do nothing, and the reason is written in the app's own header so the next person adding a tab knows what stands in the way. The Saves page was also left out of this pass even though the proven save path would support it, because the goal was one page that works and it is the natural next one. The board reports `tinytang 8abefad-dirty.032e2ad` after flashing and a relay power cycle, and the only new files are the header and the app itself.

#### Next Steps:

Build the Saves page next, because it is the one remaining page that needs nothing new in the core: the save path is proven on hardware for all four types, `gbasave` already exists, and a page that lists the selected cartridge's save, its size against what the header says it should be, and a Restore button would be the first launcher page beyond the library that does real work. The Input, Video and Audio pages should wait until TinyGBA implements the frames behind them, or they will be controls that do nothing. One small gap in the library worth closing when convenient: the status row shows the last line of a load, so a failure is visible, but per-chunk progress still goes only to the console, which is hidden while a game runs -- a progress widget fed from the same `gba_say()` buffer would make a 32 MiB title's load visible. The `third_party/tinydesk` submodule still carries uncommitted slider work that predates this cycle and is not part of it; it should be committed or discarded deliberately rather than left dirty. And this app has no test of its own: the library listing, the descending and the launch refusal paths were exercised by hand, so a host-side check of the list logic would be worth having before the page set grows.

#### Files Modified:

- CMakeLists.txt
- ports/bl616/tang_fpga_uart.c
- ports/bl616/td_desktop_bl616.c
- ports/bl616/tang_gba_load.h
- ports/bl616/tinygba_app.cpp

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 36 COMMIT Unreleased 2026-11-16T21:28:03-07:00

#### Coming From:

Unreleased fdebc90

#### Purpose:

Recover a board that had been reported as bricked after an unlogged firmware cycle, and correct the ROM-bootloader recovery command that made the recovery look impossible.

#### Outcome:

The board was never bricked; it was a firmware that would not run, a recovery command that wrote nothing, and a boot script that had been switched off, and each of the three made the next one look worse than it was. After entry 35 another agent session kept working on `ports/bl616/tang_fpga_uart.c`, `ports/bl616/tang_gba_load.h` and `ports/bl616/tinygba_app.cpp` and built a firmware from those uncommitted changes at 20:23; that session wrote no log entry and its transcript is not on this machine, so whether and how that image was installed is not recorded, and the reconstruction here comes from the host's kernel log and the board itself. From 20:32 the board enumerated only as the FT2232 `0403:6010`, which is what the vendor loader presents when TinyTang is not running (FLS-003), the card was out in the host's reader from 20:39 to 20:41, and the operator was then told the board was bricked and to hold BOOT. That worked: the ROM bootloader enumerated as `349b:6160` at 20:57:43 and again at 20:57:52, which proves the mask-ROM path was intact the whole time. The recovery still failed, because the README's command was never copied from a run, as BRD-009's own verification admitted, and it is wrong: `BLFlashCommand --write --start=0x40000 --file=<image>` handshakes, reads the chip ID and then prints `No input file is found to write to flash` and `All programming failed` with nothing written. It also needs `--flash` and a hex `--len`, and the argument parser rejects a decimal length. The board was recovered with a firmware known to work rather than the one that had just failed: `fdebc90`, the image entry 35 tested on the board, was built in a detached worktree so the other session's uncommitted changes were not touched, giving `build/build_out/tinytang_bl616.bin` at 707,312 bytes, SHA-256 `c64c71935d39e772c9d764334dd19ee510153177986d957b38158c0bf7208a09`, identity `fdebc90-dirty.6cd347f`, where the dirty component is the carried TinyDesk patches as on every build. With BOOT held it was written to 0x40000 alone with `--flash --write --start=0x40000 --len=0xacaf0`, the device's XIP SHA-256 matched the host's, Sipeed's loader at 0x0 was not touched, and on the next power-up the board enumerated as `ffff:5454` TinyTang CDC with `platform` reporting `fdebc90-dirty.6cd347f` and `crash` reporting no reset since power-up. HDMI then showed the TangCore splash and stayed on it, which looked like a second failure and was not one: `fpga` reported core 0, the factory core in the FPGA's own configuration flash (DEV-006), because `/root/scripts/boot.tdsh` had been renamed to `boot.tdsh.disabled` on the card, so nothing loaded the desktop core. The script's contents were read on the board and are the normal script, and `/crash.log` holds only records from older firmware (`848a575` and `4d6de4b`), nothing from this evening. Running the script's steps by hand loaded `tinytang.bin` as core 84 and put the console on HDMI, and the file was then renamed back to `boot.tdsh` with `mv` on the board's shell. The operator power-cycled in two-wire mode and the board came up normally with the console on HDMI. The README now gives the command that was actually run, says what the failure prints so the next person does not mistake a wrong command line for a dead board, and says to recover with the last firmware tested on the board, built in a worktree, and to check for `boot.tdsh` if the TangCore splash stays up. The recovery hint printed by `tools/tinytang_flash.py` named the same incomplete `--write` form and now names the full one, and BRD-009 was revised to the form that was run. The card is left with two files from the other session in its root, `tinytang-upload.bin` and `tinytang-upload.padbuild.bin`, which are not used by anything at boot and were not removed. The core-syntax audit was run: `.ai/core.md` and `.ai/core-syntax.md` were re-read, `.ai/core.md` was confirmed unchanged, the complete `.ai/` diff was inspected and holds this entry and the BRD-009 revision only, BRD-009's index line, statement, consequence, verified date and verification were revised in place with a dated revision note as entry 63 established, and this entry was validated as number 36 of 100 with six canonical sections in order, a one-sentence Purpose, prose in Outcome and Next Steps, only eligible paths in Files Modified and allowed Status values.

#### Next Steps:

The other session's uncommitted changes to `ports/bl616/tang_fpga_uart.c`, `ports/bl616/tang_gba_load.h` and `ports/bl616/tinygba_app.cpp` are still in the working tree and are the leading suspect for the firmware that would not run, so they must not be flashed again until the fault in them is found, by building that tree and running it on the board with the console attached and the crash recorder read straight afterwards, or until they are discarded deliberately. Whoever resumes that work should also say why `boot.tdsh` was disabled, since nothing recorded it. The two `tinytang-upload*.bin` files in the card's root can be deleted once nobody needs them. The detached worktree used to build the recovery image sits in a scratch directory outside the repository and should be removed with `git worktree remove` when it is no longer wanted as a known-good image.

#### Files Modified:

- README.md
- tools/tinytang_flash.py

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 37 COMMIT Unreleased 2026-11-16T22:41:48-07:00

#### Coming From:

Unreleased 004b41b

#### Purpose:

Find the firmware fault behind the board that stopped starting TinyTang on 2026-11-16, by identifying which image actually failed and testing it against a bench control.

#### Outcome:

The failing image is identified, the bench is cleared, and the fault did not reproduce from cold starts alone; the hunt stopped at the operator's direction with the exit chord misbehaving and the board left on the suspect firmware. The host kernel log rewrote the account entry 36 started from: an image flashed at 20:02 came up as the TinyTang CDC on three relay cold starts at 20:02, 20:08 and 20:13, and every cold start from 20:16 onwards came up as the FT2232 `0403:6010`, with no flash in between, since a commit shows as an instant CDC-to-FT2232 swap (as at 19:27:46 and 20:02:34) and none appears from 20:13 to 20:16. The 20:23 build entry 36 suspected was therefore never installed, because TinyTang never ran again to install it. Both staged images on the card were fetched with `tangget` through `tools/tinytang_sync.py` and compared with their build-identity strings blanked: `/tinytang-upload.bin`, `fdebc90-dirty.17f81b2` at `f17d3603...`, is byte-identical in code to the known-good `fdebc90` image entry 36 flashed, so it was the other session's clean recovery build; `/tinytang-upload.padbuild.bin`, `fdebc90-dirty.37aba55` at `ac9db8d2...`, is byte-identical in code to a rebuild of the uncommitted working tree, which carries `tang_gba_save()` extracted from `gbasave`, a `pad` command, and a `play_task` in `tinygba_app.cpp` that watches the pad for an exit chord and then saves and reloads the desktop core, so padbuild is almost certainly the image flashed at 20:02. All three images carry valid boot headers -- magic at 0x00, 0x08 and 0x64, body length at 0x84 matching the file, CRC-32 of the first 252 bytes correct -- so a malformed image is ruled out. The crash recorder in `ports/bl616/tang_crash.c` reads its flash record at 0x3FF000 on every boot including power-ups, and the 21:18 recovery boot found none, so the failing image either hung with interrupts off, which the recorder cannot catch, or died before the recorder started. The bench was then cleared as a cause: seven relay two-wire cold starts on the known-good `fdebc90-dirty.6cd347f` all acknowledged three steps, showed a real disconnect, and came back as the CDC, the planned ten being cut to seven at the operator's request. The suspect tree was rebuilt in a detached worktree as `fdebc90-dirty.c62e9ec`, 707,328 bytes at SHA-256 `3be550199110daf297697b094fafc247d684a6f3969d5657cf7e437e1176ef70`, code-identical to padbuild, installed with `tools/tinytang_flash.py`, and cold-started four times by relay, run by run: all four came up as the CDC with `platform` reporting `c62e9ec`, no kept resets and no crash record, including the fourth start, which is the one the 20:02 image failed on, so the failure is not a property of booting this image. The card's `/root/cores/console138k/tinygba.bin` is 5,063,660 bytes, the size of TinyGBA's `build/exitkey`, the core with the HOME-with-SELECT exit chord, so what the 20:02 session most likely did before 20:16 was exercise the new paths. The operator then launched games from the TinyGBA library and found the exit chord unreliable: minus with HOME does not reliably trigger it, it behaves as HOME with some other button, and which button differs from one game launch to the next. Whether a `.sav` was written or the save path ran during those attempts was not observed, because the desktop held the console. The three images are kept outside the repository at `/run/media/vash/GIT/tinytang-scratch/firmware-backup/2026-11-16/` with a `SHA256SUMS.txt`, because the card's known-good staging copy was overwritten by the suspect's upload and the build worktrees live in a scratch directory. No source in this repository was changed by this cycle.

#### Next Steps:

The board is on the suspect firmware `fdebc90-dirty.c62e9ec`, which has booted cleanly four times but is the code most likely to have stopped the board tonight, so the first decision is whether to leave it for the next test or put `known-good_fdebc90-dirty.6cd347f.bin` back with `tools/tinytang_flash.py` first; if it stops booting, recovery is BOOT plus the README's `BLFlashCommand --flash --write` form with that file. The exit chord is the lead, and the instrument for it is already on the board: run `pad` at the prompt with the TinyGBA exit-chord core loaded and press minus, HOME and both, which reads the word the core reports and shows directly which bit minus lands on and whether bit 15 asserts -- the observed behaviour, HOME plus a button that changes per launch, points at the core's `save_exit = home & joy1[2]` reading a bit whose meaning depends on the pad path rather than at the firmware, and TinyGBA's log carries that half. Only once the chord is reliable is it worth driving `play_task` through `tang_gba_save()` with the console attached, then a cold start, which is the 20:13 to 20:16 sequence that preceded the failure; `gbasave`'s SD write is the path TCL-064 records as wedging the console, so a failure there is the leading hypothesis for how this image stopped. Until that sequence has been run and survived, the three uncommitted files in `ports/bl616/` stay uncommitted.

#### Files Modified:

None.

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: FAIL

---
## 38 COMMIT Unreleased 2026-11-17T07:02:08-07:00

#### Coming From:

Unreleased 9777863

#### Purpose:

Measure the TinyGBA exit chord that entry 37 left unreliable, and commit the exit-and-save firmware if it held up on the board.

#### Outcome:

The exit chord works and the three files entry 36 and entry 37 held back are committed as they were built and tested, unchanged, with no rebuild or reflash in this cycle (GBA-004). The board ran `fdebc90-dirty.c62e9ec` throughout, the image entry 37 rebuilt from this working tree, whose three sources were last modified at 20:23 on 2026-11-16, before that build. With TinyGBA's exit-chord core loaded by `tangload` and Metroid Fusion started by a shell `gbaload`, the new `pad` command was read from the host with timestamps: idle was `0000` on every load, minus alone `0004`, a HOME tap `0400` for 250 ms, and twelve chords in both press orders and both release orders `8004`, with no stray tap, so the core's word is correct and TinyGBA records that half as BRD-122. Three runs that appeared to leave the game at the HOME step were not exits: after a shell launch the desk layer is off, and `desk_watch_toggle()` brings TinyDesk up on a tap, showing the shell over a game that keeps running, which the operator confirmed by switching back to it. Nothing watches bit 15 after a shell launch, so no combination ends such a game. After a power-up the operator launched from the TinyGBA library and the chord ended the game and restored the desktop, in one to two seconds because `play_task` saves through `tang_gba_save()` before the desktop core is reloaded, which is what had been read the night before as the chord not working. Afterwards the console answered, `fpga` reported core 84, `crash` reported no record and no kept resets since the power-up, and the SD was mounted. One behaviour of the new command is wrong: `pad` prints `ctrl-c stops early`, but ctrl-c did not stop it, so it held the console for its full duration twice in this session; this is recorded rather than fixed, since a fix would need a rebuild and reflash the operator did not ask for. The README now describes TinyGBA's launcher, its exit and the `pad` command. `third_party/tinydesk` still shows the carried TinyDesk patches entry 36 describes and is not part of this commit. The core-syntax audit was run: `.ai/core.md` and `.ai/core-syntax.md` were re-read, `.ai/core.md` was confirmed unchanged, the complete `.ai/` diff was inspected and holds this entry and GBA-004 only, and this entry was validated as number 38 of 100 with six canonical sections in order, a one-sentence Purpose, prose in Outcome and Next Steps, only eligible paths in Files Modified and allowed Status values, with the reference store at 171 records and 171 unique ids.

#### Next Steps:

No work remains on the exit chord, and the board can stay on `c62e9ec`, which is now the committed source; the known-good image in `/run/media/vash/GIT/tinytang-scratch/firmware-backup/2026-11-16/` remains the recovery path. Small items left over: make `pad` stop on ctrl-c or drop the claim from its banner at the next firmware build; consider a status line or OSD message the moment the chord is seen, so the one-to-two-second save does not read as a missed press; and a power cut straight after a library save-and-exit, entry 37's suspected sequence for the 2026-11-16 failure to start, has not been run and the operator judged it unnecessary. The `tinytang-upload*.bin` files in the card's root and the detached build worktrees from entries 36 and 37 can be removed when nobody needs them.

#### Files Modified:

- README.md
- ports/bl616/tang_fpga_uart.c
- ports/bl616/tang_gba_load.h
- ports/bl616/tinygba_app.cpp

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 39 COMMIT Unreleased 2026-11-17T09:32:13-07:00

#### Coming From:

Unreleased cec966c

#### Purpose:

Remove the two firmware staging images that entries 36 and 38 left in the SD card's root.

#### Outcome:

`/tinytang-upload.bin` and `/tinytang-upload.padbuild.bin`, both 707,328 bytes, were deleted with `rm` at the board's shell and a listing of the card root afterwards showed only `README.md`, `bl616-firmware.bin`, the four `.ini` files, `crash.log` and the `root` and `LICENSES` directories. Neither file is read at boot. `/tinytang-upload.bin` is the staging path `tools/tinytang_flash.py` writes before every flash, so it returns on the next one, and the code of both images is preserved in `/run/media/vash/GIT/tinytang-scratch/firmware-backup/2026-11-16/`, the padbuild image as `card-padbuild_fdebc90-dirty.37aba55.bin` and the other's code as the known-good `fdebc90-dirty.6cd347f` image entry 37 found it identical to. The deletion was part of a wider card cleanup recorded in TinyGBA's log entry 13, which also found that the library exit committed in entry 38 most likely wrote a `.sav` of power-up SDRAM for a cartridge that had never saved (TinyGBA record BRD-123): `play_task` calls `tang_gba_save()` on every exit, and the save window is neither cleared nor restored when a cartridge has no `.sav`. `bl616-firmware.bin` was kept; at 699,008 bytes it is older than the running `fdebc90-dirty.c62e9ec`. No repository source changed and nothing was built or deployed. The core-syntax audit was run: `.ai/core.md` and `.ai/core-syntax.md` were re-read, `.ai/core.md` was confirmed unchanged, the complete `.ai/` diff was inspected and holds this entry only, and this entry was validated as number 39 of 100 with six canonical sections in order, a one-sentence Purpose, prose in Outcome and Next Steps, only eligible paths in Files Modified and allowed Status values.

#### Next Steps:

Two firmware items are now open on this side. The exit save should not write a `.sav` for a cartridge that had none at launch and never wrote its save window, unless TinyGBA fills that window with `0xFF` on reset instead, and the choice between the two belongs with whichever repository takes it up after BRD-123's mechanism is reproduced. The card's `bl616-firmware.bin` is older than the running firmware, so the desktop's Software update would install an older image until `tools/tinytang_card_firmware.py` replaces it. Entry 38's smaller items stand: `pad` does not stop on ctrl-c, and the exit gives no feedback during its save.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---
## 40 COMMIT Unreleased 2026-11-17T11:22:03-07:00

#### Coming From:

Unreleased 1f4320b

#### Purpose:

Record a shell `gbaload` stall found on the board while TinyGBA's boot screen was being tested, so it is not lost before it is investigated.

#### Outcome:

On firmware `fdebc90-dirty.c62e9ec`, on a boot where TinyDesk has run, `gbaload` at the shell prints `<n> bytes -> the core at 2000000 baud` and then nothing further: no `save hardware =` line, no save restore, and no `the core is running`, for 30 seconds against the seven a 4 MiB-to-8 MiB cartridge normally takes. It was reproduced five times on 2026-11-17 with Pokemon Emerald and Metroid Fusion: three times on TinyGBA's new boot-screen core `build/bootbars_s7`, once on the earlier `build/exitkey` core, which loaded normally through the same command on the same firmware before that morning's reboot, and once more after a fresh power-up and a library session. It happened with `osd desk status` reporting the desk layer on and also after `osd desk off`, so the console mirror is not the cause. Ctrl-c returns the prompt each time, and the core then runs whatever cartridge image the SDRAM holds, so a game may appear without its save having been restored. In the same power sessions the TinyGBA library's launch, which calls the same `tang_gba_load()`, completed normally, including a Castlevania: Aria of Sorrow launch that the operator accepted, and the exit chord's save and desktop reload worked. What distinguishes the failing runs is not established: the shell path versus the desk app's task, state left by TinyDesk having run, and the absence of any progress output while the load is in flight are all open. No source changed and nothing was built or deployed. TinyGBA's log entry 14 and record BRD-124 record the same observation from the core's side. The core-syntax audit was run: `.ai/core.md` and `.ai/core-syntax.md` were re-read, `.ai/core.md` was confirmed unchanged, the complete `.ai/` diff was inspected and holds this entry only, and this entry was validated as number 40 of 100 with six canonical sections in order, a one-sentence Purpose, prose in Outcome and Next Steps, `None.` under Files Modified and allowed Status values.

#### Next Steps:

Investigate the stall when it is next taken up: compare `cmd_gbaload` with the library's call of `tang_gba_load()`, especially which task runs the load and what holds or waits on `tang_fpga_lock()` while TinyDesk's tasks are alive, and add a progress line or a timeout so a stalled load says where it stopped instead of going silent. Until it is fixed, launch TinyGBA cartridges from the library, and treat a shell load that needed ctrl-c as having run without its save restored. Entry 39's items stand: the exit save can write a junk `.sav` for a cartridge that never saved, the card's `bl616-firmware.bin` is older than the running firmware, `pad` does not stop on ctrl-c, and the exit gives no feedback during its save.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---
## 41 COMMIT Unreleased 2026-11-17T13:00:08-07:00

#### Coming From:

Unreleased 2a4856c

#### Purpose:

Correct entry 40's condition for the shell `gbaload` stall with an observation made while TinyGBA's save-store fix was tested.

#### Outcome:

Entry 40 tied the stall to a boot on which TinyDesk had run; that is too narrow. On 2026-11-17, after a relay power-up on which TinyDesk was never started, a shell `gbaload` of Mario Kart completed normally on firmware `fdebc90-dirty.c62e9ec` and TinyGBA's `build/blankfill2_s5`, then two save-window dumps were taken with TinyGBA's `tools/save/pipe_save_dump.py` over `fpgapipe` and a file was fetched with `tangget`, and the next shell `gbaload`, of Metroid Fusion, printed its first line and nothing more for 40 seconds. The load itself finished: the restored save window read back identical to the card's `.sav` and the shell answered afterwards. So the stall follows earlier console file or pipe use, or something common to it and to TinyDesk, and the silence is in the command's output rather than necessarily in the load; earlier stalls in entry 40 left the core held, so the two may not be one fault. The same cycle found the card's `/root/media/gba/Metroid Fusion (USA).gba.sav` to be 32,768 bytes of the constant `0x5A`, written by no tool either repository contains; TinyGBA records it in BRD-125 and the operator replaced it with a new save. The TinyGBA library's save-on-exit was exercised end to end with a new Minish Cap save that survived exit, reboot and reload. No source changed and nothing was built or deployed here. The core-syntax audit was run: `.ai/core.md` and `.ai/core-syntax.md` were re-read, `.ai/core.md` was confirmed unchanged, the complete `.ai/` diff was inspected and holds this entry only, and this entry was validated as number 41 of 100 with six canonical sections in order, a one-sentence Purpose, prose in Outcome and Next Steps, `None.` under Files Modified and allowed Status values.

#### Next Steps:

When the stall is investigated, start from what `fpgapipe` and `tangget` leave behind on the console or the FPGA link -- a lock, a mode, a queue -- as well as from TinyDesk's tasks, and add progress output to `cmd_gbaload` so a stall shows where it stopped. Watch for any other `.sav` turning into a constant pattern, which would mean the writer of the `0x5A` file is still live. Entry 39's items stand.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---
## 42 COMMIT Unreleased 2026-11-17T13:38:04-07:00

#### Coming From:

Unreleased eb7b5a3

#### Purpose:

Give the TinyGBA window TinySNES's tabs, with a working Saves page and placeholder Input, Video and Audio pages, after setting the board's clock that the save times depend on.

#### Outcome:

TinyGBA's window now has Library, Input, Video, Audio and Saves, and the operator accepted it on the board (GBA-005). The clock was fixed first: the DS3231 read 2000-01-01 with its oscillator-stop flag set and `/tinydesk_tz` was missing from the card, so the RTC was set to UTC with `tangi2c rtc set`, timed to a second boundary from the host clock, and `/tinydesk_tz` was written as `-25200` for America/Phoenix; the RTC then read back with the flag clear and `tangi2c rtc` reported local 13:06. The Saves page follows TinySNES's lines for the selected cartridge -- name, where the `.sav` lives, its size against its save type's size, and the last attempt from `/root/saves/last.log` -- with the save type found by a new `tang_gba_save_info()` scan running in its own task with its own buffer and cached per cartridge, because a GBA image declares its save type only as a marker string. `tang_gba_save()` now stamps `arm` and `finish` lines into that log, which is what lets the page say when a save was written. Input, Video and Audio carry TinySNES's layout with placeholder values and controls that only report that they are not connected, since the core has no frames for them. The README's TinyGBA paragraph says so. The firmware `eb7b5a3-dirty.7141e1c` built cleanly in the changed files, all 46 host tests passed including a new save-log case for GBA names, and it was installed with `tools/tinytang_flash.py --yes` on the port found by USB identity and booted by the relay sequence, after which `crash` showed no record and the desktop core loaded. The operator confirmed the tabs and the placeholders, saved a new Minish Cap game and saw its time on the Saves page, and the card's log holds the `arm` and `finish ... written` lines stamped 13:28:23-07:00. Throughout, the ESP32 another agent drives on a neighbouring port was left alone, and the relay was reached over UDP. The core-syntax audit was run: `.ai/core.md` and `.ai/core-syntax.md` were re-read, `.ai/core.md` was confirmed unchanged, the complete `.ai/` diff was inspected and holds this entry and GBA-005 only, and this entry was validated as number 42 of 100 with six canonical sections in order, a one-sentence Purpose, prose in Outcome and Next Steps, only eligible paths in Files Modified and allowed Status values, with the reference store at 172 records.

#### Next Steps:

The placeholder pages become real one at a time as TinyGBA grows the frames behind them -- a pad map first, since the Input page already lists the core's current map -- and each should keep TinySNES's pattern of writing a settings file that the next launch sends. The save work the operator is continuing now has a page to show its results on. Open items from earlier entries stand: the shell `gbaload` stall of entries 40 and 41, `pad` not stopping on ctrl-c, no feedback during the exit's save, and the card's `bl616-firmware.bin` being older than the running firmware -- which is now further behind, at `eb7b5a3-dirty.7141e1c`.

#### Files Modified:

- README.md
- ports/bl616/tang_fpga_uart.c
- ports/bl616/tang_gba_load.h
- ports/bl616/tinygba_app.cpp
- tools/tests/tb_saves_log.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 43 COMMIT Unreleased 2026-11-17T14:36:02-07:00

#### Coming From:

Unreleased e1df16a

#### Purpose:

Move TinyTang to TinyDesk 0.5.2 and TinyDesk Shell 0.5.1, and give the desktop core block glyphs so TinyDesk's Image Viewer and window furniture draw properly on the desk layer.

#### Outcome:

TinyTang now runs TinyDesk 0.5.2 with TinyDesk Shell 0.5.1, with the Image Viewer and Recycle Bin added and a desktop core whose font carries block glyphs, accepted by the operator on the board (TDESK-018). Two of the four carried TinyDesk patches were retired because upstream absorbed them, the slider patch was regenerated to record its API in upstream's snapshot, and the port needed only its Bluetooth types taken from upstream, nine more TinyDesk sources in the build, and two registrations. The desktop core's `font.vh` gained 23 glyphs in the blank control-code slots 0x01-0x17 and reports desktop ABI 1.7, and the firmware maps half blocks, quadrants, shades and pointers to those slots only after `tang_core_load` has asked a freshly loaded `tinytang.bin` for its ABI and found 1.7 or later, so game cores and older desktop cores keep the ASCII fallback. The desktop core was built at eight seeds in parallel and all eight passed every gate with identical utilisation -- 8,014 LUT4, 2,916 DFF, 14 BSRAM, 1,464 ALU -- differing only in timing, `clk` 67.24 to 77.42 MHz, pixel clock 76.34 to 88.39 MHz against 74.25, system clock 123.89 to 155.50 MHz; seed 3, the script's default and the seed of record, was deployed at 77.42, 83.00 and 144.84 MHz. Host tests went from 41 to 46 of 46 after their harnesses caught up: `tb_tang_flash` gained stand-ins and now pins that only the desktop core is asked and that ABI 1.6 or no answer leaves the glyphs off; `test_editor_limits` reads its app list from `CMakeLists.txt`; `tb_desktop_uart` expects ABI 1.7; `test_osd_desk` checks the new mapping, its repaint, and that the firmware table and `font.vh` agree slot for slot; and `test_script_limits` and `test_tinydesk_patches` now compare build diagnostics with unmodified upstream, because 0.5.2's own tests emit a deliberate format-truncation warning. The card's previous desktop core, the recorded `00d8b48c...` ABI 1.6 image, was copied to `/run/media/vash/GIT/tinytang-scratch/desktop-core-backup-2026-11-17/` before the new pair was written; the firmware `e1df16a-dirty.86c746c` was installed with `tools/tinytang_flash.py --yes` on the port found by USB identity and booted by the relay sequence, after which the shell reported 0.5.1, `fpgar 4` read `0x00010007`, `crash` had no record, and the heap stood at 84,076 of 117,800 bytes free with a 29,504-byte largest block, about 18 KB below the previous image. A test BMP was put at `/root/Desktop/testcard.bmp`. The operator passed the build and played a GBA game through the launcher. The ESP32 another agent drives was not touched. README, BUILDING.md and THIRD_PARTY.md name the new pins and the new desktop core. The core-syntax audit was run: `.ai/core.md` and `.ai/core-syntax.md` were re-read, `.ai/core.md` was confirmed unchanged, the complete `.ai/` diff was inspected and holds this entry and TDESK-018 only, and this entry was validated as number 43 of 100 with six canonical sections in order, a one-sentence Purpose, prose in Outcome and Next Steps, only eligible paths in Files Modified and allowed Status values, with the reference store at 173 records.

#### Next Steps:

The operator will exercise more of 0.5.2 as work continues; the parts not yet looked at are its sign-in and account features, the Recycle Bin, and the Image Viewer on large pictures, which matters because the heap is about 18 KB smaller and its largest free block 11.5 KB smaller than before. Game cores still draw TinyDesk in ASCII over a running game; giving TinyGBA's and TinySNES's desk fonts the same slots, and reporting it so the firmware can tell, would bring them level. Earlier items stand: the shell `gbaload` stall, `pad` and ctrl-c, feedback during the exit save, and the card's `bl616-firmware.bin` being older than the running firmware.

#### Files Modified:

- BUILDING.md
- CMakeLists.txt
- README.md
- THIRD_PARTY.md
- fpga/desktop/desktop_regs.sv
- fpga/desktop/font.vh
- ports/bl616/tang_bt_ops.h
- ports/bl616/tang_fpga_link.h
- ports/bl616/tang_fpga_uart.c
- ports/bl616/tang_osd_desk.c
- ports/bl616/tang_osd_desk.h
- ports/bl616/td_desktop_bl616.c
- ports/bl616/tdsh_tang_flash.c
- scripts/apply-tinydesk-patches.sh
- third_party/patches/tinydesk/0001-settings-bluetooth-button.patch
- third_party/patches/tinydesk/0003-slider-widget.patch
- third_party/patches/tinydesk/0004-start-menu-exit.patch
- third_party/tinydesk
- third_party/tinydesk-shell
- tools/tests/sim/tb_desktop_uart.sv
- tools/tests/tb_osd_desk.c
- tools/tests/tb_tang_flash.c
- tools/tests/test_editor_limits.sh
- tools/tests/test_osd_desk.sh
- tools/tests/test_script_limits.sh
- tools/tests/test_tinydesk_patches.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 44 COMMIT Unreleased 2026-11-17T16:43:59-07:00

#### Coming From:

Unreleased 86ff89c

#### Purpose:

Give the desktop core a pixel framebuffer and the BL616 a full-screen Reader that draws pictures on it, so scanned pages can be read at real pixel resolution.

#### Outcome:

The desktop core now has a 640x360 RGB565 pixel plane at desktop ABI 1.8, and the new Reader app draws pictures on it full screen; the operator opened a scanned Nintendo Power page and passed it (TDESK-019). The plane is `fpga/desktop/pixplane.sv`, 228 block RAMs read with the text layer's four-pixel look-ahead and shown at 2x instead of the cells while it is on, written through three new link frames, 0x18 cursor, 0x19 pixels and 0x1A show, with the geometry at register 0x0c. A new end-to-end bench drives real UART frames through the link and the raster and checks 20 pixels, and fails under a short look-ahead or a missing doubling; it found nothing wrong with the design, only a capture bug of its own that was fixed. The firmware caches the desktop ABI at core load, which gates both the block glyphs and the plane, and `tang_plane.c` and `tinyreader_app.cpp` stream a BMP, PPM or PGM row by row from the card through TinyDesk's image reader, scale it, and send each finished row, with zoom, pan and page turns; the Reader was registered and the app registry raised to 28. A header-guard collision between `TANG_PLANE_H` and the plane-height macro was caught by a compiler warning and fixed by renaming the size macros. The eight-seed desktop build passed at only one seed: seed 3, pixel clock 81.26 MHz against 74.25, with `clk` 67.40 and sys 155.04 MHz; the other seven missed the pixel clock at 58.7 to 69.2 MHz with a clock-topology regression, on the unchanged HDMI encoder's TMDS path, which the plane's block RAMs displace. Utilisation was 8,853 LUT4, 3,073 DFF, 242 BSRAM and 1,466 ALU at every seed. Host tests were 47 of 47 with the plane bench added and two benches updated for ABI 1.8. A 1280x1726 BMP of the page was put in `/root/media/reader`; the core pair and firmware `86ff89c-dirty.958a440` were deployed and booted by the relay sequence after the operator reconnected the board, and `fpgar` read ABI `0x00010008` and geometry `0x02800168` with the heap at 77,964 bytes free. An HDMI capture of the Reader could not be taken because the capture card had no signal at the time. The ESP32 another agent drives was not touched. README and BUILDING.md describe the Reader and the new core. The core-syntax audit was run: `.ai/core.md` and `.ai/core-syntax.md` were re-read, `.ai/core.md` was confirmed unchanged, the complete `.ai/` diff was inspected and holds this entry and TDESK-019 only, and this entry was validated as number 44 of 100 with six canonical sections in order, a one-sentence Purpose, prose in Outcome and Next Steps, only eligible paths in Files Modified and allowed Status values, with the reference store at 174 records.

#### Next Steps:

The operator likes the row-by-row loading as it is, so speed is not the next item; formats are: a JPEG decoder (TJpgDec, a few KB of working memory and built-in 1/2 to 1/8 scaling) would let pages be stored as JPEG instead of 6.6 MB BMPs, and a small PC script could convert a whole JPEG 2000 issue in one step. The plane is also ready for TinyDesk's RGB picture sink, which would give the upstream Image Viewer and other apps real pixels. The 1.8 core's one-in-eight seed yield is the risk to watch on any further desktop-core change. Earlier items stand: the shell `gbaload` stall, game-core desk fonts without block glyphs, `pad` and ctrl-c, feedback during the exit save, and the stale `bl616-firmware.bin`.

#### Files Modified:

- BUILDING.md
- CMakeLists.txt
- README.md
- fpga/desktop/build.tcl
- fpga/desktop/desktop_hdmi.sv
- fpga/desktop/desktop_link.v
- fpga/desktop/desktop_regs.sv
- fpga/desktop/desktop_top.sv
- fpga/desktop/pixplane.sv
- ports/bl616/tang_core_load.h
- ports/bl616/tang_plane.c
- ports/bl616/tang_plane.h
- ports/bl616/td_desktop_bl616.c
- ports/bl616/tdsh_tang_flash.c
- ports/bl616/tinyreader_app.cpp
- tools/desktop_open/synth.ys
- tools/tests/sim/tb_desktop_plane.sv
- tools/tests/sim/tb_desktop_uart.sv
- tools/tests/test_desktop_core.sh
- tools/tests/test_desktop_plane.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 45 COMMIT Unreleased 2026-11-17T17:04:34-07:00

#### Coming From:

Unreleased 638e0c3

#### Purpose:

Design a 1280x720 framebuffer controller for the dock's SDRAM and prove it in simulation before it touches the desktop core, as phases 0 and 1 of the plan to free the plane's block RAM.

#### Outcome:

The SDRAM framebuffer works in simulation against a model that checks the part's timing (TDESK-020). Phase 0 settled the choices from records rather than new measurement: one W9825G6KH-6 die, run on the 74.25 MHz pixel clock with CAS latency 2, which needs no new PLL and keeps the controller and scanout in one domain; line y at word y*1536 of bank 0; and a per-line budget of about 1,300 of 1,650 clocks for the next line's fetch, with refresh and writes in the remainder. `fpga/desktop/fb_sdram.sv` fetches each next line in seamless eight-word bursts into a two-line buffer, owes four refreshes a line, and drains a 64-entry write FIFO from the link's clock; its scanout keeps the plane's four-pixel look-ahead at 1:1. Because TinyGBA's model answers one word per command and forgives early commands, a new pin-level model, `sdram_w9825_model.sv`, implements bursts and auto-precharge and counts every timing violation. `test_fb_sdram.sh` compares all 921,600 pixels of a frame after writing 1,597 at ten times UART speed and passes with no mismatch, no violation, no underrun and no dropped write, the longest fetch 1,297 clocks, and 3,000 refreshes a frame against 2,133 needed. Only READ_DELAY 2 passes; removing refresh or shortening tRCD fails. Two faults were found and fixed on the way: a 16-entry FIFO overflowed while a fetch held the part, and the bench compared colours one raster position late, which hid a one-word capture offset behind an equal and opposite one. Synthesised alone the controller is about 1,500 cells with 3 block RAMs, against the 228 the 640x360 plane uses. Host tests are 48 of 48. Nothing was wired into the desktop core, built into a bitstream or deployed, and the board still runs the ABI 1.8 plane. The core-syntax audit was run: `.ai/core.md` and `.ai/core-syntax.md` were re-read, `.ai/core.md` was confirmed unchanged, the complete `.ai/` diff was inspected and holds this entry and TDESK-020 only, and this entry was validated as number 45 of 100 with six canonical sections in order, a one-sentence Purpose, prose in Outcome and Next Steps, only eligible paths in Files Modified and allowed Status values, with the reference store at 175 records.

#### Next Steps:

Phase 2 integrates it: replace `pixplane`'s store with `fb_sdram` in `desktop_hdmi`, widen the link's cursor to 1280x720 as x and y rather than a linear address, report 1280x720 at register 0x0c, add the SDRAM pins from TinyGBA's constraints and drive the part's clock as the inverse of the pixel clock, and make the read alignment a register so the board's value can be found without a rebuild. Then an eight-seed build, expecting block RAM near 17 and a better pixel-clock yield than 1.8's one seed in eight, and a hardware check with a 1:1 test pattern. Phase 3 teaches the Reader the 1280x720 geometry and two frames per row; a faster link is a separate, optional step.

#### Files Modified:

- fpga/desktop/fb_sdram.sv
- tools/tests/sim/sdram_w9825_model.sv
- tools/tests/sim/tb_fb_sdram.sv
- tools/tests/test_fb_sdram.sh

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---
## 46 COMMIT Unreleased 2026-11-17T17:39:49-07:00

#### Coming From:

Unreleased bc46e92

#### Purpose:

Put the SDRAM framebuffer into the desktop core as a 1280x720 plane shown 1:1, build it, and prove on the board that whole frames come back from the SDRAM intact.

#### Outcome:

Desktop ABI 1.9 holds its pixel plane in the dock's SDRAM at 1280x720 and the board returns it intact (TDESK-021). `fb_sdram` replaced `pixplane` in `desktop_hdmi`, the link's cursor became x and y, frame 0x1B sets the read alignment at run time, registers 0x10, 0x14 and 0x18 report the plane's status and a checksum of every frame scanned out, register 0x0c reports 1280x720, and the SDRAM takes TinyGBA's pins with its clock from an ODDR as the inverse of the pixel clock. The plane bench now runs UART frames through the link and the HDMI path into the timing-checking SDRAM model and passes 36 checks, and fails when the row carry, the 0x1B decode or the cursor parking is broken; host tests are 49 of 49. The eight-seed sweep exposed a fault in the timing gate (TOOL-023): it matched only nextpnr's `Info:` lines, so a routed clock failure, printed as `Warning:`, left the passing placement estimate standing. Corrected and pinned by `test_pnr_timing.sh`, it shows the ABI 1.7 sweep still clean, the ABI 1.8 core of record routed at 67.12 MHz against 74.25 with every seed failing, and this sweep passing at seed 4 alone, 75.91 MHz. Every seed uses 17 block RAMs against 242 before, and 8,958 LUT4 against 8,853. Seed 4 was deployed to `/root/cores/console138k/` and booted by the relay sequence; `fpgar` read ABI `0x00010009`, geometry `0x050002d0` and status `0x83cb0511`, ready with a longest fetch of 1,297 clocks as in simulation. The new `tools/plane_sweep.py` drew a 1280x720 picture through `fpgapipe` in 9.4 seconds and tried all fourteen read alignments: only 3 clocks on the rising edge, the built-in value, returned the frame intact, and 7,200 consecutive frames then matched. An HDMI capture showed the picture full screen, and after a later power cycle the prompt and text layer were normal. The board dropped off USB once while the capture card was connected and came back with the relay sequence. The firmware is unchanged, so the Reader still draws 640x360 rows, which land in the top-left quarter on this core. The ESP32 another agent drives was not touched. BUILDING.md records the new core and corrects the ABI 1.8 timing. The core-syntax audit was run: `.ai/core.md` and `.ai/core-syntax.md` were re-read, `.ai/core.md` was confirmed unchanged, the complete `.ai/` diff was inspected and holds this entry, TOOL-023 and TDESK-021 only, and this entry was validated as number 46 of 100 with six canonical sections in order, a one-sentence Purpose, prose in Outcome and Next Steps, only eligible paths in Files Modified and allowed Status values, with the reference store at 177 records.

#### Next Steps:

Phase 3 teaches the Reader the new geometry: read width and height from register 0x0c, draw 1280x720 at 1:1 in two frames per row, and keep the 640x360 path for an ABI 1.8 core. The operator then looks at the Reader and the plane on the board, which has not happened yet for this core. A faster link remains optional; a full screen takes 9.4 seconds at 2 Mbaud. The TMDS encoder's adder chain is the pixel clock's critical path at every seed and is worth pipelining before the next change to this core, so that seed choice stops deciding whether it meets timing.

#### Files Modified:

- BUILDING.md
- fpga/desktop/build.tcl
- fpga/desktop/desktop.cst
- fpga/desktop/desktop_hdmi.sv
- fpga/desktop/desktop_link.v
- fpga/desktop/desktop_regs.sv
- fpga/desktop/desktop_top.sv
- fpga/desktop/fb_sdram.sv
- fpga/desktop/pixplane.sv
- tools/desktop_open/pnr_timing.py
- tools/desktop_open/synth.ys
- tools/plane_sweep.py
- tools/tests/sim/tb_desktop_plane.sv
- tools/tests/sim/tb_desktop_uart.sv
- tools/tests/sim/tb_fb_sdram.sv
- tools/tests/test_desktop_core.sh
- tools/tests/test_desktop_plane.sh
- tools/tests/test_fb_sdram.sh
- tools/tests/test_pnr_timing.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---
## 47 COMMIT Unreleased 2026-11-17T18:54:49-07:00

#### Coming From:

Unreleased 12895d5

#### Purpose:

Teach the plane driver and the Reader the plane's reported size, so pictures draw at 1280x720 on the ABI 1.9 core and still at 640x360 on an ABI 1.8 one.

#### Outcome:

The Reader draws at 1280x720 on the SDRAM plane and the operator passed it (TDESK-022). `tang_plane_size` reads register 0x0c once per loaded core with a fallback by ABI, `tang_plane_write` sends a run of up to the plane's width as a cursor frame and 640-pixel frames, and the Reader fits, scales and maps clicks to the size it is given and resends only the columns a picture can cover. Firmware `12895d5-dirty.22ce042` built without warnings in the changed files, was installed with `tools/tinytang_flash.py --yes` on the port found by USB identity and booted by the relay sequence, and `platform` and `fpgar` confirmed the build, ABI `0x00010009` and geometry `0x050002d0`. The operator opened the magazine page, zoomed in with A, reported that the input seemed to stop and then that it worked but was laggy, and passed it; an HDMI capture at 2x shows the page's text sharp and readable. The lag is the link: a full 1280-pixel row is 2.5 KB, about 12.8 ms at 2 Mbaud, so each zoom repaints visibly before the next input is drawn. Host tests are 49 of 49. The card's `bl616-firmware.bin` was not refreshed. The ESP32 another agent drives was not touched. README describes the Reader on the new plane. The core-syntax audit was run: `.ai/core.md` and `.ai/core-syntax.md` were re-read, `.ai/core.md` was confirmed unchanged, the complete `.ai/` diff was inspected and holds this entry and TDESK-022 only, and this entry was validated as number 47 of 100 with six canonical sections in order, a one-sentence Purpose, prose in Outcome and Next Steps, only eligible paths in Files Modified and allowed Status values, with the reference store at 178 records.

#### Next Steps:

The Reader's lag is the next thing worth removing, by a faster link between the BL616 and the core, which was the plan's optional fourth phase, or by redrawing only what a pan exposes and moving the rest within the SDRAM. The TMDS encoder's adder chain still decides whether a seed meets the pixel clock and is worth pipelining before the next change to the core. The card's `bl616-firmware.bin` is older than the running firmware and should be refilled with `tools/tinytang_card_firmware.py` before a release. Earlier items stand: the shell `gbaload` stall, game-core desk fonts without the block glyphs, `pad` and ctrl-c, and feedback during the exit save.

#### Files Modified:

- README.md
- ports/bl616/tang_plane.c
- ports/bl616/tang_plane.h
- ports/bl616/tinyreader_app.cpp

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 48 COMMIT Unreleased 2026-11-17T22:25:15-07:00

#### Coming From:

Unreleased 92c7318

#### Purpose:

Raise the BL616-to-core link from 2 Mbaud to the fastest rate both ends make exactly, negotiated by the register channel, so the Reader's repaints stop being bound by the wire.

#### Outcome:

Desktop ABI 1.10 runs the link at 10 Mbaud and the operator passed the Reader on it (TDESK-023). `fpga/desktop/link_uart.sv` replaces `uart_fixed.v`, timing bits on the 50 MHz crystal at 25, 10 or 5 clocks for 2, 5 or 10 Mbaud, the rates the BL616's whole-number divider of 40 MHz shares with it (BL6-011, which also shows the old `fpgabaud` 12 and 16 Mbaud were really 13.3); register opcode 0x03 switches after its reply, capabilities read 0x1b, registers 0x1c and 0x20 give the rate and the fallback and framing-error counts, and a line held low for 3 us above 2 Mbaud drops the core back to 2 Mbaud. The firmware returns both ends to 2 Mbaud before every core load, negotiates 10 Mbaud and then 5 after a desktop of ABI 1.10 loads, confirms the rate with a CRC-checked read before using it, and gains `fpgabaud` with negotiation and `-raw`, `planefill` and `readerdraw`; `tools/plane_sweep.py` gains `--baud` and `--fill`. The new UART bench passes 69 checks with nine mutations caught, the plane bench passes again at 10 Mbaud with sixteen whole rows back to back and no dropped write after a frame-end wait in it was fixed, the host harnesses cover negotiation, refusal, a bad CRC, a failed confirm and the load order with five mutations caught, and host tests are 50 of 50. Of eight seeds only seed 2 met the pixel clock, at 75.90 MHz, with `sys_clk` at 149.03 MHz; its `desktop.537265ba89ee.bin` and `tinytang.fs.gz` were put on the card and firmware `92c7318-dirty.003322f` installed by the relay sequence, after which only a comment in `tang_plane.h` changed. The boot load negotiated 10 Mbaud by itself; a 1280x720 screen took 1,858 ms at 10 Mbaud, 3,689 at 5 and 9,190 at 2, each at its wire rate; 3,600 BL616-drawn frames and 1,800 PC-drawn frames matched their checksums with every reply CRC-checked at 10 Mbaud; and a deliberately desynchronised link was recovered by `fpgabaud 2000000`. A 1280x1726 Reader page went from 17.6 s at 2 Mbaud to 10.2 s at 10, of which reading the BMP off the card is about 7.7 s at every rate. The ESP32 another agent drives was not touched. BUILDING.md records the new core, README the Reader's speed and the link rates, and THIRD_PARTY.md the UART's replacement. The core-syntax audit was run: `.ai/core.md` and `.ai/core-syntax.md` were re-read, `.ai/core.md` was confirmed unchanged, the complete `.ai/` diff was inspected and holds this entry, TDESK-023 and BL6-011 with its index row and summary line only, and this entry was validated as number 48 of 100 with six canonical sections in order, a one-sentence Purpose, prose in Outcome and Next Steps, only eligible paths in Files Modified and allowed Status values, with the reference store at 180 records.

#### Next Steps:

The Reader is now bound by the card, about 860 KB/s reading a 6.6 MB BMP row by row, so the next speed-up is larger reads through FatFS or a page format closer to the plane, such as RGB565 at the plane's width, measured with `readerdraw`. The TMDS encoder's adder chain still decides whether a seed meets the pixel clock and is worth pipelining before the next change to the core. The card's `bl616-firmware.bin` is older than the running firmware and should be refilled before a release. Earlier items stand: the shell `gbaload` stall, game-core desk fonts without the block glyphs, `pad` and ctrl-c, and feedback during the exit save.

#### Files Modified:

- BUILDING.md
- README.md
- THIRD_PARTY.md
- fpga/desktop/build.tcl
- fpga/desktop/desktop_link.v
- fpga/desktop/desktop_regs.sv
- fpga/desktop/link_uart.sv
- fpga/desktop/uart_fixed.v
- ports/bl616/tang_fpga_link.h
- ports/bl616/tang_fpga_uart.c
- ports/bl616/tang_plane.h
- ports/bl616/tdsh_platform_bl616.c
- ports/bl616/tdsh_tang_flash.c
- ports/bl616/tinyreader_app.cpp
- tools/desktop_open/synth.ys
- tools/plane_sweep.py
- tools/tests/sim/tb_desktop_link.sv
- tools/tests/sim/tb_desktop_plane.sv
- tools/tests/sim/tb_desktop_uart.sv
- tools/tests/sim/tb_link_uart.sv
- tools/tests/stubs/tang_fpga_uart/bflb_mtimer.h
- tools/tests/tb_tang_flash.c
- tools/tests/tb_tang_fpga_uart.c
- tools/tests/test_desktop_core.sh
- tools/tests/test_desktop_link.sh
- tools/tests/test_desktop_plane.sh
- tools/tests/test_link_uart.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 49 COMMIT Unreleased 2026-11-17T23:31:57-07:00

#### Coming From:

Unreleased 31fd782

#### Purpose:

Turn the Reader into TinyReader, a book reader that shows two-page spreads full screen and turns pages in one frame from spreads held in the desktop core's SDRAM.

#### Outcome:

TinyReader reads books and the operator passed it on the 161-page Zelda guide (TDESK-024). Desktop ABI 1.11 keeps twelve 1280x720 pictures in the SDRAM, picked for writing by frame 0x1C and for showing by 0x1D at the next frame, with register 0x24 and 0x10[15:12] reporting them; `tools/make_book.py` renders a PDF, CBZ, CBR or image folder into a `.trb` of sector-aligned 1280x720 spreads in the link's byte order (docs/tinyreader-books.md), showing fold-out pages alone after the first build of the guide squeezed one beside its neighbour; and TinyReader, the Reader renamed, loads the spreads around the one being read into eleven buffers in the background (`ports/bl616/tang_book.c`), keeps the place in `<book>.trb.pos`, and still shows pictures from buffer 0. The new logic missed the pixel clock on all sixteen seeds tried, on the HDMI encoder's existing path; with the operator's approval `tmds_channel.sv` was pipelined by one clock, proven equal to the original one clock later by `tools/tests/test_tmds_pipeline.sh`, and all eight seeds then routed it at 86.57 to 92.99 MHz (TOOL-024). Seed 3, `desktop.a0f328f73820.bin`, and firmware `31fd782-dirty.48f603d` were deployed by the relay sequence, after which only a comment in `tinyreader_app.cpp` changed. On the board a spread loaded in about 1,995 ms, 126 ms of it on the card in 16 KB reads, which shows that TDESK-023's slow card was the Reader's row-by-row read path and is recorded there as a revision; all eleven book buffers matched their spreads' checksums from the file on the PC, each shown within two frames of its switch (once three); and HDMI captures showed a spread and a full-screen fold-out correctly. Uploading the 149 MB book exposed a fault in `tools/tinytang_put.py`, whose single serial write timed out after 30 s, now written 1 MB at a time (TOOL-025); the 162 MB book went in 127 s. Host tests are 52 of 52, with new benches for the converter and parser, the buffers in the plane bench and the encoder, and mutations caught in each. The ESP32 another agent drives was not touched. BUILDING.md records the core, README describes TinyReader, and THIRD_PARTY.md the encoder change. The core-syntax audit was run: `.ai/core.md` and `.ai/core-syntax.md` were re-read, `.ai/core.md` was confirmed unchanged, the complete `.ai/` diff was inspected and holds this entry, TDESK-024, TOOL-024, TOOL-025 and a revision note on TDESK-023 only, and this entry was validated as number 49 of 100 with six canonical sections in order, a one-sentence Purpose, prose in Outcome and Next Steps, only eligible paths in Files Modified and allowed Status values, with the reference store at 183 records.

#### Next Steps:

Zoom into one page of a spread, deferred from this cycle, is the next TinyReader feature; the converter could store each page again at 1280 wide for it. Pictures could be read in large chunks as books are, which would take most of a Reader page's 7.7 s of reading away. Page turns that outrun the 2 s per spread load could be eased by a lower-resolution placeholder or a faster link. The card's `bl616-firmware.bin` is older than the running firmware and should be refilled before a release. Earlier items stand: the shell `gbaload` stall, game-core desk fonts without the block glyphs, `pad` and ctrl-c, and feedback during the exit save.

#### Files Modified:

- BUILDING.md
- README.md
- THIRD_PARTY.md
- docs/tinyreader-books.md
- fpga/desktop/desktop_hdmi.sv
- fpga/desktop/desktop_link.v
- fpga/desktop/desktop_regs.sv
- fpga/desktop/desktop_top.sv
- fpga/desktop/fb_sdram.sv
- fpga/desktop/hdmi/tmds_channel.sv
- ports/bl616/tang_book.c
- ports/bl616/tang_book.h
- ports/bl616/tang_plane.c
- ports/bl616/tang_plane.h
- ports/bl616/tinyreader_app.cpp
- tools/make_book.py
- tools/plane_sweep.py
- tools/tests/sim/tb_desktop_plane.sv
- tools/tests/sim/tb_desktop_uart.sv
- tools/tests/sim/tb_tmds_pipeline.sv
- tools/tests/tb_tang_book.c
- tools/tests/test_desktop_core.sh
- tools/tests/test_make_book.sh
- tools/tests/test_tmds_pipeline.sh
- tools/tinytang_put.py

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 50 COMMIT Unreleased 2026-11-18T00:21:20-07:00

#### Coming From:

Unreleased a1ba0eb

#### Purpose:

Add a zoom mode to TinyReader that shows each page's top and bottom halves full screen, switched by a window toggle, X on the pad or Z, and record why its deployment cost a BOOT-mode recovery.

#### Outcome:

The zoom mode is implemented and passes the host suite but has not run on the board, because its deployment failed and the cycle was stopped for the night. `tools/make_book.py` now also renders every page's top and bottom halves as full 1280x720 screens with a 5% overlap (`--zoom-overlap`, `--no-zoom`), a wide page split into its two pages first, in a zoom section described by header fields at 96 to 103 and a zoom table after the spread table, so older books still open as spreads (docs/tinyreader-books.md); the Zelda guide becomes 88 spreads and 344 views, 796 MB, and a decoded view showed the top of PDF page 41 at about 1050 pixels wide with its captions plainly readable. `ports/bl616/tang_book.c` parses and checks the zoom fields, maps a spread to its first view and a view to its spread, and lets one cache hold both sequences by a base offset. TinyReader gains a Zoom box, Z, and X on the Wii pad, which types Escape like the keyboard's key, so `tang_wiipad.c` now counts X presses and an Escape that comes with one toggles zoom while the keyboard's leaves; the loader moves the reader's page across a switch, keeps the mode in `<book>.trb.pos`, and `readerdraw` and `plane_sweep.py --zoom` handle views. Host tests are 52 of 52, with new checks for the view table, fold-out splitting, the overlap, `--no-zoom`, mapping both ways, old-style headers, the rebased cache and the X counter. The firmware `a1ba0eb-dirty.c9ff4bb` built cleanly, but it was installed by `tools/tinytang_flash.py` and the relay power cycle was run as soon as the tool said committed, before the board had finished rewriting its application slot; the board then came up only as the FT2232. With BOOT held by the operator, a readback of 0x40000 showed the new image in sectors 0 to 136, sector 137 erased and the old image from 138, which proves the commit was cut off rather than the image being bad (TOOL-026). The board was recovered with the previous cycle's tested firmware, built from `a1ba0eb` in the worktree `/run/media/vash/GIT/tinytang-good` and written to 0x40000 alone with BLFlashCommand, the device's SHA-256 matching; after the operator's power cycle `platform` reports `a1ba0eb-dirty.131fec9`, `crash` no record, and the ABI 1.11 core answers. `tools/tinytang_flash.py` now waits until the FT2232 has enumerated before it says to power-cycle, and README fact 6 says why; the wait has not yet been exercised. The card no longer holds `zelda.trb` or its `.pos`, which were deleted before the failed reboot, and still holds `/tinytang-upload.bin`, the zoom firmware. The ESP32 another agent drives was not touched. The core-syntax audit was run: `.ai/core.md` and `.ai/core-syntax.md` were re-read, `.ai/core.md` was confirmed unchanged, the complete `.ai/` diff was inspected and holds this entry and TOOL-026 only, and this entry was validated as number 50 of 100 with six canonical sections in order, a one-sentence Purpose, prose in Outcome and Next Steps, only eligible paths in Files Modified and allowed Status values, with the reference store at 184 records.

#### Next Steps:

Install the zoom firmware with the fixed `tools/tinytang_flash.py` and let it confirm the commit's end before any power cycle, which also tests the new wait; then regenerate the Zelda book with zoom views from the PDF on `/run/media/vash/USB_HDD` with `tools/make_book.py`, upload it to `/root/media/reader` with `tools/tinytang_put.py` (about 10 minutes), check a few zoom buffers with `readerdraw <book> z<view> <buffer>` and `plane_sweep.py --book --zoom`, and hand the operator the zoom test: the Zoom box, X on the pad, Z, page order and switching back. The firmware's commit should also report or refuse a sector that still differs after its three attempts instead of resetting silently. The worktree `/run/media/vash/GIT/tinytang-good` can be removed once it is no longer wanted as a recovery image. Earlier items stand: picture reads in large chunks, the card's `bl616-firmware.bin`, the shell `gbaload` stall, game-core desk fonts, `pad` and ctrl-c, and feedback during the exit save.

#### Files Modified:

- README.md
- docs/tinyreader-books.md
- ports/bl616/tang_book.c
- ports/bl616/tang_book.h
- ports/bl616/tang_wiipad.c
- ports/bl616/tang_wiipad.h
- ports/bl616/tinyreader_app.cpp
- tools/make_book.py
- tools/plane_sweep.py
- tools/tests/tb_tang_book.c
- tools/tests/tb_wiipad.c
- tools/tests/test_make_book.sh
- tools/tinytang_flash.py

#### Status:

- Build: PASS
- Deployment: FAIL
- User Test: NOT RUN

---
## 51 COMMIT Unreleased 2026-11-18T07:50:26-07:00

#### Coming From:

Unreleased bef2658

#### Purpose:

Double the BL616 application slot by moving the update staging area above the vendor data record, and use the room to test whether this board can see Classic Bluetooth gamepads.

#### Outcome:

The board now takes firmware up to 1.75 MiB and, on a test build, finds Classic gamepads that no LE scan can (FLS-002, BLE-020). The evening began with the operator's Xbox Wireless Controller, retried on the LE build: it bonded, set up and then sent nothing and dropped the link, BLE-017's failure unchanged; an 8BitDo pad in four modes and a Wii U Pro Controller never appeared in LE scans, being Classic devices. Bouffalo SDK 2.3.36 ships a BL616 controller library with BR/EDR, ble1m2s1bredr1, keeping all LE roles with two LE connections (BLE-002), but no BR/EDR-only one, so dropping LE would not shrink it; built with it the image is 1,074,784 bytes, past the 917,504-byte slot. The factory flash read-back shows 0x201000 to 0x3FF000 erased, so `ports/bl616/tang_fw_update.c` now gives the application 0x40000 to 0x200000 and stages at 0x210000, below the crash record, with static assertions on every bound, and `tools/tinytang_flash.py`'s limit and README fact 5 match. The board was moved across in two installs with the fixed flashing tool, whose wait for the loader saw the commits end after 3 and 5 seconds: `bef2658-dirty.876b3f9`, 803,632 bytes with the new layout, through the old updater, then `bef2658-dirty.aa1a253`, the 1,074,784-byte Classic build made with `CONFIG_BTBLECONTROLLER_LIB=ble1m2s1bredr1 CONFIG_BT_BREDR=y` on the make command line in `build-btclassic`, through the new one; the vendor loader booted both, `platform` and `crash` confirmed each, and the ABI 1.11 core answered. `ports/bl616/tang_ble.c` gains `btscan`, a Classic inquiry compiled only into that build, and with the operator's 8BitDo pad pairing it reported E4:17:D8:17:4D:7F at -71 dBm, class 002508, gamepad, 'Pro Controller'. The SDK has no Classic HID host, so nothing connects yet. The Classic build leaves 42,828 bytes of heap free before the radio, against 72,332 on the ordinary build, and LE keyboards and mice were not tried on it. Starting the radio dropped the USB console for a few seconds on both builds. The board is left on the Classic test firmware, which also carries entry 50's zoom mode, still untried by the operator. `.gitignore` now ignores `build-*` directories. Host tests are 52 of 52; the ordinary build was rebuilt with the new layout and is the one installed first. The operator watched the scan but gave no pass or fail. The ESP32 another agent drives was not touched. The core-syntax audit was run: `.ai/core.md` and `.ai/core-syntax.md` were re-read, `.ai/core.md` was confirmed unchanged, the complete `.ai/` diff was inspected and holds this entry, the new BLE-020 with its index row and summary line, and dated revisions to FLS-002 (summary line, title, verified date, a leading statement sentence and its verification) and to the verifications of BLE-002, BLE-007 and BLE-017, and this entry was validated as number 51 of 100 with six canonical sections in order, a one-sentence Purpose, prose in Outcome and Next Steps, only eligible paths in Files Modified and allowed Status values, with the reference store at 185 records.

#### Next Steps:

The next Classic step is a connection test before any driver: pair with the 8BitDo pad in Switch mode, open its HID control and interrupt channels (L2CAP PSM 0x11 and 0x13) over the SDK's Classic L2CAP, and print raw input reports while buttons are pressed; only if they arrive is a Classic HID host with per-family setup worth writing, with Bluepad32 as the reference for each family's handshake. Whether LE keyboards and mice still pair on the Classic build, and the heap with the desktop running on it, need checking before it could become the default. Entry 50's zoom mode still awaits its book upload and the operator's test, and the firmware's silent failure of a sector that differs after three attempts is still unfixed. Earlier items stand.

#### Files Modified:

- .gitignore
- README.md
- ports/bl616/tang_ble.c
- ports/bl616/tang_fw_update.c
- tools/tinytang_flash.py

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---
## 52 COMMIT Unreleased 2026-11-18T08:20:53-07:00

#### Coming From:

Unreleased 8ceef48

#### Purpose:

Give the heap back the static RAM that is held for ever but used only now and then, so the desktop runs on the Classic Bluetooth build with room left for a controller driver.

#### Outcome:

The Classic build's heap doubled and the desktop runs on it, and the operator passed it (BL6-012). After entry 51 the operator confirmed the LE Rii K06 still pairs and reports on the Classic build, which then had 20,204 bytes of heap free with the radio up and could not start the desktop. The new `tools/ram_report.py` measures static RAM from a build's link map, counting only sections placed in RAM; it corrected an earlier raw count that had put the Classic build's audio code at 34 KB, the true figure being about 25 KB. Phase 1 of the approved cleanup moved buffers used only during one operation to the heap for that operation: the updater's two sectors (freed at close, kept through the commit), the AE350 load block, tangput's and tangget's sector, the JTAG programmer's block through a marked wrapper in the vendored file, Phosphor's duration header, btscan's results, and the Gutenberg app's tables while its window is open; the BLE log ring went from 64 to 32 lines. The log viewer's ring was left because it is TinyDesk's running system log. The Classic build now also turns off the SDK's default A2DP sink and AVRCP (`CONFIG_BT_A2DP=n CONFIG_BT_AVRCP=n`), which the stack initialises only when enabled. Static RAM fell from 325,330 to 280,366 bytes on the ordinary build and from 354,610 to 280,087 on the Classic one. The Classic firmware `8ceef48-dirty.ee04aee`, 1,041,952 bytes, was installed with `tools/tinytang_flash.py`, which saw the commit end after 5 s, and booted with 151,112 bytes of heap and 95,468 free after the radio started for the K06's pairing. The operator started the desktop, opened TinyGutenberg, reported all passing, and read about 76 KB in use and 62 KB for the peak on System Monitor; a game-core load and a TinyNES run on this firmware were suggested and not confirmed separately. The ordinary build was rebuilt at 803,648 bytes and not installed, so the board stays on the Classic test firmware. Host tests are 52 of 52. The ESP32 another agent drives was not touched. The core-syntax audit was run: `.ai/core.md` and `.ai/core-syntax.md` were re-read, `.ai/core.md` was confirmed unchanged, the complete `.ai/` diff was inspected and holds this entry, the new BL6-012 with its index row and summary line, and a dated revision to BLE-020's verification, and this entry was validated as number 52 of 100 with six canonical sections in order, a one-sentence Purpose, prose in Outcome and Next Steps, only eligible paths in Files Modified and allowed Status values, with the reference store at 186 records.

#### Next Steps:

The Classic gamepad connection test comes next: pair with the 8BitDo in Switch mode, open its HID control and interrupt channels over the SDK's Classic L2CAP, and print raw input reports, before any HID host is written. Phase 2 of the cleanup, a measured proposal on the desktop's four copies of the screen, about 88 KB, waits until memory is short again. A game-core load and a TinyNES run on the Classic firmware should be confirmed. Entry 50's zoom mode still awaits its book upload and the operator's test, and the updater's silent failure of a sector that differs after three attempts is still unfixed.

#### Files Modified:

- ports/bl616/phosphor/td_phosphor_app.cpp
- ports/bl616/tang_ble.c
- ports/bl616/tang_fpga_uart.c
- ports/bl616/tang_fw_update.c
- ports/bl616/tang_jtag_glue.h
- ports/bl616/tang_jtag_programmer.c
- ports/bl616/td_gutenberg_app.cpp
- ports/bl616/tdsh_tang_flash.c
- tools/ram_report.py

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 53 COMMIT Unreleased 2026-11-18T09:08:03-07:00

#### Coming From:

Unreleased e83b1d5

#### Purpose:

Remove the desktop's redundant screen copies by having TinyDesk draw the HDMI layer from cells, giving the Terminal app a screen only while it is shown, and dropping TinyDesk's unused second terminal.

#### Outcome:

The three approved options went in and the operator passed them on the board (BL6-013). TinyDesk carries two new patches: `0005-local-display.patch` adds `td_set_local_display()`, through which TinyDesk hands its composed screen to the port with no ANSI and no terminal, and a `TD_SCREENS` count for its terminals; `0006-terminal-screen-on-demand.patch` adds `TD_TERMINAL_KEEP_SCREEN`, under which the Terminal app's emulator exists only while a window shows it and the shell's output while it is closed is dropped. The port builds `TD_SCREENS=1` and `TD_TERMINAL_KEEP_SCREEN=0`. `tang_osd_desk.c` diffs TinyDesk's screen against TinyDesk's front buffer while the desktop runs, takes the frame lock and the link lock one at a time so the two cannot deadlock, and keeps the console's emulator and shadow on the heap only while the console is on the layer; the declarations are in the new `tang_osd_desk_local.h`, so tests that include `tang_osd_desk.h` need no TinyDesk path. The desk layer's host test gained local-mode checks, 130 in all, which fail under four mutations, with host stubs for the FreeRTOS mutex and task notification; TinyDesk's suite passed 34 of 34 at its defaults and with `TD_TERMINAL_KEEP_SCREEN=0`, and 33 of 34 with `TD_SCREENS=1` added, the one failure being `test_screens`, which attaches the second screen a one-terminal build refuses by design; host tests are 52 of 52. BUILDING.md's list of carried TinyDesk patches had gone stale at the move to v0.5.2, naming 0001 and 0004 and an old 0003 digest; it now lists the four patches the directory holds, with their digests. Static RAM fell by 50,005 bytes on both builds, measured with `tools/ram_report.py`. The Classic firmware `e83b1d5-dirty.0f7e1dc`, 1,042,016 bytes, was installed with `tools/tinytang_flash.py`, which saw the commit end after 5 s, and started after the two-wire relay sequence; `ble status` then read 201,112 bytes of heap and 114,448 free with the radio up and the K06 connected, against 95,468 before. The operator ran the desktop, closed and reopened Terminal, hid and showed the desktop, quit to the console, reported all passing, and read 125 KB free and a 95 KB peak on System Monitor. The ordinary build was rebuilt at 807,808 bytes and not installed. The ESP32 another agent drives was not touched. The core-syntax audit was run: `.ai/core.md` and `.ai/core-syntax.md` were re-read, `.ai/core.md` was confirmed unchanged, the complete `.ai/` diff was inspected and holds this entry, the new BL6-013 with its index rows and summary line, and a dated revision to BL6-012's consequence, and this entry was validated as number 53 of 100 with six canonical sections in order, a one-sentence Purpose, prose in Outcome and Next Steps, only eligible paths in Files Modified and allowed Status values, with the reference store at 187 records.

#### Next Steps:

The Classic gamepad connection test comes next: pair with the 8BitDo in Switch mode, open its HID control and interrupt channels over the SDK's Classic L2CAP, and print raw input reports, before any HID host is written. TinyDesk's two remaining screen buffers, about 58 KB, are the largest static users left if memory runs short again. A game-core load and a TinyNES run on the Classic firmware should still be confirmed. Entry 50's zoom mode still awaits its book upload and the operator's test, and the updater's silent failure of a sector that differs after three attempts is still unfixed.

#### Files Modified:

- BUILDING.md
- CMakeLists.txt
- ports/bl616/tang_osd_desk.c
- ports/bl616/tang_osd_desk_local.h
- ports/bl616/td_desktop_bl616.c
- third_party/patches/tinydesk/0005-local-display.patch
- third_party/patches/tinydesk/0006-terminal-screen-on-demand.patch
- tools/tests/stubs/FreeRTOS.h
- tools/tests/stubs/semphr.h
- tools/tests/tb_osd_desk.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 54 COMMIT Unreleased 2026-11-18T10:06:22-07:00

#### Coming From:

Unreleased d1230b1

#### Purpose:

Find out whether the 8BitDo gamepad in Switch mode gives the BL616 its input over a raw Classic L2CAP HID connection, before a Classic HID host is written.

#### Outcome:

It does, and the pad reconnects by itself, and the operator passed it (BLE-021). The Classic build gained the test command `bthid`, which connects a pad by address, pairs it with Secure Simple Pairing Just Works, opens HID control and interrupt channels on PSM 0x11 and 0x13, registers both PSMs as servers so a pad can open them itself, and prints input reports as raw hex, skipping repeats; `bthid send` sends bytes on the interrupt channel, and the state is taken from the heap at first use. The LE code's connection callbacks now hand Classic links to it: before, its connected callback closed any connection no LE slot owned. The pad paired and bonded with no SDP query and sent the 13-byte Switch Pro simple report `a1 3f` continuously, and pressing each control in turn mapped every button bit, the hat and the four sticks. Turning the pad off reset the first firmware, `d1230b1-dirty.cc50667`, with a Bluetooth assert, which the crash record and the disassembly traced to `bt_conn_unref`: on a link the host makes, this SDK keeps the reference `bt_conn_create_br` returns and drops it itself after the disconnected callbacks, and `bthid` had dropped it too. With that fixed, `d1230b1-dirty.55b33a2` survived the power-off, and the woken pad paged the board and connected, but `bthid` opened its own channels at the same moment and the link went down with LMP response timeout. The final firmware, `d1230b1-dirty.7c661c0`, 1,046,224 bytes, waits 3 s for the pad on a link the pad made; the pad then opened both channels itself and reports resumed, and the operator reported the reconnection working. Each firmware was installed with `tools/tinytang_flash.py`, which saw the commit end, and started with the two-wire relay sequence. The command adds 29 bytes of static RAM and about 4.4 KB of heap once used. The ordinary build was rebuilt at 807,808 bytes and does not contain it, and host tests are 52 of 52. The ESP32 another agent drives was not touched. The core-syntax audit was run: `.ai/core.md` and `.ai/core-syntax.md` were re-read, `.ai/core.md` was confirmed unchanged, the complete `.ai/` diff was inspected and holds this entry and the new BLE-021 with its index row and summary line, and this entry was validated as number 54 of 100 with six canonical sections in order, a one-sentence Purpose, prose in Outcome and Next Steps, only eligible paths in Files Modified and allowed Status values, with the reference store at 188 records.

#### Next Steps:

The Classic HID host proper comes next, as a proposal: keep the link key on the card beside the LE bonds so the pad reconnects after a reboot, accept the pad's channels on reconnection, decode the `0x3F` report into the pad word and the pointer the desk layer and the cores already take, and keep the reference rule BLE-021 records. A game-core load and a TinyNES run on the Classic firmware should still be confirmed. Entry 50's zoom mode still awaits its book upload and the operator's test, and the updater's silent failure of a sector that differs after three attempts is still unfixed.

#### Files Modified:

- ports/bl616/tang_ble.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 55 COMMIT Unreleased 2026-11-18T10:48:13-07:00

#### Coming From:

Unreleased f59a2b3

#### Purpose:

Profile the operator's Bluetooth pads, keyboards, mice and speaker on a PC before the Classic HID host is designed, so its scope follows what the devices actually do.

#### Outcome:

Nine devices and two Wiimote extension setups were profiled (BLE-022) with the new `tools/bt_profile.py`, which pairs a device with BlueZ using a NoInputNoOutput agent, the Tang's IO capability, then records BlueZ's properties, the kernel HID device with its driver and report descriptor decoded into report sizes and fields, and the input events while each control is pressed; the profiles and a summary table are in `docs/bt-profiles/`. Every device paired without a passkey. On Classic, the 8BitDo's descriptor confirms the 11-byte `0x3F` report the Tang received, while the Wii U Pro Controller and a Wiimote with Motion Plus, nunchuck or Classic Controller all speak the Wii Remote protocol under the same IDs and need a report mode and extension setup before they report. On LE, the Xbox pad's report is plain 16-byte HID, the Logitech M750 L and K950 use report layouts that are not the boot layouts the Tang's LE code expects, and the Rii K06's are boot-compatible, which is why it works. The 8BitDo Cube 2 speaker offers only SBC for A2DP. Whether the Wii devices use a legacy PIN is not established, because BlueZ answers it internally and its `LegacyPairing` property was inconsistent. Three faults in the new tool were found and fixed while running it: one-shot `bluetoothctl` lookups waited out a scan timeout, the HID address match compared lowercased text against an uppercase key, and drivers that register their HID device late were missed. `tools/tests/test_bt_profile.sh` checks the descriptor decoder against five recorded devices and fails when field sizes are mis-summed; host tests are 53 of 53. The Tang was powered off throughout and nothing was built or deployed. The ESP32 another agent drives was not touched. The core-syntax audit was run: `.ai/core.md` and `.ai/core-syntax.md` were re-read, `.ai/core.md` was confirmed unchanged, the complete `.ai/` diff was inspected and holds this entry and the new BLE-022 with its index row and summary line, and this entry was validated as number 55 of 100 with six canonical sections in order, a one-sentence Purpose, prose in Outcome and Next Steps, only eligible paths in Files Modified and allowed Status values, with the reference store at 189 records.

#### Next Steps:

Propose the Bluetooth input work from these profiles: a Classic HID host that keeps link keys on the card and accepts a reconnecting pad's channels, with a Switch Pro handler and a Wii Remote handler; boot-protocol selection or a descriptor decoder for LE mice and keyboards such as the Logitech pair; and one retry of the Xbox pad on the Classic build's controller library. A `btmon` capture while pairing a Wii device would settle the PIN question. The Cube 2 speaker is the first target for the A2DP source. Entry 54's other items stand.

#### Files Modified:

- docs/bt-profiles/8bitdo-cube-2-5fc5.json
- docs/bt-profiles/README.md
- docs/bt-profiles/k06-ble-keyboard-3aff.json
- docs/bt-profiles/logi-k950-da4e.json
- docs/bt-profiles/logi-m750-l-2b6e.json
- docs/bt-profiles/nintendo-rvl-cnt-01-tr-66d1-classic.json
- docs/bt-profiles/nintendo-rvl-cnt-01-tr-66d1-nunchuk.json
- docs/bt-profiles/nintendo-rvl-cnt-01-tr-66d1.json
- docs/bt-profiles/nintendo-rvl-cnt-01-uc-5c91.json
- docs/bt-profiles/pro-controller-4d7f.json
- docs/bt-profiles/xbox-wireless-controller-4579.json
- tools/bt_profile.py
- tools/tests/test_bt_profile.sh

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: PASS

---
## 56 COMMIT Unreleased 2026-11-18T10:51:51-07:00

#### Coming From:

Unreleased f421dee

#### Purpose:

Settle with a packet capture whether the Wii devices pair with a legacy PIN, the question entry 55 left open.

#### Outcome:

The Wii U Pro Controller pairs by legacy PIN (BLE-022, revised). The operator ran `btmon` as root while `tools/bt_profile.py` paired the pad from its sync button, and the capture, read back without root, shows a negative Link Key Request reply followed by the pad's PIN Code Request, which BlueZ's wiimote plugin answered with a 6-byte binary PIN equal to the host's Bluetooth address lowest byte first; the pad then issued a combination link key and encryption came up. The SDK's `bt_conn_auth_pincode_entry()` measures its PIN with `strlen`, so the Tang's Wii handler must send the reply with an explicit length. BLE-022 and `docs/bt-profiles/README.md` record this, and the pad was unpaired from the PC again. The capture itself, which holds a link key, stays out of the repository. Nothing was built or deployed, the Tang stayed powered off, and the ESP32 another agent drives was not touched. The core-syntax audit was run: `.ai/core.md` and `.ai/core-syntax.md` were re-read, `.ai/core.md` was confirmed unchanged, the complete `.ai/` diff was inspected and holds this entry and dated revisions to BLE-022's statement, consequence and summary line, and this entry was validated as number 56 of 100 with six canonical sections in order, a one-sentence Purpose, prose in Outcome and Next Steps, only eligible paths in Files Modified and allowed Status values, with the reference store at 189 records.

#### Next Steps:

Write the Bluetooth input proposal from BLE-021 and BLE-022: the Classic HID host with link keys on the card, a Switch Pro handler and a Wii Remote handler that answers the PIN with the Tang's address, boot protocol or a descriptor decoder for LE mice and keyboards, and one retry of the Xbox pad on the Classic build. The Wiimote's own pairing was not captured; it is expected to match the Wii U Pro's and should be confirmed when the handler is tested. Entry 54's other items stand.

#### Files Modified:

- docs/bt-profiles/README.md

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---
## 57 COMMIT Unreleased 2026-11-18T12:03:52-07:00

#### Coming From:

Unreleased 3b72d27

#### Purpose:

Build Phase 1 of the approved Bluetooth input plan, a Classic gamepad host for Switch-mode pads with pairings kept on the card, and make the Classic build the default firmware once it passed.

#### Outcome:

A Classic pad now pairs once and then works from power-up at the console and on the desktop, and the operator passed it (BLE-023). The new `ports/bl616/tang_btpad.c` replaces entry 54's `bthid` test code and takes `btscan` with it: `btpad pair` finds the gamepad in pairing mode by inquiry and pairs it into one of two slots, its link key goes to `/sd/root/ble/br_keys.bin` in a CRC-checked format (`tang_btpad_codec.c`), and each radio start puts the keys back into the host's table, registers the HID servers and turns on page scan, with the radio started at boot when a pad is saved; `tang_ble.c` hands it every Classic link and runs its card writes and channel opening in its background task. The first build saved no key, because the controller reports pairing complete before it delivers the link key, so the key is now read once encryption is up or a channel opens. The pad drives the desktop through the wired Wii Classic Controller's key map: the operator ran `desktop` from the shell's history with the D-pad and A and used the pointer, clicks and home. During the test home and L both toggled the desktop, so only home does now; the R-held pointer mode was dropped because the left stick is the pointer, and the stick's top speed was halved to 48 counts a poll after the operator found it too fast. After a relay power cycle the pairing loaded from the card and the woken pad reconnected by itself. The tested firmware was `3b72d27-dirty.3eec454`, 1,050,464 bytes, after five earlier installs in the cycle, all through `tools/tinytang_flash.py`; one attempt was refused because the desktop was running, and the board was power-cycled to the prompt first. Static RAM rose 1,909 bytes. `defconfig` now selects the BR/EDR controller library with A2DP and AVRCP off, as approved, and the default build was confirmed to differ from the tested image only in its 6-byte build identity. Host tests are 54 of 54, with `test_btpad.sh` at 87 checks and `test_osd_desk.sh` at 136, each new section failing under mutation. The ESP32 another agent drives was not touched. The core-syntax audit was run: `.ai/core.md` and `.ai/core-syntax.md` were re-read, `.ai/core.md` was confirmed unchanged, the complete `.ai/` diff was inspected and holds this entry, the new BLE-023 with its index row and summary line, and dated revisions to the verifications of BLE-002, BLE-020 and BLE-021, and this entry was validated as number 57 of 100 with six canonical sections in order, a one-sentence Purpose, prose in Outcome and Next Steps, only eligible paths in Files Modified and allowed Status values, with the reference store at 190 records.

#### Next Steps:

The approved test work comes next: a host test of `tang_btpad.c` against a fake Bluetooth stack replaying the event orders seen on the board, a script that builds both the default and the LE-only firmware, and a quick mode for `run_all` that skips the FPGA simulations and upstream suites unless their sources changed. After that, Phase 2 is the Wii family handler with its legacy PIN reply (BLE-022), then the LE checks of Phase 3. A game-core load and a TinyNES run on the Classic firmware should still be confirmed, entry 50's zoom mode still awaits its test, and the updater's silent sector failure is still unfixed.

#### Files Modified:

- defconfig
- ports/bl616/tang_ble.c
- ports/bl616/tang_btpad.c
- ports/bl616/tang_btpad.h
- ports/bl616/tang_btpad_codec.c
- ports/bl616/tang_btpad_codec.h
- ports/bl616/tang_btpad_hooks.h
- ports/bl616/tang_osd_desk.c
- tools/tests/tb_btpad.c
- tools/tests/tb_osd_desk.c
- tools/tests/test_btpad.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 58 COMMIT Unreleased 2026-11-18T12:12:49-07:00

#### Coming From:

Unreleased 9f81a68

#### Purpose:

Act on the test-suite review the operator approved: a host test of the Classic gamepad host, a build of both firmware variants, and a quick mode for the host test run.

#### Outcome:

The three pieces are in place and checked (TOOL-027). `tools/tests/test_btpad_host.sh` runs the real `tang_btpad.c` against a fake Bluetooth stack that counts connection references as the SDK does and replays the event orders seen on the board; its 43 checks pass, and each of the two faults found on the board in entries 54 and 57, and three more of the same kind, fails it under mutation. Its first run printed nothing, because `tang_btpad.c` pulls in the real console header that maps `printf` to the console; the test undefines it. `tools/build_both.sh` builds the default Classic firmware and the LE-only variant in parallel and fails on a build error, a misplaced Classic host or a warning in the project's sources; it first read the bootloader's size instead of the firmware's, now fixed, and it caught a warning planted in the LE-only branch. `run_all.sh --quick` skips nine slow tests unless what they depend on differs from the upstream branch; it ran in 23 s against the full suite's 462 s, and a new file under `fpga/` brought a simulation back into the run. The review that led here found the Bluetooth glue, the emulator apps and the firmware update path without host tests; the first is now covered and the other two remain separate proposals. Host tests are 46 of 46 in quick mode with nine skipped, and the suite now holds 55 tests. No firmware source changed, so nothing was installed; the board stays on `3b72d27-dirty.3eec454`. The ESP32 another agent drives was not touched. The core-syntax audit was run: `.ai/core.md` and `.ai/core-syntax.md` were re-read, `.ai/core.md` was confirmed unchanged, the complete `.ai/` diff was inspected and holds this entry and the new TOOL-027 with its index row and summary line, and this entry was validated as number 58 of 100 with six canonical sections in order, a one-sentence Purpose, prose in Outcome and Next Steps, only eligible paths in Files Modified and allowed Status values, with the reference store at 191 records.

#### Next Steps:

Phase 2 of the Bluetooth input plan is next: the Wii family handler with its legacy PIN reply (BLE-022), extended into the new host test before it reaches the board, and checked with `tools/build_both.sh`. Host tests for the emulator apps' pure logic and for the firmware update path remain proposals from the review. Entry 57's other items stand.

#### Files Modified:

- tools/build_both.sh
- tools/tests/run_all.sh
- tools/tests/stubs/btpad/bflb_mtimer.h
- tools/tests/stubs/btpad/bluetooth.h
- tools/tests/stubs/btpad/conn.h
- tools/tests/stubs/btpad/ff.h
- tools/tests/stubs/btpad/keys.h
- tools/tests/stubs/btpad/l2cap.h
- tools/tests/stubs/btpad/tdsh.h
- tools/tests/tb_btpad_host.c
- tools/tests/test_btpad_host.sh

#### Status:

- Build: PASS
- Deployment: N/A
- User Test: N/A

---
