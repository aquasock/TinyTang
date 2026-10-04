## 1 COMMIT Unreleased 2026-10-04T13:28:24-07:00

#### Coming From:

Unreleased 991ad9f

#### Purpose:

Make the Keychron K2 HE a working input device for TinyDesk by carrying a framed UART over the Tang's front USB data pins instead of USB, so no host controller and no AE350 involvement are needed.

#### Outcome:

The keyboard's USB data pins now carry a fixed-baud UART in one direction, CPU-free on both ends: the keyboard's STM32 drives its own D+ line as a GPIO at 281250 baud, and `src/keylink_rx.sv` in the nestang core frames it as `A5 LEN payload SUM` carrying the eight-byte HID boot report. The core maps six keycodes into the SNES-layout joypad word the BL616 already relays — the four arrows into the D-pad, and Enter and Esc into SNES A and B, which the desktop's pointer path reads as the left and right mouse buttons. The core built clean and the deployed image is `nestang-desk.bin` (4624788 bytes, MD5 `ce5b0de9fc6ac38826c5dc43bade1224`), which is where `boot.tdsh` loads from, so a plain reboot boots this core with TinyDesk; the previous image is kept on the card as `nestang-desk-orig.bin` and the original desk core remains available. The user confirms all four arrows, Enter and Esc behave correctly on the desktop, including launching an app by hovering an icon and pressing Enter, and that the grid is stable with nothing pressed. Four faults were found and fixed, each with its own evidence: Keychron's `lpm_task` tested `USBD1.state == USB_STOP`, the state our own `usbStop` created, and re-initialised the USB driver via `init_usb_driver`, which ends in `usbStart` plus `usbConnectBus`, handing PA11 and PA12 back to the OTG core and making them ignore every output-register write; QMK's own USB thread did the same thing independently and is now a no-op under `TANG_MODE`, with `usbStart` and `usbConnectBus` eliminated from the linked image; the transmit loop waited a fixed 256 cycles after each write, making a bit roughly 270 to 275 cycles and therefore five to seven percent slow, which the receiver's four-percent tolerance rejects, and is now paced against absolute cycle boundaries; and `lpm_is_kb_idle` returned true three seconds after boot, after which `lpm_standby` parked the CPU in `__WFI` with the HSE off and starved the hook entirely. Two further faults were mine and are worth recording because both cost rounds: an edge counter added on `usb2_dp` manufactured traffic from a floating pin, since `console.cst` set no pull where `primer25k.cst` sets `PULL_MODE=DOWN`, and that is what invalidated two measurement rounds until it was fixed; and two `tangput` transfers were run against a busy console and mangled the image on the card, producing `Failed to program SRAM` from a bitstream that loads correctly from a quiet console. One rejected hypothesis is worth keeping: the receiver was suspected early, and simulation against `keylink_rx` at 281250 baud showed it parsing every frame with no bad checksums, no bad lengths and no truncations, and a baud sweep showed it tolerating plus or minus four percent and failing at six. Both ends of the design are now reconstructible from this repository: `third_party/patches/0001` through `0004` apply in order to a clean nestang checkout and reproduce the working tree byte-for-byte, and `third_party/patches/qmk/` plus `scripts/apply-qmk-tang-patches.sh` reproduce all eight changed and added keyboard-firmware files byte-for-byte and are idempotent. Both reconstructions were verified against fresh clones rather than assumed. The core-syntax audit required by the change to this log was performed: `.ai/core.md` was re-read and found unchanged and is not edited here, `.ai/core-syntax.md` was re-read, the complete `.ai/` diff was inspected, and this entry was validated against the template, section order, prose, Status and numbering rules. The preceding commit carries the desk-layer work this cycle depends on, committed separately so that this entry describes only its own cycle.

#### Next Steps:

The link carries a full six-key report but only six keycodes are translated, because the channel between core and BL616 is the twelve-bit joypad word and that is all the room it has; therefore ordinary typing is not yet possible and the next cycle should decide which channel carries arbitrary keycodes, whether that is a wider pad word, a new frame type on the link, or the desktop's terminal taking keys from somewhere other than the mouse reports. The card still holds three scratch nestang images (`kl2.bin`, `nestang-keylink.bin`, `nestang-desk-orig.bin`) that were left in place until the core was proven, and they can be removed now that it has been. Nothing else is pending on this cycle.

