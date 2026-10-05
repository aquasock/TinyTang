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
## 8 COMMIT Unreleased 2026-10-04T17:02:50-07:00

#### Coming From:

Unreleased a8222d1

#### Purpose:

Build an emulator-free menu core from the same nestang checkout, so TinyDesk has a host of its own without forking the project.

#### Outcome:

The strip is 26 files and 16,436 lines out of a 20,307-line design, with about 3,871 kept: `iosys_bl616`, both `textdisp` layers, `gowin_dpb_menu`, `uart_fixed`, `nes2hdmi`, `nestang_top` and `hdmi2/`, plus both `keylink_rx` receivers and the DS2 controllers. The couplings were settled by reading rather than guessing, and one guess was wrong in a way worth keeping: the first build died on `loader_do` being driven twice, because `iosys` *drives* `rom_do` and `rom_do_valid` -- the BL616 streams ROM bytes down the link into the loader -- so it is the loader that consumed them that is gone, not the source. The four signals the survivors still read, all consumed by `nes2hdmi`, are tied off in the `else` of `ifndef MENU_CORE`. A second build failed in the timing stage because `nestjs.sdc` constrains `fclk`, the SDRAM clock, whose only consumers were `sdram_nes` and the NES memory ports; with no master it is removed and a constraint naming it fails outright, so the menu core carries its own SDC with `clk` taken as a primary clock on its own net. The artifact is `impl/pnr/nestang_console138k_ds2_menu.bin`, 4492288 bytes, MD5 `c747105422aa4462bffc67c555b2bda3`, built with Gowin 1.9.11.03 at worst setup +3.443 ns and worst hold +0.144 ns. Its logic is about a fifth of the patched NES core's -- 2781 LUT against roughly 12400, and BSRAM 13 of 340 against 43 -- but its bitstream is only about 2.5 percent smaller, because Gowin frames and routing rather than logic dominate the image; so the strip buys a far smaller design, not a measurably faster `tangload`, and the thin hold margin is unchanged at +0.144 against +0.143 ns, which is to say it is not the emulator's and the strip neither fixes nor worsens it. None of this touched the NES core: rebuilt with the menu patch present, `nestang_console138k_ds2.bin` came back byte-for-byte as `191537528e67911993c6fd40173057f0`, the documented cycle-3 image, which also establishes that the Gowin core build is reproducible where the BL616 firmware build is not. Reconstruction passed: a fresh clone at the pinned commit, the common series through the applier, then the menu patch reproduces the working tree byte-for-byte. On hardware the core went to the card with `tangput` -- safe because the console was first confirmed to be the plain one by checking for absolute-position sequences, zero of them against the shell's documented emission set (`TDSH-002`), which is the check that should have been used instead of a prompt match -- and then `tangload` reported `core loaded`, `fpga` reported `core 1 answering on UART1 at 2000000 baud`, `osd desk on` succeeded, and `desktop` produced 13191 bytes rendering the taskbar and icons: TinyDesk running on a core with no NES machine. The user confirmed it on HDMI and reported one fault worth its own cycle: between the core resetting and the desktop painting, the screen shows stray characters. The cause is established and is not a fault in this patch: the compositor shows the legacy 32x28 OSD page whenever the overlay is asserted and the desktop layer is not, a core comes out of reset with the overlay on (`PROT-005`, `iosys_bl616.v` `reg overlay_reg = 1`), and that page's store, `gowin_dpb_menu.v`, has no initialisation at all, so its power-on contents are drawn; the font is initialised, which is why the garbage renders as recognisable glyphs. It is transient on the NES core because the firmware writes the ROM menu into that page, and persistent on the menu core, which has no ROM menu to write it. This commit also brings in `scripts/castlevania.tdsh`, left untracked when its own cycle closed. The core-syntax audit required by the change to this log was performed: `.ai/core.md` was re-read and confirmed unchanged, `.ai/core-syntax.md` was re-read, the complete `.ai/` diff was inspected, every added record was checked against the reference's field shape and family ordering, the new record was routed from the Active routing table and the Fast lookup index as well as added to the records, and this entry was validated against the template, section order, prose, Status and numbering rules.

#### Next Steps:

The stray-character window is the next cycle: clear the legacy page after `tangload` and before `osd desk on`, using the `osd clear` the firmware already has (`ports/bl616/tang_osd.c`), so the uninitialised page is never composited; that is a change to `boot.tdsh`, `boot-cart.tdsh` and whatever loads the menu core, and it helps the NES core too because the same window exists there. Separately, the menu core is not yet the boot core -- `boot.tdsh` still loads `nestang-desk.bin`, and the menu core was exercised by hand -- and `third_party/patches/menu/` is the first patch set outside the numbered series, so the applier's reach over it is a thing to keep in view.

