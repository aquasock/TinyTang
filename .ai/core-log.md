## 1 COMMIT Unreleased 2026-10-18T19:02:48-07:00

#### Coming From:

Unreleased 316b93d

#### Purpose:

Open a new active log with the live project state, after the user directed the rollover of the previous log at 96 of its 100 entries.

#### Outcome:

At the user's request the previous active log, entries 1 to 96 from 2026-10-10 to 2026-10-18, was archived as `.ai/archived_logs/core-log_2026-10-18T190225-0700.tar.gz`, a single-member GNU tar.gz holding `core-log.md` in the shape of the two archives beside it, verified by extraction to be byte-identical to both the working tree and the committed object at `316b93d`, and `.ai/core-log.md` was cleared and opened with this entry; `.ai/core.md` says the archives are not to be consulted without the user's approval, so the state needed to continue is set out here. TinyTang `v0.1.0` is released as a pre-release on `2c2a69c`, and `main` has moved well past it. The board runs BL616 firmware `462a85e-dirty.d34aec6` on Bouffalo SDK 2.3.36, built with the Xuantie-900 GCC 10.2.0 toolchain and installed with `tools/tinytang_flash.py`, whose `-dirty` suffix is the two TinyDesk patches `scripts/apply-tinydesk-patches.sh` applies to `third_party/tinydesk` at every configure; the firmware's PMOD I2C command is `pmodi2c` (`scan`, `read`, `write`, `rtc`, `auto`), renamed from `i2c`, which TinyDesk Shell's STANDARDS.md reserves. The card's `/root/cores/console138k/desktop.bin` is the desktop core of record, seed 3 of desktop ABI 1.3, `desktop.24a11e81a46c.bin` (`desktop.fs` `d993899c24a4`), built by `tools/build_desktop_core_open.sh` at its default seed with nothing overridden and reproduced bit for bit, against apicula `04bc3c5` and nextpnr `8be8657b` on the aquasock forks' `epic/gw5ast138c` branches and the chipdb `aecb123e44cd` that `deps.conf` pins; its `clk` runs through SPINE18 and its pixel clock on global resources only, and it drives HDMI, VGA, the OLED and the PMOD I2C bus, with the previous seed-26 core kept on the card as `desktop.bin.abi12-s26` and rebuilt from commit `97cf0db`. The open-flow fault that had killed every core whose `clk` took SPINE17, SPINE18 or SPINE19 was the spine select ports in the logic tiles at rows 27/81, columns 92 to 94: they must hold VSS, apicula's table asked for VCC, which the vendor's `.fse` cannot express, and the placer could put logic in those tiles; TOOL-021 records it, `tools/desktop_open/clock_route_decode.py` decodes a bitstream's clock pips and select ports from the vendor's own tables, SPINE17 stays dropped in the chipdb and refused at the build's step 2d, and the fault is still present in upstream apicula. The desktop core accepts none or OLEDrgb in each socket, I2C in one socket beside none or OLEDrgb, or the PmodVGA pair, and refuses any other word; the I2C bus runs SCL on socket pins 3/9 and SDA on 4/10 at 100 kHz with no FPGA pull-up (PMOD-007), the Adafruit DS3231 on a STEMMA QT cable reads and writes from either socket, and the Digilent Pmod RTCC needs external pull-ups. The card's `/tang.ini` declares `pmod0 = oledrgb`, `pmod1 = i2c` (`0x0610`), with `/tang.ini.bak-pmod0-i2c` (`0x0160`) and `/tang.ini.bak-i2c` beside it, and the DS3231 is set to local time with its oscillator-stop flag cleared. A Gowin EDA 1.9.11.03 build of the same netlist is kept on the card as `desktop-i2c-pins-gowin.bin` for comparison only, the open flow being the project's flow. Board work in a test round waits for the user's go, one board step per call, and every console send goes through `tools/tinytang_console.py`'s guard, which refuses while the desktop holds the console. The repository is at `316b93d` before this entry and clean apart from the expected `third_party/tinydesk` patch changes. Nothing was built, deployed or tested. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed `.ai/core.md` was not changed and that the only removal from the log is the archived content, and validated this entry as entry 1 of the new active log with six canonical sections, one-sentence Purpose, prose, an allowed Status set and a count of one within the 100-entry limit.

#### Next Steps:

The open items, each the user's to schedule: a hot-board test of the new `desktop.bin`, whose seed-26 predecessor was held clean at 67.3 C; re-admitting SPINE17 and the bridge pairs the chipdb still drops as measured-dead, one at a time with a core that uses each and a board test, and retiring the step-2d refusal if SPINE17 runs, SPINE20, SPINE21 and SPINE23 having no board evidence yet; reporting the select-polarity fault to upstream apicula; false-path constraints for the socket word's crossings into the I2C synchronizers, which the Gowin comparison build flags; external pull-ups for the Digilent Pmod RTCC; the desktop clock taking its time from the DS3231 through `pmodi2c auto on`; and the longer-standing items, which are the TinyDesk patches that keep the firmware identity `-dirty`, the firmware image not being byte-reproducible, `tools/fetch-deps.sh --check` looking for fork checkouts at release-root paths this machine does not use, the stale heap records BL6-002, BL6-004 and BLE-014, and whether `v0.1.0` stays a pre-release.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 2 COMMIT Unreleased 2026-10-18T19:28:02-07:00

#### Coming From:

Unreleased 793be24

#### Purpose:

Give the desktop a wall clock from the PMOD I2C RTC, keeping UTC in the DS3231 and the time zone in TinyDesk's own setting, as the user approved.

#### Outcome:

The new `ports/bl616/tang_clock.c` and `tang_clock.h` keep the desktop's wall clock as the DS3231's UTC plus an offset from `bflb_mtimer_get_time_ms()`: the RTC is read when a core with an I2C socket has been declared and then about hourly, with a one-minute backoff after a refused read, never while a host pipe or `ae350run` owns the link, OSF set or registers that are not a date count as no valid time, and with no RTC ever read `time_now` reports the clock unset as before rather than invent a date. The logic is hardware-free and reaches the RTC through four hooks that `ports/bl616/tang_i2c.cpp` implements on the board. `td_desktop_bl616.c` now supplies TinyDesk's `time_now` and `time_set`, so the taskbar clock and the Date & time window's root-only manual set work, the zone staying in `/tinydesk_tz`; `phosphor/pmod_sockets.cpp` records whether the loaded core is the desktop core with an I2C socket declared, clears it in a new `tang_ini_core_replacing()` that `tangload` calls before every load, and tells the clock after every load and `tangini apply`. `pmodi2c` gained `rtc set YYYY-MM-DD HH:MM:SS` (UTC) and `rtc sync`, and `pmodi2c rtc` now prints the RTC in UTC and the clock in UTC and local time; every `pmodi2c` command and clock transaction holds one recursive bus lock, a failed transaction no longer leaves the once-a-second auto-read off, and the three unused-variable warnings entry 93 of the previous log named are gone. The new `tools/tests/test_clock.sh` runs the real clock against a modelled DS3231 in 35,930 checks, with expected dates from Python's `datetime`, and failed on each of four planted faults (the weekday, the OSF check, the backoff and the hourly resync); `tools/tests/tb_tang_flash.c` gained the new hook and now asserts the socket state is cleared before every load, failed ones included; the other firmware host tests pass, and `test_oled_session.sh` fails at the link step exactly as it did before this change. The first board image, `793be24-dirty.d7e1342`, showed the RTC correct and the clock wrong by one byte per field: the engine's result window is always 8 bytes, `transaction()` copied all 8 into the caller's buffer, and the clock's 1-byte status read overwrote the 7 time bytes beside it on the stack, a fault the host model could not see because it copies exactly what is asked for; `transaction()` now copies only the requested length. The fixed firmware, `793be24-dirty.e6bb18f`, 632,368 bytes, SHA-256 `5412753975e9c1d8a6520372d538fca27d82419a8dd0e87c4140a1587ed034ff`, built with no warning in the changed files, was installed with `tools/tinytang_flash.py --yes` and confirmed by `platform` after the user's power cycle; the clock then matched the RTC, `pmodi2c rtc set` moved the DS3231 from the Phoenix local time it held to UTC, the user set the zone to UTC-7 in the Date & time window, which wrote `-25200` to `/tinydesk_tz`, and after a reboot the user confirmed the desktop's time correct while the console read the RTC at `2026-10-19 02:27:20 UTC`, Monday, OSF clear, and local time `2026-10-18 19:27:19` against the host's 19:27:25. Twice the board came up after a user reboot with no USB device at all and was back once the console cable was reconnected. The README describes `pmodi2c` and the desktop clock and now lists `i2c` among the `/tang.ini` modules the desktop core takes, and BL6-005 and PMOD-007 in `.ai/core-reference.md` were revised in place with a routing row. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed `.ai/core.md` was not changed and no settled entry was rewritten, checked that the reference's six YAML blocks parse with the index and records naming the same 144 ids, and validated this entry's sequential number, six sections, one-sentence Purpose, prose, Status values and the two-entry count against the 100-entry limit.

#### Next Steps:

The clock needs no further work to be useful. File timestamps on the card stay at the SDK's fixed date until `FF_FS_NORTC` is turned off and a `get_fattime()` supplied, which means changing the SDK's own `ffconf.h` and is a decision before it is a task; a shell `date` command would need TinyDesk Shell upstream or a prefixed port command; and the MCP79410 on the Digilent Pmod RTCC can be supported once that module has pull-ups. The open items of entry 1 stand otherwise, the user running the hot-board test of the new `desktop.bin` independently.