#### Files Modified:

- THIRD_PARTY.md
- scripts/apply-qmk-tang-patches.sh
- third_party/patches/0004-keyboard-link.patch
- third_party/patches/qmk/README.md
- third_party/patches/qmk/0001-keychron-k2he-tang-mode.patch
- third_party/patches/qmk/0002-keychron-k2he-tang-keymap.patch

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 2 COMMIT Unreleased 2026-10-04T14:30:45-07:00

#### Coming From:

Unreleased cadec82

#### Purpose:

Carry arbitrary keystrokes from the keyboard link to the desktop so the board can be typed on, not merely pointed with.

#### Outcome:

Typing works end to end. A new core response type, `0x08`, carries the eight-byte HID boot report upward -- the modifier byte, the reserved byte a boot report always holds, and six usage codes -- sent on change like the joypad and with its state value doubling as the response type, as the existing responses do. On the BL616 the report is kept as sticky state exactly as the pad is (`fpga_frames_keyboard`, which also drains cached keyboard frames so they cannot crowd out a reply someone is waiting for), exposed through `tang_fpga_keyboard`, and translated by the new `ports/bl616/tang_key.c` into the byte stream the desktop's own input parser reads: letters and punctuation as themselves, shift applied so the byte carries the result rather than the intent, Ctrl as `0x01`-`0x1A` which the parser maps back to ctrl+letter, and the navigation keys as CSI sequences. The core built clean and the deployed image is `nestang-desk.bin` (4606154 bytes, MD5 `996155c8555bce6e6d7e60fef5b7f805`); the BL616 firmware was rebuilt and reflashed with `tangflash`, which commits and resets but only runs after a power cycle -- a step the README's quick start states and the first attempt omitted, leaving the board presenting an FT2232 rather than the CDC console until the power cycle. The translation module is host-tested by `tools/tests/test_key.sh`, 32 checks. The user reports that all keys work and that mashing the keyboard no longer locks the desktop up. Two faults were found in the first attempt and both were mine. The first was an infinite key repeat: a boot keyboard reports *state*, so holding a key sends one report and then nothing, which is why repeat must be generated here -- but it also meant a report that had frozen because the link died was indistinguishable from a key still held, and the last key repeated forever, driving the desktop to repaint and keeping the failed link busy, so the stall was self-sustaining and did not recover. The core now resends an unchanged report as a 100 ms heartbeat, which makes liveness measurable, and repeat stops after 300 ms without a report; `tb_key.c` carries a case that reproduces the old behaviour. The second was the transmit arbiter, which had put the keyboard ahead of the command replies the BL616 blocks on: the pad already sat ahead of them upstream, which was survivable because a pad only changes when touched, but a keyboard reports on every keystroke, so while typing the reply was never selected, `tang_fpga_wait` timed out and the desk layer stalled. The order now follows one rule -- anything with a waiter goes first, then the unprompted traffic -- and the disk requests were moved to the waiting side with it. The unprompted states also did `response_ack <= response_req`, marking a pending command answered without sending it; that was upstream's wart in the joypad path, which the keyboard path had copied and made reachable, and neither does it now. Rejected hypothesis worth recording: the receiver was suspected of losing frames under load, and it was not implicated -- the stall was the reply queue being starved and then the repeat flood keeping it that way. `scripts/apply-nestang-patches.sh` also needed a fallback: adding a port to an instantiation that patch 0003 edits made that patch's reverse-check fail, not because the tree was wrong but because its context had moved three lines, so the applier now tries strict first and falls back to one line of context, staying fail-closed on a genuinely foreign patch. Both halves of the design remain reconstructible: `third_party/patches/0001` through `0005` apply in order to a clean nestang checkout and reproduce the working tree byte-for-byte, verified against a fresh clone rather than assumed. The core-syntax audit required by the change to this log was performed: `.ai/core.md` was re-read and confirmed unchanged, `.ai/core-syntax.md` was re-read, the complete `.ai/` diff was inspected, and this entry was validated against the template, section order, prose, Status and numbering rules.

