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
## 13 COMMIT Unreleased 2026-10-04T20:29:32-07:00

#### Coming From:

Unreleased f8cde86

#### Purpose:

Remove the marks drawn down the left edge of the desktop layer, which entry 12 left open with the refusal count as the next step.

#### Outcome:

The refusal count settled the transport first: `osd desk status` reported 0 refused, so no cells were being dropped. Its other figure is mislabelled -- `s_rows` in `ports/bl616/tang_osd_desk.c` counts every row the flush examines, changed or not, so "5535945 rows sent" against 3763 cells is polls times 45, not traffic -- which is recorded here and was deferred rather than fixed, since it costs a reflash. The fault was in the layer's timing, not its contents, and it is recorded as `PROT-009`. `hdmi.sv`'s counters name the pixel sent on the next clock and the stock `rgb` path is one register deep to match, but `textdisp_wide` was three registers deeper and addressed its store with the live `cx`, so visible pixels 0-2 of every line were computed at `cx` 1647-1649 of the previous line, in blanking, where the store's address falls back to cell 0; cell (0,0)'s glyph columns 7, 0 and 0 were therefore painted down the whole left edge in that cell's colours. That predicted, and hardware showed, marks that follow the top-left character: green dashes under the prompt's `r` after `clear`, a clean edge once `help` scrolled a line beginning with a space into row 0, and white pairs after a reboot under the `b` of `boot:`, whose column-0 ink on glyph rows 0 and 6 matches the paired dots in the user's photograph exactly. One wrong turn is worth keeping: the user first read the top-left line as `tdsh`, whose `t` has no column-0 ink, and the prediction from that failed; the photograph showed `boot:` in row 0 and the mechanism held. The fix is `third_party/patches/0007-wide-layer-alignment.patch`: `textdisp_wide` takes `frame_width` and `frame_height` from `hdmi` and addresses the store ahead of the raster by its own depth, wrapping into the next line and frame as the counters do, so `color` is the pixel for the coordinate being presented. The first version looked ahead three pixels combinationally, met timing, and was not kept, because the look-ahead sat in front of the row multiply on the BRAM address and closed the pixel clock at +0.624 ns where cycle 3 had +1.064 ns overall; the address is now registered and the look-ahead is four. The old testbench passed the broken module because it held `cx` still for four clocks per check, which hides any latency, so `tools/tb_textdisp_wide.sv` now streams a free-running 1650x750 raster and checks every visible pixel of a frame plus line 0 again: it fails 445264 of 922881 pixels on the old module, passes all of them on the fixed one, and fails again with the look-ahead off by one. The NES core is `nestang-desk.bin`, 4603392 bytes, MD5 `5d0b920dd28a304ea85ca4f09edc56f2`, TNS 0 on every clock, worst setup +1.360 ns on `hdmi`'s own TMDS path, the layer no longer among the worst paths, and hold +0.143 ns unchanged on the layer's write port, with BSRAM 41 of 340; the menu core rebuilds at 4524032 bytes, MD5 `01f9e221fe220c188778f94dd182d9c6`, setup +1.609 ns and hold +0.144 ns. Reconstruction passes byte-for-byte against a fresh clone at `c2450818` with `0001` through `0007` through the applier and the menu patch on top; the applier now presumes `0002` and `0003` applied in the working tree, as `TOOL-010` describes. The core went to the card with `tangput` after the absolute-position check confirmed the plain console, the size was confirmed on the card, and `boot.tdsh` loaded it with `fpga` reporting core 1. The user reports the left edge clean with `r` in cell (0,0), the exact condition that produced the marks, and clean again after a reboot. `third_party/patches/README.md` called the old three-pixel shift invisible and now says otherwise, and `THIRD_PARTY.md` named the series as `0001` through `0004` and now names `0007`. The core-syntax audit required by the change to this log was performed: `.ai/core.md` was re-read and confirmed unchanged, `.ai/core-syntax.md` was re-read, the complete `.ai/` diff was inspected, the added record was checked against the reference's field shape and family ordering and routed from the Active routing table and the Fast lookup index, and `tools/check_core_log.py` reports every entry conforming.

#### Next Steps:

Three items are open from this cycle and earlier. The `osd desk status` row counter should count rows actually sent, a one-line firmware change that waits for the next cycle needing a reflash anyway. The FPGA self-configuration observation from entry 12 still needs its one deliberate test. And a reachability check run while adding `PROT-009` found four existing records -- `PROV-002`, `TCTL-003`, `TCTL-011` and `TDESK-008` -- in the Fast lookup index but in no row of the Active routing table, which predates this cycle and was left untouched. The pointer reports reaching the shell in console mode, the menu core not yet being the boot core, and `osd term` drawing a page the menu core no longer has all remain as entries 10 and 11 left them.

#### Files Modified:

- THIRD_PARTY.md
- third_party/patches/0007-wide-layer-alignment.patch
- third_party/patches/README.md
- third_party/patches/menu/README.md
- tools/tb_textdisp_wide.sv

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 14 COMMIT Unreleased 2026-10-04T22:19:17-07:00

#### Coming From:

Unreleased e33a213

#### Purpose:

Reserve F12 to switch the screen between TinyDesk and the running core the way MiSTer does, and record the faults this session's testing exposed alongside it.

#### Outcome:

F12 now flips the overlay from the keyboard as L already did from the pad, at the console, inside the desktop and during a game, and the user reports everything works, including on other monitors and capture devices. The key is reserved in `ports/bl616/tang_key.c` as `TANG_KEY_USAGE_TOGGLE`: it is never typed and never becomes the repeating key, so pressing it while a key repeats no longer steals the repeat, and the core does not map it into the pad word, so no game sees it. `ports/bl616/tang_osd_desk.c` reads the keyboard once per poll, since its freshness flag is read-and-clear and the toggle and the typing must share it, detects the F12 edge in both branches, and while the core has the screen absorbs the report with the new `tang_key_absorb()`, so a key held across the switch is neither a fresh press nor a repeat into the shell. With the layer off altogether F12 brings it up on a fresh grid, and `tangload` now starts the task that watches for it, because a core carrying the keyboard link only exists after a `tangload`. Making F12 work in a game meant keeping the layer up across a cartridge load, which `boot-cart.tdsh` and `castlevania.tdsh` had dropped since cycle 4, and the reason they had to is now established rather than assumed: the layer's re-arm after `tangload` forced the overlay on for eight polls, but it runs below the shell, so `nesload`'s `taskYIELD()` starves it until the ROM has streamed and `nesload` has cleared the overlay, after which the re-arm put it back -- on cycle 4's stock `nestang.bin`, which has no wide layer, that was the legacy page and its logo with the game running behind, exactly the symptom recorded then. The re-arm now restores the overlay to `tang_osd_shown()` instead of forcing it, so both scripts keep the layer up and `castlevania.tdsh` gained the `osd clear` that `boot-cart.tdsh` got in cycle 9. Folded in at the user's direction, `osd desk status` now counts rows actually sent rather than rows examined. The key translation is host-tested by `tools/tests/test_key.sh`, 41 checks, including the F12 cases and absorb, and the repeat case fails when the reservation is removed. The firmware built clean at 321552 bytes, MD5 `00dcfd426b5a55aa1a2b3101498da076`; it went to the card with `tangput` after the absolute-position check, was installed with `tangflash` run by hand, showed the FT2232 signature, and ran after the user's power cycle, and both scripts were deployed at 2675 and 1723 bytes; the FPGA cores are unchanged from cycle 13. While testing displays the board began booting to a TangCore splash, and the cause was that `/scripts/boot.tdsh` had been deleted from the card -- nothing else was missing or moved -- so nothing ran `tangload` and the FPGA stayed on the core it loads by itself, answering core 0. It was restored with `tangput`, 4358 bytes and identical to the committed file. How it was deleted is not proven; the suspect is TinyDesk's Files app, whose Delete key opens a confirmation with Delete focused so that Enter removes the selected file, and that is recorded as a possible TinyDesk bug in `TDESK-010`, INFERRED from the source and not reproduced. The same incident settles entry 12's open observation: `DEV-003`, which said the FPGA comes up empty, is superseded by `DEV-006`, recorded as INFERRED because the configuration storage behind it is not traced to a schematic or a Gowin document. The core-syntax audit required by the change to this log was performed: `.ai/core.md` was re-read and confirmed unchanged, `.ai/core-syntax.md` was re-read, the complete `.ai/` diff was inspected and its only deletions are `DEV-003`'s status, routing pointer and index line as the reference's supersession rule requires, with its statement untouched, the two added records were checked against the field shape and family ordering and reached from both indexes, and `tools/check_core_log.py` reports every entry conforming.

#### Next Steps:

`TDESK-010` is a candidate for an upstream issue against TinyDesk -- a destructive confirmation should not default to its destructive button -- and until it is fixed the card's boot chain is one confirmed dialog away from removal, with `tangput` of `scripts/boot.tdsh` as the restore. The pointer reports reaching the shell in console mode, the menu core not yet being the boot core, and `osd term` drawing a page the menu core no longer has remain as entries 10 and 11 left them.

#### Files Modified:

- ports/bl616/tang_key.c
- ports/bl616/tang_key.h
- ports/bl616/tang_osd_desk.c
- scripts/boot-cart.tdsh
- scripts/castlevania.tdsh
- tools/tests/tb_key.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 15 COMMIT Unreleased 2026-10-04T22:34:08-07:00

#### Coming From:

Unreleased c9ce9bb

#### Purpose:

Remove the pointer from console mode, so that at the bare prompt nothing is drawn, nothing moves and right-alt with the arrows does nothing.

#### Outcome:

The pointer now exists only while the desktop runs. `ports/bl616/tang_osd_desk.c` gained `tang_osd_desk_set_pointer()`, and `cmd_desktop` in `ports/bl616/td_desktop_bl616.c` turns it on immediately before `desktop_run()` and off when it returns. With it off, `pointer_cell()` returns the plain cell, so the highlighted cell is not drawn and the shadow diff repaints whatever it last sat on, and the shown branch of `desk_poll()` no longer runs `tang_pad_step()`, so the D-pad and right-alt with the arrows move nothing and no SGR mouse reports reach the shell -- the rough edge entries 11 and 12 recorded, closed by removing the pointer rather than by a second input ring, at the user's direction. The pad's previous state is still tracked, so L's edge and F12 keep working, and right-alt with the arrows, Enter or Esc types nothing at the prompt because the core already withholds those usages from the report while right-alt is held (patch 0006); no core change was needed. The firmware built clean at 321632 bytes, MD5 `3161da57f2312d2815e9afeecaeef758`, went to the card with `tangput` after the absolute-position check and was installed with `tangflash`, and the user confirms all three tests pass: no pointer and no movement at the console, a working pointer in the desktop, and none again after exiting it. One observation is recorded rather than explained: this time `tangflash` came back up on the TinyTang CDC console within seconds, with uptime restarting, instead of presenting the FT2232 as `FLS-001` says a soft reset does and as the previous two installs did, so the image that ran before the user's power cycle could not be identified from the host -- the board reports no build identity and the image's embedded date stamp comes from an unchanged file -- and the user's test after the power cycle is what establishes the new firmware is running. The core-syntax audit required by the change to this log was performed: `.ai/core.md` was re-read and confirmed unchanged, `.ai/core-syntax.md` was re-read, the complete `.ai/` diff was inspected and is this entry alone, and `tools/check_core_log.py` reports every entry conforming.

#### Next Steps:

The `tangflash` reset that returned straight to the console contradicts `FLS-001` and should be watched on the next install; if it recurs, `FLS-001` needs a correcting record, and a build identity the board can report would make an install verifiable from the host rather than by behaviour. `TDESK-010` remains a candidate upstream issue, and the menu core not being the boot core and `osd term` drawing a page the menu core no longer has remain as entries 10 and 11 left them.

#### Files Modified:

- ports/bl616/tang_osd_desk.c
- ports/bl616/tang_osd_desk.h
- ports/bl616/td_desktop_bl616.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 16 COMMIT Unreleased 2026-10-04T22:54:48-07:00

#### Coming From:

Unreleased 9bbcbeb

#### Purpose:

Make the emulator-free menu core the core the board boots, and bring the README up to date with the system as it now stands.

#### Outcome:

`scripts/boot.tdsh` now loads `/cores/console138k/nestang-menu.bin`; the `CORE` line is the only executable change, its header now describes F12 and the menu core, and its `if`/`endif` structure is unchanged at 2/2, which is also what the committed revision has -- cycle 4's 3/3 predates cycle 10's rewrite. Checking the card first found its menu core stale: 4492288 bytes, the cycle-10 build, which predates patch `0007` and so would have brought the left-edge marks back. The menu core was rebuilt with `tools/build_menu_core.sh` and came back byte-identical to cycle 13's, 4524032 bytes, MD5 `01f9e221fe220c188778f94dd182d9c6`, with TNS 0 on every clock, which also confirms the menu build is reproducible as the NES build was shown to be in cycle 8. The core and then the script went to the card with `tangput` after the absolute-position check, and both sizes were confirmed on the card. No firmware change was needed: the keyboard link, F12, the desktop and the layer's re-arm already serve the menu core. Both cores answer core 1, so `fpga` cannot tell them apart; the user's test is what identifies the menu core, and the user reports everything passes -- the board boots to the console with a clean left edge, F12 shows an empty frame and returns to the console, the desktop works, and `boot-cart.tdsh` starts Castlevania on the NES core with F12 switching between the game and TinyDesk. The README had not changed since 2026-10-03 and was rewritten against the code and the reference rather than from memory: it now describes the desktop on the HDMI output, console mode, F12 and L, the keyboard link and its firmware, the pointer existing only in the desktop, both cores and the nestang patch series, the build and test tools, and three new board facts -- the FPGA's own power-up core (`DEV-006`), the console's exclusive input (`TOOL-009`) and the layer's raster alignment (`PROT-009`). It also corrects what had become wrong: the FPGA does not come up empty, `osd term` is described as legacy and due for retirement, the cartridge example uses the patched NES core rather than the stock one, and fact 6 now records that one `tangflash` came straight back to the console as entry 15 found, rather than stating that a power cycle is always needed. Two of its claims were corrected before committing after checking them against source: the put tool's guard checks for the desktop's markers and the prompt, not for absolute positioning, and the QMK licence is GPL-2.0 overall with GPL-3.0-only board files and keymap, as `THIRD_PARTY.md` records. The core-syntax audit required by the change to this log was performed: `.ai/core.md` was re-read and confirmed unchanged, `.ai/core-syntax.md` was re-read, the complete `.ai/` diff was inspected and is this entry alone, and `tools/check_core_log.py` reports every entry conforming.

#### Next Steps:

`osd term` is to be retired, as the user decided: remove the command and `ports/bl616/tang_osd_term.c`, and the README's mention of it with them. The user has since said that `/scripts/boot.tdsh` went missing because they deleted it themselves, so `TDESK-010`'s suspicion that the Files app's dialog removed it is wrong; the dialog's default is still a source fact, and the record needs superseding by one that drops the suspected incident, with the user to decide whether it stays an upstream suggestion. The `tangflash` reset behaviour from entry 15 is still to be watched on the next install.

#### Files Modified:

- README.md
- scripts/boot.tdsh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 17 COMMIT Unreleased 2026-10-04T23:02:44-07:00

#### Coming From:

Unreleased 486985e

#### Purpose:

Retire `osd term`, which the desktop layer replaced and which does nothing on the menu core, and drop the TinyDesk delete-dialog issue now that the missing boot script is known to have been the user's own deletion.

#### Outcome:

`osd term` is gone: `ports/bl616/tang_osd_term.c` and `tang_osd_term.h` are deleted, the `term` subcommand and its usage line are out of `tang_osd.c`, the console writer in `usb_cdc_bl616.c` feeds only the desktop layer, and `cmd_desktop` no longer stops a mirror that cannot be running. The first build failed on the one remaining dependency -- `osd`'s usage text printed the terminal's row count -- and that line now states the fact it stood for, that the core's logo owns rows 25 and 26, and that the page is the NES core's. `usbwatch` now tells the user to run `osd desk on` for the cable-swap test rather than `osd term on`, with a note that its row-27 reading is on the NES core's page; a stale comment paragraph describing the old mirror came out of `scripts/boot.tdsh`, and `THIRD_PARTY.md` and the README no longer list the file. Three settled reference records mention `tang_osd_term.c` in their consequences and were left as history. The firmware built clean at 319600 bytes, MD5 `7dbeb02fd78b1b16ba1f24c95c365ac6`, 2032 bytes smaller, with no `tang_osd_term` symbol in the link map; it went to the card with `tangput` after the absolute-position check, `boot.tdsh` went with it at 4389 bytes, and `tangflash` committed it. This install presented the FT2232 as `FLS-001` describes, so entry 15's straight-to-console reset did not recur. The user power-cycled and reports everything passes: the board boots to the console, the desktop runs and exits to the console, and F12 switches. The host-side check that `osd term` is now rejected was not run, because the board was not on USB when it was attempted; the link map is the evidence that the code is gone. `TDESK-010` is superseded by `TDESK-011`, which keeps only the source fact that TinyDesk's delete confirmations focus their Delete button, records that `/scripts/boot.tdsh` was deleted by the user rather than by that dialog, and states that the project is not raising it upstream, at the user's direction; the routing table now leads to `TDESK-011`. The core-syntax audit required by the change to these files was performed: `.ai/core.md` was re-read and confirmed unchanged, `.ai/core-syntax.md` was re-read, the complete `.ai/` diff was inspected and its only deletions are `TDESK-010`'s status, routing pointer and index line as the reference's supersession rule requires, with its statement untouched, the added record was checked against the field shape and family ordering and reached from both indexes, and `tools/check_core_log.py` reports every entry conforming.

