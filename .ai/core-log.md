## 41 COMMIT Unreleased 2026-10-06T09:04:44-07:00

#### Coming From:

Unreleased 5eb6326

#### Purpose:

Hand the project to the next agent with the state of the upstream work, the board, and the decisions made since entry 40.

#### Outcome:

Three threads are open upstream on tinydesk-project/tinydesk-shell, all under the user's account. Issue #1 (peek and poke): the author proposed building it in TinyTang as an optional root-only feature limited to ranges the port lists, and asked which side and which widths; the user's reply, posted at 2026-10-06T16:02Z, answers both sides (the BL616's RAM and registers in safe ranges, and FPGA core registers through a core's own debug protocol, as `phosphor peek` and `poke` already do for Tang-Phosphor), 8, 16 and 32-bit access with misaligned addresses refused, and asks whether the port should use `tangpeek` and `tangpoke`, as STANDARDS.md suggests for generic port commands, or plain `peek` and `poke` to save a later rename. That naming answer is awaited. Issue #2 (script memory, `TDSH-006`): the author replied at 15:12Z approving the pull request. It is to wrap the `tdsh.h` limits in `#ifndef` with the current values as defaults, document that an override must apply to every component built with `tdsh.h` because it changes the session's layout, test with the defaults and with smaller limits, and leave the per-script variable copy unchanged. The author also asks, as a separate change, that script loading report an error when it runs out of memory instead of returning silently. Pull request #3, "README: TinyTang's prefix is tang", is open and unreviewed. It changes one row of the Community ports table, recording the prefix `tang` and current notes, from the branch `community-port-tinytang` of the fork `aquasock/tinydesk-shell`, cloned at `/run/media/vash/GIT/tinydesk-shell` with `origin` the fork and `upstream` the project. That clone is where upstream work is done, never the TinyTang submodule. The commit is `e36a084` on `8456dd1`, and the host tests pass 8 of 8 there. STANDARDS.md section 9 asks ports to describe what they did for topics with no rule yet; three such issues were drafted (the boot script, the desktop layer and F12, and media playback) and are saved unposted in `docs/upstream/issue-drafts.md` for the user to approve. Two hardware ideas were discussed and set aside. The first was a Pmod I2S2 for audio in and out on Tang-Phosphor: it needs a real master clock locked to the sample rate, either a new PLL, or a bench generator on the module's MCLK pins with the ADC as I2S master. That is Tang-Phosphor work and was not begun. The second was the Adafruit 2.2-inch PiTFT HAT, which fits the dock's free 40-pin header mechanically; that header is an SDRAM connector with +5 V on pin 11, so the HAT must not be fitted directly (`BRD-008`), and the user does not want an adapter. The dock's LCD connector is the intended display path and is deliberately not set up yet. At the close the board was not on USB; it was last in one-wire mode after the FPGA flash read-back of entry 39, which erased the FPGA's SRAM, so it needs a cold start with both cables to run TinyTang. The firmware on it is `3b025c1-dirty.0b9ad3e` (entry 37). The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff (this entry and `BRD-008` with its routing row and index line, with no deletions), confirmed `.ai/core.md` unchanged, validated this entry as number 41 of 100 with `tools/check_core_log.py`, and confirmed no settled history was rewritten.

#### Next Steps:

The approved issue #2 pull request comes next. Work in the fork's clone on a new branch from upstream `main`: add `#ifndef` around `TDSH_SCRIPT_TASK_STACK`, `TDSH_MAX_VARS`, `TDSH_VAR_NAME_MAX` and `TDSH_VAR_VALUE_MAX`, keeping the defaults. Document in `tdsh.h` and the README that an override must apply to every component built with it. Build and run the host tests with the defaults and with smaller limits such as `TDSH_MAX_VARS=32` and `TDSH_VAR_VALUE_MAX=128`. Show the user the diff and the pull request text before pushing. The out-of-memory error for script loading may be offered as a second pull request. TinyTang then sets the smaller limits from its build, without editing the submodule, once a release carries the change, and checks that scripts started from the desktop run. Also pending: the author's naming answer on issue #1, after which peek and poke become a TinyTang cycle; review of pull request #3; the user's decision on the three drafted issues; a note to nand2mario before TinyTang is listed anywhere visible, sent by the user; the discovery stall when both Bluetooth devices reconnect at once; and Phosphor's load-progress readout and file-size limit. The open items from entry 28 stand.

#### Files Modified:

- docs/upstream/issue-drafts.md

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 42 COMMIT Unreleased 2026-10-06T10:00:49-07:00

#### Coming From:

Unreleased 4176a68

#### Purpose:

Make scripts launched from TinyDesk fit the BL616 heap by carrying the configurable-memory-limit change the upstream author approved and setting smaller limits consistently across TinyTang.

#### Outcome:

Desktop script launches pass. The existing draft was reviewed and finished as `third_party/patches/tdsh/0001-configurable-script-limits.patch` against Shell v0.1.5, applied idempotently during CMake configuration by `scripts/apply-tdsh-patches.sh`; it preserves upstream defaults and each script's copied session and variables, and documents the requirement for consistent definitions in every component including `tdsh.h`. TinyTang sets 32 variables, 32-byte name buffers, 128-byte value buffers and a 16 KB worker stack through the SDK's shared definitions, confirmed in both C and C++ flags. The target ELF's session is 5,752 bytes instead of 19,096, saving 13,344 bytes for the static console session and every script copy, and the board's heap total grew from 124,768 to 138,112 bytes. `tools/tests/test_script_limits.sh` reconstructs the patch byte-for-byte and checks repeat application, both pinned upstream suites, the patch's defaults, smaller tables, boundaries, script isolation and cleanup. Default and 48-variable/128-byte-value suites pass Shell 8/8 and TinyDesk 10/10 without compiler warnings; at 32 variables the seven compatible Shell tests and all ten desktop tests pass. The comprehensive upstream script retains more than 32 variables, and Linux pthread runs with a 16 KB stack crash in the script tests even with default tables, so native suites retain their 32 KB stack and the exact 16 KB request is checked by worker probes and the board's high-water mark. All eight existing TinyTang regression scripts pass; the changed header and probe were formatted with clang-format 16.0.6. The firmware built with `make CHIP=bl616 BOARD=bl616dk`, with existing JTAG-programmer and C-standard-option warnings, at 617,568 bytes, MD5 `a43f16cfd436b569425ca8e3d3abc253`, build identity `4176a68-dirty.92d0758`, and was installed through the guarded two-wire flash tool. After the cold power cycle the user reported every desktop test passing, with nothing they could break, and music playing after Castlevania. The console probe answered 1, `platform` confirmed the build, and `ble` and `crash` showed no allocation refusals since boot, 51,280 bytes free with a 26,200-byte largest block during playback, the last script using 10,728 of its 16,384-byte stack, and no previous crash. `phosphor status` confirmed track 3 playing a 44.1 kHz MP3 with zero underruns; both paired Bluetooth devices had report counts from the test and were waiting when checked. `TDSH-007` supersedes `TDSH-006` without rewriting its failure evidence. The matching upstream change is prepared in the separate fork clone on `configurable-script-limits` from `8456dd1`, with its diff matching the carried patch and its draft description saved in `docs/upstream/script-limits-pr.md`; nothing was published upstream. The committed source matches the deployed build, with the records and PR draft added after deployment. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed `.ai/core.md` unchanged, validated entry 42 of 100 with `tools/check_core_log.py`, checked reference routing and field structure, and confirmed that settled history was changed only by the required supersession metadata and lookup pointers.

#### Next Steps:

Review the prepared upstream diff and pull request text with the user before pushing the fork branch or opening the pull request, then drop the carried patch when a pinned release includes it. The author's separate request for an allocation-error message remains a candidate for a second upstream change. The desktop script memory fault is resolved for the tested limits and paths; no further hardware validation is pending for this cycle. The discovery stall when both Bluetooth devices reconnect at once, Phosphor's load-progress readout and file-size limit, and the other open items from entry 41 stand.

#### Files Modified:

- CMakeLists.txt
- README.md
- THIRD_PARTY.md
- docs/upstream/script-limits-pr.md
- ports/bl616/tdsh_platform_bl616.c
- scripts/apply-tdsh-patches.sh
- third_party/patches/tdsh/0001-configurable-script-limits.patch
- tools/tests/tb_script_limits.c
- tools/tests/test_script_limits.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 43 COMMIT Unreleased 2026-10-06T11:42:55-07:00

#### Coming From:

Unreleased f3097e1

#### Purpose:

Increase the script variable budget to 48 while keeping desktop launches reliable across core changes and desktop restarts, with measured headroom for future ID3 metadata and artwork.

#### Outcome:

The 48-variable build is accepted on hardware after fixing two additional causes of failed launches. With the 32-byte name and 128-byte value buffers and 16 KB worker stack unchanged, the target session is 8,328 bytes and its script job 8,584, saving 10,768 bytes per copied session against upstream defaults; both C and C++ flags agree. The README, carried patch's example and fork draft now use 48, which fits the comprehensive upstream fixture; the earlier 32-variable probes remain. All default and 48-variable host suites passed Shell 8/8 and TinyDesk 10/10, with boundary, isolation and stack-request probes. At 48, switching cores while music played with the Phosphor window open initially refused a 16,384-byte xTaskCreate stack allocation in task tdsh, before tangload could reach its playback-stop hook; this was memory pressure, not an intentional busy-core interlock. Separately, reopening TinyDesk left its Terminal without a shell: upstream retains its started flag across td_shutdown while this port stops the shell at desktop exit. The port now explicitly restarts the bridge on entry and returns a visible error and nonzero status on startup failure. USB echo worked when the physical keyboard was disconnected. The Phosphor window also reserved 24,576 bytes for 256 names regardless of folder size; a two-pass FatFs scan now allocates only eligible entries, capped at 256, preserving filtering and sorting and clearing the list on errors. The tested 19-file folder needs 1,824 bytes, recovering 22,752. New regressions against the real app and bridge fail with the original implementations and pass catalog sizing, limits, directory changes, errors and cleanup, plus Terminal reentry, input, shutdown and failed-start retry; the Phosphor transport/media, keyboard and desktop-layer checks also pass. Firmware built with make CHIP=bl616 BOARD=bl616dk, retaining the existing C-standard-option warning for C++, at 618,288 bytes, MD5 ff9e0b950bf1d14feba3f7f775375a90, build identity f3097e1-dirty.a912290, and was installed through the guarded two-wire tool. After the cold start the user reported repeated swaps between both cores working and accepted the result. Platform confirmed the build, no allocations were refused or crash recorded, and the last ordinary and diagnostic script used 10,728/16,384 stack bytes. At the console, free heap increased from 52,292 to 75,044 bytes, exactly the catalog saving, with a 62,072-byte largest block and music playing at 44.1 kHz with zero underruns. The generated Castlevania heap probe then passed from the desktop with music playing, the Phosphor window open and both Bluetooth devices ready: at all three active-worker checkpoints it reported 24,936 bytes free of 135,536 and a 19,372-byte largest block, without refusals; the user confirmed completion. This supports a bounded 16 KB future metadata/artwork budget with 8,552 free bytes beyond it for this configuration, not an allocated reserve or an implemented decoder; docs/phosphor-memory-budget.md records that boundary and reproduction through tools/make_script_heap_probe.py. Final console diagnostics still showed no refusals or crash and the player stopped by the core switch with zero underruns. TDSH-008 supersedes TDSH-007 without changing its settled statement, and TDESK-014 records upstream's retained Terminal state. The matching upstream fork diff remains uncommitted and unpublished, byte-for-byte equal to the carried patch. The committed firmware source matches the deployed image; documentation and tests were finalized afterward. The core-syntax audit re-read core.md and core-syntax.md, inspected the entire .ai diff, confirmed core.md and settled log entries unchanged, checked reference records and lookup pointers, and validated entry 43 of 100 with tools/check_core_log.py.