#### Next Steps:

The keyboard link now carries a full boot report but the desktop consumes it as terminal input, so anything a terminal cannot express has no path yet -- function keys, and modifier combinations beyond ctrl and shift. The obvious next move is to decide whether that matters before adding it. Separately, the board was seen at a shell prompt rather than the desktop after one power cycle even though `boot.tdsh` is present and a core was loaded; the user reports the desktop did come up, so this may have been a probe racing the boot rather than a real fault, but it is unconfirmed and worth one deliberate check. Nothing else is pending on this cycle.

#### Files Modified:

- ports/bl616/fpga_frames.c
- ports/bl616/fpga_frames.h
- ports/bl616/tang_fpga_link.h
- ports/bl616/tang_fpga_uart.c
- ports/bl616/tang_key.c
- ports/bl616/tang_key.h
- ports/bl616/tang_osd_desk.c
- scripts/apply-nestang-patches.sh
- third_party/patches/0005-keyboard-typing.patch
- tools/tests/tb_key.c
- tools/tests/test_key.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 3 COMMIT Unreleased 2026-10-04T14:57:01-07:00

#### Coming From:

Unreleased a42460a

#### Purpose:

Stop the keyboard's arrow keys doing two jobs at once, so the same six keycodes are either a pointer or ordinary keys and never both.

#### Outcome:

The keyboard now has two modes and a modifier between them. With right-alt held, the arrows and Enter/Esc are the desktop's pointer and its two mouse buttons; released, they are only keystrokes and the keyboard types normally. The user confirms both modes behave. The change is core-only, which matters for the loop as much as for the code: it deployed as an upload and a `tangload` with no reflash and no power cycle. The reason a modifier was needed at all is that the arrows are genuinely wanted as two things -- a pointer on the desktop and arrow keys inside a window -- and the first attempt at the keyboard link had them doing both simultaneously, so scrolling across the desktop while typing in the Terminal was impossible; that was the fighting the user reported and the reason this cycle existed. The first attempt at the fix was wrong and is worth recording, because the mistake was a half-solution that looked complete: gating the pad word on the modifier stopped the pointer half but not the key half, since those six keycodes reach the desktop twice -- once as pad bits built by this core and once inside the HID report relayed upward -- so in pointer mode an arrow moved the pointer and typed an arrow. The core now also withholds those six usage codes from the report while the mode is held, zeroing them, which in a boot report is indistinguishable from a key never pressed. Both halves therefore live where the mode is decided, which is the only place that can keep them consistent. The deployed image is `nestang-desk.bin` (4606154 bytes, MD5 `191537528e67911993c6fd40173057f0`); the build reports timing MET with the worst slack a hold of 0.143 ns and the worst setup +1.064 ns, and 9 percent logic, 5 percent registers and 13 percent BSRAM, so neither the link nor the typing path nor this mode cost anything material in area. Two pre-existing conditions are recorded rather than fixed: the 0.143 ns hold margin is thin enough that a future change could turn it into a violation without any warning in the source, and the tool warns that `sys_clk` is not on a dedicated clock route. The patch applier needed a second repair, and the reason is structural: this cycle modified lines that patch 0004 had added, so 0004 could be neither applied nor reverse-applied and no context tolerance could detect it, because the patch's own added lines are no longer present verbatim. The applier now presumes the series applied in a tree with local changes, and still fails hard in a clean tree where a non-applying patch is a real fault; the reconstruction test against a fresh clone remains the actual guarantee that the series is correct, and it passed for all six patches with the tree reproduced byte-for-byte. One rejected approach is worth keeping: a USB mouse was considered for the second front port and abandoned, because the stock low-speed host in this core speaks only low-speed USB while a modern mouse is full-speed, and the user stopped it before any work was done; the mouse, if it happens, will follow the keyboard's route -- device firmware driving its own data line as a UART -- not a USB host. The core-syntax audit required by the change to this log was performed: `.ai/core.md` was re-read and confirmed unchanged, `.ai/core-syntax.md` was re-read, the complete `.ai/` diff was inspected, and this entry was validated against the template, section order, prose, Status and numbering rules.