#### Next Steps:

The `tangflash` reset behaviour now has one install of each kind, the FT2232 this time and the console in entry 15, so `FLS-001` stands and the earlier case stays unexplained; a build identity the board can report would let an install be confirmed from the host. Nothing else is pending from this cycle.

#### Files Modified:

- README.md
- THIRD_PARTY.md
- ports/bl616/tang_osd.c
- ports/bl616/tang_osd_term.c
- ports/bl616/tang_osd_term.h
- ports/bl616/tang_usbstat.c
- ports/bl616/td_desktop_bl616.c
- ports/bl616/usb_cdc_bl616.c
- scripts/boot.tdsh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 18 COMMIT Unreleased 2026-10-04T23:23:47-07:00

#### Coming From:

Unreleased 318b23c

#### Purpose:

Give the firmware a build identity the board reports, so an install can be confirmed from the host, and close an overlay race that left TinyDesk over a running game.

#### Outcome:

The firmware now reports which build it is. `cmake/tinytang_build_id.cmake` runs as a step of every build, from a custom target in `CMakeLists.txt` that `app` depends on, and writes `build/tinytang/tinytang_build_id.h` with the commit and, when the tree has uncommitted changes, a short hash of `git diff HEAD`, so that two test builds in one cycle are told apart; the header is rewritten only when the identity changes. The port's platform name carries it, so `platform` and `version` print it without touching the TinyDesk Shell submodule, and the startup banner and `tang` print it too. On the build side a rebuild with no change recompiled nothing and produced a byte-identical image, and a README edit changed the identity and recompiled only `tdsh_platform_bl616.c`. On the board the old firmware reported `platform: bl616/freertos` with no identity, and after the install and the user's power cycle `platform` reported `bl616/freertos, tinytang 318b23c-dirty.0928fb2`, the build that was flashed -- the first install in this project confirmed from the host rather than by behaviour. That identity names this cycle's tree before this entry was written, which is why it carries the previous commit and a dirty hash. The second change came from a fault the user reported mid-cycle: running `castlevania.tdsh` from the Files app opened the Terminal, printed the script's lines correctly, and left TinyDesk on the screen instead of the game. Run from the console the same script reached `castlevania: up` and showed the game, and after a reboot it worked from the desktop too, so the fault was seen once and not reproduced. The code holds a defect that explains it: the layer's re-arm after `tangload` called `tang_osd_set(tang_osd_shown())`, reading the overlay state before taking the link lock, and `nesload` holds that lock while it reads the ROM from the card, where an SD read can let the desk task run, read the state as shown and then wait; once `nesload` cleared the overlay and released the lock, the waiting re-arm sent its stale value and put TinyDesk back over the game. The desktop's Terminal shell runs at priority 4 and its UI at 5 against the console shell's 5, which leaves the desk task, at 2, more gaps to land in. `tang_osd.c` now has `tang_osd_reassert()` and `tang_osd_toggle()`, which read the state and send it in one locked stretch; the re-arm and the F12 / L switch use them, and no read-then-set of the overlay remains. The fix rests on the code, not on a reproduction. The image is 319840 bytes, MD5 `64ec8f981bdf5cef12664792a21ac574`, it went to the card behind a guard that refuses unless the console is a plain shell, and `tangflash` presented the FT2232 as `FLS-001` describes; the user reports that every run of Castlevania from Files passed, back to back and across a reboot, with F12 switching both ways. One error of mine during the investigation is recorded because it repeats `TOOL-009`'s hazard: I treated three seconds of console silence as proof the desktop was not running -- an idle desktop sends nothing -- and chained commands after a probe that had reported 64 absolute-position sequences, so they were typed into a TinyDesk Editor window holding `castlevania.tdsh`; the user stopped it before anything was saved, the file on the card was confirmed at its committed 1723 bytes, and every later send in this cycle was gated on a guard that exits non-zero unless it sees the prompt and no desktop markers. The core-syntax audit required by the change to this log was performed: `.ai/core.md` was re-read and confirmed unchanged, `.ai/core-syntax.md` was re-read, the complete `.ai/` diff was inspected and is this entry alone, and `tools/check_core_log.py` reports every entry conforming.

#### Next Steps:

`tools/tinytang_run.py` sends commands with no shell guard while `tools/tinytang_put.py` has one, and that gap is what let this cycle's stray commands reach the desktop; it should get the same `require_shell()` check, failing closed. If TinyDesk ever stays over a game again after a cartridge load, the overlay race was not the cause and the next step is a listen-only capture of the console during a run from the desktop.

#### Files Modified:

- CMakeLists.txt
- README.md
- cmake/tinytang_build_id.cmake
- ports/bl616/tang_osd.c
- ports/bl616/tang_osd.h
- ports/bl616/tang_osd_desk.c
- ports/bl616/tdsh_platform_bl616.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 19 COMMIT Unreleased 2026-10-05T01:01:51-07:00

#### Coming From:

Unreleased d0801a9

#### Purpose:

Bring up the Tang-Phosphor core on TinyTang as far as its transport and a first WAV played to the end, porting Tang-Control's host side rather than rewriting it.

#### Outcome:

TinyTang now speaks Tang-Phosphor's extended protocol and plays a WAV through it. The host side is ported from Tang-Control into `ports/bl616/phosphor/` as C++, at the user's direction: `fpga_ext_frame.h` and `flac_stream_prefix.h` are copied unchanged with their host tests (`tools/tests/test_phosphor_frames.sh`, both passing), and `fpga_debug`, `fpga_stream` and `fpga_file_stream` keep Tang-Control's frames, sequence and CRC checks and session logic while their plumbing is replaced -- each transaction sends with `tang_fpga_frame`, collects its reply with `tang_fpga_wait`, and holds `tang_fpga_lock` across both as `EXTCTL-005` requires, in place of Tang-Control's receive task and semaphores. `tang_fpga_uart.c` gained `tang_fpga_set_baud()` for the 5 Mbaud switch a file stream negotiates, and a `phosphor` command offers `caps`, `peek`, `poke`, `play` and `stats`; `play` refuses while the desk layer is on, since Phosphor has none and its cell frames would only take bandwidth. The core on the card is Tang-Phosphor entry 68's merged `place3` image as `/cores/console138k/phosphortang.bin`, 4987082 bytes, CRC-32 `11d738b5`, chosen by the user over rebuilding entry 66's proven image, and the test track is `/music/test-tones.wav` from the new deterministic `tools/make_test_wav.py`, 705644 bytes, CRC-32 `021ea0ca`, four rising notes in both channels and then the right only. On hardware Phosphor answered `fpga` as core 80, `caps` as `0x1f` with every capability, `peek 0` as `TPH0`, and its core capabilities register as `0xff`. The first play delivered the file intact -- the core's own counters showed one session, 705644 bytes and CRC `021ea0ca`, and all 176411 samples played -- but with 96897 samples of underrun, and the user heard only the core's boot tone. Measuring rather than guessing found the cause on this side: of 6213 ms, 5364 went to writing frames to the UART, because `bflb_uart_putchar()` reads the millisecond timer before every byte and capped the link near 133 KB/s at any baud, against the 176 KB/s CD-quality audio needs; it is recorded as `BL6-007`. `uart_write` now fills the TX FIFO from its free count and consults the clock only while the FIFO is full, after which the same track streamed in 3967 ms with sends at the 5 Mbaud wire rate, the rest of the time the core's own back-pressure, and zero underruns; the user heard the notes as intended. That change carries every frame on the link, so the board was returned to its boot state and `boot-cart.tdsh` run, and the user confirms Castlevania plays, F12 switches, and the desktop draws and points normally. The tone the user heard throughout the first play stopped once a stream had been seen, which is the core's documented audio policy and showed the first stream had reached the player. The build identity also failed this cycle in exactly the case it exists for: the new files were untracked, `git diff HEAD` ignored them, and two different builds reported the same identity; `cmake/tinytang_build_id.cmake` now hashes untracked, non-ignored files' names and contents too, and the identity changed as expected. The deployed firmware is `d0801a9-dirty.fbcfb61`, 326896 bytes, MD5 `627805364a4ff31bef1a1cffc95badc4`, confirmed by `platform` after the power cycle; it is this cycle's tree before the README, `THIRD_PARTY.md` and reference edits that followed the test. No change was made in the Tang-Phosphor repository. The core-syntax audit required by the change to these files was performed: `.ai/core.md` was re-read and confirmed unchanged, `.ai/core-syntax.md` was re-read, the complete `.ai/` diff was inspected and adds `BL6-007` with its routing row and index line and this entry, with no deletions, and `tools/check_core_log.py` reports every entry conforming.