#### Next Steps:

Review the prepared upstream guards and PR text before publishing the fork branch or pull request, then remove the carried patch when a pinned release includes it. The separate script-loading allocation-error request remains pending. Implement future ID3/artwork within the documented bounded 16 KB budget and requalify its actual decoder and buffers; a full 256-file catalog still costs 24 KB, so the measured headroom does not cover every future combination. The simultaneous Bluetooth discovery stall and the other open items from entry 42 stand.

#### Files Modified:

- CMakeLists.txt
- README.md
- THIRD_PARTY.md
- docs/upstream/script-limits-pr.md
- docs/phosphor-memory-budget.md
- ports/bl616/td_bridge_bl616.c
- ports/bl616/td_desktop_bl616.c
- ports/bl616/phosphor/td_phosphor_app.cpp
- third_party/patches/tdsh/0001-configurable-script-limits.patch
- tools/make_script_heap_probe.py
- tools/tests/test_script_limits.sh
- tools/tests/tb_terminal_lifecycle.c
- tools/tests/stubs/terminal_lifecycle.h
- tools/tests/test_terminal_lifecycle.sh
- tools/tests/phosphor_catalog_test.cpp
- tools/tests/stubs/phosphor_catalog/ff.h
- tools/tests/test_phosphor_catalog.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 44 COMMIT Unreleased 2026-10-06T12:03:20-07:00

#### Coming From:

Unreleased 4633b4d

#### Purpose:

Publish the reviewed configurable-script-memory-limits change to TinyDesk Shell upstream and record its review handoff.

#### Outcome:

The user reviewed the two-file diff and shortened description, then explicitly authorized publication. The fork clone at /run/media/vash/GIT/tinydesk-shell was committed as b4f9404, Shell: let ports set script memory limits, on configurable-script-limits from upstream main at 8456dd1, which was still the remote head. It was pushed to aquasock/tinydesk-shell and published as tinydesk-project/tinydesk-shell PR #4, https://github.com/tinydesk-project/tinydesk-shell/pull/4, addressing issue #2. The change adds the four requested guards and README guidance, preserves upstream defaults and per-script variable copies, and documents consistent definitions for every tdsh.h consumer. Its committed diff matches TinyTang's carried patch byte-for-byte, and the changed header passes clang-format 16.0.6. The published title, body, base, head commit and two-file scope were checked through GitHub; the PR is open and no GitHub check results were reported at that check. The description cites the already completed default and 48-variable host suites, boundary and isolation probes, and BL616 hardware acceptance from entry 43; tests were not repeated because the reviewed code is unchanged. TinyTang's Terminal restart and catalog-allocation fixes remain outside the upstream change, and the script-loading allocation-error message remains a separate follow-up. docs/upstream/script-limits-pr.md now records the published URL, commit and exact description. No firmware changed, so this publication and documentation cycle required no build, deployment or board test. The core-syntax audit re-read core.md and core-syntax.md, inspected the complete .ai diff, confirmed core.md and all settled log entries unchanged, and validated entry 44 of 100 with tools/check_core_log.py.

#### Next Steps:

Await upstream review of PR #4 and address any requested changes as a new cycle. Remove the carried patch only when a pinned upstream release includes the guards, preserving TinyTang's consistent overrides. The separate script-loading error, the bounded future ID3/artwork implementation and the other open items from entry 43 remain pending.

#### Files Modified:

- docs/upstream/script-limits-pr.md

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 45 COMMIT Unreleased 2026-10-06T12:44:14-07:00

#### Coming From:

Unreleased f8d6fdf

#### Purpose:

Carry the TinyTang half of Tang-Phosphor entry 71, so playback selects the AE350 at its new register and the Phosphor script declares the PMOD sockets, making the OLED work under the Phosphor core.

#### Outcome:

The user reported the PMOD OLED dark on the Phosphor core, and it had two causes, both recorded in Tang-Phosphor entry 71: nothing in this firmware declared the sockets, which that core powers up released, and `ae350_play.cpp` selected the AE350 by writing `0xc0 = 1`, which in the merged core is also the socket control word, so every track held the renderer and released both sockets. Tang-Phosphor moved `cpu_mode` to its own word at `0xa8` at register ABI 1.8 in `b7ec367`. Here `ports/bl616/phosphor/ae350_play.cpp` reads the ABI at `0x04`, refuses a core older than 1.8 because that core would ignore `0xa8` and play the file's bytes raw, and writes `0xa8 = 1` on every play instead of only when it restarts the loader; `scripts/phosphor.tdsh` declares `0xc0 = 0x2410`, the OLEDrgb on PMOD0 and the encoder on PMOD1 with its seating bit, after the link probe, overridable with the session's `PMOD`, as the stopgap the user chose over a `/tang.ini` parser. The firmware built with `make CHIP=bl616 BOARD=bl616dk` with no warnings from the changed file, `tinytang_bl616.bin` at 618,432 bytes, MD5 `a6f5d1cb37451c5135b06be69387aacd`, build identity `f8d6fdf-dirty.43c548f`, and all eleven host test scripts pass. Through the guarded tools the ABI 1.8 core went to `/cores/console138k/phosphortang.bin` at 5,031,936 bytes with entry 26's image kept as `phosphortang.bin.bak`, the script to `/scripts/phosphor.tdsh` at 2,657 bytes, and the firmware was installed with `tools/tinytang_flash.py`; during the power cycle the user seated the OLEDrgb in PMOD0 and the encoder in PMOD1, and `platform` then reported the new identity with no crash record. From the console `tdsh run /scripts/phosphor.tdsh` loaded core 80 and declared the sockets, `0x04` read `0x00010008`, `0xa8` read 1, `0xc0` read `0x2410` before, during and after two plays of `/music/test.mp3`, each ending at 441000 samples at 44.1 kHz with zero underruns, the panel frame counter at `0xd8` advanced throughout, and the panel signature matched the source at `0x76491800`. The user confirmed the OLED lit with the core's cell frame and heard the tone perfectly. The reference supersedes `PMOD-003`, whose `0x10` is the bring-up core's address, with `PMOD-004`, and adds `PHOS-011` for the moved register and the firmware's guard; `PHOS-006` is left as written, since it was sourced at `a22ec9c` and is accurate for that commit. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff (this entry, `PMOD-004` and `PHOS-011` with their routing rows and index lines, and `PMOD-003` marked superseded with its routing row pointed at `PMOD-004` and its index line prefixed, with its statement untouched), confirmed `.ai/core.md` unchanged, parsed the reference YAML with every record matching an index line, validated this entry as number 45 of 100 with `tools/check_core_log.py`, and confirmed no settled history was rewritten.