#### Files Modified:

- README.md
- ports/bl616/phosphor/pmod_sockets.cpp
- ports/bl616/tang_clock.c
- ports/bl616/tang_clock.h
- ports/bl616/tang_i2c.cpp
- ports/bl616/td_desktop_bl616.c
- ports/bl616/tdsh_tang_flash.c
- tools/tests/tb_clock.c
- tools/tests/tb_tang_flash.c
- tools/tests/test_clock.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 3 COMMIT Unreleased 2026-10-19T14:19:57-07:00

#### Coming From:

Unreleased 8a155c9

#### Purpose:

Give one PMOD socket a UART beside the desktop core's I2C bus, so the RTC and an RYLR998 LoRa module share the socket, as the user approved.

#### Outcome:

The approved cycle added desktop personality 7 (`i2c_uart`, desktop ABI 1.4): the I2C engine stays on the upper row (SCL pin 3, SDA pin 4) and an 8N1 UART at 115200 takes the lower row (NRST pin 8, TX pin 9, RX pin 10). `fpga/desktop/pmod_uart.sv` holds the engine: 50 MHz divided by 434 (0.007 % from exact), 256-byte transmit and 512-byte receive FIFOs, held-data toggles between the control and crystal clocks, and an internal loopback; `desktop_pmod.sv` selects it for personality 7 and leaves personality 6 untouched, so an I2C Pmod that ties its two rows together is never exposed to a UART on the same socket. `desktop_regs.sv` gained the window at 0x170 (write a byte), 0x174 (read a byte, popping it), 0x178 (status, counts at 19:8 and 31:20) and 0x17c (bit 0 NRST low, bit 1 flush, bit 2 loopback, bit 3 hold), `supported_word` accepts 7 beside none or OLEDrgb with one I2C-engine socket, the personality bitmap reads 0x000000cf, and the ABI reads 0x00010004. The firmware gained `i2c_uart` in `tang_ini.c`, counts personality 7 as an I2C-declared socket for the desktop clock in `pmod_sockets.cpp`, and adds `ports/bl616/tang_lora.cpp` with `pmodlora at|recv|reset|loopback|flush|status`, registered from `tdsh_platform_bl616.c` and built from CMakeLists; `docs/tang.ini`, the two READMEs and the desktop register table describe the layout. The host regressions passed: the PMOD testbench checks personality 7 on both sockets (I2C scan finding 0x68 on pins 3/4, UART bit timing on 9/10, NRST on 8, the hold-and-burst behaviour and internal loopback), the desktop UART test covers the new registers and the 0xcf bitmap, and `test_ini.sh`, `check_help_width.py` and `test_tang_flash.sh` pass. `tools/sweep_desktop_core_open.sh` built seeds 0 to 7 in parallel; all eight passed setup/hold, the clock-topology comparison and the SPINE17 hop check, with `clk` on global resources only at 73.32 to 84.91 MHz against 21.49 MHz required. Firmware 8a155c9-dirty.ab068ab (636,496 bytes) was flashed and the board power-cycled; `/tang.ini` declares pmod0 oledrgb, pmod1 i2c_uart (0x0710), and the seed-3 core desktop.fe5289f5e446.bin (4,497,792 bytes) was installed as /root/cores/console138k/desktop.bin with the previous core kept as desktop.bin.abi13. The first board image answered +ERR=1 once per byte to every AT command: the driver wrote each byte through a separate register transaction, leaving the line idle 150 to 300 us between bytes, longer than a character at 115200, so the RYLR998's parser treated each byte as its own command; the UART control register's hold bit lets `pmodlora at` flush the FIFO, fill it with the transmitter held, and release it as one burst. After that fix the power-cycled board reported `platform` 8a155c9-dirty.ab068ab, `tangini` core 0x54 at ABI 1.4 with 0xc0 = 0x0710, the DS3231 at 2026-10-19 21:17:43 UTC with OSF clear and local 14:17:43, and the RYLR998 at 115200 answering AT with +OK, AT+ADDRESS? with +ADDRESS=0, AT+BAND? with +BAND=915000000, AT+NETWORKID? with +NETWORKID=18, AT+PARAMETER? with +PARAMETER=9,7,1,12, AT+VER? with +VER=RYLR998_REYAX_V1.2.4 and AT+CRFOP? with +CRFOP=22, with `pmodlora status` at 0 and no receive overrun; the user accepted the result. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed `.ai/core.md` was not changed and no settled entry was rewritten, checked that the reference's YAML blocks parse with the index and records naming the same ids, and validated this entry's sequential number, six sections, one-sentence Purpose, prose, Status values and the three-entry count against the 100-entry limit.

#### Next Steps:

The LoRa transport stops at a raw passthrough: `pmodlora send` and the RYLR998's configuration subcommands (address, network, band, parameters, power) are the next cycle, and the module's +22 dBm default is FCC-legal at 915 MHz but must be set to +14 dBm for CE. The desktop on HDMI, the keyboard, the OLED on PMOD0 and the desktop clock should be exercised on the new core if they were not part of the acceptance; the RTC's I2C is otherwise unchanged. The older open items stand: FatFS timestamps need FF_FS_NORTC off and get_fattime(), a shell date command needs upstream or a prefixed command, the MCP79410 waits on pull-ups, and the carried TinyDesk patches keep the firmware identity -dirty, the image is not byte-reproducible, tools/fetch-deps.sh --check looks for fork paths this machine does not use, BL6-002, BL6-004 and BLE-014 are stale, and v0.1.0 remains a pre-release.

#### Files Modified:

- CMakeLists.txt
- README.md
- docs/tang.ini
- fpga/desktop/README.md
- fpga/desktop/build.tcl
- fpga/desktop/desktop_link.v
- fpga/desktop/desktop_pmod.sv
- fpga/desktop/desktop_regs.sv
- fpga/desktop/desktop_top.sv
- fpga/desktop/pmod_uart.sv
- ports/bl616/phosphor/pmod_sockets.cpp
- ports/bl616/tang_ini.c
- ports/bl616/tang_ini.h
- ports/bl616/tang_lora.cpp
- ports/bl616/tdsh_platform_bl616.c
- tools/desktop_open/synth.ys
- tools/sweep_desktop_core_open.sh
- tools/tests/sim/tb_desktop_pmod.sv
- tools/tests/sim/tb_desktop_uart.sv
- tools/tests/tb_ini.c
- tools/tests/test_desktop_core.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 4 COMMIT Unreleased 2026-10-20T09:06:01-07:00

#### Coming From:

Unreleased 9758cec

#### Purpose:

Make the mouse useful at the shell prompt, so the wheel walks the shell's own command history and a left click runs the line, as the user approved.

#### Outcome:

The approved cycle made the mouse an input device for the console prompt. `ports/bl616/tang_osd_desk.c` gained `console_mouse()`, called from the console branch of `desk_poll()`: each wheel detent pushes `ESC [ A` or `ESC [ B` -- the shell's own Up and Down history keys -- and a left-click press pushes a carriage return, both into the ring `desk_in_push()` feeds, which `tdsh_bl616_input_read_byte()` drains ahead of the CDC, so the line editor cannot tell them from the keyboard; a spin is clamped to four detents per poll against the 192-byte ring, and both are sent only while the layer is enabled, the desktop is not running and `tdsh_bl616_console_state()` reports `TDSH_BL616_AT_PROMPT`, so a click during a command is dropped rather than queued. `tang_mouse_t` gained `pressed`, latched at report time in `tang_mouse_boot_add()` because a click is shorter than the 40 ms poll and a sampled level would miss it, and cleared in `tang_ble_mouse()` beside movement and wheel; the desktop's pointer path is untouched. The README describes the gesture and `scripts/boot.tdsh` prints a hint line. The history stays the shell's, in `~/.tdsh_history`, so a card with no history has nothing for the wheel to recall until a command has been typed once -- a limitation the user accepted in preference to the boot script writing into the user's history. `tools/tests/tb_osd_desk.c` grew console-mode checks (the wheel gives `ESC [ A`, wheel down gives `ESC [ B`, a nine-detent spin is clamped to four, a latched press gives a carriage return, nothing is pushed while `TDSH_BL616_BUSY`, and a press made then does not arrive at the next prompt) and `tools/tests/tb_pad.c` the latch (a press and a release inside one step still count, the level stays clear, a held button is one press); `test_osd_desk.sh` passed 88 checks and `test_pad.sh` 46, and each new group was shown to fail with the call, the prompt gate or the latch removed, while the rest of the host suite passed except the standing `test_oled_session.sh` link failure, in which nothing changed. Firmware `9758cec-dirty.75f797c`, 636,496 bytes, SHA-256 `76a2b8ddf003a1527b7a4d42aeea18169d580c30280af31f08b36a503fa973aa`, built with no warning in the changed files, was installed with `tools/tinytang_flash.py --yes` and confirmed by `platform` after the user's power cycle; the 4,740-byte `/root/scripts/boot.tdsh` was placed with `tools/tinytang_put.py`, and the user tested the wheel recall and the click-to-run and accepted the result. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed `.ai/core.md` was not changed and no settled entry was rewritten, and validated this entry's sequential number, six sections, one-sentence Purpose, prose, Status values and the four-entry count against the 100-entry limit.

#### Next Steps:

The console mouse is complete, but its vocabulary is all the shell has: the shell parses no mouse reports and its line editor has no clickable target, so anything beyond history and Enter would need an upstream change to TinyDesk Shell. The proposed `pmodlora send` and the RYLR998 configuration subcommands remain unapproved, and their documented ranges should be opened from the module's datasheet into `.ai/core-reference.md` before any code enforces them. The desktop on HDMI, the keyboard, the OLED on PMOD0 and the desktop clock should be re-exercised on this firmware if this test did not cover them. The older items stand: FatFS timestamps need `FF_FS_NORTC` off and a `get_fattime()`, a shell `date` needs upstream or a prefixed command, the MCP79410 waits on external pull-ups, the carried TinyDesk patches keep the firmware identity `-dirty`, the image is not byte-reproducible, `tools/fetch-deps.sh --check` looks for fork paths this machine does not use, BL6-002, BL6-004 and BLE-014 are stale, `v0.1.0` remains a pre-release, and `tools/tests/test_fat_rename.sh` lacks the executable bit.

#### Files Modified:

- README.md
- ports/bl616/tang_ble.c
- ports/bl616/tang_osd_desk.c
- ports/bl616/tang_osd_desk.h
- ports/bl616/tang_pad.c
- ports/bl616/tang_pad.h
- scripts/boot.tdsh
- tools/tests/tb_osd_desk.c
- tools/tests/tb_pad.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 5 COMMIT Unreleased 2026-10-20T10:40:13-07:00

#### Coming From:

Unreleased 78fa8cc

#### Purpose:

Add a third Bluetooth slot for one device that is both keyboard and mouse, so a combo keyboard with a touchpad can drive the board beside the dedicated slots, as the user approved.

#### Outcome:

The approved cycle added the combo slot. `ports/bl616/tang_ble_bonds.h` moved the pairing file to version 2 with three slots and taught `tang_bonds_decode()` to accept a version 1 file into slots 0 and 1, so the K950 and M750 pairings survived the change; `hid_dev_t` carries a keyboard and a mouse value handle instead of one boot handle, each slot keeps its own keyboard report and mouse state, and `HID_COMBO` (`blecombo pair|watch|off|on|forget`) subscribes to both boot inputs and the report characteristic. `tang_ble_keyboard()` and `tang_ble_mouse()` choose a source per role -- a dedicated slot wins, the combo fills what is left -- and a poll where the source changes emits one neutral report, so a key or button held on the device being left is released rather than carried across. The Bluetooth window grew a third row at the cost of five rows of height, and `tb_bluetooth_app` moved the scan's Pair button to row 30. Pairing the user's Rii K06 then showed the device accepts boot protocol but never notifies on its boot inputs, sending an 8-byte keyboard boot report on report characteristic 0x001E and a 4-byte mouse report on 0x002C; since boot protocol defines exactly those two formats and no others, the plan was reduced with the user's approval to routing the combo slot by format (8 bytes keyboard, 3 or 4 mouse) in boot mode, without the descriptor and report-map parser that was proposed for report-protocol-only devices. `tb_bonds` passed 20 checks including the version 1 migration, `tb_bluetooth_app`, `test_osd_desk.sh` (88), `test_pad.sh` (46), `test_key.sh`, `test_ini.sh`, `test_script_limits.sh`, `test_terminal_lifecycle.sh` and `test_editor_limits.sh` passed, and `tools/check_help_width.py` found no entry over 80 columns. Firmware `78fa8cc-dirty.0e89091`, 636,512 bytes, SHA-256 `c0050893c749e93a363a822de372479a3481107793e43952e22d16e8bbd9d81d`, was installed with `tools/tinytang_flash.py --yes` and confirmed by `platform` after the user's power cycle; the bond file decoded, the K06 reconnected by itself as `blecombo: ready, ... boot protocol`, and the user accepted the result: keys at the console, the desktop pointer and its buttons, the wheel-and-click history gesture at the prompt, and typing into a desktop window. `.ai/core-reference.md` gained BLE-019 for the device's HID layout and its boot reports on report characteristics, and BLE-007 was revised in place because it had recorded the K06 as Classic-only and unusable while the unit pairs and reports over LE; both were reported to the user. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed `.ai/core.md` was not changed and no settled entry was rewritten, checked that the reference's six YAML blocks parse with the index and records naming the same 146 ids, and validated this entry's sequential number, six sections, one-sentence Purpose, prose, Status values and the five-entry count against the 100-entry limit.

#### Next Steps:

The combo slot works for a device that takes boot protocol; a device that refuses it is paired and logged but its reports are not decoded, and the descriptor capture with a streaming report-map parser remains the work to do when such a device appears. The K06's consumer and system reports, 2 and 1 bytes, are also undecoded, so its media keys do nothing. The Bluetooth window is now 34 rows tall and should be checked on a shorter display if one is ever used. The proposed `pmodlora send` and the RYLR998 configuration subcommands remain unapproved, and their documented ranges should be opened from the module's datasheet into `.ai/core-reference.md` before code enforces them. The older items stand: FatFS timestamps need `FF_FS_NORTC` off and a `get_fattime()`, a shell `date` needs upstream or a prefixed command, the MCP79410 waits on external pull-ups, the carried TinyDesk patches keep the firmware identity `-dirty`, the image is not byte-reproducible, `tools/fetch-deps.sh --check` looks for fork paths this machine does not use, BL6-002, BL6-004 and BLE-014 are stale, `v0.1.0` remains a pre-release, and `tools/tests/test_fat_rename.sh` lacks the executable bit.

#### Files Modified:

- README.md
- ports/bl616/tang_ble.c
- ports/bl616/tang_ble.h
- ports/bl616/tang_ble_bonds.c
- ports/bl616/tang_ble_bonds.h
- ports/bl616/td_bluetooth_app.c
- tools/tests/tb_bluetooth_app.c
- tools/tests/tb_bonds.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 6 COMMIT Unreleased 2026-10-20T21:21:38-07:00

#### Coming From:

Unreleased 2d4a872

#### Purpose:

Record the verified Tang Console J5 battery pinout and onboard 1S charging path after the user's bench and battery tests.

#### Outcome:

BRD-010 now records the 32001C schematic's J5 nets, one-series 3.7 V Li-ion and 4.2 V maximum, TP4057 charger path, the official bottom-silkscreen erratum, and the connector-face mapping established on this board: looking into J5, GND is top-left, +BATT top-right and the two differential speaker contacts occupy the bottom row. The user first demonstrated why the footprint cannot be transferred directly to the right-angle socket face: 4.00 V on the wrong contact pair drew only 0.037 A, left PMOD power at 1.28 V and did not configure the FPGA, while the corrected pair booted immediately into two-wire mode at 0.343 A idle. A partly discharged 3.6 V 18650 then ran the Tang, and ordinary USB power with the cell attached engaged the onboard charger. The no-cell 4.37 V handheld-meter reading at the internal BAT pad is retained only as an open-terminal observation; the schematic's 4.2 V maximum remains the recorded limit, and the consequence explicitly forbids a 2S pack. The routing table points battery questions to BRD-010 and the reference review date is current. No firmware build or deployment applied to this documentation-only cycle. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed `.ai/core.md` was not changed and no settled log entry was rewritten, and validated BRD-010's schema plus this entry's sequential number, six sections, one-sentence Purpose, prose, Status values and the six-entry count against the 100-entry limit.

#### Next Steps:

Use a protected 1S harness keyed to the verified socket face, retain the 4.2 V maximum at J5, and monitor the first complete charge for actual cell voltage and temperature; no project implementation work remains from this reference update.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 7 COMMIT Unreleased 2026-10-20T22:17:50-07:00

#### Coming From:

Unreleased f44aeca

#### Purpose:

Begin an open-flow battery-voltage query path by measuring and adding the GW5AST-138C ADC voltage-mode configuration, stopping before any board deployment as the user directed.

#### Outcome:

The Tang Console schematic and package data put the divided battery voltage on the dedicated ADCV pair, FPGA balls L10 and M9, and the existing aquasock Apicula and nextpnr forks already model the 138C's lower-right `ADCLRC` block, its fabric ports, placement and routing, but Apicula deliberately refused `ADC_MODE=1` because only `DIV_CTL` had a complete measured fuse attribution. Two otherwise identical Gowin EDA 1.9.11.03 reference builds swept the full binary `ADC_MODE` axis with `VSENCTL=0`: voltage mode set exactly two additional fuses in tile R109C182, local coordinates `(20,14)` and `(20,45)`, which are code 56 in the die's `unknown_138` and `unknown_137` tables; temperature mode set neither. The aquasock Apicula fork now attributes that field, continues to refuse every unswept parameter, carries both sweep points and regression coverage, and passed 35 focused ADC and shape tests; commit `8918f14` (`GW5AST-138C: pack ADC voltage mode`) was pushed to `aquasock/apicula` branch `epic/gw5ast138c`. A regenerated local device database had SHA-256 `4b257561ac40e82524bfe27fb4d25ab3edaeb95b21f252295a7c88e0f5558f`, but it is generated and uncommitted. TinyTang's dependency pin, nextpnr database, FPGA RTL, register ABI and BL616 firmware were not changed, no TinyTang desktop core or firmware was built, and nothing was copied to, flashed on or loaded by the board.

#### Next Steps:

Resume from Apicula `8918f14`: update TinyTang's dependency pin, regenerate the GW5AST-138C database and rebuild nextpnr against it, prove a minimal `ADCLRC` voltage-mode design completes synthesis, place-and-route and packing entirely through the open flow, then add a conversion controller to the desktop core, expose raw value and status through an unused register such as `0x138`, add a calibrated `battery` firmware command and regressions, and build a candidate open-flow desktop core; do not deploy or load that candidate until the user explicitly authorizes board deployment.

#### Files Modified:

None.

#### Status:

- Build: NOT RUN
- Deployment: NOT RUN
- User Test: NOT RUN

---

## 8 COMMIT Unreleased 2026-10-21T06:16:16-07:00

#### Coming From:

Unreleased bc65b09

#### Purpose:

Make the Phosphor window a two-tab player whose Load button loads the core from the desktop, and give the XY scope the controls only a shell `phosphor poke` could reach.

#### Outcome:

`ports/bl616/phosphor/td_phosphor_app.cpp` was rebuilt on the TinyNES Control Deck's page arrangement, since TinyDesk has no tab widget: a tab row at the top, two pages toggled with `td_widget_set_visible`, and bookkeeping reset per window so a reopened window cannot exhaust the page arrays. The Player tab keeps the library, transport, progress, auto-advance and status line, and adds a Load button; the Scope tab carries an Enable checkbox, four Trail buttons (Short, Medium, Long, Brief, mapped to `0xac` bits 2:1 as 0, 1, 2 and 3), a Glow checkbox at bit 3, a status line decoded from `0xb4` (bits 0 enabled, 1 initialised, 2 drawing, 3 loading, 4 cache ready, 16:8 FIFO level, 23:20 active settings) and the sweep and dropped-sample counters from `0xb8` and `0xb0`. Every write is read back through `0xac` so the page shows what the core took, and the counters are polled only while the Scope tab is visible, leaving the Player tab's link traffic unchanged. The first board build exposed the Load failure the user had reported: with the desktop open, `tdsh run /root/scripts/phosphor.tdsh` refused to load the script and printed nothing at all. Updating the carried shell from v0.1.5 (`a067fa7`) to v0.1.6 (`e85ad85`) made the failure say so -- v0.1.6's `load_script()` prints `uscript: <file>:<line>: not enough memory to load the script`, upstream issue #2, where v0.1.5 returned status 2 in silence -- and the user reproduced exactly that message, confirming the desktop's heap cannot always supply a 16 KB worker task or the script's line table even with the `tang_heap.c` fallbacks. Load therefore no longer goes through the shell: `ports/bl616/tang_core_load.h` declares `tang_core_load()`, implemented in `ports/bl616/tdsh_tang_flash.c` beside `cmd_tangload`, which now calls it, so the whole sequence -- busy guard, `tang_phosphor_core_replacing`, `tang_oled_core_replacing`, `tang_ini_core_replacing`, `fpga_program`, then `tang_osd_desk_core_reloaded`, `tang_ini_core_loaded` and `tang_oled_core_loaded` -- exists once and the shell keeps its distinct "another tangload is running" message through a three-value result. The button calls it in-process on `/sd/root/cores/console138k/tinyphosphor.bin`, then calls `tang_osd_set(false)` to hand the screen to the scope as the script's `osd off` did, and reports the core's answer. `scripts/phosphor.tdsh` now loads the stable `/root/cores/console138k/tinyphosphor.bin` instead of the retired Tang-Phosphor image, keeping its `0xac 0xb` default look for the icon and shell paths, which the Scope tab reads back. Firmware `bc65b09-dirty.af91058`, 640,656 bytes, SHA-256 `dd580d4b3fc8fdeb8f8f4fd0ff9549fbc845c53b84221ad13f9e16027dad7610`, built with no warning in the changed files, was installed with `tools/tinytang_flash.py --yes` and confirmed by `platform` after the user's power cycle; `tools/tests/test_tang_flash.sh` passed, so the refactored `tangload` still clears the socket state before every load. The user pressed Load from the open desktop, watched it load with no Terminal hop, switched tabs, changed trails and glow, and reported PASS. `.ai/core-reference.md` revised TDSH-004's pin to v0.1.6 with the new message and BL6-010 with the recurrence and the in-process path, and corrected TDESK-013's claim that both projects pin the same shell revision; the README describes the two tabs and the Load path. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed `.ai/core.md` was not changed and no settled entry was rewritten, and validated this entry as number 8 of 100 with exactly six sections and allowed Status values.

#### Next Steps:

The Scope tab's Trail and Glow settings are written but not yet exercised on the hardware as a distinct check; the user changed them during the acceptance, so that stands as one observation rather than three. Trails and glow are still unreachable from TinyDesk outside this app. The desktop's heap can still refuse a script, so any other desktop-launched `.tdsh` -- the Phosphor icon included -- can fail the same way; either those paths move in-process as Load did, or the heap accounting gets a record of what the desktop holds. The carried TinyDesk patches still keep the firmware identity `-dirty`, and the older open items stand: FatFS timestamps need `FF_FS_NORTC` off and a `get_fattime()`, the MCP79410 waits on external pull-ups, `tools/fetch-deps.sh --check` looks for fork paths this machine does not use, and `v0.1.0` remains a pre-release.

#### Files Modified:

- CMakeLists.txt
- README.md
- deps.conf
- ports/bl616/phosphor/td_phosphor_app.cpp
- ports/bl616/tang_core_load.h
- ports/bl616/tdsh_tang_flash.c
- scripts/phosphor.tdsh
- third_party/tinydesk-shell

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 9 COMMIT Unreleased 2026-10-21T10:06:37-07:00

#### Coming From:

Unreleased 6843692

#### Purpose:

Turn the Scope tab into a working control surface for the core's ABI 1.14 register and fix the three faults the first board test exposed.

#### Outcome:

The Scope tab now writes and reads the 16-bit control word at `0xac`: the **Persistence (Afterglow)** row offers 67, 133, 267 and 533 ms (assigned to codes 3, 0, 1 and 2, since the codes are a fade-rate class and not in duration order), plus **Swap X/Y**, **Dots**, a **Channels** row of XY, X and Y, and **Bloom**; the fade that the core calls instant was dropped on the user's direction after the first test, because with no plane brightness the picture is blank, and a **freeze** control was built, tested, blanked the screen on every tick, and was removed at the user's direction. Three faults from that test are fixed. The readback mask in `sync_scope()` was `control &= 0xf`, so freeze, swap, dots, bloom and the channel select always read as off; every four-second sync unticked them and the next write cleared them in the core, which is why the effects applied and the boxes never showed it -- the mask is now `0xffff`, and the page invalidates after a sync so the glyphs redraw. The `Freeze` and `Swap X/Y` captions were clipped because the checkboxes sat nine columns apart while the widgets are wider, so the row is now spaced by caption width. `scripts/phosphor.tdsh` writes `0x13` (enable, 267 ms, bloom) instead of the old `0xb`, which in ABI 1.14 would have meant instant, and the README describes the tab as it now is. Firmware `6843692-dirty.34f401b`, 640,656 bytes, was installed with `tools/tinytang_flash.py --yes`; the first board image of the cycle exposed the readback mask and the clipped captions, and the user reported PASS for the rebuilt one: persistence, swap, dots, channels and bloom all take effect and their controls hold their state, with no freeze control left. The core deployed beside it is TinyPhosphor seed 7 of entry 19, `e9460424…`, which is the image these controls were accepted against. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed `.ai/core.md` was not changed and no settled entry was rewritten, and validated this entry as number 9 of 100 with exactly six sections and allowed Status values.

#### Next Steps:

Colour is deferred in TinyPhosphor until its palette multiplexer stops moving the bus clock, and bit 5 of `0xac` stays reserved, so the app has no control for either and needs none. The tab's rows are not exercised at the window's minimum height, which is now 20 rows, and Swap, Dots and the two single-channel views were accepted as one observation rather than as separate checks. The older items stand: the desktop's heap can still refuse a script launched from the desktop, the carried TinyDesk patches keep the firmware identity `-dirty`, FatFS timestamps need `FF_FS_NORTC` off and a `get_fattime()`, the MCP79410 waits on external pull-ups, `tools/fetch-deps.sh --check` looks for fork paths this machine does not use, and `v0.1.0` remains a pre-release.

#### Files Modified:

- README.md
- ports/bl616/phosphor/td_phosphor_app.cpp
- scripts/phosphor.tdsh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 10 COMMIT Unreleased 2026-10-21T15:23:04-07:00

#### Coming From:

Unreleased ca48302

#### Purpose:

Fold the cartridge browser and the settings window into one TinyNES app, with the browser as its first tab.

#### Outcome:

`ports/bl616/tinynes_app.cpp` now holds the cartridge browser as its **Library** page and the settings as Input, Video and Saves, so the desktop has one TinyNES entry instead of two. `TABS` went from 3 to 4 and `PAGE_WIDGETS` from 12 to 16, the window from 66x22 to 66x26 with a 18-row minimum, and the title from "TinyNES Control Deck" to "TinyNES"; the tab row is spaced by caption width because "Library" is wider than the three tabs after it and the old nine-column pitch would have run the labels together. The browser is the launcher's code moved rather than rewritten -- the same path label, list, Launch, Up and Refresh, the same 128-byte guard before handing a path to the shell, and the same `FILE=...; tdsh run /root/scripts/open/nes.tdsh` handler, so a launch still reloads the core, which the window survives because it lives on the BL616. Its symbols carry a `lib_` prefix so they cannot be confused with the pages beside it, its list is freed in the shared `on_close`, and it reports through the Library page's own status label rather than a second message line; the existing pages shifted one place right and the status row moved from 16 to 17 to sit under the new buttons. `ports/bl616/tinynes_launcher.cpp` is deleted, along with its registration in `td_desktop_bl616.c` and its entry in `CMakeLists.txt`. The user's choices fixed the shape: the tab is called Library, the window opens on it, and both the second window and its menu entry are gone. Firmware `ca48302-dirty.b44b9d4` was installed with `tools/tinytang_flash.py --yes`, and after a power cycle the user opened the window and reported PASS for all of it: four tabs opening on Library, the list browsing `/root/media/nes` with folders, `..` and Refresh, Launch starting the highlighted game, the status line following each tab, Input, Video and Saves still behaving, Save still writing `/nes.ini`, and one TinyNES entry in the start menu. TinyNES's README, which documents the window, was updated to name the Library tab rather than a separate launcher. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed `.ai/core.md` was not changed and no settled entry was rewritten, and validated this entry as number 10 of 100 with exactly six sections and allowed Status values.