#### Next Steps:

The pointer still depends on the keyboard, so a real pointing device remains the open item, and the decision already taken is that it follows the keyboard's route rather than a USB host -- which needs the mouse's own firmware and cannot start until its MCU and a build target for it are known. Smaller and independent: right-alt is consumed as the mode switch and is not forwarded as a key, so AltGr characters are unreachable, and function keys have no path because the desktop consumes input as terminal bytes. Nothing else is pending on this cycle.

#### Files Modified:

- scripts/apply-nestang-patches.sh
- third_party/patches/0006-pointer-mode.patch

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 4 COMMIT Unreleased 2026-10-04T15:17:30-07:00

#### Coming From:

Unreleased dc683cf

#### Purpose:

Make the two boot scripts safe to run from a live desktop, and stop a raw transfer from ever being aimed at a console that is not a shell.

#### Outcome:

Two faults were fixed and one was made impossible by construction. The scripts: `boot-cart.tdsh` and `boot.tdsh` now drop the desk layer before programming a core, because programming the FPGA from underneath a live overlay leaves the overlay asserted over whatever the new core draws, and a core that cannot draw the layer falls through to the legacy page -- a black screen with the core's logo on it and the game running invisibly behind it. That is exactly what the user saw when they ran `boot-cart.tdsh`: it looked like a ROM that had failed to load. `boot-cart.tdsh` also now defaults to `nestang-desk.bin` rather than the stock `nestang.bin`, since on this board the patched image is the NES core and the stock one carries neither the keyboard link nor the desktop layer, so the old default cost the keyboard its input until a reboot. The tool: `tools/tinytang_put.py` now refuses to send unless the console is at a shell prompt, checking for the desktop's own markers first. That guard exists because of what this cycle actually cost. A 131 KB ROM was sent to the console while the desktop was running, and raw bytes and keystrokes are the same thing on this wire, so the desktop received the file as typing into whichever window had focus rather than `tangput` receiving it as data; the symptom on the card was a root directory full of `new.txt` and `New folder` entries with binary data where filenames should be, and the file allocation table came apart with it. The card now mounts and still accepts writes, but its top-level directories -- `/cores`, `/scripts`, `/roms` -- are no longer reachable, so `boot.tdsh` cannot find its core and the board came up on a stock core instead: `fpga` reports core 0 where ours reports core 1, which is why the user saw a NES menu and no desktop. The repair is outside this repository: `fsck.vfat` on the card from a PC, conducted before any reformat, because the card holds cores that are not in this repository. Two things were verified before committing and one was not. The scripts hold their `if`/`endif` structure at 3/3 and 7/7, and the committed revision fails `bash -n` at the same kind of line, confirming that bash is simply the wrong checker for tdsh syntax rather than anything having been broken; `tools/tinytang_put.py` compiles. The fixes were deployed to the card successfully before the damage was understood, but they were never exercised, because the card had already come apart by the time the corrected scripts could be run -- hence the status below. Nothing about the keyboard, the typing path or the pointer mode is affected: those live in the FPGA patches, the keyboard's own firmware and the BL616 flash, none of which is on the card. The core-syntax audit required by the change to this log was performed: `.ai/core.md` was re-read and confirmed unchanged, `.ai/core-syntax.md` was re-read, the complete `.ai/` diff was inspected, and this entry was validated against the template, section order, prose, Status and numbering rules.

#### Next Steps:

The card must be repaired on a PC with `fsck.vfat` before anything further is written to it, and the boot scripts' fixes then need the test they have not had: run `boot-cart.tdsh` from a live desktop and confirm the game appears rather than a black screen. The card's contents are reconstructible in part from this repository -- `nestang-desk.bin` and the scripts by rebuilding, the BL616 image likewise -- but the other cores under `/cores/console138k` are not carried here and must come from their own projects or from whatever `fsck` recovers. Also worth doing while the card is out: the guard in `tinytang_put.py` should be exercised on both paths, at a prompt and with the desktop running, since only its refusing case matters and that case has not been run.