#### Next Steps:

A `/tang.ini` parser (`PMOD-002`) should replace `phosphor.tdsh`'s fixed declaration and also cover cores loaded by other paths, such as the Phosphor app after a `tangload` from the Terminal, which today leaves the sockets released until the script runs. Tang-Phosphor's next cycle restores its `clk_pixel` margin, which entry 71 found closing at one placement of four. The open items from entry 44 stand, and the `.bak` Phosphor core on the card can be removed once the user is satisfied.

#### Files Modified:

- ports/bl616/phosphor/ae350_play.cpp
- scripts/phosphor.tdsh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 46 COMMIT Unreleased 2026-10-06T18:00:42-07:00

#### Coming From:

Unreleased bbe5651

#### Purpose:

Let a program on Tang-Phosphor's AE350 request byte ranges of a file from the card on demand, as Tang-PSX requests disc sectors from Tang-Control, so Phosphor can stop sending every track whole before it decodes.

#### Outcome:

This is step 1 of a plan the user approved in Tang-Phosphor, recorded there as entry 75, to play tracks by reading them on demand. The new `ports/bl616/phosphor/ae350_file_server.cpp` is Tang-Control's Tang-PSX disc service (`core/tangpsx.cpp`, Apache-2.0) cut down to one file and one caller: it polls the sequence word of a mailbox in the AE350's result words at `0x4074`, reads the offset at `0x4078` and the length at `0x407c`, accepts the request only if the sequence reads the same before and after them, and answers it with `fpga_file_stream` using that offset and length, while the loader is in RUN; a length of 0 or over 16 MiB, or a request with no file to serve, is refused, and a cancelled send ends the service because the shell's Ctrl-C reader consumes the keystroke. `ports/bl616/phosphor/ae350_play.cpp` now exposes its register helpers, the ABI check and `0xa8` selection as `ae350_select`, and the loader restart with its settle delay as `ae350_restart_loader`, with the register addresses in `ae350_play.h`, and `ae350_play_file` uses them unchanged in behaviour. The new `phosphor run <image.tpi> [file]` in `ports/bl616/phosphor/phosphor_cmd.cpp` refuses to run while a track plays, restarts the loader, takes the mailbox baseline before sending the image, serves the program's requests, and prints each request with its byte count, CRC and time, then the loader state, the result and USER(0..12); `README.md` documents it. The firmware built with `make CHIP=bl616 BOARD=bl616dk` with no warnings from the changed files, `tinytang_bl616.bin` at 620,656 bytes, MD5 `c151e42333eab53bc78752f11651b401`, was installed with `tools/tinytang_flash.py`, and after the user's power cycle `platform` reported `bbe5651-dirty.3473ba7`. On the entry 72 Phosphor core, `phosphor run /ae350/fileread.tpi /music/test.wav` served Tang-Phosphor's probe six requests with no failures or refusals, and the AE350's CRC of every range equalled the CRC this firmware sent and a CRC of a copy regenerated on the PC, including an unaligned 100001-byte range, a range running past the end that correctly returned 10 bytes, and the whole 1764044-byte file in 5179 ms, about 340 KB/s, with a request of a few bytes taking about 10 ms in total; the user accepted the result. The required `.ai` core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai` diff, confirmed that `.ai/core.md` is unchanged, validated this entry as number 46 of 100 with exactly six sections, and confirmed that no settled history was rewritten.

#### Next Steps:

Step 2 has the playback task serve the resident player's requests for the length of a track instead of sending the file whole, once Tang-Phosphor's player reads its input through the mailbox and plays as it decodes; it is qualified by `tools/phosphor_format_sweep.py`. The open items from entry 45 stand.

#### Files Modified:

- README.md
- ports/bl616/phosphor/ae350_file_server.cpp
- ports/bl616/phosphor/ae350_file_server.h
- ports/bl616/phosphor/ae350_play.cpp
- ports/bl616/phosphor/ae350_play.h
- ports/bl616/phosphor/phosphor_cmd.cpp

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 47 COMMIT Unreleased 2026-10-06T18:24:39-07:00

#### Coming From:

Unreleased 3d86aea

#### Purpose:

Serve the Phosphor player's file requests for the length of each track, instead of sending the track's file whole after the player, so that playback starts as soon as the player has been sent.

#### Outcome:

This is step 2 of the plan recorded in Tang-Phosphor entries 75 and 76, whose player now asks for its input on demand. `ports/bl616/phosphor/ae350_file_server.cpp` is now built around a non-blocking `ae350_file_server_step`, which reads the request mailbox once and answers a new request, and `ae350_serve_file`, used by `phosphor run`, loops over it; a request of length 0 is answered with the file's size as a four-byte session sent with `fpga_stream_send`, a changed sequence is served only while the loader is in RUN because a trap writes the mailbox's words, and a send cancelled by Ctrl-C ends the service. `ports/bl616/phosphor/ae350_play.cpp` replaces `ae350_play_file` with `ae350_start_player`, which restarts the loader, takes the mailbox baseline and sends only the player, since the player still returns after each track. `ports/bl616/phosphor/phosphor_player.cpp` begins serving the track's file after the player is sent and, while the track plays, wakes every millisecond to answer requests and looks at the track every 250 ms as before, failing the track if the core stops answering; the status gained `first_sample_ms`, the time from the play to the first sample, which `phosphor play` and `phosphor status` print in `ports/bl616/phosphor/phosphor_cmd.cpp`, the send time now being the player's alone. `phosphor_track.h`'s comment and `README.md` follow the change. The firmware built with `make CHIP=bl616 BOARD=bl616dk`, `tinytang_bl616.bin` at 621,520 bytes, MD5 `e7b5bb27c322935c0358beb690aa9fc8`, with no warnings from the changed files beyond the build-wide `-std=gnu11` notice for C++ sources, and all eleven host test scripts pass. It was installed with `tools/tinytang_flash.py`, and after the user's power cycle `platform` reported `3d86aea-dirty.34645d2`. Tang-Phosphor's on-demand player went onto the card as `/ae350/resident.tpi`, 865816 bytes, with the qualified player kept as `/ae350/resident-qualified.tpi`, 863764 bytes. On the entry 72 core, `tools/phosphor_format_sweep.py` passed all thirteen plays with zero underruns and FLAC now at exactly 441000 samples, short files reached their first sample 2723 ms after the play, and `Fleetwood Mac - Landslide.mp3`, 3236120 bytes, started at 2733 ms against 31 s before and played its full 3:19 with zero underruns; the user heard them play correctly and accepted the result. The required `.ai` core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai` diff, confirmed that `.ai/core.md` is unchanged, validated this entry as number 47 of 100 with exactly six sections, and confirmed that no settled history was rewritten.

#### Next Steps:

Step 3 keeps the player resident between tracks, so a play sends no player and starts in well under a second, and plays consecutive tracks gaplessly; the task will hand the player its next track through the mailbox. The `/tang.ini` parser (`PMOD-002`) follows, so the sockets are declared after every core load rather than only by `phosphor.tdsh`. The open items from entry 45 stand.

#### Files Modified:

- README.md
- ports/bl616/phosphor/ae350_file_server.cpp
- ports/bl616/phosphor/ae350_file_server.h
- ports/bl616/phosphor/ae350_play.cpp
- ports/bl616/phosphor/ae350_play.h
- ports/bl616/phosphor/phosphor_cmd.cpp
- ports/bl616/phosphor/phosphor_player.cpp
- ports/bl616/phosphor/phosphor_player.h
- ports/bl616/phosphor/phosphor_track.h

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 48 COMMIT Unreleased 2026-10-07T00:09:59-07:00

#### Coming From:

Unreleased fd2933e

#### Purpose:

Commit the user's intentional shortening of the active log to entries 41 to 47 with a conforming archive of the full log, so that future agents recover project context from those entries only.

#### Outcome:

The user shortened the active `.ai/core-log.md` to entries 41 to 47 after entry 47 to limit the history a resuming agent works from, and placed an uncompressed copy of the full log, entries 1 to 47, in `.ai/archived_logs/core-log_2026-10-06.md`. That copy was not a `tar.gz` archive and the trim was never committed, so the repository still held entries 1 to 47 while the local log held 41 to 47. The uncompressed copy was removed and replaced by `.ai/archived_logs/core-log_2026-10-07T000959-0700.tar.gz`, which contains the complete log as committed at `fd2933e`, entries 1 to 47, verified byte-identical to that commit's `.ai/core-log.md`; the archive was created without being consulted. Entries 41 to 47 remain in the active log unchanged and byte-identical to `fd2933e`, and this commit removes entries 1 to 40 from it. This is a user-directed trim, not the 100-entry rollover, and the archival procedure in `.ai/core.md` and `.ai/core-syntax.md` is unchanged; numbering continues from the highest entry present, and entries 1 to 40 and the archive are not to be inspected or cited without user approval. The same correction was made in Tang-Phosphor as its entry 80. Nothing was built, deployed or tested. The required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai` diff, confirmed that `.ai/core.md` is unchanged and that no retained entry was rewritten, and validated this entry as number 48 with 8 entries in the active log and exactly six sections.

#### Next Steps:

Entry 47's next steps stand: keep the player resident between tracks with gapless track changes, handing it the next track through the mailbox, then add the `/tang.ini` parser (`PMOD-002`), with the open items from entry 45 still standing. The working tree's uncommitted changes to `ports/bl616/phosphor/ae350_file_server.cpp`, `ae350_file_server.h`, `phosphor_cmd.cpp` and `phosphor_player.cpp`, the `third_party/tinydesk-shell` submodule and the untracked `backups/` directory remain outside this commit.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 49 COMMIT Unreleased 2026-10-07T02:31:50-07:00

#### Coming From:

Unreleased f893906

#### Purpose:

Replace the board's firmware, whose source could not be reproduced from any recorded tree, with firmware built entirely from committed source, and requalify the Phosphor cores on it.

#### Outcome:

The board ran `fd2933e-dirty.d2b8b07`, the firmware Tang-Phosphor entries 78 and 81 qualified with, and `build/build_out/tinytang_bl616.bin`, 621520 bytes, MD5 `067a494fed8d6d394b4a0aeab0e2c71e`, carries that identity, but no TinyTang log entry records it and its source is not recoverable: `cmake/tinytang_build_id.cmake` hashes the uncommitted diff, and recomputing that hash from the current working tree under every combination of the log trim, the untracked archive and `backups/` being present at build time gave no match, so the current uncommitted changes to `ports/bl616/phosphor/ae350_file_server.cpp`, `ae350_file_server.h`, `phosphor_cmd.cpp` and `phosphor_player.cpp`, which are the resident player's BL616 side and unqualified, are not provably what was flashed. That binary is kept as `build/rollback-fd2933e-dirty.d2b8b07.bin`. The `third_party/tinydesk-shell` modification that entry 48 listed as outside its commit is not uncommitted work: it is `third_party/patches/tdsh/0001-configurable-script-limits.patch`, which `scripts/apply-tdsh-patches.sh` applies at every configure, so every build's identity carries a deterministic `-dirty` hash from it. The firmware was built with `make CHIP=bl616 BOARD=bl616dk` in a clean detached worktree of `f893906` with its submodules initialised, excluding the four experimental files, giving identity `f893906-dirty.a217ee5`, whose diff is the submodule patch line alone, 621520 bytes, MD5 `4ab5a3c8632f02cab95f0d1ce36e2b0c`, with only the build-wide `-std=gnu11` notices and existing SDK warnings. It was installed with `tools/tinytang_flash.py`, and after the user's power cycle `platform` reported `f893906-dirty.a217ee5`. On Tang-Phosphor's entry 81 scope core with the qualified on-demand player, the ABI read 1.10, Tang-Phosphor's `tools/oscope_check.py` passed all seven fixtures with exact sample counts, zero underruns, zero visual drops and exact native MCLK counts, and `tools/i2s2_format_sweep.py` passed all thirteen plays with Tang-Phosphor entry 76's sample counts and both rate transitions; the user then accepted the shell, TinyDesk, the Bluetooth keyboard and mouse, and normal playback. The required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete staged `.ai` diff, confirmed that `.ai/core.md` is unchanged and that no settled entry was rewritten, and validated this entry as number 49 with 9 entries in the active log and exactly six sections.

#### Next Steps:

Entry 47's next steps stand, with the open items from entry 45: the resident player with gapless track changes, whose BL616 side remains uncommitted in the four files above and is summarised in Tang-Phosphor's `docs/experiment-resident-player.md`, then the `/tang.ini` parser (`PMOD-002`). For Tang-Phosphor's plan of one visualizer per core, measure a core switch and add resume-at-offset to the player. Firmware deployed for qualification must be built from a tree whose identity a log entry records.

#### Files Modified:

None.

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 50 COMMIT Unreleased 2026-10-07T09:25:33-07:00

#### Coming From:

Unreleased 5564baa

#### Purpose:

Make TinyDesk's Settings > Software update button install new BL616 firmware from `/bl616-firmware.bin` on the card behind a red warning window.

#### Outcome:

The button called `td_app_launch("Software Update")`, which found nothing because TinyDesk's own `apps/update.c` is network OTA and is left out of this port with the other socket apps (`TDESK-008`), so it silently did nothing. `tangflash`'s header check, staging, verification and TCM commit moved unchanged in substance into `ports/bl616/tang_fw_update.c`, a stepwise engine that opens and validates an image, then stages and verifies it one 4 KiB sector per call and reports progress, with the commit last; staging now erases and writes each sector in turn instead of erasing the whole area first, which leaves the same staged contents. `tangflash` in `ports/bl616/tdsh_tang_flash.c` drives that engine and still prints `OK committing`, which `tools/tinytang_flash.py` waits for, while its error lines now read `tangflash: <path>: <reason>`. The new `ports/bl616/td_update_app.c` registers an app named "Software Update" through `td_update_register()`, called from `td_apps_register_all()` in `ports/bl616/td_desktop_bl616.c`, so the unchanged Settings button opens it. It only ever installs `/bl616-firmware.bin` from the card root: a bright red window warns that the Tang may be bricked, must not lose power, and would then need recovery over USB in BOOT mode, shows the image size, and offers Cancel, focused, and I understand, or names the reason and offers only Close when the file is missing or invalid; I understand stages and verifies with a progress bar and refuses to close, a failure there leaves the firmware unchanged and says so, and after verification the window goes full screen with "WRITING FIRMWARE - DO NOT POWER OFF" and an instruction to wait a minute and power-cycle, and commits 1.5 s later so that screen reaches HDMI and remains after the BL616 resets. The firmware was built with `make CHIP=bl616 BOARD=bl616dk` in a clean detached worktree of `5564baa` with its submodules initialised, carrying only these files and excluding the four uncommitted resident-player files, giving identity `5564baa-dirty.14d42e6`, 624384 bytes, MD5 `af905bd0d06ffa78fe08ff4822aa336e`, with no warnings in the changed files and `commit_staged_image` still inside `.tcm_code`. A rebuild of entry 49's `f893906` reproduced identity `f893906-dirty.a217ee5` and its 621520 bytes, though not its MD5, because the SDK embeds the build date and time. On hardware, `tools/tinytang_flash.py` installed the new firmware; the window showed the missing-file message with no image on the card; with the `f893906` rebuild placed as `/bl616-firmware.bin`, the window installed it and `platform` reported `f893906-dirty.a217ee5` after the power cycle; `tinytang_flash.py` restored the new firmware; on it the refactored `tangflash` refused a missing file and a WAV, then installed the new firmware through `tinytang_flash.py` and the board returned as `5564baa-dirty.14d42e6`. The card's `/bl616-firmware.bin` is now that build, and the user installed it through the window, after which `platform` again reported `5564baa-dirty.14d42e6`. The user accepted the window and its behaviour. The required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete staged `.ai` diff, confirmed that `.ai/core.md` is unchanged and that no settled entry was rewritten, and validated this entry as number 50 with 10 entries in the active log and exactly six sections.