#### Next Steps:

The user is to decide how far the Phosphor effort goes past this first step. Step 2 is Rockbox single-file playback through the AE350, which needs `ae350_play` ported and `/ae350/resident.tpi` on the card; step 3 is playlists and pad controls; step 4 is metadata and cover art; step 5 is launching from TinyDesk. Two Phosphor-side facts are carried for whoever takes those on: its keyboard receiver runs at 750 kbaud while the Keychron link settled at 281250, so the keyboard does not work on Phosphor, and the core has no desktop layer, so F12 and TinyDesk are absent while it runs.

#### Files Modified:

- CMakeLists.txt
- README.md
- THIRD_PARTY.md
- cmake/tinytang_build_id.cmake
- ports/bl616/phosphor/flac_stream_prefix.h
- ports/bl616/phosphor/fpga_debug.cpp
- ports/bl616/phosphor/fpga_debug.h
- ports/bl616/phosphor/fpga_ext_frame.h
- ports/bl616/phosphor/fpga_file_stream.cpp
- ports/bl616/phosphor/fpga_file_stream.h
- ports/bl616/phosphor/fpga_stream.cpp
- ports/bl616/phosphor/fpga_stream.h
- ports/bl616/phosphor/phosphor_cmd.cpp
- ports/bl616/tang_fpga_link.h
- ports/bl616/tang_fpga_uart.c
- ports/bl616/tdsh_platform_bl616.c
- tools/make_test_wav.py
- tools/tests/flac_stream_prefix_test.cpp
- tools/tests/fpga_ext_frame_test.cpp
- tools/tests/test_phosphor_frames.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 20 COMMIT Unreleased 2026-10-05T02:36:08-07:00

#### Coming From:

Unreleased dcb282e

#### Purpose:

Play every format Tang-Phosphor supports through its resident AE350 Rockbox player, and requalify all twelve against the figures that project recorded.

#### Outcome:

All twelve formats play from TinyTang with every sample count equal to Tang-Phosphor entry 43's: `441000` for WAV, MP3, Ogg Vorbis, AAC, ALAC, WavPack and TTA, `444240` for FLAC (that entry's own unexplained divergence), `440735` MP2, `442368` AC-3 and WMA, and `479688` Opus at 48 kHz, each with zero underruns and its own output rate, and the rate switching both ways between 44.1 and 48 kHz; the user confirmed the pitch by ear. Tang-Control's `ae350_play` is ported into `ports/bl616/phosphor/`, and `phosphor play <file>` now restarts the AE350's loader, sends `/ae350/resident.tpi`, streams the file and waits for the track, with `nowait` returning once the file is sent. Two faults in that path were found and fixed here. A restart through `0x43f0` takes effect about a millisecond after the write, so polling for WAIT straight away saw the state left by the previous track and sent the player into a CPU about to be reset, which failed back-to-back plays; the port waits 20 ms first, its one deliberate change from Tang-Control's sequence. And the player register still read complete from the previous track, so completion is now taken only after the loader's run count, zeroed by the restart, has moved. This cycle also corrects entry 19: the path it called the FPGA's own decoder is not one. The merged core's FPGA player is `pcm_sink`, a raw-PCM sink taking its rate from the AE350, so step 1's WAV played correctly only because it is PCM after a 44-byte header that played as 11 samples -- 441011, not 441000 -- and played at 48 kHz once Opus had set that rate, which the user heard; `phosphor stream` was removed with it, as was the timing instrumentation that only it used, and this is recorded as `PHOS-006`. The player itself hung on hardware before its first decode. The image Tang-Phosphor's current tree builds (863748 bytes, CRC `3d762d13`, reproduced byte for byte) receives the file and stops, with the AE350 jumping to address 0; padding its empty input by 0 or 32 bytes hangs every time and by 16 or 48 bytes plays every time with the code unchanged, and instrumenting it to find the faulting step moved the code and hid the hang, so the cause is not found. Polling was ruled out by a run that touched the link only at the end. At the user's direction the workaround is in Tang-Phosphor's `software/rbhost/Makefile`, which now reserves the 16 bytes, recorded in its entry 69 and here as `PHOS-007`; the player it builds (863764 bytes, CRC `ef1502ed`) has the qualified layout and is the one the sweep passed with. The user believes the hang was a one-off; the evidence is that it is deterministic for a given layout, and the record says so. The corpus comes from the new `tools/make_codec_corpus.sh`, which regenerates entry 43's ten encoded files from the same tone with ffmpeg's bitexact flags placed on the output -- on the input they left Ogg's stream serial random -- and copies the surviving `test.wma` and `test.opus`; and `tools/phosphor_format_sweep.py` plays it behind the console guard and checks each file. The firmware is `dcb282e-dirty.5f90aba`, 327440 bytes, MD5 `b46c933c0fd14103dd1eaf03682aaaf5`, confirmed by `platform`; on the card are the player at `/ae350/resident.tpi` and the twelve files under `/music`. The core-syntax audit required by the change to these files was performed: `.ai/core.md` was re-read and confirmed unchanged, `.ai/core-syntax.md` was re-read, the complete `.ai/` diff was inspected and adds `PHOS-006` and `PHOS-007` with their routing rows and index lines and this entry, with no deletions, and `tools/check_core_log.py` reports every entry conforming.

#### Next Steps:

The user is to decide on the remaining Phosphor steps, and step 3 has changed shape: playlists on the merged core must go through the AE350, since its FPGA player cannot decode, and the gapless handover of `PHOS-004` belongs to the deployment core. The player hang's cause is open in Tang-Phosphor, whose entry 69 names the first step towards it. `tools/tinytang_run.py` still lacks the console guard its siblings have.

#### Files Modified:

- README.md
- THIRD_PARTY.md
- ports/bl616/phosphor/ae350_play.cpp
- ports/bl616/phosphor/ae350_play.h
- ports/bl616/phosphor/fpga_file_stream.cpp
- ports/bl616/phosphor/fpga_file_stream.h
- ports/bl616/phosphor/fpga_stream.cpp
- ports/bl616/phosphor/fpga_stream.h
- ports/bl616/phosphor/phosphor_cmd.cpp
- tools/make_codec_corpus.sh
- tools/phosphor_format_sweep.py

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 21 COMMIT Unreleased 2026-10-05T10:08:53-07:00

#### Coming From:

Unreleased c89745b

#### Purpose:

Move TinyDesk and TinyDesk Shell to their 0.1.4 release, and fix what the update and its testing exposed.

#### Outcome:

Both submodules are at v0.1.4 -- TinyDesk `f4c1d29`, which pins the shell at `3b7d7f8`, and TinyDesk Shell `3b7d7f8` -- and the port built on the first attempt with no change to `td_hal_t`, the backend interface or the source lists; upstream reformatted every source with clang-format 16, so the real changes were found by formatting the old sources with the release's own `.clang-format` before diffing, which showed `terminal.c`, `vterm.c` and `td.c` unchanged and `wm.c` moved to character-safe text copying. The user confirmed the console, the desktop, the Terminal, Files, the Editor keeping its file name on save, Tab completion, F12 and Castlevania. Three fixes came out of the testing. The Terminal window wrapped typing as one full row then four characters: shell 0.1.4 lets a bridge give the line editor its width through a new optional `columns()` callback, which upstream's bridges fill from the Terminal's `start()` and `resize()`, and ours did not, so the editor assumed 80 columns in a 76-column window; `ports/bl616/td_bridge_bl616.c` now forwards the width and implements `resize()`, and `tdsh_platform_bl616.c` returns it while the shell runs in the window and 0 at the console, where the editor keeps 80. Command output run from the Terminal window -- echo in a script, `tangload`, `nesload`, the JTAG programmer, even `ls` -- went to the console writer, whose mirror feed drew it raw onto the layer at the desktop's cursor, through the window frame and in its last colours, where the desktop's diff never saw it; loading a core repaints the layer from that mirror, which is why the user saw it after starting Castlevania, as a fault present since the Terminal existed. Output now goes through `tdsh_bl616_output()`: at the console as before, and in the window to the window and the USB console but not to the mirror, which the user required, since they will not give up USB output; a listen-only capture confirmed `ls /music` run in the window still reached USB. And `tangflash` and `tools/tinytang_flash.py` said the board would reset, when it needs a power cycle (`FLS-001`); both now say to power-cycle, keeping the `OK committing` text the tool waits for, and the new message was seen on the next flash. `tools/tests/test_osd_desk.sh` had failed to build since cycle 2's keyboard repeat and nobody had run it: its stubs lacked the tick count, the keyboard link and cycle 18's locked overlay calls, and two expectations were older than deliberate changes -- the pointer, which since cycle 15 exists only while the desktop runs, and a re-arm forcing the overlay on, which cycle 14 removed because it covered a running game; with those corrected and three checks added for the current behaviour it passes 50 checks, and every host test passes. One fault is recorded unexplained: on the first boot of the first 0.1.4 build, opening the Terminal froze the screen and keyboard at once, and it did not happen again after a power cycle; nothing in the changed code explained it, and if it recurs, whether F12 still works will say whether the whole BL616 stopped. The reference now has `TDSH-003` and `TDESK-012` for the 0.1.4 pair, superseding `TDSH-001` and `TDESK-001`, and `THIRD_PARTY.md` and the build's comment carry the new pins. The deployed firmware is `c89745b-dirty.0c42a85`, 332528 bytes, confirmed by `platform`; what changed after it is comments, documents and the reference only. The core-syntax audit required by the change to these files was performed: `.ai/core.md` was re-read and confirmed unchanged, `.ai/core-syntax.md` was re-read, the complete `.ai/` diff was inspected and its only deletions are the two superseded records' status lines, routing pointers and index lines, with their statements untouched, and `tools/check_core_log.py` reports every entry conforming.