#### Next Steps:

The merged window is the place the TinyNES desktop work continues, so anything that used to say "the Launcher" in this project's records now means its Library tab. The library's ceiling is 256 entries and 56-byte names, which TinyNES's reference records as this project's own limits rather than FAT's; a folder past it says so instead of hiding files. Nothing else in the merge is open: the pages, the settings file and the launch path are unchanged.

#### Files Modified:

- CMakeLists.txt
- ports/bl616/td_desktop_bl616.c
- ports/bl616/tinynes_app.cpp
- ports/bl616/tinynes_launcher.cpp

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 11 COMMIT Unreleased 2026-10-21T15:53:37-07:00

#### Coming From:

Unreleased b744811

#### Purpose:

Add a Manual app to the desktop, so the board's own documentation can be read on the board.

#### Outcome:

`ports/bl616/td_manual_app.cpp` is a reader for Markdown chapters on the card: `index.md` lists them one per line as `- [Title](file.md)`, the window opens on that contents list, and choosing an entry opens the chapter one line per desktop row with `#` and `##` headings in the accent colour and the `**bold**`, backtick and `[text](link)` markers stripped. A chapter is read whole into a 12 KB buffer and split into lines there, so a chapter costs a few kilobytes rather than the editor's 16 KB ceiling, and drawing only copies from memory. The Markdown is a subset on purpose: the desktop draws 80x45 character cells and has no pixel path, so headings are colour, diagrams are ASCII, and links are live only in the contents list. `manual/` holds the first six chapters -- getting started, the desktop, Phosphor, TinyNES, files and firmware, and troubleshooting -- written to the reader's rules, the longest line 70 columns against the 78 the reader shows. The app is registered in `td_desktop_bl616.c` and `CMakeLists.txt`, and the README describes it beside the Phosphor app. Two faults were found on the board and fixed. The first build hard-reset the board when the window opened: `read_line` answered the line length, which is 0 both for an empty line and at end of file, so `load_index` looped while the answer was `>= 0` and spun at the end of `index.md` until the watchdog fired -- `crash` recorded "a task that never yielded", which named the fault exactly where the stack theory had been wrong. `read_line` now answers -1 at end of file. The other was a design fault rather than a bug: `Contents` and `Reload` both returned to the same list, since only the second re-read the index, so they were folded into one `Contents` button that re-reads `index.md` every time, which is also how a chapter added while the window is open gets picked up. The first version also read each line back from the card inside the widget draw callback; that was not the fault, but it moved to the preload shape regardless, keeping FatFS out of the draw path. Firmware `b744811-dirty.d9e3cf8` was installed with `tools/tinytang_flash.py --yes`, the six chapters were put at `/root/manual` with `tools/tinytang_put.py` (the shell's `mkdir` takes no `-p`, which cost one round), and after a power cycle the user reported PASS for the reader: contents listing all six chapters, chapters opening and reading, and one Contents button behaving as intended. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed `.ai/core.md` was not changed and no settled entry was rewritten, and validated this entry as number 11 of 100 with exactly six sections and allowed Status values.

#### Next Steps:

The reader is unqualified in three ways the user is better placed to judge than any test: whether a page at 80x45 reads comfortably, whether one heading colour is enough, and whether paging keys would suit a manual better than arrow scrolling. Growing the manual is now content work and needs no code; a chapter over 12 KB or 512 lines says so in the footer rather than truncating silently, and the reader truncates lines past 78 columns rather than wrapping them, so chapters must keep their paragraphs short. Pictures would need the separate viewer core this cycle deliberately avoided.

#### Files Modified:

- CMakeLists.txt
- README.md
- manual/desktop.md
- manual/files.md
- manual/getting-started.md
- manual/index.md
- manual/phosphor.md
- manual/tinynes.md
- manual/troubleshooting.md
- ports/bl616/td_desktop_bl616.c
- ports/bl616/td_manual_app.cpp

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 12 COMMIT Unreleased 2026-10-21T16:05:26-07:00

#### Coming From:

Unreleased 60728f1

#### Purpose:

Make every connected Bluetooth HID device act at once, as several keyboards and mice do on a PC, instead of one slot per role outranking the others.

#### Outcome:

The slots no longer compete. `kbd_source()` and `mouse_source()` are gone, and with them the rule that a dedicated slot beat the combo for its role and the poll of neutral reports that released a key when a role changed hands. `tang_ble_keyboard()` now merges every ready keyboard slot, modifiers OR-ed and the six key slots a union, and `tang_ble_mouse()` sums the motion and wheel of every ready mouse slot and takes the union of the buttons and presses, clearing each slot's deltas as it takes them. Two keyboards therefore type together and two pointing devices move one pointer, with no device disabled and nothing to choose. The merge is safe because each slot already owns its input state and `hid_clear_input()` zeroes it on disconnect, on pairing and on `off`, so a key held on a device that goes away leaves the union on the next poll rather than sticking -- the release logic the old arbitration needed has no work left to do. Testing on the board found a second, older fault behind it: the Logitech mouse moved nothing while `blemouse watch` showed its reports arriving. The device reports on its report characteristic rather than its boot input, and the format routing that covered that case was conditioned on `d->combo`, so a mouse paired into the mouse slot was silently ignored by strict handle matching. Routing by length now applies to every slot that carries the role -- 8 bytes a keyboard, 3 or 4 a mouse -- which is the protocol's own rule rather than one device's quirk, and the role each slot carries keeps the two shapes apart. The user's ask fixed the shape: no priority, nothing disabling anything, "like old windows". Firmware `60728f1-dirty.531f750` was installed with `tools/tinytang_flash.py --yes`, and the user reported PASS for the K950 and the Rii K06 combo typing together, the Logitech mouse and the combo's touchpad both moving the pointer, and clicks from either registering. The README's `blecombo` bullet lost the arbitration paragraph and gained "Every connected device acts." describing the merge and why the formats are routed by length. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed `.ai/core.md` was not changed and no settled entry was rewritten, and validated this entry as number 12 of 100 with exactly six sections and allowed Status values.

#### Next Steps:

Three slots is still the ceiling, so a fourth device needs one freed with `off` or `forget`. A device that refuses boot protocol is still paired and logged without its reports decoded, and the length routing assumes the boot shapes of 8 bytes for a keyboard and 3 or 4 for a mouse, so a device that sends its reports in another size is counted as a report and not read. Accelerated pointer reports are not looked at: motion is the boot deltas as sent, which is why two mice simply add.

#### Files Modified:

- README.md
- ports/bl616/tang_ble.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 13 COMMIT Unreleased 2026-10-21T17:47:24-07:00

#### Coming From:

Unreleased a797a17

#### Purpose:

Rename the desktop's manual reader to TinyHelp.

#### Outcome:

The app is TinyHelp everywhere it is named: `ports/bl616/td_manual_app.cpp` is `ports/bl616/td_help_app.cpp` by `git mv` so its history follows, `td_manual_register()` is `td_help_register()`, and the start-menu name and window title are both "TinyHelp", with the `?` icon kept because it still reads as help where "T" is now Terminal's letter too. The declaration and the call in `td_desktop_bl616.c`, the source entry and comment in `CMakeLists.txt`, the app bullet in the README and the desktop chapter's app table and F12 paragraph were updated with them. The rename is the app, not the document: the card folder stays `/root/manual`, so no chapter moved and `files.md`'s `/root/manual this manual` line stays true, and the reader's own comments still call the content a manual for the same reason. Firmware `a797a17-dirty.0fe5190` was built with `make CHIP=bl616 BOARD=bl616dk`, installed with `tools/tinytang_flash.py --yes` and confirmed by `platform` after the power cycle, the changed chapter was sent with `tools/tinytang_put.py` to `/root/manual/desktop.md` (1,096 bytes), and the user reported PASS for the start-menu entry, the window title and the contents list and chapters still opening. `strings` on the image shows `TinyHelp` and no `Manual` title or `td_manual_register` symbol left behind. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed `.ai/core.md` was not changed and no settled entry was rewritten, and validated this entry as number 13 of 100 with exactly six sections and allowed Status values.

#### Next Steps:

The manual's content still says "TinyTang manual" in `index.md` and the folder is still `/root/manual`; both are the document rather than the app, so they were left, and moving them would mean re-putting every chapter and editing the two chapters that name the path. The reader's three open questions from entry 11 remain unanswered: page density at 80x45, whether one heading colour is enough, and whether paging keys suit a manual better than arrow scrolling. The uncommitted edits inside `third_party/tinydesk` are still there and still outside any cycle.

#### Files Modified:

- CMakeLists.txt
- README.md
- manual/desktop.md
- ports/bl616/td_desktop_bl616.c
- ports/bl616/td_help_app.cpp

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 14 COMMIT Unreleased 2026-10-22T06:30:11-07:00

#### Coming From:

Unreleased 5edea0c

#### Purpose:

Start a cartridge from the TinyNES app's Library tab without the shell, so a launch from the open desktop stops failing with "not enough memory to load the script", as the user approved after the fault was traced.

#### Outcome:

The Library started a game by typing `FILE=...; tdsh run /root/scripts/open/nes.tdsh` into the Terminal, so a launch paid for a script worker's 16 KB stack, the shell's job and the script's line table on a heap the desktop had already spent: `tang` on firmware `5edea0c-dirty.ee27253` reported one refused allocation of 109 bytes for task `tdsh_script`, and the shell's `load_script()` printed the v0.1.6 message at `third_party/tinydesk-shell/src/core/tdsh_script.c:228`. The adaptive fallback in `ports/bl616/tdsh_platform_bl616.c`, which was found uncommitted on the tree as the previous attempt at this same fault, had turned a refused stack into a refused line table rather than removing either, so it is carried into history here with its now-dead `words` variable removed. The launch no longer uses the shell. `ports/bl616/tang_ae350_run.h` declares `tang_ae350_run()`, the whole of `ae350run` taking resolved FatFS paths and no session, implemented in `ports/bl616/tang_fpga_uart.c` where `cmd_ae350run` was split into that work and a shell half that resolves `argv` and calls it, so the two cannot drift apart; the one visible consequence for the shell is that its `ae350run:` messages now name the resolved path rather than the typed one. `ports/bl616/tinynes_app.cpp` starts the sequence -- `tang_core_load()` of `tinynes.bin`, `tang_ae350_run()` of the player with the cartridge, then `tang_core_load()` of `desktop.bin` -- in a task of the app's own, 8 KiB at `tskIDLE_PRIORITY + 1`, the priority and half the stack a shell worker gave it, and prints the stack left when the game ends. The Library's name array is freed before the handoff because a game does not need it, the window's tick puts the page back afterwards, and a second launch while one runs is refused instead of reloading the core under the first. Firmware `5edea0c-dirty.2b7105b`, 644,816 bytes, SHA-256 `cd9eecc64df71b0aefbd8adf74aa11dc87bd6e526fd0cf7df36e58208acc2e05`, built with no warning in the changed files, and `tools/tests/test_ae350_load.sh`, `test_tang_flash.sh` and `test_nes_ini.sh` passed; it was installed with `tools/tinytang_flash.py --yes` and the user power-cycled and reported PASS, opening a game from the Library with the desktop up and returning to the desktop core afterwards. Two things are recorded rather than smoothed over: the printed stack margin was not captured, so 8 KiB rests on the Phosphor player's precedent and not on a measurement of this path, and the image also carried two uncommitted Phosphor edits that lazily create the player task, plus working-tree edits inside `third_party/tinydesk`, all of which stay uncommitted and outside this cycle. The README says the Library starts a game without the shell. `.ai/core-reference.md`'s BL6-010 gained the recurrence and the in-process path and moved its verified date. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed `.ai/core.md` was not changed and no settled entry was rewritten, and validated this entry as number 14 of 100 with exactly six sections and allowed Status values.

#### Next Steps:

The stack margin is the one number this cycle did not take, so the next board session should read the `nes: done (0), N bytes of stack left` line and raise `LAUNCH_STACK_WORDS` if 8 KiB leaves little. A `.tdsh` started from the desktop's open handler still goes through `tdsh run` and can be refused exactly as this was, so the Files-app double-click and the Phosphor icon are the remaining exposure of BL6-010 until they move in-process the same way; the two Phosphor lazy-task edits and the `third_party/tinydesk` working tree are still uncommitted and belong to a cycle of their own. The older items stand: FatFS timestamps need `FF_FS_NORTC` off and a `get_fattime()`, the MCP79410 waits on external pull-ups, `tools/fetch-deps.sh --check` looks for fork paths this machine does not use, and `v0.1.0` remains a pre-release.

#### Files Modified:

- README.md
- ports/bl616/tang_ae350_run.h
- ports/bl616/tang_fpga_uart.c
- ports/bl616/tdsh_platform_bl616.c
- ports/bl616/tinynes_app.cpp

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 15 COMMIT Unreleased 2026-10-22T06:46:12-07:00

#### Coming From:

Unreleased d399c9b

#### Purpose:

Remove the .nes launch capability from TinyDesk's Files app and the desktop, at the user's direction, so TinyNES starts only from its own Library tab.

#### Outcome:

The user asked why a patch was being added rather than the old one removed, and the answer changed the work: carried patch 0002 was not .nes support but a generic rule that ran `<home>/scripts/open/<ext>.tdsh` for any opened file, and `nes.tdsh` was its only consumer in this project and on the release card. Deleting it alone would not have removed the launcher cleanly -- `td_open_with_handler()` returning false sends Files and the desktop to `td_editor_open()`, so a cartridge would have opened in the Editor as binary text and could have been saved over. The mechanism is therefore gone and one small patch replaces it: `0002-open-with-handler-script.patch` (90 lines, `td_open_with_handler`, and the call-site changes in `files.c` and `desktop.c`) is deleted, and `0002-cartridge-is-not-text.patch` (32 lines) makes `td_editor_open()` answer a `.nes` with "Cartridges start from the TinyNES app's Library", which covers Files, the desktop, the Editor's own File/Open and the Run path from the one place they all pass through. The series again came to the failure TOOL-010 records, a later patch editing lines an earlier one added so that the applier's reverse check refuses it, which is why the change lives in 0002 rather than in a third file. A fresh clone at `feaf841` with both patches reproduces the working tree byte for byte in all six touched files, and `BUILDING.md`'s digest for 0002 is now `c985f1d9bac1b0332cf8b897e83d0532516cd07b23c464037fad0f0e2e4aeec6`. `README.md` lost the handler paragraph and the `/root/scripts/open/` row and gains what a double-clicked cartridge now does, `THIRD_PARTY.md` and the v0.1.0 release notes describe the new patch, and the comment in `ports/bl616/tinynes_app.cpp` names it. Firmware `d399c9b-dirty.a6c8ba4`, 644,800 bytes, SHA-256 `12d7ef98d20af2bec5820770e1f08a389aaa4b32b6e9811f9577d8d4f87ac6c6`, 16 bytes smaller than entry 14's, was built with no warning in the changed files and `test_tinydesk_patches.sh`, `test_terminal_lifecycle.sh` and `test_editor_limits.sh` passed; it was installed with `tools/tinytang_flash.py --yes`, the user power-cycled, `platform` reported the new identity, and the user reported PASS for the cycle, including a double-clicked cartridge that is refused rather than opened. The card's `/root/scripts/open/nes.tdsh` and its folder were removed with `rm -r`, leaving `boot.tdsh`, `phosphor.tdsh` and `cheatsheet.txt` under `/root/scripts`. `.ai/core-reference.md`'s BL6-010 gained the removal and its fast-index line no longer claims a desktop script can be refused. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed `.ai/core.md` was not changed and no settled entry was rewritten, and validated this entry as number 15 of 100 with exactly six sections and allowed Status values.

#### Next Steps:

A `.zip` double-clicked in Files or on the desktop still opens in the Editor, unchanged by this cycle, and the same refusal would cover it if the user wants that. The launch task's stack margin was still not read: the run completed, so 8 KiB served the whole start-play-exit-reload path, but the `nes: done (0), N bytes of stack left` line has not been captured and the headroom is unknown. The two Phosphor lazy-task edits and the `third_party/tinydesk` working tree (now only patch 0001 applied) are still uncommitted and belong to a cycle of their own, and the older items stand: FatFS timestamps need `FF_FS_NORTC` off and a `get_fattime()`, the MCP79410 waits on external pull-ups, `tools/fetch-deps.sh --check` looks for fork paths this machine does not use, and `v0.1.0` remains a pre-release.

#### Files Modified:

- BUILDING.md
- README.md
- THIRD_PARTY.md
- docs/releases/v0.1.0.md
- ports/bl616/tinynes_app.cpp
- third_party/patches/tinydesk/0002-cartridge-is-not-text.patch
- third_party/patches/tinydesk/0002-open-with-handler-script.patch (removed)

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 16 COMMIT Unreleased 2026-10-22T15:26:10-07:00

#### Coming From:

Unreleased 0b0685a

#### Purpose:

Stop the Phosphor player from holding a task and its 8 KiB stack from power-up, by creating it the first time something asks to play.

#### Outcome:

The two edits entry 15 named as uncommitted are now a cycle of their own, and they are the ones the user asked for: `phosphor_player_init()` creates the player's task and 8 KiB stack, and `tang_phosphor_register()` used to call it at boot from the shell-command registration, so a board that never played a track held that stack from power-up until the end of time. `ports/bl616/phosphor/phosphor_cmd.cpp` now only registers the commands, and `ports/bl616/phosphor/phosphor_player.cpp` creates the task on the first `PLAY` in `request()` before its existing `s_task == nullptr` check, so a stop or a pause with no task is still nothing to do and the commands' other subcommands behave as before. The change was compiled and flashed as part of the TinyGBC launcher cycle's firmware, `0b0685a-dirty.45da5d7`, 648,960 bytes, SHA-256 `24cd6eb9eee4308d8ce127f5aec89117abd21024a48f90950cab0973f8075671`, which built with no warning in the two files; it was installed with `tools/tinytang_flash.py`, the user power-cycled, `platform` reported the new identity, and the user reported PASS for this cycle. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and no settled entry was rewritten, and validated this entry as number 16 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The TinyGBC launcher's four files in this tree — `ports/bl616/tinygbc_app.cpp`, `ports/bl616/tang_fpga_uart.c`, `ports/bl616/td_desktop_bl616.c` and `CMakeLists.txt` — are still uncommitted and belong to a cycle of their own, which would also record the token change that let the deck run this tree's player as well as TinyNES's. The older items stand: a `.zip` double-clicked in Files or on the desktop still opens in the Editor, the launch task's stack margin has not been captured, FatFS timestamps need `FF_FS_NORTC` off and a `get_fattime()`, the MCP79410 waits on external pull-ups, `tools/fetch-deps.sh --check` looks for fork paths this machine does not use, and `v0.1.0` remains a pre-release.

#### Files Modified:

- ports/bl616/phosphor/phosphor_cmd.cpp
- ports/bl616/phosphor/phosphor_player.cpp

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 17 COMMIT Unreleased 2026-10-22T16:03:40-07:00

#### Coming From:

Unreleased 6b080b4

#### Purpose:

Give the desktop a window that starts a Game Boy cartridge from the card, the way TinyNES's window starts a NES one.

#### Outcome:

The window is `ports/bl616/tinygbc_app.cpp`, built from `tinynes_app.cpp` and reduced to what the Game Boy player can serve: a Library page that lists the card's `.gbc`, `.gb` and `.zip` files from the first of `/root/media/gb`, `/root/media/gbc`, `/root/media/gbcolor` and `/root/media` that exists, and launches one by loading `/root/cores/console138k/tinygbc.bin`, running `/root/ae350/tinygbc-play.elf` through `tang_ae350_run()` with the file as its data, and putting `/root/cores/console138k/desktop.bin` back afterwards; Input, Video and Saves are labelled placeholders. The deck could not have run that player at all as it stood: `tang_ae350_run()` matched only `TinyNES LOAD`, `TinyNES WANT`, `TinyNES LINK` and `TinyNES EXIT`, and TinyGBC's boot ROM and player say `TinyGBC`, so a new `ae350_says()` matches either product name and hands back the fields past it, with the NVRAM and settings requests left TinyNES-only until the player has saves. The window was registered in `td_desktop_bl616.c` and `CMakeLists.txt`. Two firmwares were built from this tree while the files were still uncommitted: `0b0685a-dirty.45da5d7`, 648,960 bytes, installed with `tools/tinytang_flash.py` and power-cycled, on which the user launched `ucity.gbc` from the window, played it and came back to the console; and `6b080b4-dirty.4c13dc7`, 648,960 bytes, SHA-256 `ede0f0732eca09a44b6e5d38b127c7cf978b69a2a15a1555b73aa5da501d8a06`, which added `.zip` to the Library's extensions and on which the user launched `zelda-ladx.zip` and passed. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and no settled entry was rewritten, and validated this entry as number 17 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The window's three other pages stay placeholders until the TinyGBC player has something to set, which needs a save and a savestate on the A25 and the NVRAM and settings requests this deck still answers only in TinyNES's names. A `.zip` is listed and sent here but unpacked by the player, so the deck never needs an inflater; a `.zip` holding no `.gb` or `.gbc`, two of them, or a wrong CRC is refused on the AE350 and reported as a failed cartridge. The older items stand: the launch task's stack margin has not been captured, FatFS timestamps need `FF_FS_NORTC` off and a `get_fattime()`, the MCP79410 waits on external pull-ups, `tools/fetch-deps.sh --check` looks for fork paths this machine does not use, and `v0.1.0` remains a pre-release.

#### Files Modified:

- CMakeLists.txt
- ports/bl616/tang_fpga_uart.c
- ports/bl616/td_desktop_bl616.c
- ports/bl616/tinygbc_app.cpp

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 18 COMMIT Unreleased 2026-10-22T18:57:18-07:00

#### Coming From:

Unreleased 14f27e9

#### Purpose:

Lock the TinyNES and TinyGBC windows to one fixed size, as the user asked, the width the narrowest the windows could already be made and the height showing twelve Library entries, so the two look the same.

#### Outcome:

Both windows opened at 66 by 26 cells and could be resized down to `min_w = 50` and `min_h = 18`, the Library list followed the window as `td_rect(1, 3, -1, -11)`, and its Launch, Up and Refresh buttons and status line were fixed on client rows 16 and 17, which fell outside the window at its minimum height. `ports/bl616/tinynes_app.cpp` and `ports/bl616/tinygbc_app.cpp` now share the same three constants, `WIN_W = 50`, `WIN_H = 20` and `LIB_ROWS = 12`: the window opens at that size, its minimum is the same size, and `TD_WIN_RESIZABLE` is dropped, which in TinyDesk's window manager also removes the resize grip, the title-bar maximise and F11. The Library list is `td_rect(1, 3, -1, LIB_ROWS)` with a scrollbar of the same height, client rows 3 to 14 above the unchanged buttons and status line, so the 18-row client area is the frame's 20 less its border (`td_win_client`). At 50 columns the client is 48 wide, and TinyNES's Input page had put Change, Restore defaults and Save, and two lines of help, at column 38, where Restore defaults, 20 cells wide by `td_button`'s caption plus four, ran to column 58 and was cut off; the three buttons now sit in one row under the eight-row list at client row 13, columns 1, 13 and 35, with the help on row 14 as one line. TinyGBC's pages are labels well inside 48 columns and needed no change. The firmware was built with `make CHIP=bl616 BOARD=bl616dk` against `~/.cache/tangcore-dev/sdk-2.3.36`, the SDK the build tree is configured for, after a first attempt with `BL_SDK_BASE` set to the older `~/.cache/tangcore-dev/sdk` stopped on CherryUSB's missing `cherryusb_config_template.h` and produced nothing; the image is `14f27e9-dirty.3bfa327`, 648,976 bytes, SHA-256 `7bf89bdcca6984aee46b87b2d0a6c5140399c2597e7957b56b2b586af908294f`, with no warning in either file. It was installed with `tools/tinytang_flash.py --yes`, the user power-cycled the board, and `platform` reported `tinytang 14f27e9-dirty.3bfa327`. The user opened both windows, found them the same size with no way to resize or maximise them, twelve Library entries showing and every page whole, and passed the cycle. `README.md` now states the fixed size and corrects where the controllers are, player 1 having moved to the USB-A port through Adafruit's adapter in TinyNES's core-log entry 47. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and no settled entry was rewritten, and validated this entry as number 18 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The TinyNES launch task's stack margin is now read, in TinyNES's entry 47, as 5,400 bytes left of 8 KiB, so `LAUNCH_STACK_WORDS` needs no change. After that run, reloading the desktop core printed `tang.ini: the core did not take the socket declaration`, which is this tree's to explain. The older items stand: a `.zip` double-clicked in Files or on the desktop still opens in the Editor, TinyGBC's Input, Video and Saves pages wait on the player, FatFS timestamps need `FF_FS_NORTC` off and a `get_fattime()`, the MCP79410 waits on external pull-ups, `tools/fetch-deps.sh --check` looks for fork paths this machine does not use, and `v0.1.0` remains a pre-release.

#### Files Modified:

- README.md
- ports/bl616/tinygbc_app.cpp
- ports/bl616/tinynes_app.cpp

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 19 COMMIT Unreleased 2026-10-22T19:42:34-07:00

#### Coming From:

Unreleased 42ea867

#### Purpose:

Keep TinyNES's one save state per cartridge on the card and show it on the TinyNES window's Saves page, as the user approved, the player's half being TinyNES's core-log entry 49.

#### Outcome:

`ports/bl616/tang_fpga_uart.c`'s battery-save receiver now writes whichever destination the arming line names, `s_ae350_dest`, instead of always the `.sav`, and makes every folder above it, so the same verified path, a `.part` file read back and checked against the length and CRC-32 before it replaces the old file, serves both: `TinyNES NVRAM DATA` and `SAVE` still go to `/root/saves/nes/<cartridge>.sav`, and the new `TinyNES STATE DATA <bytes> <crc32>`, which the player sends as bytes while the game runs, goes to `/root/states/nes/<cartridge>.state`, its messages naming a state rather than a save. `ae350_named_path` builds both names from the cartridge's basename, so `Game.zip` and `Game.nes` share one state as they share one save. At launch `TinyNES STATE WANT <address> <room>` is answered with that file, headerless, by `ae350_send_named`, which the battery save's request now uses too, and with nothing when the card has none. `ports/bl616/tinynes_app.cpp`'s Saves page, a placeholder until now, reads the selected Library cartridge's `.state` each time the page is opened and shows its path, size and a CRC-32 of the whole file, the number the player prints as it sends, then from TinyNES's 32-byte header the game's CRC-32, the frame and the core, or says that there is no state, that a folder is selected, or that the file is not a TinyNES state; it says there is no creation time while the board has no clock. The firmware `42ea867-dirty.f1b9751`, 649,008 bytes, SHA-256 `a053fdb16059e8fe94de82b9580fe2fa1d932981b274076b69f3af612b7e14c4`, built against `~/.cache/tangcore-dev/sdk-2.3.36` with no warning in either file, was installed with `tools/tinytang_flash.py --yes` beside TinyNES's player `a865a41d`; the user power-cycled, tested saving, loading and the Saves page and reported that everything works perfectly, and afterwards `platform` read `tinytang 42ea867-dirty.f1b9751` and `ls -l /root/states/nes` showed `Castlevania.state` at 22,047 bytes. `README.md` describes the state. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and no settled entry was rewritten, and validated this entry as number 19 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

A state that does not arrive whole leaves the previous one and says so only on this console; showing that on the desk layer would make a lost send visible during play. The Saves page's creation time waits on the RTC and FatFS timestamps. The older items stand: `tang.ini: the core did not take the socket declaration` after the desktop core reloads, a `.zip` double-clicked in Files or on the desktop still opens in the Editor, TinyGBC's Input, Video and Saves pages wait on the player, `tools/fetch-deps.sh --check` looks for fork paths this machine does not use, and `v0.1.0` remains a pre-release.

#### Files Modified:

- README.md
- ports/bl616/tang_fpga_uart.c
- ports/bl616/tinynes_app.cpp

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 20 COMMIT Unreleased 2026-10-22T21:34:13-07:00

#### Coming From:

Unreleased f035e73

#### Purpose:

Add an Audio tab after Video to the TinyNES and TinyGBC windows with one master volume slider from 0 to 100 percent and a mute, for HDMI and the I2S2 together, kept in each player's settings file and sent to the player at launch, as the user approved, the players' halves being TinyNES core-log entry 50 and TinyGBC core-log entry 20.

#### Outcome:

The user first named two controls, HDMI and the I2S2, then settled on one master volume for both, which the players apply to the one stream both outputs play, so no core changed. TinyDesk has no slider, so carried patch `third_party/patches/tinydesk/0003-slider-widget.patch` adds `td_slider` and `td_slider_set` to `td_widgets.h` and `widgets.c`: a focusable bar drawn like the progress bar, in the focus colour when focused, that Left and Right move one step, Home and End take to the ends, and a click or a drag sets to the cell under the pointer, running its callback only on a change; `scripts/apply-tinydesk-patches.sh` applied 0001 to 0003 to a fresh TinyDesk v0.1.5 worktree and every touched file matched the submodule byte for byte, and the patch's new `test_slider` case brought TinyDesk's `test_widgets` to 126 checks with no failure, with `test_wm` and `test_render` also passing. The settings readers were split: `ports/bl616/tang_settings_ini.c` now holds the line walker `tang_nes_ini.c` had, as `tang_settings_walk`, and the shared `[audio]` keys, `volume` 0 to 100 clamped and `mute` yes or no, defaulting to 100 and no; `tang_nes_ini.c` reads `[audio]` again, which entry 78 had dropped, and the new `tang_gbc_ini.c` reads `/gbc.ini`, holding only `[audio]`. `tools/tests/test_nes_ini.sh` builds all three and passed 45 checks, among them the clamps, a `volume` outside `[audio]` counting nothing, `/gbc.ini` ignoring a NES key, an absent `/gbc.ini` keeping the defaults, and the shipped `docs/nes.ini`, now with `[audio]`, and the new `docs/gbc.ini` parsing. `ports/bl616/tang_fpga_uart.c` sends the volume and mute as bytes 19 and 20 of TinyNES's settings blob and answers the new `TinyGBC SETTINGS WANT` with those two bytes from `/gbc.ini`, both through a shared `ae350_send_blob`, never sending more than was asked; the `tang` command prints both files' audio. Both windows went from 50 to 54 columns, the narrowest that fits five tabs a column apart, and gained the Audio page at tab 3, Saves moving to 4: a label, a twenty-cell slider in 5 percent steps, the percentage and "muted" beside it, a Mute checkbox, Save and a note that it is taken at the next game start. TinyNES's Save rewrites `[audio]` in place as it does `[input]` and `[video]`; TinyGBC's has its own writer that keeps every other line, creates the file when the card has none, and goes through `/gbc.ini.tmp` and `/gbc.ini.bak` as TinyNES's does. The firmware `f035e73-dirty.70177ad`, 653,232 bytes, SHA-256 `bf8d59ad508f15752b025df12f1b20fd823b9dd67be8ecea2fde92874e49cda1`, built against `~/.cache/tangcore-dev/sdk-2.3.36` with no warning in the changed files, was installed with `tools/tinytang_flash.py --yes` beside the two new players. Before the flash the card's `/nes.ini` still held entry 78's dropped `[audio]` with `volume = 75`, which the new reader takes, so the TinyNES tab opened at 75 percent and the user set it to 100 and saved as part of the test; the user power-cycled and reported PASS for both windows. The board was off USB when this entry was written, so `platform`, `/nes.ini` and `/gbc.ini` were not read back. `README.md` describes the tab and the window size. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and no settled entry was rewritten, and validated this entry as number 20 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The volume reaches the player only at launch; changing it over a running game would need a frame type in the cores' `desk_link` that the player can read, a fabric change and a seed sweep in both projects, to be proposed only if the user asks. A `/nes.ini` written before entry 78 keeps its old `channels` key, which is preserved and not counted. The older items stand: a lost state send shows only on this console, the Saves page's creation time waits on the RTC, `tang.ini: the core did not take the socket declaration` after the desktop core reloads, a `.zip` double-clicked in Files or on the desktop still opens in the Editor, TinyGBC's Input, Video and Saves pages wait on the player, `tools/fetch-deps.sh --check` looks for fork paths this machine does not use, and `v0.1.0` remains a pre-release.

#### Files Modified:

- README.md
- docs/gbc.ini
- docs/nes.ini
- ports/bl616/tang_fpga_uart.c
- ports/bl616/tang_gbc_ini.c
- ports/bl616/tang_gbc_ini.h
- ports/bl616/tang_nes_ini.c
- ports/bl616/tang_nes_ini.h
- ports/bl616/tang_settings_ini.c
- ports/bl616/tang_settings_ini.h
- ports/bl616/tdsh_platform_bl616.c
- ports/bl616/tinygbc_app.cpp
- ports/bl616/tinynes_app.cpp
- third_party/patches/tinydesk/0003-slider-widget.patch
- tools/tests/stubs/ff.h
- tools/tests/tb_nes_ini.c
- tools/tests/test_nes_ini.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 21 COMMIT Unreleased 2026-10-23T04:16:19-07:00

#### Coming From:

Unreleased 5877729

#### Purpose:

Boot the board from a card rebuilt from empty, with the desktop core named `tinytang.bin` and its matching `.fs` gzipped beside it as `tinytang.fs.gz`, as the user directed.

#### Outcome:

The user reformatted the 120 GB card as one empty FAT volume, labelled `TINYTANG`, to rebuild it from scratch rather than restore the backup, and the first goal was the core and firmware alone. The card was written on the host with only `/root/scripts/boot.tdsh` and `/root/cores/console138k/tinytang.bin`, the latter being seed 3 of desktop ABI 1.4 from `build/desktop/seed3/open-s3/`, `desktop.fe5289f5e446.bin`, 4,497,792 bytes, still the core of record because nothing under `fpga/desktop` has changed since entry 3; the user renamed it from `desktop.bin` and asked for the `.fs` beside it, and chose, of the options offered, a companion copy that the board does not read over teaching `tangload` to inflate and parse a `.fs`. `tinytang.fs.gz` is `gzip -9 -n` of the build's paired `desktop.c71a7cfe81de.fs`, 442,662 bytes, which decompresses to that file's SHA-256. The name had been hardcoded in four places, so `scripts/boot.tdsh`'s `CORE` line and its loaded message, `DESKTOP_REAL` in `ports/bl616/tinynes_app.cpp` and `ports/bl616/tinygbc_app.cpp`, which is the core a game's exit puts back, and the path in `tools/tests/tb_tang_flash.c` now name `tinytang.bin`; `README.md` and `fpga/desktop/README.md` describe the card name, the README's put commands gained the `gzip` step and lost a stale `24a11e81a46c`, and the build scripts and their outputs keep the `desktop.*` names this log and the reference use. The firmware `5877729-dirty.35d6fd9`, 653,232 bytes, SHA-256 `379ca7b0425e6b813924ade857ea6fde9ed0fee67e5c9ee82d4c2244ef653248`, built against `~/.cache/tangcore-dev/sdk-2.3.36` with no warning in the changed files and no `console138k/desktop.bin` string left in the image, and `tools/tests/test_tang_flash.sh` passed. On the new card the board first booted on entry 20's `f035e73-dirty.70177ad`, `fpga` answering core 84 and `tangini` reporting no `/tang.ini`, core 0x54 at ABI 1.4 and `0xc0 = 0x0000`; the new firmware was installed with `tools/tinytang_flash.py --yes`, and after the user's power cycle `platform` read `tinytang 5877729-dirty.35d6fd9` with the same core and socket state. The card holds no pairings, so `blekbd pair` was run for the Logitech keyboard; the console dropped as the radio started, as USB-008 records, re-enumerated by itself, and the board then reported `blekbd: ready` with a report received. The user then connected the mouse, opened the desktop and reported PASS. `/root/ble/bonds.bin` was not read back, because the desktop held the console when the cycle closed. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and no settled entry was rewritten, and validated this entry as number 21 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The new `DESKTOP_REAL` is exercised only when a game exits, which waits on the TinyNES and TinyGBC cores and players being put back on the card, and that exit is this cycle's one untested path. The card is otherwise bare: no `/tang.ini`, so the OLED, the DS3231 and the RYLR998 are released and the desktop clock is unset, no time zone, manual, Phosphor core or media, and its rebuild is the user's to order. Whether the pairings reached `/root/ble/bonds.bin` should be read at the next prompt. The older items stand: `tang.ini: the core did not take the socket declaration` after the desktop core reloads, a lost state send shows only on this console, a `.zip` double-clicked in Files or on the desktop still opens in the Editor, TinyGBC's Input, Video and Saves pages wait on the player, `tools/fetch-deps.sh --check` looks for fork paths this machine does not use, and `v0.1.0` remains a pre-release.

#### Files Modified:

- README.md
- fpga/desktop/README.md
- ports/bl616/tinygbc_app.cpp
- ports/bl616/tinynes_app.cpp
- scripts/boot.tdsh
- tools/tests/tb_tang_flash.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