#### Next Steps:

Entry 49's next steps stand: the resident player with gapless track changes, whose BL616 side remains uncommitted in `ports/bl616/phosphor/ae350_file_server.cpp`, `ae350_file_server.h`, `phosphor_cmd.cpp` and `phosphor_player.cpp`, then the `/tang.ini` parser (`PMOD-002`), and for Tang-Phosphor's one-visualizer-per-core plan a measured core switch and resume-at-offset in the player. A future firmware can now be installed by copying it to `/bl616-firmware.bin` and using Settings > Software update.

#### Files Modified:

- ports/bl616/tang_fw_update.c
- ports/bl616/tang_fw_update.h
- ports/bl616/td_desktop_bl616.c
- ports/bl616/td_update_app.c
- ports/bl616/tdsh_tang_flash.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 51 COMMIT Unreleased 2026-10-07T11:29:50-07:00

#### Coming From:

Unreleased 515969c

#### Purpose:

Record the discrepancy between the project's earlier upstream publication and the rule the user added to `.ai/core.md` at `515969c` forbidding pushes to any GitHub repository outside the `aquasock` account, together with the stale local clone that hid that rule from the resuming agent, and confirm that both have been addressed.

#### Outcome:

The user added to `.ai/core.md`, under Agent Behavior, that nothing is to be pushed to any GitHub repository for any reason except those under the username `aquasock`, and pushed that change directly to GitHub as `515969c`. Settled entries 41 and 44 record work that this rule now forbids: entry 44 pushed the fork branch to `aquasock/tinydesk-shell`, which the rule permits, but then opened pull request #4 on `tinydesk-project/tinydesk-shell`, and entry 41 records pull request #3 and the reply on issue #1 on that same repository, all of which write to a repository outside the `aquasock` account. Those entries are left as written, since they were within the instructions in force at the time. A second discrepancy hid the rule at first: the resuming agent read `.ai/core.md` from the local clone, whose `main` was at `069c64d`, one commit behind `origin/main`, because shell access had failed with a transient error from the auto-mode safety check and the fetch could not run, so its first read lacked the rule and it proposed a plan without it. Once a later fetch succeeded, the clone was fast-forwarded to `515969c`, whose only change is the two-line addition to `.ai/core.md`, leaving the four uncommitted resident-player files and the `third_party/tinydesk-shell` patch state untouched, and the rule was re-read and applied. The user is handling the open upstream threads on their own side. From this entry on, agents push only to repositories under `aquasock`, and do not open, comment on or update pull requests or issues on any other account's repositories, and the earlier next steps that called for upstream publication from this project no longer stand. Nothing was built, deployed or tested. The required core-syntax audit re-read `.ai/core.md` at `515969c` and `.ai/core-syntax.md`, inspected the complete `.ai` diff, confirmed that `.ai/core.md` was not changed by the agent and that no settled entry was rewritten, and validated this entry as number 51 with 11 entries in the active log and exactly six sections; `tools/check_core_log.py` passed every header, section, terminator and Status check and reported only its numbering rule, which expects the active log to begin at 1 and so flags every retained entry since entry 48's user-directed trim kept numbering from 41.

#### Next Steps:

The resident player with gapless track changes comes next, with its BL616 side still uncommitted in `ports/bl616/phosphor/ae350_file_server.cpp`, `ae350_file_server.h`, `phosphor_cmd.cpp` and `phosphor_player.cpp`, under the plan proposed in this session and awaiting the user's approval; then the `/tang.ini` parser (`PMOD-002`), and for Tang-Phosphor's one-visualizer-per-core plan a measured core switch and resume-at-offset in the player. The upstream items in entries 41 to 44, namely pull requests #3 and #4, issue #1's naming answer, the script-loading allocation error and the three drafts in `docs/upstream/issue-drafts.md`, are the user's to handle, and any upstream text an agent prepares is left in this repository for the user to post. Before acting on `.ai/core.md`, fetch `origin` and confirm the local clone is current.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 52 COMMIT Unreleased 2026-10-07T11:36:59-07:00

#### Coming From:

Unreleased b22cb4e

#### Purpose:

Record what the uncommitted resident gapless player experiment established in `.ai/core-reference.md` and then discard its BL616 code, at the user's direction that the lessons matter and the code does not.

#### Outcome:

The four uncommitted files `ports/bl616/phosphor/ae350_file_server.cpp`, `ae350_file_server.h`, `phosphor_cmd.cpp` and `phosphor_player.cpp` were reviewed against Tang-Phosphor's uncommitted `software/rbhost/host/platform_ae350.c` and untracked `docs/experiment-resident-player.md` at its `7cf9ede`. They held TinyTang's side of step 3 of Tang-Phosphor entry 75's plan, a player that stays resident and plays consecutive tracks gaplessly, and had never been built, deployed or tested. Two reference records now carry what they established. `PHOS-012` records the contract: a length-0 mailbox request is a question selected by its offset, SIZE, NEXT and POLL, with STOP as `0xffffffff` answered once, and the player publishes RES1 and its begun, finished, samples and decode-status words at `0x4040` to `0x4050`. `PHOS-013` records the BL616-side rules, which are residency as loader RUN plus RES1, a track numbered as tracks begun plus one and over when tracks finished reaches it, a soft stop through the mailbox with a 2 s halt fallback, and copied paths, together with one defect found in review: a resident player the BL616 did not start in this boot leaves the mailbox baseline at 0, so a stale sequence would be served. `PHOS-013` also records Tang-Phosphor's order of work, which settles `PHOS-007` and the RAM bridge before qualifying such a player. Both records are `INFERRED` because neither side was run and their sources were never committed, so the records themselves are now the durable copy. The user directed that the code itself not be kept, and the four files were then restored to `b22cb4e` with `git checkout`, leaving the committed on-demand player of entry 47 as TinyTang's playback path; the Tang-Phosphor side was left untouched for that project. Nothing was built, deployed or tested. The required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai` diff, which adds only `PHOS-012` and `PHOS-013` with their routing rows and index lines and this entry, confirmed by parsing every YAML block of `.ai/core-reference.md` that its 143 records are unique and each has an index line, confirmed that `.ai/core.md` is unchanged and that no settled entry or record was rewritten, and validated this entry as number 52 with 12 entries in the active log and exactly six sections; `tools/check_core_log.py` again reported only its numbering rule, as entry 51 records.