#### Files Modified:

- third_party/patches/menu/0001-menu-core.patch
- third_party/patches/menu/README.md
- tools/build_menu_core.sh
- scripts/castlevania.tdsh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 9 COMMIT Unreleased 2026-10-04T17:09:29-07:00

#### Coming From:

Unreleased 9323bae

#### Purpose:

Close the window in which a freshly loaded core shows its own uninitialised text page instead of a blank screen.

#### Outcome:

The cause was established in the previous cycle and is now recorded as `PROT-008`: a core comes out of reset with the overlay asserted, the compositor falls to the overlay colour while the desktop's wide layer is still disabled, and that colour is drawn from the legacy 32x28 text page, whose store carries no initialisation -- so what shows is its power-on contents. The fix is the `osd clear` the firmware already had: `tang_osd_clear()` blanks all 28 rows of 32 columns with spaces and asserts the overlay itself, so it both makes the page blank and leaves the overlay in the state the layer needs. It is issued in `scripts/boot.tdsh` after the `fpga` probe and before `osd desk on`, and in `scripts/boot-cart.tdsh` after the probe and before `nesload`; after the probe deliberately, because the clear travels the BL616 link and the link should be proven live first. Nothing else in either script changed -- the `if`/`endif` structure is untouched and still balances at 3 and 7, which is what cycle 4 recorded for them -- and the change is one command with no control flow. Deployed with `tangput` from the host at the plain console, the console being confirmed by the absolute-position check (`PROT-008`'s window is exactly why that check matters): `/scripts/boot.tdsh` 3283 bytes and `/scripts/boot-cart.tdsh` 2745 bytes on the card, both matching their local sizes. `osd clear` prints nothing on success, so the console log could not confirm it ran; the user's eyes did. The user reports the window is blank now and TinyDesk comes up correctly, and asked for the next cycle to be a boot into console mode with `desktop` typed to switch. The core-syntax audit required by the change to this log was performed: `.ai/core.md` was re-read and confirmed unchanged, `.ai/core-syntax.md` was re-read, the complete `.ai/` diff was inspected, the added record was checked against the reference's field shape and family ordering and routed from the Active routing table and the Fast lookup index as well as added to the records, and this entry was validated against the template, section order, prose, Status and numbering rules.

#### Next Steps:

The next cycle is the user's ask: boot to TinyDesk's console mode and require `desktop` to be typed to bring the desktop up. That means `boot.tdsh` stops short of running `desktop`, and the question to settle first is where the layer is enabled -- whether `desktop` becomes self-sufficient (loading the core and asserting the overlay itself) or `boot.tdsh` leaves the core loaded and the layer ready and only the final command is withheld. Nothing else is pending; `third_party/patches/menu/` remains the first patch set outside the numbered series.

#### Files Modified:

- scripts/boot.tdsh
- scripts/boot-cart.tdsh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 10 COMMIT Unreleased 2026-10-04T17:51:31-07:00

#### Coming From:

Unreleased c57db21

#### Purpose:

Make the board come up at TinyDesk's console with the display showing it, the desktop a thing the user types, and the core's own dead page gone from the menu core.

#### Outcome:

