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
