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

## 54 COMMIT Unreleased 2026-10-07T13:59:07-07:00

#### Coming From:

Unreleased 3c9d3c8

#### Purpose:

Let the Bluetooth window pair, turn off and on, and forget each slot's device without blocking the desktop, by running that work as jobs on the Bluetooth task.

#### Outcome:

`blekbd pair` blocked its caller for about 20 s (radio start, an 8 s scan, an 8 s watch) and `off` for 500 ms, all printing straight to the console, so the window could not call them. `ports/bl616/tang_ble.c` now runs the window's requests as jobs on its existing `ble` task: `tang_ble_scan_start`, `tang_ble_pair_start`, `tang_ble_set_reconnect` and `tang_ble_forget`, declared in `tang_ble.h`, return at once or with the reason they were refused, and `tang_ble_info` reports the job, its message, a scan's remaining time and each slot's latest event, while `tang_ble_scan_results` returns the last scan strongest first. The scan, connect, off and forget code is shared with the shell; a new `ble_say` prints to the console when called from the shell and keeps the line as the job's message when called on the `ble` task, so the commands' output is unchanged, and `hid_connect` skips its watch for a job. One claim is taken by `blescan`, by every `blekbd` and `blemouse` subcommand other than status and `watch`, and by each job, so the window and the shell cannot overlap and whichever comes second is refused. A scan is refused while either slot is connecting, pairing or discovering, and one that would start the radio is refused below 32 KB of free heap. `ports/bl616/td_bluetooth_app.c` gives each slot Pair..., Off or On, and Forget, a line with its latest event, and a bottom line with the job's progress or the refusal; Pair... runs an 8 s scan and lists the slot's own kind first, then other HID devices, then other named devices, leaving unnamed ones out. Forget always asks, Off asks when the device is connected, and pairing over a different paired device asks to replace it, each warning that a connected device stops at once and pointing to the USB keyboard or a controller. The new `tools/tests/test_bluetooth_app.sh` builds the real window with TinyDesk's real core and a recording stand-in for the engine on an 80x45 screen, and checks the confirmations and their cancellation, the requests made, the list order and exclusions, the countdown and a refusal; it failed against a reversed list order and against a missing connected-device warning before passing, and all thirteen host test scripts pass. The firmware was built with `make CHIP=bl616 BOARD=bl616dk` in a clean detached worktree of `3c9d3c8` carrying only these files, with no warnings from the changed files beyond the existing ones, giving identity `3c9d3c8-dirty.a4d7471`, 633296 bytes, MD5 `a91ff997d974ed981d9233bbb9a926cb`, installed with `tools/tinytang_flash.py`. After the power cycle the user reported every check passing: Off with its confirmation and On for the M750, Forget cancelled and then confirmed, a fresh M750 pairing from the scan list followed through connecting, pairing, discovering and ready, a `blekbd` command refused as busy during a window scan, and the K950 re-paired over its own pairing through the Replace confirmation. On the console afterwards `platform` reported `3c9d3c8-dirty.a4d7471`; `ble` showed the radio up, the pairings saved to the card, the K950 now at `DB:88:A7:81:D9:DA` after re-pairing as `BLE-006` describes, both devices reconnecting by themselves, and 73160 bytes free of 131824 with a 58360-byte largest block, the heap total being 1792 bytes below entry 53's for the new static state; `crash` showed no record from a previous run. The required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai` diff, which adds only this entry, confirmed that `.ai/core.md` is unchanged and that no settled entry was rewritten, and validated this entry as number 54 with 14 entries in the active log and exactly six sections; `tools/check_core_log.py` again reported only its numbering rule, as entry 51 records.

#### Next Steps:

The Bluetooth window is complete for two slots. An antenna on `U35` (`BLE-001`) may remove the occasional HCI 0x3E connection failures and is the user's purchase; the stall when both devices reconnect at once remains open, as do the `/tang.ini` parser (`PMOD-002`) and the open items of entry 52.

#### Files Modified:

- README.md
- ports/bl616/tang_ble.c
- ports/bl616/tang_ble.h
- ports/bl616/td_bluetooth_app.c
- tools/tests/tb_bluetooth_app.c
- tools/tests/test_bluetooth_app.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 55 COMMIT Unreleased 2026-10-07T15:25:13-07:00

#### Coming From:

Unreleased 268021f

#### Purpose:

Halve the heap TinyDesk's Editor takes while open by setting its text and undo limits from this build, as the upstream author advised in tinydesk issue #6.

#### Outcome:

The user had reported that opening the Editor cost 20 to 25 KB of the 132 KB heap; the author confirmed in issue #6 that it allocates its whole 16,385-byte text buffer and an undo history of about 8.7 KB on opening, whatever the file's size, and suggested smaller limits, recorded as `TDESK-016`. `CMakeLists.txt` now defines `TD_EDITOR_MAX=8192` and `TD_EDITOR_UNDO=2048` for every translation unit, so the Editor needs about 12.8 KB; files over 8 KB open read-only. TinyDesk's own `test_history_limits` pastes 10,001 bytes and cannot pass at these limits, so the new `tools/tests/test_editor_limits.sh` includes upstream's `tests/test_editor.c` and runs its other Editor tests at the limits it reads from `CMakeLists.txt`, with checks in that test's place that a file one byte short of 8 KB grows to exactly 8 KB, that typing at 8 KB is refused with "The file is full", that a file of 8,193 bytes opens read-only and is never written, that a paste past 8 KB is refused whole, and that a 1 KB paste undoes while one over 2 KB stays but cannot be undone; its 98 checks pass, it refused to build with the limit changed back to 16 KB, and all fourteen host test scripts pass. `tools/make_editor_test_files.py` writes the 7,000-byte and 9,000-byte files the hardware test used, identical on every run, which were copied to the card root with `tools/tinytang_put.py`. The firmware was built with `make CHIP=bl616 BOARD=bl616dk` in a clean detached worktree of `268021f` carrying only these files, with both definitions confirmed in the build flags and no new warnings, giving identity `268021f-dirty.bab3b26`, 633296 bytes, MD5 `c8400170003c77ed08bc66891db4a964`, the same size as entry 54's since the buffers are heap allocations, and was installed with `tools/tinytang_flash.py`. After the power cycle the user read System Monitor at 54 KB free on the desktop and 50 KB with Files open, 38 KB with the Editor open on `/editor-7k.txt`, so 12 KB for the Editor, back to 50 KB each time it closed; the file took three typed characters, kept them across a reopen, and `/editor-9k.txt` opened read-only and could not be edited; the user accepted the result. On the console afterwards `platform` reported `268021f-dirty.bab3b26`, `crash` showed no record from a previous run and 73160 bytes free of 131824 with a 58360-byte largest block, unchanged from entry 54, and the card held `/editor-7k.txt` at 7,003 bytes and `/editor-9k.txt` at its original 9,000. The required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai` diff, which adds only `TDESK-016` with its routing row and index line and this entry, parsed every YAML block of `.ai/core-reference.md` and found its 145 records unique with an index line each, confirmed that `.ai/core.md` is unchanged and that no settled entry or record was rewritten, and validated this entry as number 55 with 15 entries in the active log and exactly six sections; `tools/check_core_log.py` again reported only its numbering rule, as entry 51 records.

#### Next Steps:

The card's `/bl616-firmware.bin` is still entry 50's `5564baa-dirty.14d42e6`, so Settings > Software update would install it and roll the board back; it should be replaced with the current build or removed, at the user's choice. The Bluetooth interface is to follow the shape of TinyDesk's `td_net_ops_t`, as the author asked in tinydesk issue #7, before the controller work; `peek` and `poke` wait for tinydesk-shell's next release or a decision to pin its `main`; the controller phases begin when the user's controller arrives, and the open items of entry 54 stand. `/editor-7k.txt` and `/editor-9k.txt` may be removed from the card.

#### Files Modified:

- CMakeLists.txt
- tools/make_editor_test_files.py
- tools/tests/tb_editor_limits.c
- tools/tests/test_editor_limits.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 56 COMMIT Unreleased 2026-10-07T15:34:32-07:00

#### Coming From:

Unreleased 4aba99a

#### Purpose:

Bring the board back to the current firmware after a restore from Settings installed the stale card backup, and make that backup the current build, exercising Software Update in both directions.

#### Outcome:

The user restored the firmware through Settings > Software update while `/bl616-firmware.bin` still held entry 50's image, 624384 bytes, as entry 55's next steps had warned. The install itself worked: the board came back cleanly as `5564baa-dirty.14d42e6`, `crash` showed no record from a previous run and the heap at 75208 bytes free of 133872, entry 50's layout, but that rolled back entries 53 to 55, taking away the Bluetooth window and the Editor limits, while the pairings on the card and the shell's Bluetooth commands were unaffected. Entry 55's qualified image, `268021f-dirty.bab3b26`, 633296 bytes, MD5 `c8400170003c77ed08bc66891db4a964`, kept from that cycle's deployment, was checked by MD5 and its build identity and copied to `/bl616-firmware.bin` with `tools/tinytang_put.py`, which placed 633296 bytes, and the card listing confirmed the size. The user then installed it through Settings > Software update, with the window showing 633296 bytes, and after the power cycle confirmed that Settings again had the Bluetooth button opening the Bluetooth window, accepting the result. On the console `platform` reported `268021f-dirty.bab3b26`, `crash` showed no record from a previous run and 73160 bytes free of 131824 with a 58360-byte largest block, entry 55's figures, and `ble` showed the pairings loaded from the card and both devices paired and reconnecting by themselves. Nothing was built. The required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai` diff, which adds only this entry, confirmed that `.ai/core.md` is unchanged and that no settled entry was rewritten, and validated this entry as number 56 with 16 entries in the active log and exactly six sections; `tools/check_core_log.py` again reported only its numbering rule, as entry 51 records.

#### Next Steps:

The card's backup now matches the running firmware. Software Update installs whatever `/bl616-firmware.bin` holds, so a future cycle that deploys new firmware should also copy its qualified image there, or the next restore rolls the board back. The Bluetooth interface in the shape of TinyDesk's `td_net_ops_t` (tinydesk issue #7), `peek` and `poke` after tinydesk-shell's next release, and the controller phases stand, as do the open items of entry 54; `/editor-7k.txt` and `/editor-9k.txt` may be removed from the card.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: PASS
- User Test: PASS

---

## 57 COMMIT Unreleased 2026-10-07T16:00:02-07:00

#### Coming From:

Unreleased 969e6ff

#### Purpose:

Read /tang.ini after every core load and declare the PMOD sockets from it, replacing phosphor.tdsh's fixed declaration, and hand the cycle to the next agent built and deployed but not yet tested on hardware.

#### Outcome:

The user's decisions for the design were: TinyTang keeps its own table of module names copied from Tang-Phosphor's numbering; `pmodN_flip` is applied as declared with no flip-safety check for now; unknown keys are warned about and ignored; `phosphor.tdsh`'s `PMOD` override is removed; the file is edited in the Editor, and a file past its 8 KB limit is edited on a PC. `ports/bl616/tang_ini.c` and `tang_ini.h` parse the file with no card or core access, line by line in any size of chunk, into the socket control word for `0xc0` and up to eight problems with their lines. The contract's names `oledrgb`, `vga_j1` and `vga_j2` are accepted, with this project's names `encoder` for personality 4 and `i2s2` for personality 5, the I2S2 that Tang-Phosphor's merged core now has (`PMOD-005`), plus `none`. A missing file or entry leaves a socket released. An unknown module, a flip other than yes or no, and one PmodVGA half without the other each release that socket with a message, and a later valid line does not cancel a bad flip. Unknown keys, other sections and keys outside `[tang]` are warned about and ignored, the last of a repeated key wins with a warning, and comments may follow a value. The new `ports/bl616/phosphor/pmod_sockets.cpp` runs from `tangload` in `ports/bl616/tdsh_tang_flash.c` after every successful load: it asks the core for its ID, and for ID `0x50` at register ABI 1.8 or later it prints the declaration and any problems, writes the word to `0xc0` and reads it back with the frame selector masked; it writes nothing to other cores. The new shell command `tangini`, registered in `ports/bl616/tdsh_platform_bl616.c`, shows the file's declaration against the loaded core's `0xc0`, and `tangini apply` sends it again without a reload. `scripts/phosphor.tdsh` no longer declares the sockets. `docs/tang.ini` is a commented example of the OLEDrgb-and-encoder layout, `0x2410`, and `README.md` documents the file and the command. The new `tools/tests/test_ini.sh` builds the parser with AddressSanitizer and UBSan and checks the contract's example, an empty file, CRLF, spacing, case and comments, every personality on either socket, the VGA pairing in both orders, a refused partner, each refusal, repeated keys, unknown keys and sections, an overlong line, a note overflow, input fed a byte at a time, and that `docs/tang.ini` parses to `0x2410` with nothing to report. It failed with the VGA pairing rule removed, with the bad-flip refusal removed and with a typo in the example, and all fifteen host test scripts pass. The firmware was built with `make CHIP=bl616 BOARD=bl616dk` in a clean detached worktree of `969e6ff` carrying only these files, with no warnings from them, giving identity `969e6ff-dirty.71fe989`, 637872 bytes, MD5 `2fab6aba64c72324a53710c981369de9`; it was committed to flash with `tools/tinytang_flash.py`, and `/scripts/phosphor.tdsh` on the card was replaced with this version, 2220 bytes. The handoff came before the power cycle: the board was off USB, waiting for it, so `platform` has not yet reported the new identity, and no hardware test has run. There is no `/tang.ini` on the card, deliberately, since what is seated was not known: entry 45 seated the OLEDrgb in PMOD0 and the encoder in PMOD1, and entry 49's I2S2 work used PMOD0 since. The card's `/bl616-firmware.bin` is entry 55's `268021f-dirty.bab3b26`, so Settings > Software update would remove this cycle's firmware. `docs/upstream/editor-limits-reply.md`, a draft of the user's reply on tinydesk issue #6 with the Editor results of entry 55, is included unposted for the user to review and post. The required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai` diff, which adds only `PMOD-005` with its routing row and index line and this entry, parsed every YAML block of `.ai/core-reference.md` and found its 146 records unique with an index line each, confirmed that `.ai/core.md` is unchanged and that no settled entry or record was rewritten, and validated this entry as number 57 with 17 entries in the active log and exactly six sections; `tools/check_core_log.py` again reported only its numbering rule, as entry 51 records.

#### Next Steps:

Power-cycle the board with both cables and confirm with `platform` that it runs `969e6ff-dirty.71fe989`, then run the qualification the user planned for the rotary encoder, the OLEDrgb, the I2S2 and the PmodVGA, writing `/tang.ini` for each layout and loading the Phosphor core with `tdsh run /scripts/phosphor.tdsh` or resending with `tangini apply`. With no file, expect `0xc0 = 0x0000`. For the OLEDrgb in PMOD0 and the encoder in PMOD1 with `pmod1_flip = yes`, expect `0x2410`. For the I2S2 in PMOD0 with JP1 at SLV and the encoder in PMOD1 flipped, expect `0x2450` and audio while a track plays. For `vga_j1` in PMOD0 and `vga_j2` in PMOD1, expect `0x0320` and a picture. With `pmod0 = oled`, expect a line-2 message and PMOD0 released. A module that fails while `0xc0` holds the declared word points at the core rather than the parser. After the user accepts, copy the qualified image to `/bl616-firmware.bin` (entry 56) and record the result in a new entry, superseding `PMOD-005`'s verification with a hardware record. Still open: the Bluetooth interface in the shape of TinyDesk's `td_net_ops_t` (tinydesk issue #7); `peek` and `poke` once tinydesk-shell's next release carries them; the controller phases when the user's controller arrives; the reconnect stall when both Bluetooth devices return at once; the antenna on `U35`; and removing `/editor-7k.txt` and `/editor-9k.txt` from the card. Upstream threads are the user's to answer, and agents push only to `aquasock` repositories.

#### Files Modified:

- README.md
- docs/tang.ini
- docs/upstream/editor-limits-reply.md
- ports/bl616/phosphor/pmod_sockets.cpp
- ports/bl616/tang_ini.c
- ports/bl616/tang_ini.h
- ports/bl616/tdsh_platform_bl616.c
- ports/bl616/tdsh_tang_flash.c
- scripts/phosphor.tdsh
- tools/tests/tb_ini.c
- tools/tests/test_ini.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 58 COMMIT Unreleased 2026-10-07T16:43:14-07:00

#### Coming From:

Unreleased ed2211c

#### Purpose:

Confirm entry 57's firmware boot and qualify its automatic PMOD declaration with the normally seated I2S2 and the existing oscilloscope core.

#### Outcome:

The console confirmed `969e6ff-dirty.71fe989`, with the watchdog enabled, no resets retained since power-up and no previous crash record. Loading the card's default `phosphortang.bin` without `/tang.ini` automatically declared `0xc0 = 0x0000`, and `tangini` confirmed both sockets released; that image reports ABI 1.8 and is older than the separate I2S2 playback and oscilloscope images. The user confirmed I2S2 in its usual PMOD0 position, normally oriented. A 65-byte `/tang.ini` now declares `pmod0 = i2s2`, `pmod0_flip = no`, `pmod1 = none` and `pmod1_flip = no` under `[tang]`. Loading `phosphortang-i2s2-play.bin` (ABI 1.9) automatically applied `0x0050`, and `tangini apply` wrote and read it back successfully. The old `/music/test.wav` and `/music/test.mp3` files are absent, so playback used `/music/Fleetwood Mac - Landslide.mp3`. The user wanted the visualizer, so the final test loaded the existing `phosphortang-oscope.bin` (5,158,912 bytes, ABI 1.10) directly through `tangload`, again automatically declaring `0x0050`, enabled medium trails and glow at `0xac = 0x0b`, hid the overlay and restarted the track. The user reported perfect audio and a perfect oscilloscope and asked to let the song finish. It ended at 3:19 with 8,796,143 samples at 44,100 Hz and zero underruns; the socket declaration remained `0x0050`, and no crash was recorded. During playback the clock status was `0x17` and the MCLK count `0x00113a00`, as expected at 44.1 kHz. Nothing was built or reflashed. The required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai` diff, confirmed that only this entry was added and no settled history or core directive changed, and validated entry 58 with 18 entries in the active log and exactly six sections; the existing checker reports only the numbering mismatch documented in entry 51.

#### Next Steps:

Qualify the OLEDrgb, encoder and PmodVGA layouts and the invalid-module refusal from entry 57 when the user is ready to change modules. The I2S2 automatic declaration, reapplication and full-track playback with the oscilloscope have passed and need not be repeated without a new concern. Keep the card's distinct core images in mind: the default Phosphor script still loads the older ABI 1.8 image, while `/scripts/oscope.tdsh` selects the visualizer and currently also writes its own socket declaration. The card's `/bl616-firmware.bin` remains entry 55's older image; refresh it with entry 57's qualified firmware after the remaining qualification is accepted. The other open items of entry 57 stand.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: PASS
- User Test: PASS

---

## 59 COMMIT Unreleased 2026-10-07T17:02:41-07:00

#### Coming From:

Unreleased f888ae4

#### Purpose:

Qualify the encoder's lower-row seating and the OLED-and-encoder declaration, including reapplication and invalid-module rejection, on entry 57's firmware.

#### Outcome:

The user clarified that the Digilent Pmod ENC is a single-row module, then seated it face up in PMOD1, the socket nearest HDMI, on pins 7-12, with PMOD0 empty. Tang-Phosphor's reference explicitly records that the socket's flip bit also selects the alternate row for a 1x6 module; the earlier instruction to seat the encoder flipped did not distinguish row selection from turning it over. A 69-byte `/tang.ini` declared PMOD0 none and PMOD1 encoder with `pmod1_flip = yes`; loading the existing default Phosphor image (ABI 1.8) automatically wrote and read back `0x2400`. With the user operating each control, five clockwise clicks changed `0xe0` from `0x80000000` to `0x80000014`, and five counterclockwise clicks restored the baseline, proving four counts per detent in both directions. Pressing and releasing the button changed `0xe4` from `0xb2` to `0xf3` and back without moving the count; toggling the slide switch changed it to `0x30` and back to `0xb2`. The user reported an accidental knob step during the switch test, and its count was exactly minus four, ending at `0x7ffffffc`. After powering off, the user installed the OLEDrgb face up in PMOD0, keeping the encoder on PMOD1's lower row. The repository's 664-byte `docs/tang.ini` was copied to the card; a core load automatically applied `0x2410`, source and panel signatures both read `0x76491800`, and the panel frame counter advanced from 202 to 401. The user reported that the image looked great. `tangini apply` preserved the declaration, and a brief play of Landslide at 44.1 kHz with zero underruns left the sockets at `0x2410` and the OLED signatures matching. A temporary line-2 declaration `pmod0 = oled` produced the expected unknown-module warning and released PMOD0 while preserving the encoder at `0x2400`; restoring `docs/tang.ini` and applying it returned `0x2410` and matching OLED signatures. Playback was stopped, the final layout is OLED plus encoder, and firmware `969e6ff-dirty.71fe989` has no previous crash record. Nothing was built or reflashed. The required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai` diff, confirmed only this entry was added with no core directive or settled-history changes, and validated entry 59 with 19 active entries and exactly six sections; the checker reports only its existing numbering mismatch documented in entry 51.

#### Next Steps:

The remaining module qualification is PmodVGA, J1 on PMOD0 and J2 on PMOD1 with the normal orientation, expecting `0x0320` and a picture. I2S2, encoder, OLED, missing-file release and invalid-module refusal have passed for the exercised layouts and paths. Refresh the stale card firmware backup with entry 57's qualified image after the remaining qualification is accepted. The final `/tang.ini` matches the currently seated OLED and lower-row encoder; change it before using another layout. The other open items of entry 58 stand.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: PASS
- User Test: PASS

---

## 60 COMMIT Unreleased 2026-10-07T17:13:26-07:00

#### Coming From:

Unreleased 5548664

#### Purpose:

Complete PmodVGA qualification, persist the header order that matches the dock's physical seating, and record the completed PMOD hardware checks.

#### Outcome:

The user powered down, replaced the OLED and encoder with PmodVGA facing up and connected the CRT. The initial `/tang.ini` declared PMOD0 vga_j1 and PMOD1 vga_j2, neither flipped; the default Phosphor image (ABI 1.8) applied and read back `0x0320` automatically, and reapplication succeeded, but the user reported only the color pattern on HDMI and a CRT that woke without a picture. Tang-PSX's reference already recorded the verified face-up placement with J1 on PMOD1 and J2 on PMOD0, opposite the agent's instruction. Writing `0xc0 = 0x0230` without moving the module immediately gave mirrored HDMI and VGA images, as the user confirmed. The card now holds a 69-byte `/tang.ini` with `pmod0 = vga_j2`, `pmod1 = vga_j1` and both flip keys set to no; reloading the core automatically applied `0x0230`, and `tangini` confirmed the match. Firmware remains `969e6ff-dirty.71fe989`, with no previous crash record. Entries 58 and 59 together with this result complete the exercised I2S2, encoder, OLED and VGA layouts, missing-file release, invalid-module refusal and reapplication checks; `PMOD-006` supersedes `PMOD-005` with their bounded hardware evidence, including the encoder's lower-row meaning of flip and the corrected VGA header order. No source changed, and nothing was built or reflashed. The intended refresh of `/bl616-firmware.bin` could not be completed: the qualified entry 57 image was not found in this checkout, /tmp or the retained board-backup directory. The local `build/build_out/tinytang_bl616.bin` is also 637872 bytes but identifies as `969e6ff-dirty.3054a25`, MD5 `f4c00d946084dfcba28643cbf98e8f52`, rather than the running image's `969e6ff-dirty.71fe989`, MD5 `2fab6aba64c72324a53710c981369de9`; it was not substituted, and the card backup remains entry 55's older firmware. The required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai` diff, confirmed unchanged core directives and settled log entries and preserved reference statements except for supersession metadata and lookup pointers, validated entry 60 with 20 active entries and exactly six sections, and parsed the reference YAML with 146 unique records (excluding the schema template) and an index line for each; the log checker reports only its existing numbering mismatch documented in entry 51.

#### Next Steps:

No further PMOD hardware qualification is pending for the exercised layouts. Keep the current VGA declaration while that module is seated; future layouts must update /tang.ini to match their actual rows and headers. Locate or recover entry 57's exact qualified firmware image to refresh the SD backup, or propose a new reproducible build and deployment cycle; do not silently substitute the unqualified local binary. The card's default Phosphor image remains the older ABI 1.8 core, while the oscilloscope and I2S2 images are separate. The other open items of entry 57 stand, including the Bluetooth ops interface, peek and poke after the next shell release, controller work and simultaneous Bluetooth reconnect discovery.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: PASS
- User Test: PASS

---

## 61 COMMIT Unreleased 2026-10-07T17:50:00-07:00

#### Coming From:

Unreleased 7369f99

#### Purpose:

Consolidate Phosphor's normal launch path around the qualified merged oscilloscope image, with PMOD selection owned only by /tang.ini, and qualify it from the desktop.

#### Outcome:

The user approved consolidating the scripts during this cycle. `scripts/phosphor.tdsh` now loads the canonical `/cores/console138k/phosphortang.bin`, probes the link, enables the stereo XY oscilloscope with medium trails and glow at `0xac = 0x0b`, and hands it the screen. It has no PMOD register write: `tangload` applies `/tang.ini`. The missing `/music/test.mp3` default was removed; without `FILE` it loads ready for track selection in the desktop's Phosphor app, while a supplied `FILE` is checked before programming and played in the background. The README describes this single launcher. The qualified scope image from Tang-Phosphor entry 82 was verified locally at 5,158,912 bytes, MD5 `0d0e2b5c1df97bfb3a66e729bbfc4391`, SHA-256 `1c63387762443a3e90ec06c77b75c6421440ec5158b8d7cbdb8b7e947ea535d9`, staged through `tinytang_put.py` and renamed to the canonical card path. The old ABI 1.8 canonical image is preserved at `/cores/console138k/rollback/phosphortang-abi1.8.bin`, and `oscope.tdsh`, `i2s2-play.tdsh` and `i2s2-tone.tdsh` were moved to `/scripts/diagnostics/`; their diagnostic core images remain available. The normal scripts folder now has only one Phosphor launcher, copied from this tree at 2,848 bytes. Creating the new directories with `mkdir -p` failed while walking the mount root; plain `mkdir` succeeded, and this cycle does not change that filesystem behaviour. On the console, launching with no `FILE` returned zero and loaded ABI 1.10 with VGA declaration `0x0230`, scope control `0x0b` and no playback. A nonexistent `FILE` returned one before programming. A path containing spaces launched Landslide successfully, and at 17 s it reported 44.1 kHz, zero audio underruns, zero scope queue drops and unchanged VGA sockets. Playback was stopped, `FILE` unset and the normal boot script run to return the board to its console; the last `crash` check had no prior record. The user then reported all desktop tests passing: right-click Run, track selection in the Phosphor app, the moving oscilloscope on HDMI and VGA, pause/resume, stop and repeated F12 switching. USB was absent when the final diagnostic probe was attempted after that acceptance, so no later board readings were collected. Nothing was rebuilt or reflashed; BL616 firmware remains entry 57's qualified image. The required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai` diff, confirmed only this entry was added and no directives or settled history changed, and validated entry 61 with 21 active entries and exactly six sections; the checker reports only its existing numbering mismatch documented in entry 51.

#### Next Steps:

The canonical Phosphor launcher and VGA desktop cycle are accepted, with no further validation pending for that path. Future PMOD changes use /tang.ini and the same core and launcher. The stale `/bl616-firmware.bin` remains to be refreshed after locating or recovering entry 57's exact qualified image or completing a new reproducible firmware cycle; the mismatching local binary must not silently replace it. The other open items from entry 60 stand, and the mount-root failure in `mkdir -p` can be addressed separately if needed.

#### Files Modified:

- README.md
- scripts/phosphor.tdsh

#### Status:

- Build: N/A
- Deployment: PASS
- User Test: PASS

---

## 62 COMMIT Unreleased 2026-10-07T18:33:54-07:00

#### Coming From:

Unreleased 8613cc9

#### Purpose:

Create TinyTang's desktop host core with HDMI and configurable VGA output, and make it the normal boot core.

#### Outcome:

The desktop host is reconstructed from pinned nestang plus the common, menu and new desktop patches, with native GPL-3.0-only HDL and revision-C constraints under `fpga/desktop/`; it preserves the desktop layer and keyboard link while replacing the old PMOD controller and LED outputs with released sockets or a complete VGA pair. The version-1 register endpoint validates requests and CRCs, acknowledges socket changes at vertical blank, reports core ID `0x54` and its own ABI 1.0, and safely rejects unsupported module words. Firmware now recognizes that ID and applies `/tang.ini` after loading it; the boot script selects `/cores/console138k/desktop.bin`. Reconstructed UART tests passed 17 replies with independent Python CRC checks, malformed-request recovery, keyboard traffic and layer enable; PMOD tests passed pin permutations, flips, frame commits, released pins, blanking and full-frame sync counts, and the legacy decoder passed 18 checks. A deliberately broken pin mapping failed, as did negative-slack and missing-summary timing reports. The final Gowin 1.9.11.03 placement 2 on GW5AST-LV138PG484AC1/I0 revision C passed with setup +2.880 ns, hold +0.167 ns and zero timing violations; its 4,524,032-byte binary has SHA-256 `ceb82e4a0924bc4104a3d78a37e1f83a4dc86f388ab7068049093ec0b7a204b0`. The 637,904-byte BL616 build `8613cc9-dirty.21e7755`, SHA-256 `3ae2edf06194f449041b9e5a0a4d640dcb8d79d6df5e5549a633237a36257988`, was retained locally and flashed successfully. After the user's power cycle, the console confirmed that identity, desktop ID 84, ABI 1.0, automatic VGA word `0x0230`, no refused desktop cells and no prior crash record. The old menu image remains on the card and its boot script was saved at `/scripts/diagnostics/boot-menu-rollback.tdsh`; serial readback confirmed both the old and new script bytes against source. The user reported a working boot console on both displays but a narrow vertical strip at VGA's left edge, absent from HDMI, and requested a separate corrective build cycle. Source review identified the new VGA path's raw blanking against registered RGB as an alignment defect related to the earlier PROT-009 glyph leak; the static-color PMOD test did not cover that boundary. The card's stale `/bl616-firmware.bin` was not replaced before acceptance. The required core-syntax audit re-read core.md and core-syntax.md, inspected the full .ai diff, confirmed unchanged directives and settled history, and validated this entry's six sections and contiguous numbering with 22 active entries; the checker retains only the existing numbering mismatch documented in entry 51.

#### Next Steps:

Correct VGA sync and blanking alignment with the registered compositor pixels, add a streamed regression that reproduces the left-edge artifact, and perform the user's requested placement 0-3 sweep with resource and timing results reported before deployment. After visual acceptance, finish desktop input and Phosphor transition checks and refresh the SD firmware backup with the retained running image; the other open work from entry 61 remains separate.

#### Files Modified:

- README.md
- THIRD_PARTY.md
- ports/bl616/phosphor/pmod_sockets.cpp
- scripts/boot.tdsh
- fpga/desktop/README.md
- fpga/desktop/board.v
- fpga/desktop/build.tcl
- fpga/desktop/desktop.cst
- fpga/desktop/desktop.sdc
- fpga/desktop/desktop_pmod.sv
- fpga/desktop/desktop_regs.sv
- third_party/patches/desktop/0001-desktop-host.patch
- tools/build_desktop_core.sh
- tools/check_gowin_timing.py
- tools/tests/sim/tb_desktop_pmod.sv
- tools/tests/sim/tb_desktop_uart.sv
- tools/tests/test_desktop_core.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: FAIL

---

## 63 COMMIT Unreleased 2026-10-07T19:02:37-07:00

#### Coming From:

Unreleased e7c8d8e

#### Purpose:

Correct the desktop core's VGA left-edge artifact and qualify the fix with a four-placement sweep before deployment.

#### Outcome:

VGA blanking and sync now register on the same pixel-clock edge as the compositor's RGB, aligning the complete active region without changing the HDMI or text-layer pipeline. The new regression streams the actual compositor and HDMI raster, checks physical VGA pins against an independent glyph image through line and frame wraps, and separately checks every HDMI visible pixel. Before the fix it reproduced 720 left-edge blanking leaks and 2,942 VGA color or sync mismatches while all 921,600 HDMI visible pixels passed; afterward it passed all 1,237,500 VGA raster pixels and all HDMI pixels with no leak. The UART, independent reply CRC, PMOD row/flip/reset/frame-commit and legacy desktop decoder regressions also passed. New deterministic sweep and reporting tools build placements 0-3, validate all timing summaries, report resources and select the greatest setup margin; a report-parser check also rejected mixed-source variants. The requested Gowin 1.9.11.03 revision-C sweep passed all four variants with zero setup/hold violations, and its resource and timing table was reported before deployment. Every variant uses 2,655 LUTs, 1,777 FFs, 12 BSRAM, one DSP and three PLLs; ALU use is 302 except placement 2's 301. Placement 1 was selected at setup +5.135 ns and hold +0.153 ns, with reported pixel Fmax 120.002 MHz against the unchanged 74.25 MHz target. Its 4,524,032-byte image, SHA-256 `c0237974d6404833c3aa15064586f4669b9bf73fc5644057842228c71f3e9f7f`, replaced the canonical desktop image after saving the previous image at `/cores/console138k/rollback/desktop-unaligned.bin`; the normal boot script loaded it and automatically applied `0x0230`. The user reported the requested console left-edge, desktop keyboard/pointer, repeated F12 and power-cycle checks passing and returned to the console. Final diagnostics confirmed firmware `8613cc9-dirty.21e7755`, desktop ID `0x54`, ABI 1.0, matching VGA declaration, zero refused cells and no crash record. An agent-run Phosphor launch confirmed ID `0x50`, ABI 1.10, scope control `0x0b` and automatic `0x0230`, then successfully returned through the boot script to the desktop core; no track was started. The retained accepted 637,904-byte firmware image from entry 62 was uploaded to refresh `/bl616-firmware.bin`, preserving the old 633,296-byte backup at `/bl616-firmware-pre-desktop.bin`; temporary upload and script files were removed. No firmware was rebuilt or reflashed in this cycle. The required core-syntax audit re-read core.md and core-syntax.md, inspected the complete .ai diff, confirmed unchanged directives and byte-preserved settled history, and validated six sections, contiguous actual numbering and the 23-entry cap count; the checker passes with headings normalized in a temporary audit copy, leaving its existing start-at-one mismatch unchanged.

#### Next Steps:

No validation remains for this corrected desktop boot and VGA cycle. The canonical desktop core and Phosphor launcher both use the existing /tang.ini declaration, and the SD firmware backup now matches the accepted running build. The separate Bluetooth ops, controller, reconnect and filesystem work carried from earlier entries remains outside this cycle.

#### Files Modified:

- fpga/desktop/README.md
- fpga/desktop/desktop_pmod.sv
- tools/check_gowin_timing.py
- tools/report_desktop_sweep.py
- tools/sweep_desktop_core.sh
- tools/tests/sim/tb_desktop_pmod.sv
- tools/tests/sim/tb_desktop_video.sv
- tools/tests/test_desktop_core.sh
- tools/tests/test_desktop_video.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 64 COMMIT Unreleased 2026-10-07T19:56:35-07:00

#### Coming From:

Unreleased 16b49b3

#### Purpose:

Implement an independent fixed 24×16 OLED terminal on the desktop core with an original 4×4 font.

#### Outcome:

The user approved the independent OLED terminal and later deferred deployment with “wait”, then explicitly requested this commit and push for agent handoff. Desktop ABI 1.1 adds OLEDrgb selection on either socket, 384 coloured character cells, a cursor, validated 1–64-cell block writes, frame count and CRC32 of emitted RGB565 bytes. The original 4×4 font supports printable ASCII and a boxed fallback; TinyDesk's compact terminal emulator substitutes unsupported printable Unicode for display only. The BL616 OLED Terminal app, independent session, task-local input/output routes inherited by script workers, background UART updater and core-replacement hooks are implemented and compile, but runtime routing and lifecycle qualification remain pending. FatFS descriptor allocation is protected briefly without placing any filesystem calls inside critical sections. The panel/SPI engine comes from committed Tang-Phosphor 7cf9ede, with complete-pixel latching added to the panel; THIRD_PARTY.md records its GPL provenance and the MIT TinyDesk emulator instance, and no colibri code was copied or adapted. The reconstructed UART regression passed 36 replies with independent CRC checks, successful ordered writes and rejected requests causing no partial writes; existing PMOD, legacy decoder and full HDMI/VGA compositor tests passed. The OLED regression passed initialization, minimum power/SPI timing, all 6,144 pixels of a nonuniform frame, cursor inversion, coherent live pixel bytes and an independent Python check of serialized-frame CRC32 79285abe; compact terminal wrap, scroll, ANSI colours, cursor and Unicode fallback checks passed, as did the 70-check desktop-layer host regression. All four Gowin 1.9.11.03 revision-C placements passed with zero timing violations and were reported before any deployment: every variant uses 3,702 LUTs, 383 ALUs, 2,313 FFs, 14 BSRAMs, 1.5 DSPs as reported by Gowin, and three PLLs. Placement 2 has the best setup margin, +2.193 ns setup and +0.246 ns hold, pixel Fmax 88.689 MHz against 74.25 MHz; its 4,481,940-byte image has SHA-256 dfe6ad996aefeee605d95b03ba56608e55d12fe22096d2b160ba50542ce3c301. The report parser now accepts fractional DSP usage. The final compile check produced BL616 identity 16b49b3-dirty.201ddb8, 648,640 bytes, SHA-256 c04b10de1ea9c4b2f00fd080837f75e08b158f2a9fbff77c48cc3da4deafc183. Exact images, build identity, reports, logs, manifest, source diff and changed-source archive are retained locally in build/oled-terminal/handoff; all four placement outputs remain in build/oled-terminal/sweep. Neither new image was uploaded or flashed, and no user acceptance is claimed. Before USB disappeared, the user seated OLEDrgb face up in PMOD0 with PMOD1 empty, the old desktop core was explicitly released at 0xc0 = 0, and /tang.ini was uploaded with oledrgb on PMOD0, no flips and PMOD1 none. Last known running firmware and canonical desktop image remain entry 63's accepted versions; their final state could not be re-probed because the USB console was absent. Existing dirty TinyDesk and TinyDesk Shell submodules are carried patches and remain unstaged. The core-syntax audit re-read core.md and core-syntax.md, inspected the complete .ai diff, confirmed unchanged directives and byte-preserved settled history, and validated entry 64's six sections, prose, statuses, contiguous numbering and 24-entry count.

#### Next Steps:

Resume from the committed sources and fpga/desktop/README.md, preserving the user's deployment hold until they explicitly restore the console connection and report readiness. Review concurrent shell history and shared-device commands, add dedicated session-routing, worker inheritance and core-transition tests, and measure available heap and task stack margin before calling the OLED app runtime-qualified; the shell task reserves 16 KB plus its independent session. Rebuild the final firmware after any changes, retain its exact identity and source evidence, and rerun the four-placement sweep if HDL changes; the completed FPGA regressions need repeating only for relevant changes. Once ready, preserve the accepted desktop image and firmware backup, stage the selected desktop candidate and new firmware, deploy, request the required physical power cycle, and verify identity, ABI 1.1, automatic OLED word 0x0010, advancing frames and stable-frame CRC. Ask the user to check font readability, fixed geometry, typing and editing, wrap/scroll, unsupported-character substitution, file read/write, focus isolation from TinyConsole and Terminal, close/reopen persistence, F12 and core reloads. Deployment and hardware acceptance remain outstanding; log those results in a new cycle rather than rewriting this deferred handoff.

#### Files Modified:

- FreeRTOSConfig.h
- README.md
- THIRD_PARTY.md
- fpga/desktop/README.md
- fpga/desktop/build.tcl
- fpga/desktop/desktop_oled.sv
- fpga/desktop/desktop_pmod.sv
- fpga/desktop/desktop_regs.sv
- fpga/desktop/oled_font.vh
- fpga/desktop/oled_panel.sv
- fpga/desktop/oled_spi.sv
- ports/bl616/oled_vterm.c
- ports/bl616/oled_vterm.h
- ports/bl616/phosphor/oled_link.cpp
- ports/bl616/tang_oled.c
- ports/bl616/tang_oled.h
- ports/bl616/tang_osd_desk.c
- ports/bl616/td_desktop_bl616.c
- ports/bl616/tdsh_bl616.h
- ports/bl616/tdsh_fs_bl616.c
- ports/bl616/tdsh_platform_bl616.c
- ports/bl616/tdsh_tang_flash.c
- third_party/patches/desktop/0001-desktop-host.patch
- tools/make_oled_font.py
- tools/report_desktop_sweep.py
- tools/tests/oled/test_oled_vterm.c
- tools/tests/sim/tb_desktop_oled.sv
- tools/tests/sim/tb_desktop_pmod.sv
- tools/tests/sim/tb_desktop_uart.sv
- tools/tests/sim/tb_desktop_video.sv
- tools/tests/tb_osd_desk.c
- tools/tests/test_desktop_core.sh
- tools/tests/test_desktop_oled.sh
- tools/tests/test_desktop_video.sh
- tools/tests/test_oled_vterm.sh

#### Status:

- Build: PASS
- Deployment: NOT RUN
- User Test: NOT RUN

---
## 65 COMMIT Unreleased 2026-10-07T20:48:24-07:00

#### Coming From:

Unreleased 9313085

#### Purpose:

Qualify entry 64's OLED terminal for deployment with routing, concurrency and core-transition tests and runtime measurements, then deploy it and obtain the user's hardware acceptance.

#### Outcome:

Review of entry 64 found four defects, fixed with regressions that fail against the committed code. `ports/bl616/phosphor/oled_link.cpp` sent extended READ32 frames once a second to whatever core was loaded, so a game core received frames it does not speak (EXTCTL-004) and each unanswered one stalled `osddesk`, which also watches F12, for 250 ms; it now asks the core's ID once per load through the new `tang_fpga_core_id()` in `ports/bl616/tang_fpga_uart.c`, at most three times a second apart, and sends extended frames only to core `0x54`. `tangput` read the USB console directly from any shell and is now refused from a private route, and a second `tangload` is refused while one programs, both in `ports/bl616/tdsh_tang_flash.c`. `tang_oled_start()` in `ports/bl616/tang_oled.c` could start several shell tasks under concurrent callers and now starts one, and its prompt buffer holds a full path. `crash tasks` in `ports/bl616/tang_crash.c` lists every task's stack margin. The new `tools/tests/test_oled_link.sh`, `test_oled_session.sh` and `test_tang_flash.sh` drive the real sources, the session test on POSIX threads through `tools/tests/stubs/rtos_threads/` inside TinyDesk's window manager, checking one session under eight concurrent starts, output and input isolation between the OLED and console shells, route inheritance by foreground and background script workers, focus-gated keys, and the link's behaviour across core loads, game cores, refused writes and socket changes; the session test passed ten repeated runs, and all 19 host test scripts pass. Firmware `9313085-dirty.41ff066`, 649,600 bytes, SHA-256 `92f18a3442c75ff18765c56ad7d5be1aaccf7cd9f9cf382055eb2bc62b86b540`, was built with `make CHIP=bl616 BOARD=bl616dk` in a clean worktree of `9313085` carrying the firmware and test changes, with no warnings from the changed files, and installed with `tools/tinytang_flash.py` after the user authorised it; entry 64's placement-2 desktop core went onto the card with the accepted ABI 1.0 core saved as `/cores/console138k/rollback/desktop-abi1.0.bin`. After the power cycle the board reported that identity, desktop ABI 1.1 and `0xc0 = 0x0010` applied automatically, and the panel's frame count advanced, but the cursor stayed 0 and the CRC did not change because the core refused every block write with status 5: `fpga/desktop/desktop_regs.sv` read a 32-bit count with words from byte 12, while EXTCTL-002 and the firmware's Tang-Control encoder use a count byte with words from byte 9, and `tools/tests/sim/tb_desktop_uart.sv` had been written to the same wrong layout. Single WRITE32 cell and cursor writes drew correctly on the panel. The decoder now follows EXTCTL-002 and returns the count, the testbench encodes it, rejects the old layout, and replays eight frames made by `tools/tests/sim/gen_desktop_block_frames.cpp` from the firmware's own `fpga_ext_frame.h`; the committed decoder fails that test with the board's status 5, and `tools/tests/test_desktop_core.sh` passes all 45 replies, 388 replayed cells and the PMOD, legacy and video regressions. The Gowin 1.9.11.03 revision-C sweep, with placement 0 rebuilt because a README edit mid-sweep changed its source fingerprint, built all four placements from one fingerprint with zero violations at 3,780 LUTs, 2,313 FFs, 14 BSRAMs and 1.5 DSPs; placement 2 was selected at setup +2.735 ns and hold +0.252 ns, pixel Fmax 93.174 MHz, SHA-256 `4fcc62e6570805b4ea02fb7356c3344e7df90ad980bfe78c66475f1245439873`, and the table was reported before deployment. The refused image was kept as `/cores/console138k/rollback/desktop-abi1.1-countfault.bin`, and the fixed core, 4,463,306 bytes, was installed and loaded with `tangload`; the cursor then followed the OLED shell, the CRC changed after `oledterm run "echo hello"`, and the user confirmed the panel. The README text in `fpga/desktop/` changed again after that build, so the committed tree's fingerprint differs from the image's while the HDL is identical. With the terminal running the heap was 40,896 bytes free of 124,600 with a 37,048-byte largest block, against 72,312 of 130,976 before this cycle, so the terminal costs about 25 KB and its fixed buffers 6.4 KB; `oledterm` had 7,928 of 16,384 stack bytes free and `osddesk` 3,160 of 4,096, with no crash record. The user reported every desktop check passing and accepted the result; the agent's readback afterwards found no OLED-session commands in the shared `~/.tdsh_history`, no `/oled-test.txt` and the panel CRC unchanged since the agent's own command, so the board did not record that session; asked about it, the user confirmed the terminal works functionally. One sent command was appended to a half-typed console line because the probe reports a prompt regardless of a partial line; the agent now clears the line with Ctrl-U before sending. The card's `/bl616-firmware.bin` now holds this firmware, with entry 63's kept as `/bl616-firmware-pre-oled.bin`. Images, logs, sweep summary and source diff are retained in `build/oled-terminal/qualify/`. Both root sessions share `~/.tdsh_history`, so simultaneous entries can lose a history line; that is documented, not fixed, since it needs an upstream change. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai` diff, confirmed `.ai/core.md` unchanged and settled entries byte-preserved, and validated this entry's header, six sections, prose, Status values, contiguous numbering and the 25-entry count.

#### Next Steps:

The OLED terminal needs no further validation for the tested paths. A desktop session typed in the OLED Terminal window was not captured by the board's own records in this cycle, so if a later cycle touches that window's keyboard path, confirm from the history file or the panel CRC that its keys reach the session. With the terminal open about 23 KB less heap remains on the desktop, so heavy combinations such as the Editor plus a script should be measured before relying on them. The status codes in `desktop_regs.sv` still differ from EXTCTL-002's status 2 for bad count, length or alignment, which the firmware does not distinguish. Still open from earlier entries are the Bluetooth ops interface in the shape of `td_net_ops_t`, `peek` and `poke` after the next shell release, controller work, simultaneous Bluetooth reconnect discovery, the `mkdir -p` mount-root failure, and removing `/editor-7k.txt` and `/editor-9k.txt` from the card.

#### Files Modified:

- README.md
- fpga/desktop/README.md
- fpga/desktop/desktop_regs.sv
- ports/bl616/phosphor/oled_link.cpp
- ports/bl616/tang_crash.c
- ports/bl616/tang_fpga_link.h
- ports/bl616/tang_fpga_uart.c
- ports/bl616/tang_oled.c
- ports/bl616/tdsh_tang_flash.c
- tools/tests/sim/gen_desktop_block_frames.cpp
- tools/tests/sim/tb_desktop_uart.sv
- tools/tests/stubs/rtos_threads/FreeRTOS.h
- tools/tests/stubs/rtos_threads/bflb_mtimer.h
- tools/tests/stubs/rtos_threads/bflb_sec_trng.h
- tools/tests/stubs/rtos_threads/rtos_threads.c
- tools/tests/stubs/rtos_threads/semphr.h
- tools/tests/stubs/rtos_threads/task.h
- tools/tests/stubs/rtos_threads/tinytang_build_id.h
- tools/tests/stubs/tang_flash/ff.h
- tools/tests/tb_oled_link.cpp
- tools/tests/tb_oled_session.c
- tools/tests/tb_tang_flash.c
- tools/tests/test_desktop_core.sh
- tools/tests/test_oled_link.sh
- tools/tests/test_oled_session.sh
- tools/tests/test_tang_flash.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 66 COMMIT Unreleased 2026-10-07T21:49:36-07:00

#### Coming From:

Unreleased 985cbcb

#### Purpose:

Show what the OLED Terminal's 4x4 mode can do as a graphics display with no firmware change, and leave a fire demo that plays on the OLED at power-up for filming.

#### Outcome:

The user now treats the 24x16 OLED mode as the graphics mode for small demos, with a later 8x8 mode intended for text, and chose to make no firmware changes for this work. The new `tools/make_oled_demo.py` renders a sine plasma and a fire as 48 pre-rendered ANSI frames each, 1,569 to 2,614 bytes, shading between palette colours by drawing the font's density glyphs in one colour over another, and writes the players `play.tdsh`, `fire.tdsh` and `fire10.tdsh`, the screen restore `restore.ans`, and `boot-fire.tdsh`, which is `scripts/boot.tdsh` plus one line that starts `fire10.tdsh` in the OLED session; its output is identical on every run and byte-identical to the copies on the card. The frames and players are in `/demos/` on the card, `/scripts/boot.tdsh` is the fire-on-boot version, and the normal boot script is kept as `/scripts/diagnostics/boot-before-fire.tdsh`, with `scripts/boot.tdsh` in the repository unchanged. Played from the console with `oledterm run "tdsh run /demos/play.tdsh"` on firmware `9313085-dirty.41ff066`, 288 frames took about 16.8 s, about 17 frames a second against the link's 25, and while it ran the heap fell from 40,896 to 12,964 bytes free with a 9,552-byte largest block, the script costing about 28 KB, and the script task used 13,752 of its 16,384 stack bytes. A script graphics kit with `draw` and `line` functions was tried and dropped at the user's direction after two failures that host runs of the same shell did not show: a 170-line showcase returned silently because loading it exhausted the heap, and a four-deep demo overflowed the script task's stack, which the crash recorder logged as a stack overflow in `tdsh_script` and which left the board off USB until a power cycle. Measured on the board, `cat` alone uses 9,576 stack bytes, each enclosing loop or `if` about 1,392 more and a function call about 5,000, so a board script can nest about three blocks around a `cat` and cannot call a function from inside loops; the players stay within that. The user reported the fire-on-boot script working after a power cycle and accepted the fire demo. The board was off USB at the close, so `/gfx/`, holding the dropped kit and five stack probe scripts, is still on the card. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai` diff, confirmed `.ai/core.md` unchanged and settled entries byte-preserved, and validated this entry's header, six sections, prose, Status values, contiguous numbering and the 26-entry count.

#### Next Steps:

Remove `/gfx/` from the card at the next connection, and restore the normal boot from `/scripts/diagnostics/boot-before-fire.tdsh` when filming is done. A kid-friendly drawing kit without firmware changes would have to avoid functions and keep within three nested blocks, and a script that nests deeper still crashes the board, so ease of use for scripts depends on a firmware change to the script stack or a guard, which the user has not asked for. The 8x8 text mode for the OLED is a separate core change for a later cycle. The other open items of entry 65 stand.

#### Files Modified:

- tools/make_oled_demo.py

#### Status:

- Build: N/A
- Deployment: PASS
- User Test: PASS

---
## 67 COMMIT Unreleased 2026-10-07T22:10:13-07:00

#### Coming From:

Unreleased 7f0ed90

#### Purpose:

Return the card to the normal boot and the PmodVGA socket layout for music with the visualizer, and record the measured demo frame rate and a stdout routing defect found while doing so.

#### Outcome:

With no PMOD seated and firmware `9313085-dirty.41ff066` running, `/gfx/` was removed from the card, `/scripts/boot.tdsh` was restored from `/scripts/diagnostics/boot-before-fire.tdsh` at 3,326 bytes, the size of `scripts/boot.tdsh`, and `/tang.ini` was rewritten as entry 60's 69-byte layout with `pmod0 = vga_j2`, `pmod1 = vga_j1` and both flips `no`; `tangini apply` then had the loaded desktop core at ABI 1.1 report `0xc0 = 0x0230` as declared. The `/demos/` frames and players stay on the card. Measured from the user's 59.94 fps video of the fire on the panel, the demo updates at 14.97 frames a second, a 4-video-frame period, not the about 17 that entry 66 gives from console timing; most of each 66.7 ms frame is the player's `sleep 0.04`. While the fire started by the boot script still ran in the OLED session, fragments of its frames appeared on the USB console ahead of commands' output, because the firmware neither enables newlib reentrancy nor changes stdout's buffering, so stdio output such as `cat`'s `fputs` lands in one global buffer and is flushed through the route of whichever task flushes next; the output of one shell can therefore reach the other's display, and concurrent unlocked use of that buffer is unsafe. Entry 65's session test exercised the shell's printf path, not stdio, and did not catch it. No repository file and no firmware changed. The user will reboot with the PmodVGA seated to play music with the visualizer, so that hardware result is not yet reported. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai` diff, confirmed `.ai/core.md` unchanged and settled entries byte-preserved, and validated this entry's header, six sections, prose, Status values, contiguous numbering and the 27-entry count.

#### Next Steps:

After the user's reboot with the PmodVGA seated, confirm `0xc0 = 0x0230` on the desktop and Phosphor cores and VGA output with the visualizer if any doubt remains. Fix the stdout leak in a firmware cycle, likely by making stdout unbuffered at startup so each write leaves through the calling task's route, and extend `tools/tests/test_oled_session.sh` to send stdio output from both shells at once. A shorter player sleep, such as 0.01 s, would bring the fire close to the 25 frames a second the link allows. The other open items of entry 66 stand.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: PASS
- User Test: NOT RUN

---
## 68 COMMIT Unreleased 2026-10-07T22:40:00-07:00

#### Coming From:

Unreleased a8f1e9d

#### Purpose:

Restore desktop script launches by allocating the OLED Terminal's buffers only when it starts, after the static buffers added in entry 64 left too little heap to run `phosphor.tdsh` from the desktop.

#### Outcome:

The user reported that right-click Run on `/scripts/phosphor.tdsh` in the desktop returned to the Terminal prompt at once with no output, while the same quoted `tdsh run "/scripts/phosphor.tdsh"` loaded the Phosphor core, applied `0x0050` for the I2S2 in PMOD0 and played Landslide from the console, which the user heard with the scope. `crash` showed an 80-byte allocation refused for task `tdsh_script` from the script loader, whose failure upstream returns silently, so the desktop launch was running out of heap. Entry 64's firmware reserved about 6 KB statically for the OLED Terminal, which took the heap total from entry 63's 130,976 bytes to 124,600 whether or not the terminal was used. `ports/bl616/tang_oled.c` now allocates its emulator and input ring when the terminal first starts, freeing them again if that start fails, and gains `tang_oled_running()`; `ports/bl616/phosphor/oled_link.cpp` allocates its cell, shadow and word buffers on its first poll after the terminal is running. `tools/tests/test_oled_link.sh` checks through a wrapped `calloc` that the link allocates nothing before the start and once after, and it and `test_oled_session.sh` fail if either file reserves static data over 256 bytes, which the previous `tang_oled.c` did with 3,208 and 1,024 bytes and the previous link failed; all 19 host test scripts pass. Firmware `a8f1e9d-dirty.1c81d39`, 649,824 bytes, SHA-256 `96d3e911930f7e4f3405ca23d7666bfbbe5117f9f3feab894049b40c0b0e265e`, was built with `make CHIP=bl616 BOARD=bl616dk` in a clean worktree of `a8f1e9d` carrying the firmware and test changes, with no warnings from the changed files, and installed with `tools/tinytang_flash.py`; image, log and diff are in `build/oled-lazy/`. After the power cycle the heap was 71,828 bytes free of 130,616, 6,016 more in total, and the user reported desktop Run of `phosphor.tdsh`, track selection, I2S2 audio and the scope all passing, with no allocation refused since boot. Afterwards `oledterm start` took the heap from 58,156 to 27,220 bytes free, so the started terminal costs about 31 KB and is held until reboot, too much for a desktop script launch alongside it; its panel was not attached, so only its memory was checked. `fpga/desktop/README.md` records this and changed after the build, which it does not affect. The card's `/bl616-firmware.bin` is this build, with entry 65's kept as `/bl616-firmware-pre-lazy.bin`, and `/tang.ini` declares the I2S2 layout. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai` diff, confirmed `.ai/core.md` unchanged and settled entries byte-preserved, and validated this entry's header, six sections, prose, Status values, contiguous numbering and the 28-entry count.

#### Next Steps:

Power-cycle before the next desktop script launch, since the OLED Terminal started for the measurement holds its 31 KB until reboot. The OLED Terminal and a desktop script launch cannot both run within the heap, so a smaller script stack or a way to stop the terminal would be needed to use them together. The stdout leak of entry 67 remains for a firmware cycle. A third PMOD port on the dock's free 2x20 header was discussed: it needs an adapter with a 3.3 V regulator from pin 11's 5 V, a third socket in the desktop and Phosphor cores and a `pmod2` key, and its FPGA pins and bank voltage are still to be read from the schematic. The other open items of entry 67 stand.

#### Files Modified:

- fpga/desktop/README.md
- ports/bl616/phosphor/oled_link.cpp
- ports/bl616/tang_oled.c
- ports/bl616/tang_oled.h
- tools/tests/tb_oled_link.cpp
- tools/tests/test_oled_link.sh
- tools/tests/test_oled_session.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 69 COMMIT Unreleased 2026-10-08T07:12:09-07:00

#### Coming From:

Unreleased 62b3805

#### Purpose:

Stop one shell's stdio output from reaching the other shell's display by making the shared stdout unbuffered, the first of the fixes for entry 67's stdout defect.

#### Outcome:

The firmware runs newlib without per-task state, so `stdout` is `_REENT->_stdout` on the single global `_impure_ptr` that the SDK's `__getreent()` returns, and its buffer is shared by every task while `_write_r` picks the route of whichever task flushes. Per-task newlib state was measured at 1,064 bytes of `struct _reent` per task on this target, about 15 KB for the board's tasks, and a per-session state for the OLED shell alone at about 1.1 KB was proposed; the user chose the smaller step of an unbuffered stdout for now. `tdsh_bl616_init()` in `ports/bl616/tdsh_platform_bl616.c` now calls `setvbuf(stdout, NULL, _IONBF, 0)` before anything prints, so each stdio write leaves through its own task's route at once. The shell parser's redirects and pipes still assign the global `stdin` and `stdout` (`tdsh_parser.c`), so while one shell runs `echo x > file` or a pipe the other shell's stdio output can go to that file; that remains open. `tools/tests/test_oled_session.sh` now wraps `setvbuf` and fails unless startup makes stdout unbuffered, which the previous source does, and all 19 host test scripts pass. The new `tools/check_stdout_isolation.py` starts the fire demo in the OLED session and runs `cat /tang.ini` 40 times from the USB console, counting the demo's colour codes that arrive on USB; on entry 68's firmware it counted 1,405. Firmware `62b3805-dirty.64ca308`, 649,856 bytes, SHA-256 `51db60b1752a45472187612b0202adabde9e8bbff3ffbcbdb14243a998cddbc0`, was built with `make CHIP=bl616 BOARD=bl616dk` in a clean worktree of `62b3805` carrying these changes, with no warnings from the changed file, installed with `tools/tinytang_flash.py`, and retained with its log and diff in `build/stdout-unbuffered/`. After the power cycle the heap was 72,856 bytes free of 130,616, 1,028 more than entry 68 because stdout no longer allocates a buffer, the user reported desktop Run of `phosphor.tdsh` with I2S2 audio and the scope and Terminal output passing, and `tools/check_stdout_isolation.py` counted 0 colour codes on USB. The card's `/bl616-firmware.bin` is still entry 68's build. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai` diff, confirmed `.ai/core.md` unchanged and settled entries byte-preserved, and validated this entry's header, six sections, prose, Status values, contiguous numbering and the 29-entry count.

#### Next Steps:

UTF-8 file names follow as the next cycle, from the investigation of the Hungarian track names. The redirect and pipe hijack between the two shells needs per-session newlib state if it is to be fixed, which the user has deferred. The other open items of entry 68 stand.

#### Files Modified:

- ports/bl616/tdsh_platform_bl616.c
- tools/check_stdout_isolation.py
- tools/tests/tb_oled_session.c
- tools/tests/test_oled_session.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 70 COMMIT Unreleased 2026-10-08T07:40:53-07:00

#### Coming From:

Unreleased 8ec32b0

#### Purpose:

Fix file names that code page 437 cannot carry, after the Hungarian track names in `/music` were listed under their short names, counted as zero bytes and refused to open or move.

#### Outcome:

With `FF_LFN_UNICODE 0` the shell and TinyDesk read long names through code page 437, so a name holding a letter 437 lacks, such as the Hungarian `ő`, `ű`, `ú` or `Á`, was listed under its 8.3 short name, and a short name holding a 437 byte such as `0xA3` could not be looked up again: the file showed 0 bytes, would not open and refused to move, which the card's own `/music` listing showed as `08-M\xa3L~1.FLA` and `01-HA_~1.FLA` at zero bytes. `fatfs_conf_user.h` now defines `FF_LFN_UNICODE 2`, UTF-8, the encoding the shell and TinyDesk use, leaving `FF_CODE_PAGE 437` for short names and `FF_LFN_BUF 255` unchanged; its comment records that a name too long for `FF_LFN_BUF` still falls back to its short name, which in UTF-8 converts back correctly. The new harness, `tools/tests/test_fat_names.sh` with `tools/tests/make_fat_names_image.py`, `tools/tests/tb_fat_names.c` and `tools/tests/check_fat_names.py`, builds the SDK's FatFs R0.15 with this repository's `fatfs_conf_user.h` against a FAT32 image whose twelve Hungarian names are stored exactly as Linux wrote them on the card, then lists the folder, looks each listed name up again, opens, reads and hashes it, moves it out and back and lists once more, and runs twice: forced back to code page 437 it reproduces the card, reporting that 1 of 12 listed names cannot be looked up again, and as committed it passes every check, with `fsck.fat` accepting the image after the moves; of the host suite's 23 scripts 22 pass, and `test_desktop_video.sh` fails only because `NESTANG_DIR` is unset here, which this change does not touch. Firmware `8ec32b0-dirty.e30863d`, 650,368 bytes, SHA-256 `7da08074dedbf88d4ca104e5878d2e70e33114d411172e594bfcd016e4b60443`, was built with `make CHIP=bl616 BOARD=bl616dk` in a clean worktree of `8ec32b0` carrying these changes, with no warnings from the changed files beyond the existing ones, installed with `tools/tinytang_flash.py`, and retained with its log and diff in `build/utf8-names/`; a build of the same sources left by the interrupted session, identity `8ec32b0-dirty.e0c20c7`, was superseded because its identity counted a stray `build.log` at the worktree root and so did not reproduce. After the power cycle the board reported `8ec32b0-dirty.e30863d`: `ls -l /music` listed its seven tracks under their full UTF-8 names with their exact sizes, `01 - Ha Újra Látom.flac` at 26,107,070 bytes and `08 - Múló Idő.flac` at 28,048,198 bytes among them, a quoted `ls -l` resolved a UTF-8 path, `head -l 1` read the `fLaC` header and Vorbis comment of such a file, a rename to an ASCII name and back to the original UTF-8 name preserved the name and size and left the file readable, and the listing was unchanged afterwards; heap was 68,496 bytes free of 130,616 with a 60,536-byte largest block, the last script task had used 10,728 of its 16,384 bytes of stack, and `crash` recorded no refused allocation and no record from a previous run. The user reported that the fix held under everything they tried, which included dragging `05 - Híd A Folyón.flac` to `/root/Desktop` and reorganising tracks into `/music_O-scope`, `/musichhh`, `/music_codec-test` and `/music_oscope-test`, so `/music` now holds seven of the harness image's twelve names. The card's `/bl616-firmware.bin` is this build, with entry 68's kept as `/bl616-firmware-pre-utf8.bin`. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai` diff, confirmed `.ai/core.md` unchanged and settled entries byte-preserved, and validated this entry's header, six sections, prose, Status values, contiguous numbering and the 30-entry count; `tools/check_core_log.py` exits 1 on all thirty entries for numbering alone, because it expects the active log to begin at `1` while this one continues from `41` as the last rollover left it, a disagreement that predates this entry and that renumbering settled history must not repair.

#### Next Steps:

The redirect and pipe hijack between the two shells that entry 69 left open still needs per-session newlib state, which the user has deferred, and the other open items of entry 69 stand: the OLED Terminal and a desktop script launch cannot both fit the heap, a third PMOD port on the dock's free 2x20 header, the author's naming answer on the upstream peek and poke issue, review of pull request #3, the user's decision on the three drafted issues, the note to nand2mario before TinyTang is listed anywhere visible, the Bluetooth discovery stall when both devices reconnect at once, and Phosphor's load-progress readout and file-size limit. No card work is outstanding for this cycle.

#### Files Modified:

- fatfs_conf_user.h
- tools/tests/check_fat_names.py
- tools/tests/make_fat_names_image.py
- tools/tests/tb_fat_names.c
- tools/tests/test_fat_names.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 71 COMMIT Unreleased 2026-10-08T09:18:59-07:00

#### Coming From:

Unreleased 29f40c0

#### Purpose:

Put the shell's `peek`/`poke` memory regions in the BL616 port so the board's own RAM and registers can be read and written from the console, and find out what the chip's security controller does to the addresses it guards.

#### Outcome:

`ports/bl616/tdsh_platform_bl616.c` now fills the shell's optional `mem_regions` hook, so `peek` and `poke` exist as root-only core commands over a table the port owns: `sram`, the firmware's own RAM from `0x62FC0400` to the linker's `__HeapLimit` with every width and read/write, whose end comes from the symbol so the list follows the RAM reserved for BLE and Wi-Fi and a zero-size region is skipped rather than guessed; `flash`, the XIP window, read-only; `glb`, the 32-bit system control block, read-only; and `tzc_sec` and `tzc_nsec`, the trustzone blocks, read-only. Because `peek`/`poke` are not in v0.1.5, `third_party/tinydesk-shell` is pinned to upstream `main` (`a067fa7`), which the author invited in the peek/poke thread, and with that pin the carried configurable-limits patch is no longer applied: `scripts/apply-tdsh-patches.sh` now recognises a checkout newer than the pinned release that already carries the change, its marker being the `#ifndef` guard the patch adds, and refuses anything else. Firmware `29f40c0-dirty.4c95e97`, 653,200 bytes, SHA-256 `3962123d7b97131667cfc1091d2786645fe00cf691754f206003d1c04ba798e6`, was built with `make CHIP=bl616 BOARD=bl616dk` in a clean worktree of `29f40c0` carrying exactly this tree, so the deployed identity is the committed tree's own, and retained with its log and diff in `build/peek-poke/`. On the board `peek -l` prints the five regions, reads return real content, the banner string at `0xA0095200` came back as the `\r\n ESC[1;36` the map predicts, `flash` at `0xA0000000` reads `0xc2fc7197`, `glb` at `0x20000000` reads `0x68000000` and the TZC ROM control register reads `0xf1c3ff00`, every refusal path fires before memory is touched (a `poke` at flash and at glb reports read-only, an unlisted address, a misaligned one, a width the region does not allow, and an access crossing a region end), `echo $(peek 0xA0000000)` shows the single value is usable in a script, and a `poke` of the shell's own `s_hostname` at `0x62FC770C` changed `0x796E6974` ("tiny") to `0x746E6174` ("tant"), read back, and was restored to the value first read, a byte dump afterwards showing `tinytang` with its NULs. The mask ROM cannot be read at all: `peek` at `0x90000000`, at `0x9001FFF0` and at `0x90020000` each reset the chip, and a monitor watching the console recorded the command echoed at about 100 ms, the USB console dead at about 123 ms with 71 bytes and nothing further printed, and the board re-enumerating as its ISP loader (`0403:6010`) within a second. No exception ran and no crash record was left, `GLB_RESET_STS0` at `0x20000530` read `0x0000007d` and `GLB_TZC_CFG0` at `0x20000490` read `0x00000000` both before and after, so nothing the running firmware can read latches the violation, and a security-violation reset returns to the loader where a watchdog or exception reset returns to the app; the TZC blocks themselves are safe to read and their SEC and NSEC ROM descriptors are identical (control `0xf1c3ff00`, address mask `0x00000000`, R0 `0x00000053`, R1 `0x03ff03ff`, R2 `0x000003ff`, R3 `0x00000000`), recorded raw because decoding them needs the BL616 manual the project does not have. That measurement is the rule the table now follows: an address is listed only after it has been read without taking the board down, which is why the ROM window was tried and removed. The new `tools/tinytang_memdump.py` dumps any region the firmware reports at a width that region allows, checking every row's address against the one it asked for, and its first run caught two bugs of its own by refusing to write anything: the listing's widths are bits, and its END is inclusive. Reading the firmware back with it is how the flash window was checked: dumping 653,200 bytes from `0xA0000000` differed from the deployed image at 636,454 bytes, the image's bytes at offset `0x1000` appeared at dump offset 0, and shifted by that one header all 649,104 bytes the window covers are identical, SHA-256 `1fe6468c26ea8023426acd89abc4ffbf1b0504f834cabea7843357db065f3a4a`, because the SDK's linker places the 4 KB firmware header at `0xA0000000 - 0x1000` (`fw_header_memory` in `bl616_flash.ld`), so the XIP window begins one header into the image and the header itself is unreachable, its address lying in the range that resets the chip. The host suite caught a regression in this change before the commit: the port referenced `__HeapLimit` unconditionally, and `test_oled_session.sh`, which compiles the port on the host where there is no linker script, failed to link; the reference is now weak, resolving to zero there so the `sram` region is dropped, and the suite is back to 22 of 23 with only the pre-existing `NESTANG_DIR` gap. Heap was 72,792 bytes free of 130,552 with a 60,472-byte largest block and the last script task used 10,936 of its 16,384 bytes of stack, with no refused allocation and no record from a previous run. The README and `THIRD_PARTY.md` record the new pin, the retired patch and the feature, and were part of the tree the identity was computed from. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai` diff, confirmed `.ai/core.md` unchanged and settled entries byte-preserved, and validated this entry's header, six sections, prose, Status values, contiguous numbering and the 31-entry count; `tools/check_core_log.py` still exits 1 on all thirty-one entries for numbering alone, its rule expecting the active log to begin at `1` while this one continues from `41` as the last rollover left it, the pre-existing disagreement entry 70 records.

#### Next Steps:

When the shell's next release carries `peek`/`poke` and the configurable limits, pin its tag and delete `third_party/patches/tdsh/0001-configurable-script-limits.patch`, which now exists only for a v0.1.5 build. The trustzone result belongs upstream: the shell's issue #1 is where the author asked ports to describe what they did, and "a read at `0x90000000` resets the chip into its loader in 23 ms with no exception and no crash record" is the concrete case the PORTING guidance about listing only verified addresses lacks, and `docs/PORTING.md` is the file to cite. The three drafted issues in `docs/upstream/issue-drafts.md` remain unposted, the user's own commitment in tinydesk issue #6 to profile each app's memory is still open, and the redirect and pipe hijack between the two shells that entry 69 left open still needs per-session newlib state; the other open items of entry 70 stand.

#### Files Modified:

- README.md
- THIRD_PARTY.md
- ports/bl616/tdsh_platform_bl616.c
- scripts/apply-tdsh-patches.sh
- third_party/tinydesk-shell
- tools/tinytang_memdump.py

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 72 COMMIT Unreleased 2026-10-10T10:03:11-07:00

#### Coming From:

Unreleased bbfbf4e

#### Purpose:

Give the desktop core on-fabric instruments that measure its own clocks on silicon, as entry 57 of the Tang-Build log chose, and commit the PLL placement pins the open flow needs to build a violation-free core at all.

#### Outcome:

The instrument is built, verified off the board, deployed and read back, and the repository now carries the constraint its own working baseline was built with. `fpga/desktop/clock_monitor.sv` is new and holds five free-running 32-bit counters: `clk` twice, read directly because `clk` is the register domain; `hclk` twice and the 50 MHz crystal `sys_clk` once, taken into `clk` as gray code through two flops, because a binary counter sampled mid-carry would hand the synchroniser a mixture while gray changes one bit per increment. There is no reset, so no net is added, and nothing is pinned, so the placer chooses where each probe lands and the two probes of a clock test whether both regions it chose are actually running. `fpga/desktop/desktop_regs.sv` returns them from the endpoint's existing read32 at 0x120, 0x124, 0x128, 0x12c and 0x130, with 0x134 reading the stamp `4d4f4e31`, so no new frame type and no new protocol were needed. `third_party/patches/desktop/0002-clock-monitor.patch` carries the wiring -- a `sys_clk` input on `iosys_bl616` under `DESKTOP_CORE`, `.hclk` and `.ref_clk` on the endpoint instance, and the crystal connection in `nestang_top` -- and was kept as its own patch so the instrument can be removed without touching the desktop host; it was generated from a clean nestang clone and the reconstruction applies the whole series cleanly. The BL616 gained two shell commands, `fpgar <hex-addr>` for one read32 and `fmon [hold-ms]` for all five counters read twice with their deltas and rates, and the request encoder was checked against the project's own independently-computed vector rather than trusted: `tools/tests/fpga_ext_frame_test.cpp` expects CRC bytes `73 17` for command `0x10` and payload `01 02 10 02 00 00 00 20 12 34 56 78`, and the new C encoder reproduces `7317` exactly. The firmware builds as `4d7051da...` against this tree's previous `e2205009...`, and the only difference is the two commands. Off the board the core's own regression, `tools/tests/test_desktop_core.sh` -- extended to compile `clock_monitor.sv` and to drive the monitor's three clocks at 46.53, 13.468 and 20 ns -- now reports `clock monitor: clk=452041/ref=1051671 pix=1561740/ref=1051671 probes agree PASS`, which is `clk` and `hclk` measured at 21.492 and 74.25 MHz with both probe pairs agreeing inside one percent, alongside 56 register responses, the independent Python CRC check and the unmodified video test. The second change is the constraint: the open flow had no violation-free placement at all, eight seeds gave none and two did not route, because `TinyTang/fpga/desktop/desktop.cst` carried no PLL pins and the placer's free choice put `pll_nes` inside a row-108 clock gap, which is the same defect that made the board's working baseline unreconstructable from this repository -- its pins existed only in build artefacts. `INS_LOC` pins for `pll_27` at `PLL_L[1]`, `pll_hdmi` at `PLL_L[3]` and `pll_nes` at `PLL_R[0]` are now committed, the third differing from the vendor's `PLL_B[1]` deliberately because that site is inside the gap the open database does not model, and a fresh reconstruction and build with no override reproduces the same bitstream byte for byte, which is the verification that the fix is complete. On the board, after the firmware was flashed with `tools/tinytang_flash.py` and the console returned at `bbfbf4e-dirty`, `fpgar` first validated the readback against the running baseline's own registers -- `0x00` returning `0x00544453`, `0x04` returning `0x00010001`, `0x100` returning `0x00180010`, and `0x134` correctly absent with status 4 -- and then the monitor core was loaded with `tangload` and `fmon` run three times, each agreeing: `clk` 21.5038 to 21.5053 MHz against its 21.49 MHz constraint, `hclk` 74.2499 to 74.2500 MHz against its intended 74.25, the reference counting 500,235,740 cycles in 10,004 ms, and both probes agreeing to about 0.007 percent, with the 32-bit counters observed to wrap mid-measurement and still give the correct delta. The clocks are therefore at their intended rates on silicon in an open-flow-built core, which is the question entry 57 asked and the timing model could not answer, and the small excess on `clk` is not an error but the design's own PLL dividing a 2000 MHz VCO by 93, which the constraint treats conservatively. The user's part was the power cycle and the display was not reported, so the hardware acceptance of the *screen* is not established here; the log's own rule that an agent-run device-side diagnostic is not user acceptance is why `User Test` is `NOT RUN`. Evidence, including the raw `fmon` output, lives at `evidence/clock-monitor.txt` in the Tang-Build repository. The required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai` diff, confirmed that `core.md` and settled history are intact and that only this appended entry changed under `.ai/`, and validated the four-field header, six canonical sections, prose in Outcome and Next Steps, an allowed Status set, sequential numbering as entry 72 of the active log and a count within the 100-entry limit. No part of this repository, Tang-Build or Tang-Phosphor was found to use intellectual property beyond what `THIRD_PARTY.md` already records; `clock_monitor.sv` is this project's own work under GPL-3.0-only, and patch `0002` modifies GPL-3.0 nestang sources and marks its added regions `TinyTang:`.

#### Next Steps:

Ask the user what the screen shows with the monitor core loaded, since the display is the one result this cycle produced no evidence for, and record it in the next entry rather than here; the core is timing-clean at seed 17 but carries 1199 `clk` fabric fallbacks where the baseline had 943, exactly the 256 `clk`-domain flops the monitor adds, so the display should be compared against the lined baseline rather than assumed. The instrument itself needs no more: `fmon` reports rate and probe agreement, and a future measurement can put a second probe pair anywhere by instantiating another counter, since nothing is pinned. `desktop.bin` on the card was deliberately left as the lined baseline and the monitor image is `/cores/console138k/desktop-monitor.90d3fa3a84fa.bin`, so a power cycle returns to the known state. The uncommitted `ports/bl616/phosphor/pmod_sockets.cpp` socket-retry fix and the pre-existing dirty `THIRD_PARTY.md` and `tinydesk` submodule were left alone as they belong to other work.

#### Files Modified:

- fpga/desktop/clock_monitor.sv
- fpga/desktop/desktop_regs.sv
- fpga/desktop/desktop.cst
- fpga/desktop/build.tcl
- ports/bl616/tang_fpga_uart.c
- third_party/patches/desktop/0002-clock-monitor.patch
- third_party/patches/README.md
- tools/build_desktop_core.sh
- tools/tests/sim/tb_desktop_uart.sv
- tools/tests/test_desktop_core.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 73 COMMIT Unreleased 2026-10-10T10:19:40-07:00

#### Coming From:

Unreleased ea72e0f

#### Purpose:

Record the user's acceptance of the clock-monitor core image across repeated power cycles, which entry 72 left as `User Test: NOT RUN` and which the display half of that work had no evidence for.

#### Outcome:

The user reports the VGA coming up perfectly on four consecutive power cycles with the monitor core booting from the card, so the image `desktop.90d3fa3a84fa.bin` is accepted as stable across the boots tested and the display result entry 72 could not report is now on the record. Nothing in this repository changed in this cycle, which is why Files Modified is `None.` -- the image was built by the Tang-Build repository's open flow from these sources, and that repository's entries 58, 59 and 60 carry the build configuration, the placement, the clock measurements and the boot-by-boot tally, with the raw evidence in its `evidence/clock-monitor.txt`. The one board change was made there: `/cores/console138k/desktop.bin` was set to the monitor image so `/scripts/boot.tdsh` boots it, with the lined baseline preserved as `desktop-vga-baseline.bin`. Each boot was tied to the bitstream by two independent reads of the monitor's stamp rather than by file size, `fpgar 134` returning `0x4d4f4e31` with status 0 on all four, and the counters were re-read every boot without moving, `clk` between 21.5020 and 21.5053 MHz and `hclk` between 74.2496 and 74.2501 MHz, both probes agreeing inside 0.02 percent. One finding narrows entry 72's account of why this image renders where the board's earlier baseline did not: entry 59 had offered `clk` skew as the lead, and measuring the only other timing-clean placement refutes it as a sufficient explanation, because seed 19 routes at 4.657 ns of `clk` skew, above the lined baseline's 4.389, while seed 17's 4.077 renders perfectly, so skew does not order the outcome and the mechanism is still unknown. The limits are the same ones the Tang-Build record states: four boots on one board with one monitor is a bar rather than a proof, and the seed-19 discriminator that would separate a skew threshold from an unrelated cause is built and deliberately unrun. Entry 72 keeps `User Test: NOT RUN` because settled entries are not rewritten and the syntax rules require later results to be recorded in a new entry, which this is. The required core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai` diff, confirmed that `core.md` and settled history are intact and that only this appended entry changed under `.ai/`, and validated the four-field header, six canonical sections, prose in Outcome and Next Steps, an allowed Status set, sequential numbering as entry 73 of the active log and a count within the 100-entry limit. No part of this repository, Tang-Build or Tang-Phosphor was found to use intellectual property beyond what `THIRD_PARTY.md` already records.

#### Next Steps:

Run the seed-19 discriminator in the Tang-Build repository and record which reading it supports, because a skew threshold below about 4.2 ns predicts it lined while an unrelated cause predicts it perfect, and that answer decides whether a placement can be screened by `tools/clock-skew-report.py` before it is staged or whether the mechanism still has to be found. Do not re-measure `clk` skew as an explanation for the vertical lines without that result, since the three clean placements now on record do not order by it. Whether this image stays the card's boot default is the user's call and is currently yes, and reverting to the lined baseline is one `tangput`. The active log's numbering remains non-conforming from an earlier rollover and a mechanical renumber is the fix.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: PASS

---
