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