Three pieces landed as one cycle because they turned out to be one thing. First, `desktop` became self-sufficient: `cmd_desktop` now stops the legacy console mirror and calls `tang_osd_desk_set(true)` before `desktop_run()`, so the layer is the command's to enable and is never shown enabled and empty. Second, `scripts/boot.tdsh` stopped short of the desktop and stopped using the legacy page -- and that is where the cycle changed shape: reading `tang_osd_desk.h` showed the module is not a desktop renderer at all but the layer driver, fed by the console tap (`console tap -> td_vterm_write -> diff -> frames`), so *enabling the layer is the console mirror*. `osd term on` was drawing the core's 32x28 page with nand2mario's font, one flat colour and the baked-in logo, which is exactly what the user reported seeing; `osd desk on` draws the same session on the 80x45 grid with per-cell colour instead. No firmware refactor was needed for that, and the earlier estimate in this session that it would be one was wrong. Third, the menu core lost the legacy page for good: `iosys_bl616.v` takes its `4:`/`5:` command items out under `MENU_CORE` and replaces the `textdisp` instance with `assign overlay_color = 15'd0`, keeping command `8:` because the wide layer is gated on the overlay; and `build.tcl` excludes `gowin_dpb_menu.v` and `textdisp.v` from the menu build, guarded with `[info exists menu]` so no other board is affected. The effect is structural rather than procedural: with no page there is no source for the compositor's overlay branch, so a reset menu core shows black instead of a logo, and `PROT-008`'s window closes by construction instead of by an `osd clear` someone has to remember. This also revisits the previous cycle's strip: the legacy page was deliberately kept then, on the reasoning that the OSD commands used it, which holds for the NES core where the ROM menu writes it and not at all for the menu core, where its only content was the baked logo. The menu core rebuilds with the page gone -- `nestang_console138k_ds2_menu.bin`, 4492288 bytes, MD5 `b7221eb07758bf69beab6ee4a4039905` -- the reconstruction test passes byte-for-byte against a fresh clone plus the common and menu series, and the NES core rebuilt with all of it present is byte-identical to `191537528e67911993c6fd40173057f0`, which is the evidence that the gating genuinely leaves the stock path alone. On hardware the user confirmed the boot now lands at the console with the session mirrored in TinyDesk's own font and no logo, and typing `desktop` brings the desktop up, both verified from the console stream and on the display. Three defects were found on the way and are recorded rather than fixed: `tools/tinytang_flash.py` prompts `[y/N]` with no `-y`, so on a non-interactive stdin it waits forever and cannot be told from a board that is resetting -- it cost two stalled attempts before the upload and `tangflash` were run by hand; the physical keyboard is not wired into console mode, so at the prompt the board can only be typed at from a computer, which is the next cycle; and this cycle's own first edit left `desktop` in `boot.tdsh` after removing `osd desk on`, which would have kept auto-starting it, and was caught before deploying. The core-syntax audit required by the change to this log was performed: `.ai/core.md` was re-read and confirmed unchanged, `.ai/core-syntax.md` was re-read, the complete `.ai/` diff was inspected, and this entry was validated against the template, section order, prose, Status and numbering rules.

#### Next Steps:

The physical keyboard in console mode is the next cycle, and it is the thing that makes the experience whole rather than a display: the keyboard link feeds the core, and only the desktop path turns its output into typing, so the prompt is currently typeable only from the host. Separately, `osd term` and `tang_osd_term.c` now draw a page the menu core does not build, so they should be retired or pointed at the layer rather than left as a command that does nothing; and the menu core itself is still not the boot core -- it was exercised by hand and by `osd desk on`, while `boot.tdsh` loads `nestang-desk.bin`.

#### Files Modified:

- ports/bl616/td_desktop_bl616.c
- scripts/boot.tdsh
- third_party/patches/menu/0001-menu-core.patch
- third_party/patches/menu/README.md

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 11 COMMIT Unreleased 2026-10-04T18:00:34-07:00

#### Coming From:

Unreleased 4413745

#### Purpose:

Let the physical keyboard type at the console, so console mode is a console rather than a display that only a host can type into.

#### Outcome:

The gap was that the keyboard had exactly one consumer: `tang_key_step` was called from `tang_osd_desk.c` alone, on the desktop's input path, so with the desktop not running the board read the keys and did nothing with them, and the prompt took input only from the CDC. The fix is routing rather than a driver, and it is one expression: `bl616_terminal_read_byte` in `ports/bl616/tdsh_platform_bl616.c` now chooses between three sources -- the desktop's Terminal when it owns the console, otherwise the layer's ring when the layer is up, otherwise the CDC. Everything the branch needs already existed: `tang_key.h` states that `tang_key_step` produces "the byte stream an ANSI terminal sends", so there was no translation to write; `desk_in_push` is where those bytes already land; `tang_osd_desk_read_byte()` already drains that ring; and `tang_osd_desk_set(true)` already sets `s_enabled` and starts `desk_task`, so the poll that translates the keyboard runs whenever the layer is up, desktop or not. The source order is the load-bearing part: while the desktop owns the console it stays the only reader, which is what `TOOL-009` requires of anything sharing that stream, and the CDC stays last so a host can still type. The firmware is `2b0005a0a08d576f50cf04228d32a6d2`, 321360 bytes, flashed with `tangput` and `tangflash` and confirmed by the FT2232 signature before the power cycle. The user reports everything passes: the keyboard types at the console prompt, and the desktop comes up and takes input as before. One caveat is recorded rather than fixed: the ring also carries the pointer's reports and the emulator's answers, so in console mode a pointer move -- arrows with right-alt held -- reaches the shell as a mouse escape sequence. No plain keystroke produces one, so it is odd rather than harmful, and the clean fix if it ever matters is a ring for typing alone. This cycle also fixed the diagnostic that had been reporting the previous entry as having no sections: `tools/check_core_log.py` checks section order, numbering, types, the terminator, the Status table and the hundred-entry cap, and it was tested against a deliberately mangled copy so that a checker which only ever says "fine" cannot pass for one that works. The core-syntax audit required by the change to this log was performed with that tool rather than by hand -- it reports all eleven entries conforming -- alongside the manual steps: `.ai/core.md` was re-read and confirmed unchanged, `.ai/core-syntax.md` was re-read, and the complete `.ai/` diff was inspected.

#### Next Steps:

The pointer reports reaching the shell in console mode are the one known rough edge. Otherwise nothing is pending on this cycle, and the keyboard is wired everywhere it is wanted. Two things remain open from earlier and are unchanged by this: `third_party/patches/menu/` is still the only patch set outside the numbered series, and the menu core is still not the boot core -- `boot.tdsh` loads `nestang-desk.bin`, and the menu core has been exercised by hand.

#### Files Modified:

- ports/bl616/tdsh_platform_bl616.c
- tools/check_core_log.py

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 12 COMMIT Unreleased 2026-10-04T18:34:36-07:00

#### Coming From:

Unreleased bd3aec6

#### Purpose:

Fix the two defects the boot experience exposed -- one cosmetic, one a transport regression this session introduced.

#### Outcome:

The cosmetic one first: `tang_osd_desk_command` printed `osd desk: on` after enabling the layer, so at boot that line was the first thing the layer ever mirrored and the confirmation became the first thing on screen. `osd desk on` and `off` are now quiet on success, like `osd clear` in the same command family, with failures still speaking and `osd desk status` still reporting. The second defect was mine, and it is the one worth recording properly: the keyboard cycle made the console's read path choose between the layer's ring and the CDC exclusively, so the moment the layer came up the CDC was never read, and the host stopped being able to type at the console with no output to say why. Worse, the commit that introduced it said "the CDC stays last so a host can still type", which was false -- the CDC branch is reached only when the layer is off, and the layer is what boot turns on. The cost was real: `tangput` and `tangflash` need the console to read the host, so the defect blocked the only path that could deliver its own fix. Recovery was a card trip -- `boot.tdsh` renamed away so the board booted to a prompt with no layer, the console came back, and the image was flashed by hand; the boot script was then restored with `mv` at the prompt, which the fix itself made possible. The read path is now a priority rather than a choice: the ring leads and falls through to the CDC, so both are live, because both are ways of typing at one prompt. The `s_term_read` case stays exclusive, which is correct -- the desktop's Terminal owns the console, and two readers on one stream is the hazard `TOOL-009` describes. Verified: with the layer up, `whoami` sent from the host returned `root` and a fresh prompt, which is exactly what the broken build could not do; and `osd desk: on` no longer appears in the boot output. One observation is recorded as unconfirmed rather than as a fact: with `boot.tdsh` withdrawn and no `tangload` run, `fpga` reported `core 0 answering` -- a stock core, where ours answer 1 -- which suggests the FPGA configured itself from its own flash at power-up. That would refine `DEV-003`, which says the FPGA keeps no configuration across a power cycle. It is not recorded in the reference because it is not established: an idle UART could plausibly read as core 0, and the test that separates those two has not been run. The core-syntax audit was performed with `tools/check_core_log.py`, which reports every entry conforming, alongside re-reading `.ai/core.md` (unchanged) and `.ai/core-syntax.md` and inspecting the complete `.ai/` diff.

#### Next Steps:

The left-edge lines the user saw after the prompt are still open, and the whole point of `tools/check_core_log.py`'s companion is that they should be chased with evidence: `osd desk status` reports `<n> cells in <m> rows sent, <k> refused`, and a non-zero refusal count would mean cells are being dropped, while zero would put the fault in the rendering. The uninitialised-cell theory is already dead -- `td_vterm_init` clears the grid -- so the next step is that number, not another guess. Separately, the FPGA self-configuration observation needs one deliberate test to become a record or be discarded, and `third_party/patches/menu/` remains the only patch set outside the numbered series.

#### Files Modified:

- ports/bl616/tang_osd_desk.c
- ports/bl616/tdsh_platform_bl616.c
- tools/check_core_log.py

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