#### Files Modified:

- scripts/boot-cart.tdsh
- scripts/boot.tdsh
- tools/tinytang_put.py

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---
## 5 COMMIT Unreleased 2026-10-04T15:35:00-07:00

#### Coming From:

Unreleased d60afe2

#### Purpose:

Record in the reference library the two facts this session learned that were not already in it, and audit the change to the core project folder.

#### Outcome:

Two records were added to `.ai/core-reference.md` and nothing else in the project folder was touched. `TOOL-009` records that the console's input is exclusive and that a raw upload's bytes are indistinguishable from typing, which is why `tangput` is only safe while the console is at a shell prompt; its consequence names the damage caused on 2026-10-04 -- a 131088-byte ROM sent while the desktop was running became a root directory of `new.txt` entries and damaged the allocation table badly enough that `/cores`, `/scripts` and `/roms` became unreachable and the board came up on a stock core -- and names `tools/tinytang_put.py`'s new `require_shell()` guard as the mechanism that replaces remembering. `TOOL-010` records that the patch applier stops recognising a patch once a later cycle edits the lines that patch added, since the reverse check wants those lines verbatim and the forward check wants them absent, and that the guarantee is therefore the reconstruction test against a fresh clone rather than the applier's heuristic. Both records follow the file's existing shape and sit after `TOOL-008` in ascending order. The audit found one claim in a draft of `TOOL-010` that had not been verified -- that reduced context cannot help -- and it was removed rather than left in, because it rested on a test of a different case. Worth recording plainly, since it bears on why this entry exists: the black screen the user hit was **already documented**, as `PROT-005` for the overlay's mechanism and `PROT-007` for a ROM stream into a core that is not listening, and the index at the top of the file asks the question in those words. The remedy applied in the previous cycle, `osd desk off`, is the command 0x08 clearing that `PROT-005` states. So the documentation was not the gap for that fault; consulting it was, and `TOOL-009` is the only record here whose subject was genuinely absent. The core-syntax audit required by this change was performed: `.ai/core.md` was re-read and confirmed unchanged, `.ai/core-syntax.md` was re-read, the complete `.ai/` diff was inspected and is twenty-seven lines of additions to `core-reference.md` alone with no deletions, every added record was checked against the existing field shape and family ordering, this entry was validated against the template, section order, prose, Status and numbering rules, and the project-control change is an addition to the reference rather than a rewrite of settled history.

#### Next Steps:

The card must still be repaired on a PC with `fsck.vfat` before anything further is written to it, and the boot scripts' overlay fix then needs the test it has not had. Independently, the habit this cycle exposed is worth acting on rather than noting: `.ai/core-reference.md` opens with an index of questions, and two entries answering one of those questions were never consulted before deriving the answer again from first principles. The next piece of work should start by reading that index for the question at hand.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---
## 6 COMMIT Unreleased 2026-10-04T15:49:39-07:00

#### Coming From:

Unreleased b99264d

#### Purpose:

Restore TinyDesk's on-card boot chain after the SD card's root directory was destroyed, and requalify the shell, the desktop and the NES cartridge boot on the board.

#### Outcome:

The card's filesystem was intact and only its root directory was destroyed, so the repair was a userspace reset rather than a recovery tool: the card was absent when the previous session closed, and when it returned its root held twenty-seven `new.txt` and `New folder` entries with binary data where filenames belong, while a twenty-megabyte write-and-read roundtrip on the same card was clean and reclaimed its space on deletion. A `fsck.vfat` could not be run from this host -- the session is uid 1000, is not in the `disk` group, and `sudo` is blocked by the no-new-privs flag, so the block device is not writable -- so the root was cleared in userspace and `/cores/console138k`, `/scripts` and `/roms` were recreated with `nestang-desk.bin` (4606154 bytes, MD5 `191537528e67911993c6fd40173057f0`), `boot.tdsh`, `boot-cart.tdsh` and `castlevania.nes` (131088 bytes). About 4.6 MB of orphaned clusters stay marked allocated until a `fsck.vfat` is run from a PC, which is harmless on the 29 GB volume and is the only residue of the damage. The BL616 firmware was rebuilt from clean (`make clean` then `make CHIP=bl616 BOARD=bl616dk`, all 203 objects) and the build passed, but the image is not byte-reproducible: it differs from the previously flashed build in exactly six bytes, the embedded `__DATE__` and `__TIME__` strings, so an image on the board can be confirmed only by its code and never by its hash. The patched core written to the card is byte-identical to the documented known-good, and the patch series `0001` through `0006` resolves on the `nestang` checkout with the applier idempotent, its before and after tree hashes equal; the core itself was not re-synthesised. On hardware the board booted `boot.tdsh` into TinyDesk, the desktop drew its start menu and application list over the CDC, the Terminal window opened onto the `root@tinytang:~#` prompt, and `tdsh run /scripts/boot-cart.tdsh` reported `tangload: core loaded`, `fpga: core 1 answering on UART1 at 2000000 baud`, `nesload: 131088 bytes in; the core is running` and `boot-cart: up`; the user reports Castlevania is playing on the HDMI, which is the result the previous cycle left open. The BL616 on the board was not reflashed, because no firmware source has changed since cycle 2 reflashed it and a rebuild would differ only in that same date stamp. The core-syntax audit required by the change to this log was performed: `.ai/core.md` was re-read and confirmed unchanged, `.ai/core-syntax.md` was re-read, the complete `.ai/` diff was inspected, and this entry was validated against the template, section order, prose, Status and numbering rules.

#### Next Steps:

The remaining piece of the project's stated end state is the Tang-Phosphor core, which is not on the card and comes from its own project. Two smaller items are open: the card's orphaned clusters should be reclaimed with `fsck.vfat` from a PC when convenient, and the boot scripts' overlay fix and `tinytang_put.py`'s `require_shell()` guard still carry the tests they were left with in cycles 4 and 5, which this requalification did not exercise.

#### Files Modified:

None.

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 7 COMMIT Unreleased 2026-10-04T16:17:45-07:00

#### Coming From:

Unreleased 8cdb6c0

#### Purpose:

Make TOOL-009 and TOOL-010 reachable from the reference's own lookup path, which they were not.

#### Outcome:

The Active routing table and the Fast lookup index were audited against the records and exactly two of the eighty were unreachable: `TOOL-009`, the console's exclusivity and the raw-upload hazard, and `TOOL-010`, the patch applier's recognition limit. Both were added in an earlier cycle but neither was wired into the two places `core.md` sends an agent first, so the Fast lookup index ran `TOOL-008` straight into `EXTCTL-001` and the routing table carried no question that led to either -- nothing like "is it safe to send a file to the board". That matters because the index is the reference's whole interface, and the record a reader most needs before touching the card was the one it could not find. The fix adds two routing rows, "Is it safe to send a file to the board, or is the desktop holding the console?" and "Why does the patch applier stop recognising a patch?", and two fast-lookup lines summarising the same records, all following the existing table and one-line shapes and inserted in topic order after `TOOL-008`. The audit that found the gap was rerun after the change and reports no orphans among the eighty records. Worth recording as the reason this cycle exists: the gap was found by walking into it, when a session sent files at TinyDesk's Terminal believing it was the plain console -- the hazard `TOOL-009` states, and one the reference's index could not have warned about because it did not list the record. The core-syntax audit required by the change to this file was performed: `.ai/core.md` was re-read and confirmed unchanged, `.ai/core-syntax.md` was re-read, the complete `.ai/` diff was inspected and is four added lines to `core-reference.md` with no deletions, and the two added lines were checked against the records they summarise.

#### Next Steps:

The Castlevania cart-boot script, `scripts/castlevania.tdsh`, is written in the tree but is not committed and is not on the card, so its own cycle -- deploy, then confirm it boots the ROM from the desktop -- is still open. That cycle is blocked on reaching the card safely: the board boots into TinyDesk while `/scripts/boot.tdsh` is present, so the console is reached either by exiting the desktop or by renaming that file and rebooting, after which the script goes over with `tools/tinytang_put.py` and is run. The card's own state after the second incident has not been inspected, and inspecting it needs either the microSD reader or that confirmed console.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---