#### Next Steps:

The `/tang.ini` parser (`PMOD-002`) is the next TinyTang work, so the PMOD sockets are declared after every core load rather than only by `phosphor.tdsh`. A resident gapless player is deferred until Tang-Phosphor settles `PHOS-007` and its RAM bridge experiment, and would then be rebuilt from `PHOS-012` and `PHOS-013`, including the baseline fix. For Tang-Phosphor's one-visualizer-per-core plan, a measured core switch and resume-at-offset in the player remain open, as do the open items of entry 51.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 53 COMMIT Unreleased 2026-10-07T13:27:43-07:00

#### Coming From:

Unreleased f1b66ab

#### Purpose:

Give TinyDesk a Bluetooth window of TinyTang's own, opened from Settings through a renamed Bluetooth button, which shows the radio and both paired devices, after the TinyDesk author's go-ahead for this port to build its own Bluetooth app.

#### Outcome:

Settings' Network... button called `td_app_launch("Network")`, which did nothing because this port leaves TinyDesk's network app out (`TDESK-008`), and the network tray that also launches it is never drawn without network ops (`TDESK-015`). The user chose to name the button for what it opens and to make the first version status only. The new carried patch `third_party/patches/tinydesk/0001-settings-bluetooth-button.patch` against TinyDesk v0.1.5 makes the button Bluetooth... launching `Bluetooth`, and moves Date & time... from column 16 to 18 because the longer caption is 16 columns wide. `scripts/apply-tinydesk-patches.sh` applies it idempotently at CMake configuration, as the shell's patches are applied, and `THIRD_PARTY.md` and `README.md` describe it. `ports/bl616/tang_ble.c` gained `tang_ble_info()`, declared in `tang_ble.h`, which returns the radio state, the stack's heap cost, the card pairings' state, and for each slot the connection state, address, name, protocol, report count, pairing and whether it reconnects, copying shared fields under a critical section as `hid_status` does; it never blocks and starts nothing, and the shell commands are unchanged. The new `ports/bl616/td_bluetooth_app.c` registers `Bluetooth`, called from `td_apps_register_all()` in `ports/bl616/td_desktop_bl616.c`, and draws those values with free heap and the largest block, refreshed every second, naming `blekbd` and `blemouse` for pairing and forgetting; opening it does not start the radio. The new `tools/tests/test_tinydesk_patches.sh` checks that the patch applies once to a clone of the pinned commit, reproduces the submodule checkout exactly, and leaves TinyDesk's host suite passing 10 of 10 without diagnostics, and all twelve host test scripts pass. The firmware was built with `make CHIP=bl616 BOARD=bl616dk` in a clean detached worktree of `f1b66ab` carrying only these files, with no warnings from the changed files beyond the existing JTAG-programmer ones, giving identity `f1b66ab-dirty.6090e46`, 626960 bytes, MD5 `4e15af76c07917ed921ff79a46b5cfaa`, and was installed with `tools/tinytang_flash.py`. After the power cycle the user reported every check passing: the two buttons side by side, the window opening from Settings with the radio, pairings, heap and both devices, the K950 and M750 rows following power cycles of the devices, `blemouse off` and `on` shown in the window, and a second launch focusing the open window. On the console afterwards `platform` reported `f1b66ab-dirty.6090e46`, `ble` showed the radio up with the stack having taken 15828 bytes, the pairings loaded from the card and both devices paired, reconnecting by themselves and waiting, 74952 bytes free of 133616 with a 60152-byte largest block, and `crash` showed no record from a previous run and 10920 of 16384 bytes of script stack used. `TDESK-015` records the launch-by-name behaviour and the patch. The required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai` diff, which adds only `TDESK-015` with its routing row and index line and this entry, parsed every YAML block of `.ai/core-reference.md` and found its 144 records unique with an index line each, confirmed that `.ai/core.md` is unchanged and that no settled entry or record was rewritten, and validated this entry as number 53 with 13 entries in the active log and exactly six sections; `tools/check_core_log.py` again reported only its numbering rule, as entry 51 records.

#### Next Steps:

The window's next version can add pairing from a scanned list, forgetting, and reconnect on and off, which needs the blocking `blekbd` and `blemouse` work moved into non-blocking calls driven by the `ble` task, with a heap check before a scan starts the radio and a confirmation before the device driving the desktop is forgotten. The `/tang.ini` parser (`PMOD-002`) and the open items of entry 52 stand. Any upstream contribution of this work is for the user to make.

#### Files Modified:

- CMakeLists.txt
- README.md
- THIRD_PARTY.md
- ports/bl616/tang_ble.c
- ports/bl616/tang_ble.h
- ports/bl616/td_bluetooth_app.c
- ports/bl616/td_desktop_bl616.c
- scripts/apply-tinydesk-patches.sh
- third_party/patches/tinydesk/0001-settings-bluetooth-button.patch
- tools/tests/test_tinydesk_patches.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