#### Next Steps:

The Phosphor work continues with the plan agreed before this update: the desktop layer, keyboard link and F12 in Tang-Phosphor's core, so it behaves like the NES core; a background playback task in place of the blocking `phosphor play`; and a Phosphor app in TinyDesk with a file list, now-playing, progress and transport buttons. `tools/tinytang_run.py` still lacks the console guard, and no single command runs all the host tests, which is how the broken one went unnoticed.

#### Files Modified:

- CMakeLists.txt
- THIRD_PARTY.md
- ports/bl616/td_bridge_bl616.c
- ports/bl616/tdsh_bl616.h
- ports/bl616/tdsh_console_stdio_bl616.c
- ports/bl616/tdsh_fs_bl616.c
- ports/bl616/tdsh_platform_bl616.c
- ports/bl616/tdsh_tang_flash.c
- ports/bl616/usb_cdc_bl616.c
- third_party/tinydesk
- third_party/tinydesk-shell
- tools/tests/stubs/FreeRTOS.h
- tools/tests/tb_osd_desk.c
- tools/tests/test_osd_desk.sh
- tools/tinytang_flash.py

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 22 COMMIT Unreleased 2026-10-05T10:42:54-07:00

#### Coming From:

Unreleased 18b9bef

#### Purpose:

Stop the USB console from stalling TinyDesk on HDMI when a computer is attached, and keep the desktop's drawing off USB now that HDMI is the primary display.

#### Outcome:

TinyDesk on HDMI is now smooth with the USB cable attached and nothing reading the port, which the user had not been able to do before and had not logged: the console flooded and the desktop went laggy and unresponsive. The cause, recorded as `USB-007`, is that a bulk IN packet leaves only when the host reads it, and a Linux host reads a CDC port only while a program has it open; `console_flush()` waited up to a million yields per 512-byte packet when nobody was reading, every writer waited with it, and the desktop draws through the console, so the HDMI desktop crawled. It also cleared its own busy flag after the wait and started the next packet over one the hardware still held. Now nothing is sent unless DTR is up, a packet waits at most 50 ms, and a packet the host has not taken is left pending with further output dropped until the host reads it, its completion clearing the way. At the user's choice of the second option offered, the desktop's drawing goes only to the layer's mirror while the layer shows it, so USB carries the shell and not the window redraws; with the layer off the desktop still draws to the console as before. Because the user would not give up USB output, the line editor's writes in the Terminal window -- the prompt and the command line -- are copied to USB as command output already was in entry 21, so USB holds a whole transcript. A listen-only capture while the user went into the desktop, dragged windows, ran `ls /music` in the Terminal and exited held 556 bytes: the desktop's start line, the Terminal shell's banner, the prompt, the command, its listing, `desktop: exited` and the console prompt, with no cursor positioning at all; the console answered after the exit, and the user also launched NES from the desktop with the cable attached and found it smooth. This most likely explains entry 21's Terminal freeze too: opening the Terminal is one of the largest bursts the desktop writes, and no program had the port open at the time. One consequence matters for the tools. The desktop is now invisible on USB, so the console guard can no longer see its markers; it still failed closed once in this cycle, refusing because no prompt came back, but its probe is a carriage return, which reaches the desktop as Enter if the desktop is running. The firmware build is `18b9bef-dirty.3162998`, 332576 bytes, confirmed by `platform`, and every host test passes. The core-syntax audit required by the change to these files was performed: `.ai/core.md` was re-read and confirmed unchanged, `.ai/core-syntax.md` was re-read, the complete `.ai/` diff was inspected and adds `USB-007` with its routing row and index line and this entry, with no deletions, and `tools/check_core_log.py` reports every entry conforming.

#### Next Steps:

The console guard needs to be sound again without the desktop's drawing: it should listen first and go by the last of the `desktop: starting TinyDesk` and `desktop: exited` lines, which still reach USB, before sending anything, and the same applies to `tools/tinytang_put.py` and `tools/tinytang_run.py`, the latter still having no guard at all. The Phosphor sequence then continues as planned: the desktop layer, keyboard link and F12 in Tang-Phosphor's core, a background playback task, and the Phosphor app.

#### Files Modified:

- ports/bl616/td_desktop_bl616.c
- ports/bl616/tdsh_platform_bl616.c
- ports/bl616/usb_cdc_bl616.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 23 COMMIT Unreleased 2026-10-05T12:12:51-07:00

#### Coming From:

Unreleased e912c5d

#### Purpose:

Make the console guard sound again now that the desktop no longer draws to USB, by having the host tools ask the board what holds the console before they send anything.

#### Outcome:

The host tools now ask the board before sending. The firmware takes the probe `ESC [ ? 7 7 n` out of the USB input in the receive interrupt, so neither the shell nor the desktop sees it, and the next task to read the console replies `ESC [ ? 7 7 ; <state> n`: 1 at the shell prompt, set around `bl616_readline`; 2 while the desktop runs, set around `desktop_run()`; and 3 for a command that reads the console, while a command that reads nothing answers only when it ends. `tools/tinytang_console.py` holds the one `require_shell()`, which sends nothing unless the answer is 1 and treats no answer as not ready; `tinytang_put.py`, `tinytang_flash.py` and `phosphor_format_sweep.py` use it in place of the old carriage-return check, and `tinytang_run.py`, which had no guard, now asks before every line, so a second command waits for the first to end while the first one's output is still shown. The probe is matched only within one USB packet, so a lone Esc typed at a terminal is never held back, and `tangput` turns the matching off while it receives, because a file may contain the sequence. Entry 22 proposed following the desktop's start and exit lines instead; asking was chosen because it types nothing and does not depend on having seen the session begin, and it closes a hole the old guard had: with the Terminal window open, the shell in it prints a real prompt to USB, so the old check could pass with the desktop up. The board's firmware did not know the probe, so this one flash went out under the old check, with the user confirming the console was at a plain prompt. On the deployed firmware, `e912c5d-dirty.3417d90`, 333056 bytes, confirmed by `platform`, the probe answered 1 at the prompt; during `sleep 5` it gave no answer within 1 s and answered 1 after 4.8 s, when the prompt returned; `tinytang_run.py` held `echo` until `sleep 3` ended; a 6120-byte file holding twenty copies of the probe arrived whole; and with the desktop and its Terminal window up the probe answered 2, `tinytang_run.py` and `tinytang_put.py` both refused with exit status 1, and the user saw nothing typed on screen. State 3 was not seen, since no command that reads the console was running during a probe. The reference adds `TOOL-011` and supersedes `TOOL-009`, and all seven host test scripts pass. The core-syntax audit required by the change to these files was performed: `.ai/core.md` was re-read and confirmed unchanged, `.ai/core-syntax.md` was re-read, the complete `.ai/` diff was inspected and its only deletions are `TOOL-009`'s status line, routing pointer and index line, with its statement untouched, and `tools/check_core_log.py` reports every entry conforming.

#### Next Steps:

The Phosphor sequence continues as planned: the desktop layer, keyboard link and F12 in Tang-Phosphor's core so it behaves like the NES core, a background playback task in place of the blocking `phosphor play`, and a Phosphor app in TinyDesk. No single command yet runs all the host test scripts.

#### Files Modified:

- ports/bl616/td_desktop_bl616.c
- ports/bl616/tdsh_bl616.h
- ports/bl616/tdsh_platform_bl616.c
- ports/bl616/tdsh_tang_flash.c
- ports/bl616/usb_cdc_bl616.c
- tools/phosphor_format_sweep.py
- tools/tinytang_console.py
- tools/tinytang_flash.py
- tools/tinytang_put.py
- tools/tinytang_run.py

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 24 COMMIT Unreleased 2026-10-05T12:51:07-07:00

#### Coming From:

Unreleased 6c01152

#### Purpose:

Add a one-step Phosphor test launcher, move the desktop pointer's hotkey from right-alt to left-alt, and record how the board's one-wire and two-wire modes are reached and what each can debug.

#### Outcome:

`scripts/phosphor.tdsh`, placed on the card as `/scripts/phosphor.tdsh`, is the Phosphor counterpart of `castlevania.tdsh`: it checks for the core, `/ae350/resident.tpi` and the track, turns the desktop layer off because `phosphor play` refuses while it is on and the Phosphor core cannot show it, loads `phosphortang.bin`, probes the link, plays `/music/test.mp3` or the session's `FILE` to the end, and then reloads the menu core, clears its text page and turns the layer back on, so the board ends where `boot.tdsh` leaves it; the README lists it. The user ran it and heard the track over the Phosphor core's video; the F12 toggling they saw afterwards between TinyDesk and a grey screen was the restored menu core, since the ten-second track had ended, and was expected. Pointer mode is decided in the core, not the firmware: patch 0006 now tests `link_mods[2]`, left-alt, instead of bit 6, with its comment rewritten line for line so no hunk count moved; the firmware's key translator ignores both alts and the desktop uses only Alt+Tab and Alt+F4, neither of which the pointer withholds. A fresh nestang clone with the series and the menu patch applied reproduced the checkout exactly. `tools/build_nestang_core.sh` built `nestang-desk.bin` at 4603392 bytes, MD5 `ab96565b269f6a1b352c3eacbc88d2eb`, and `tools/build_menu_core.sh` built `nestang-menu.bin` at 4524032 bytes, MD5 `bf01d35064fedef43498c9a649f27702`, both with zero setup and hold TNS on `sys_clk`; both went onto the card through `tinytang_put.py` with the previous images kept beside them as `.bak`, and after a power cycle the user found left-alt moving the pointer, right-alt doing nothing, and everything else passing. The BL616 firmware is unchanged at `e912c5d-dirty.3417d90`, whose source is commit `6c01152`; only a comment in `tang_osd_desk.h` changed here. The user also exercised the wiring modes, recorded as `BRD-006` and `TOOL-012`: removing the power cable from a running board changed nothing, a cold start on the OTG cable alone came up as SIPEED's FT2232 debugger with the CDC gone and the TangCore splash on HDMI, and a cold start with both cables brought TinyTang back. In one-wire, `openFPGALoader -c ft2232 --detect` read IDCODE `0x0001081b` without programming anything, and the core-ID frame `AA 00 01 01` sent on `/dev/ttyUSB1` at 2 Mbaud was answered `AA 00 02 01 00`, core 0, the FPGA's own core. One-wire reaches the FPGA only, and reaching it restarts the FPGA, so it cannot inspect anything TinyTang or TinyDesk set up. The core-syntax audit required by the change to these files was performed: `.ai/core.md` was re-read and confirmed unchanged, `.ai/core-syntax.md` was re-read, the complete `.ai/` diff was inspected and adds `BRD-006` and `TOOL-012` with their routing rows and index lines and this entry, with no deletions, and `tools/check_core_log.py` reports every entry conforming.

#### Next Steps:

The Phosphor sequence continues as planned: the desktop layer, keyboard link and F12 in Tang-Phosphor's core so it behaves like the NES core, then a background playback task in place of the blocking `phosphor play`, then the Phosphor app in TinyDesk. The `.bak` cores on the card can be removed once the user is satisfied with the new ones. Which signal makes the vendor loader choose one-wire at power-up is untested.

#### Files Modified:

- README.md
- ports/bl616/tang_osd_desk.h
- scripts/phosphor.tdsh
- third_party/patches/0006-pointer-mode.patch

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 25 COMMIT Unreleased 2026-10-05T13:01:19-07:00

#### Coming From:

Unreleased 4f59cee

#### Purpose:

Prove that a core can be deployed, debugged and exercised entirely over one-wire with no TinyTang involved, record the method, and leave a handoff of the project's state for the next agent.

#### Outcome:

With the board cold-started on the OTG cable alone, so the BL616 ran SIPEED's FT2232 debugger instead of TinyTang (`BRD-006`), the whole core loop ran from the host, recorded as `TOOL-013`. Tang-Phosphor's `scripts/flash-otg.sh`, run with `bash` because it has no execute bit, SRAM-loaded the merged place3 image `build/merged/place3/tang_phosphor_merged.fs`, the `.fs` twin of the card's `phosphortang.bin`, in 17 s; the core answered the core-ID frame as `0x50`; its registers read back over `tools/fpga_uart.py` on `/dev/ttyUSB1` at 2 Mbaud; the scratch register `0x0020` took two pokes, read both back and was restored; and `scripts/play_stream.py` streamed the resident AE350 player in 10.1 s and a 402304-byte `test.mp3` in 4.0 s, after which the core reported 441000 samples, 0 underruns and 44100 Hz, equal to Tang-Phosphor entry 43's figure. The player found in Tang-Phosphor's ignored `build/rbhost/bench/` was the 863748-byte image that hangs, so it was rebuilt from the fixed Makefile with `~/.cache/tangcore-dev/toolchain/bin` on PATH, giving entry 69's qualified 863764-byte image, CRC-32 `ef1502ed`, byte for byte, recorded as `PHOS-008`; the corpus came from `tools/make_codec_corpus.sh` in scratch, its `test.flac` again 131601 bytes. The user's account that one-wire and two-wire cannot be combined because the board's modules are powered from one input or the other is recorded as `BRD-007`, marked inferred because no written record of the earlier attempts was found in Tang-Control, Tang-Phosphor or Tang-PSX. Nothing in this repository or in Tang-Phosphor's tracked files changed, and the user did not report whether the tone was heard, so the user test is recorded as not run although every figure was read back from the core. For the agent taking over, the live state is this: the BL616 firmware is `e912c5d-dirty.3417d90`, built from the source of `6c01152`; the card holds the left-alt menu and NES cores of entry 24 with the previous images beside them as `.bak`, `/scripts/phosphor.tdsh`, `/ae350/resident.tpi` and the twelve-file corpus in `/music`; and the board was last left in one-wire with the Phosphor core in SRAM, so a cold power-up with the power input is needed before any TinyTang tool will find `/dev/ttyACM0`. Every host tool now refuses to send unless the firmware's status probe answers 1 (`TOOL-011`), and that rule is the one to keep: a byte sent while the desktop is up is a keystroke. The core-syntax audit required by the change to these files was performed: `.ai/core.md` and `.ai/core-syntax.md` were confirmed unchanged since they were re-read in this session, the complete `.ai/` diff was inspected and adds `BRD-007`, `TOOL-013` and `PHOS-008` with their routing rows and index lines and this entry, with no deletions, and `tools/check_core_log.py` reports every entry conforming.

#### Next Steps:

Return the board to two-wire with a cold power-up on the power input and confirm the status probe answers 1 before sending anything. The agreed work then continues with Phosphor following the NES core's model: add the desktop layer, the keyboard link and F12 to Tang-Phosphor's core, logging that work in Tang-Phosphor's own `core-log.md` and not reading its `core.md`, and settle the keyboard link's baud, which differs between the two projects (750k against 281250); one-wire is now the fastest way to iterate on that core by itself. After it come a background playback task on the BL616 in place of the blocking `phosphor play`, and a Phosphor app in TinyDesk with a file list of `/music`, now-playing, a progress bar, transport buttons and a status line; playlists and gapless playback are deferred. Smaller open items are the 5 Mbaud switch over one-wire, the resident player's unexplained layout hang (`PHOS-007`), what signal selects one-wire at power-up, a single command to run all the host test scripts, and removing the `.bak` cores once the user is satisfied.

#### Files Modified:

None.

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 26 COMMIT Unreleased 2026-10-05T13:46:03-07:00

#### Coming From:

Unreleased aafca8e

#### Purpose:

Run the Phosphor core under TinyDesk the way the NES core runs, with the desktop layer kept up, F12 switching between the player and the desktop, the keyboard reaching TinyDesk and left-alt driving the pointer, and settle the keyboard link's baud at 281250 for both cores.

#### Outcome:

The resumed session first confirmed the board back in two-wire after entry 25's one-wire work: the status probe answered 1 and `platform` reported `tinytang e912c5d-dirty.3417d90` with TinyDesk Shell 0.1.4. The core side is Tang-Phosphor's entry 70, which ports this project's desktop layer (`textdisp_wide.sv` and commands 0x13-0x15), the 0x08 keyboard report and left-alt pointer mode into the merged core, moves its keyboard receiver from 750k to 281250 baud, and built a place3 image, MD5 `28536b620a1c0b324ea43c5cf08036a4`, that meets timing. On this side the firmware needed no protocol change, because `fpga_frames.c` queues frames by type and already caches 0x08 reports from any core; `phosphor_cmd.cpp` no longer refuses to play while the desktop layer is on, since the core now draws it and the layer's cell frames take the link lock one frame at a time between stream frames. `scripts/phosphor.tdsh` was rewritten after `castlevania.tdsh`: it loads `phosphortang.bin` with the desktop left up, probes the link, runs `osd off` to hand the screen to the player, and starts the track with `phosphor play ... nowait`, and it no longer reloads `nestang-menu.bin` afterwards, which is what had put the menu core on screen when F12 was pressed after a track. The README describes the new behaviour, and patch 0004's `keylink_rx.sv` comment, which still claimed 750 kbaud although both nestang instantiations pass 281250, was corrected line for line in the patch and in the nestang checkout, and `scripts/apply-nestang-patches.sh` still recognises the series as applied. The firmware was built with `make CHIP=bl616 BOARD=bl616dk` after `git submodule update --init` populated this checkout's TinyDesk submodules at their recorded commits, giving `tinytang_bl616.bin` at 332960 bytes, MD5 `712d95d4642563cb1083ab1fc856426e`, build identity `aafca8e-dirty.a7d2d8d`, installed with `tools/tinytang_flash.py`; the Phosphor image went to `/cores/console138k/phosphortang.bin` with the previous one kept as `phosphortang.bin.bak`, and the script to `/scripts/phosphor.tdsh`, all through the guarded tools. After a power cycle the user reported everything working: the new build identity, the track playing on the Phosphor screen, F12 switching to TinyDesk and back during the track and after it ended with the menu core never appearing, typing in TinyDesk, and left-alt moving the pointer. A later `platform` from the host was correctly refused by the guard because the desktop was running. `phosphor play` without `nowait` still blocks the shell for the length of the track. The core-syntax audit required by the change to these files was performed: `.ai/core.md` was re-read and `git diff` confirms it unchanged, `.ai/core-syntax.md` was re-read, the complete `.ai/` diff was inspected and adds only this entry, and `tools/check_core_log.py` reports every entry conforming in this log and in Tang-Phosphor's.

#### Next Steps:

The next step of the agreed sequence is a background playback task on the BL616 in place of the blocking `phosphor play`, so a track can be started, watched and stopped from TinyDesk, followed by the Phosphor app in TinyDesk with a file list of `/music`, now-playing, a progress bar, transport buttons and a status line. The `.bak` cores on the card can be removed once the user is satisfied with the new ones. The open items from entry 25 stand: the 5 Mbaud switch over one-wire, the resident player's layout hang (`PHOS-007`), what selects one-wire at power-up, and a single command to run all the host test scripts.

#### Files Modified:

- README.md
- ports/bl616/phosphor/phosphor_cmd.cpp
- scripts/phosphor.tdsh
- third_party/patches/0004-keyboard-link.patch

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 27 COMMIT Unreleased 2026-10-05T15:35:51-07:00

#### Coming From:

Unreleased 8071e09

#### Purpose:

Replace the blocking `phosphor play` with a background playback task on the BL616, so a track on the Phosphor core can be started, watched and stopped from the shell or TinyDesk's Terminal while the shell stays free.

#### Outcome:

Before any change the baseline on `aafca8e-dirty.a7d2d8d` passed: `tools/phosphor_format_sweep.py` played all thirteen tracks with zero underruns at the expected rates, and every host test script passed. The new `ports/bl616/phosphor/phosphor_player.cpp` runs a task, `phosplay`, that owns the track: a play request claims the status at once and the task sends the player and file through `ae350_play_file`, which now takes the file stream's cancel callback, and then reads the loader and sink registers every 250 ms, deciding the end of the track with `phosphor_track.h`, a header-only port of the old blocking wait's rules that `tools/tests/phosphor_track_test.cpp` checks on the host through `test_phosphor_frames.sh`. `phosphor play` still waits by default and prints the old completion line unchanged, so the sweep needs no change; `nowait` returns at once; `phosphor status` reports idle, loading, playing, ended, stopped or failed with the file, elapsed time from samples and rate, underruns and the send time; and `phosphor stop`, Ctrl-C during a waited play, a new play and `tangload` (through `tang_phosphor_core_replacing` in `tdsh_tang_flash.c`) all stop the track by pausing the sink and restarting the AE350, where the old Ctrl-C only ended the wait and left the audio playing. Hardware testing found two faults in the sink's behaviour on a stop, recorded as `PHOS-009`: left unpaused, the next track counted its whole load as underruns (168717 on one track), because the stopped sink stays in its playing state and the count clears only with the core; and held paused, the next track never started, because the stopped track's samples stay queued ahead of its START. The task therefore drains that tail at the next play, at most about 58 ms of the old track and audible, then holds the sink paused until the START count at 0x30 moves, and reports underruns as a per-track difference. The user's first test under TinyDesk then found that Ctrl-C did not stop a waited play in the Terminal, because the wait read only the USB console; the shell's input routing is now `tdsh_bl616_input_read_byte` in `tdsh_platform_bl616.c`, used by both the shell's read and the wait. The firmware was built with `make CHIP=bl616 BOARD=bl616dk` without new warnings, giving `tinytang_bl616.bin` at 337984 bytes, MD5 `ef0763d2da9066fe0bfad287a887ed37`, build identity `8071e09-dirty.0dccbf7`, installed with `tools/tinytang_flash.py` and confirmed by `platform` after a power cycle. On it Ctrl-C over USB stopped a waited play, the sweep passed again with zero underruns, and every host test passed. The user then reported every test passing under TinyDesk: the script's track with F12 and typing in the Terminal during it, `phosphor status` and `phosphor stop`, Ctrl-C stopping a waited play in the Terminal, a new track replacing a playing one, and `tangload` during a track. The README describes the new commands. The core-syntax audit required by the change to these files was performed: `.ai/core.md` was re-read and `git diff` confirms it unchanged, `.ai/core-syntax.md` was re-read, the complete `.ai/` diff was inspected and adds only this entry and the `PHOS-009` record with its question row and index line, and `tools/check_core_log.py` reports every entry conforming in this log and in Tang-Phosphor's.

#### Next Steps:

The next step of the agreed sequence is the Phosphor app in TinyDesk, with a file list of `/music`, now-playing, a progress bar, transport buttons and a status line, built on the playback task's status; the core reports no track duration (`PHOS-009`), so the progress bar needs a duration from the BL616 or the player, or shows elapsed time only. Packaging the desktop-layer integration as a kit for other cores can follow it. The `.bak` cores on the card can be removed once the user is satisfied with the new ones, and entry 25's open items stand: the 5 Mbaud switch over one-wire, the resident player's layout hang (`PHOS-007`), what selects one-wire at power-up, and a single command to run all the host test scripts.

#### Files Modified:

- README.md
- ports/bl616/phosphor/ae350_play.cpp
- ports/bl616/phosphor/ae350_play.h
- ports/bl616/phosphor/phosphor_cmd.cpp
- ports/bl616/phosphor/phosphor_player.cpp
- ports/bl616/phosphor/phosphor_player.h
- ports/bl616/phosphor/phosphor_track.h
- ports/bl616/tdsh_bl616.h
- ports/bl616/tdsh_platform_bl616.c
- ports/bl616/tdsh_tang_flash.c
- tools/tests/phosphor_track_test.cpp
- tools/tests/test_phosphor_frames.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 28 COMMIT Unreleased 2026-10-05T16:57:43-07:00

#### Coming From:

Unreleased 4068d91

#### Purpose:

Add a Phosphor player app to TinyDesk on top of the background playback task, with the options the user chose: a progress bar for WAV and FLAC from their headers, Pause, auto-advance with a checkbox, a no-core message with disabled buttons, and FatFS locking so the app, the shell and the player can share the card.

#### Outcome:

The new `ports/bl616/phosphor/td_phosphor_app.cpp` is a resizable "Phosphor" window registered from `td_desktop_bl616.c`: a case-insensitively sorted list of the playable files in `/music`, the title, a status line, a progress bar, Prev, Play, Pause/Resume, Stop and Next, an auto-advance checkbox that plays the folder through only from tracks the window started, and a message line; it reads only `phosphor_player_get` every 500 ms and checks for the core by reading the magic at 0x0000, saying so and dimming its buttons when the Phosphor core is absent. Because the core reports no duration (`PHOS-009`), the duration comes from the file: `phosphor_media.h` walks WAV chunks and reads FLAC's STREAMINFO (after an ID3v2 tag), and both gave exactly 7250 ms on ffmpeg-made WAV with a LIST chunk, 24-bit 96 kHz WAV and FLAC; other formats show elapsed time only. The playback task gained `phosphor_player_pause` and the shell gained `phosphor pause` and `phosphor resume`: the pause register is applied by the task, recorded during the drain hold so the hold's release keeps it, and `phosphor_track.h` reports nothing but a trap, completion or decoder error while paused and moves its timers on by the paused time, so a pause is never a stall. `fatfs_conf_user.h` sets `FF_FS_REENTRANT` to 1. Testing found two defects, both fixed within this cycle: a `cp` on the card during a send failed the stream, because the higher-priority shell task held the CPU for a few hundred ms and the link's wait loops in `tang_fpga_uart.c`, `fpga_debug.cpp` and `fpga_stream.cpp` judged their deadline before looking at the FIFO or the reply ring, and now always look once more first; and `tangload` failed from TinyDesk's Terminal, because the JTAG programmer called `f_read`, which now takes an RTOS mutex, inside a critical section, so `tang_jtag_programmer.c` now reads each block outside it and shifts the block inside (`TOOL-015`), which also stops a failed load from returning with the section still entered. The host tests (`tools/tests/test_phosphor_frames.sh`, now including `phosphor_media_test.cpp` and a pause test in `phosphor_track_test.cpp`) pass. The final build `4068d91-dirty.0bb182d` was flashed over the CDC; across it and the two builds before it, pause held at 0 samples through loading and at 98,799 samples mid-FLAC without a stall and both tracks then completed with 0 underruns, two passes of a 705 KB `cp` and an MP3 `cp` during a WAV send ended at 441,000 samples with 0 underruns, `tools/phosphor_format_sweep.py` passed all thirteen plays (FLAC's 444,240 samples is its qualified figure), and `tangload` of the Phosphor and menu cores succeeded at the console. The user then tested the app on the desktop, including `tangload` from the Terminal, the list, playback, the progress bar, pause, stop, prev and next, auto-advance and reopening the window, and reported that everything passes. `README.md` describes the app, pause and resume, and the card's locking. `TOOL-014` records, at the user's request, the Raspberry Pi Pico 2 CMSIS-DAP JTAG probe on the module's U1201 header as a third debug path beside one-wire and two-wire, from Tang-Phosphor's `scripts/flash-pico.sh` and README and Tang-PSX's record, not yet run by this project. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff (this entry and `TOOL-014` and `TOOL-015` with their routing and index lines), confirmed `.ai/core.md` is unchanged, validated this entry as number 28 of 100 with exactly six sections, and confirmed that no settled history was rewritten.

#### Next Steps:

The fourth step of the agreed sequence is done; the next is packaging the desktop-layer integration as a kit for other cores, as planned after entry 27. The Phosphor core's on-core metadata text slots and artwork registers (0x7c, 0x80, 0x100 onward) are unused and could carry the app's now-playing text to the core's own display. The shell's file-descriptor table in `tdsh_fs_bl616.c` is not thread-safe, which matters only if two tasks open files through the shell layer rather than FatFS directly. The Pico 2 probe (`TOOL-014`) is untried here and is the candidate for JTAG checks while TinyTang runs. The `.bak` cores on the card can be removed, and entry 25's open items stand: the 5 Mbaud switch over one-wire, the `PHOS-007` layout hang, what selects one-wire at power-up, and a single command for all host tests.

#### Files Modified:

- README.md
- fatfs_conf_user.h
- ports/bl616/phosphor/fpga_debug.cpp
- ports/bl616/phosphor/fpga_stream.cpp
- ports/bl616/phosphor/phosphor_cmd.cpp
- ports/bl616/phosphor/phosphor_media.h
- ports/bl616/phosphor/phosphor_player.cpp
- ports/bl616/phosphor/phosphor_player.h
- ports/bl616/phosphor/phosphor_track.h
- ports/bl616/phosphor/td_phosphor_app.cpp
- ports/bl616/tang_fpga_uart.c
- ports/bl616/tang_jtag_programmer.c
- ports/bl616/td_desktop_bl616.c
- tools/tests/phosphor_media_test.cpp
- tools/tests/phosphor_track_test.cpp
- tools/tests/test_phosphor_frames.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 29 COMMIT Unreleased 2026-10-05T20:02:08-07:00

#### Coming From:

Unreleased cb9e7d4

#### Purpose:

Bring up the BL616's own Bluetooth LE radio as the standard input path, prove it with a scan, and connect a Bluetooth LE keyboard far enough to print its key reports, growing the application slot to make room.

#### Outcome:

The user asked for Bluetooth on the BL616 to replace low-speed USB and the custom wired keyboard link. The dock schematics show the BL616's antenna pin running to U35, a U.FL jack with nothing fitted (`BLE-001`). The new `ports/bl616/tang_ble.c` adds `blescan`, which starts the radio on first use with the SDK's all-roles controller library `ble1m10s1bredr0` (`proj.conf`; `BLE-002`) and lists advertisers by signal strength. Bluetooth took the image from 346,160 to 582,256 bytes, over the 0x80000 slot, and cost 52.6 KB of boot heap (`BLE-002`). A read-back of the whole flash showed 0x76000-0x200000 erased and the old limit to be Tang-Control's choice, so the slot became 0xE0000 with staging at 0x120000 in `tdsh_tang_flash.c` and `tools/tinytang_flash.py`, with static assertions against the vendor data at 0x200000 (`FLS-002`, superseding `FLS-001`). It was installed in two steps because the old `tangflash` refuses anything over 0x80000: a non-Bluetooth image with the new limits, then the Bluetooth image, both booting after a power cycle. With no antenna `blescan` found the user's phone. Research on the user's candidate devices found the Rii K06 to be Classic-only, and the SDK has no Classic HID host, so only Bluetooth LE HID over GATT devices can be used (`BLE-007`); the user chose the Logitech MK955 set, and its K950 keyboard was the first target. `blekbd` connects to a given address, pairs with Just Works, discovers the HID service, writes boot protocol, subscribes to every input report, reads back the protocol mode and each CCC, and prints decoded boot reports from a ring the Bluetooth callbacks fill and the shell drains. Hardware runs found three SDK behaviours, each fixed and recorded: filtered GATT discovery returned nothing against the K950, so discovery is unfiltered and filtered in the callback (`BLE-003`); a vendor patch reports a successful CCC write as a NULL notification, which the first version took for an unsubscribe (`BLE-004`); and issuing all the setup requests at once deadlocked the host, so they now run one at a time through a sequencer (`BLE-005`). Connection attempts sometimes failed with HCI 0x3E on the bare jack and succeeded when run again. The final build `cb9e7d4-dirty.aee808e`, 594,400 bytes, MD5 `5ef6cb9460c72c062f3585c54efdbf5c`, was built with `make` without new warnings, installed with `tools/tinytang_flash.py` and confirmed by `platform` after a power cycle. On it `blekbd` paired with the K950 at a random address that changes with every pairing (`BLE-006`), read back boot protocol and all seven CCCs enabled, and decoded 404 boot reports in a 40-second watch while the user typed letters, Space, Enter, punctuation and the right-hand modifiers, with rollover and releases correct; the user confirmed it works. The committed tree differs from that build only in comments in `proj.conf` and in `README.md`, which describes `blescan`, `blekbd`, `tang_ble.c`, the new slot and the antenna as fact 17. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff (this entry, the BLE topic, `FLS-002` with `FLS-001` marked superseded, `BLE-001` through `BLE-007`, and their routing and index lines), confirmed `.ai/core.md` unchanged, validated this entry as number 29 of 100 with `tools/check_core_log.py`, and confirmed no settled history was rewritten.

#### Next Steps:

Feed the Bluetooth keyboard's boot reports into the same path as the wired keyboard link, so the K950 types at the console and drives the desktop's pointer, with arrows, Esc and the function keys checked, since they have not been typed yet. After that: keep pairings across resets with `CONFIG_BT_SETTINGS` and storage for the keys; reconnect after a dropout; find the keyboard by name or HID appearance instead of by address, which changes with every pairing; measure the heap the running stack takes, for which there is no shell command yet; then the M650 mouse and a Bluetooth LE controller such as the Xbox Series, which will need report protocol and report-map parsing. An antenna on U35 would remove the 0x3E connection failures. The open items from entry 28 stand.

#### Files Modified:

- README.md
- ports/bl616/tang_ble.c
- ports/bl616/tdsh_platform_bl616.c
- ports/bl616/tdsh_tang_flash.c
- proj.conf
- tools/tinytang_flash.py

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
