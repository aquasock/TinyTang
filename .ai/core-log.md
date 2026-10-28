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

## 22 COMMIT Unreleased 2026-10-23T05:38:33-07:00

#### Coming From:

Unreleased a4b6faf

#### Purpose:

Make `/tang.ini` name one of the only two PMOD layouts the user now supports, the I2S2 beside HDMI with the OLEDrgb beside USB-A, or the PmodVGA across both, every module right side up with no flip or socket swap, as the user approved.

#### Outcome:

The user fixed the hardware to two layouts and chose, of the options offered, the key `layout = i2s2_oled | vga` and leaving the I2C and UART code dormant rather than removing it. `ports/bl616/tang_ini.c` and `tang_ini.h` now read only `layout` under `[tang]`; a missing file or no layout line releases both sockets, and so do an unknown layout and any `pmod0`, `pmod1`, `pmod0_flip` or `pmod1_flip` line, each refused by name with a note saying what to write instead. The new `tang_ini_word()` turns the layout into each core's own word, because the desktop core refuses a word naming the I2S2 and would release the OLED with it (PMOD-006): `vga` is `0x0230` on both cores, PMOD0 `vga_j2` and PMOD1 `vga_j1` as qualified, and `i2s2_oled` is `0x0510` on Phosphor and `0x0010`, the OLED alone, on the desktop core; no word sets a flip bit. `ports/bl616/phosphor/pmod_sockets.cpp` sends and reads back that word and `tangini` prints the layout and what each socket gets; neither layout has an I2C socket, so `pmodi2c`, `pmodlora` and the PMOD RTC stay in the image unreachable, saying so when used, and the desktop clock stays unset. `tools/tests/tb_ini.c` was rewritten for the new contract and the per-core words, and failed on each of two planted faults, the desktop core sent the I2S2 and old keys not releasing; `test_ini.sh`, `test_tang_flash.sh` and `tools/check_help_width.py` passed. `docs/tang.ini` declares `i2s2_oled`, and `README.md`, `fpga/desktop/README.md`, `manual/files.md` and `manual/troubleshooting.md` describe the layouts. Firmware `a4b6faf-dirty.2622c8b`, 653,232 bytes, SHA-256 `00c417a38a54e5a8a3075dcd2361846b80b72a46e9954b9b2ac2b2b6f076c64c`, built against `~/.cache/tangcore-dev/sdk-2.3.36` with no warning in the changed files, was installed with `tools/tinytang_flash.py --yes`; the commit left the board as the FT2232 until the user's power cycle, so `docs/tang.ini` was put as `/tang.ini`, 1,073 bytes, with `tools/tinytang_put.py` afterwards, and `platform` read `tinytang a4b6faf-dirty.2622c8b`. `tangini apply` then declared `0xc0 = 0x0010: pmod0 oledrgb, pmod1 none` to the desktop core, but the user reported the panel dark and the desktop's OLED Terminal not drawing, and the cause of that first failure was not established. After a power cycle the boot path declared the same word by itself, `oledterm start` was run from the console, and `0x118` read Standard, `0x108` counted frames from `0x0dd5` to `0x0e9c` and `0x10c` held `0xeda1179a`; the user then reported the panel lit with text, and passed the cycle. `.ai/core-reference.md` revised PMOD-002, whose per-socket contract TinyTang no longer follows, and PMOD-006 with the layouts, the per-core words and what is qualified, and gained the fast-index line BRD-010 had lacked since entry 6. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and no settled entry was rewritten, checked that the reference's six YAML blocks parse with the index and records naming the same 147 ids in the same order, and validated this entry as number 22 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

Whether a `tangini apply` to a running desktop core lights the panel is open: the one attempt did not, the reboot did, and nothing distinguished a firmware cause from an OLED Terminal that was not running, so the next reseat or layout change should be applied with `tangini apply` and the panel checked before rebooting. `i2s2_oled` on Phosphor, `0x0510` with the I2S2 in PMOD1, has never run on the board and waits on Phosphor being put back on the card, and `vga` under the new key is untested though its word is the qualified one. TinyNES and TinyGBC read a Wii controller on PMOD0's pins, which the OLED now holds, and drive both sockets whichever layout is seated, which is for those projects to settle. The dormant `pmodi2c`, `pmodlora` and RTC paths, and the desktop core's personalities 6 and 7, can be removed in a cycle of their own if the user confirms. The older items stand: the game-exit path to `tinytang.bin`, whether the pairings reached `/root/ble/bonds.bin`, a lost state send shows only on this console, a `.zip` double-clicked in Files or on the desktop still opens in the Editor, TinyGBC's Input, Video and Saves pages wait on the player, `tools/fetch-deps.sh --check` looks for fork paths this machine does not use, and `v0.1.0` remains a pre-release.

#### Files Modified:

- README.md
- docs/tang.ini
- fpga/desktop/README.md
- manual/files.md
- manual/troubleshooting.md
- ports/bl616/phosphor/pmod_sockets.cpp
- ports/bl616/tang_ini.c
- ports/bl616/tang_ini.h
- tools/tests/tb_ini.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 23 COMMIT Unreleased 2026-10-23T11:24:30-07:00

#### Coming From:

Unreleased fc2e144

#### Purpose:

Move the desktop clock's DS3231 to the USB-A port's STEMMA QT chain beside the Adafruit Wii adapter that TinyNES and TinyGBC read their controller through, and remove the Keychron keyboard and LoRa code, as the user approved.

#### Outcome:

The user chose the command name `tangi2c`, the removal of LoRa, and a separate cycle for the legacy nestang and menu keyboard link. Desktop ABI 1.5 (`0x04` reads `0x00010005`, bitmap `0x0f`) runs `fpga/desktop/i2c_master.sv` always on with SDA on D+ (H13) and SCL on D- (G13), open-drain with `PULL_MODE=UP` as TinyNES wires them, and needs no `/tang.ini` declaration; the PMOD personalities 6 and 7, the UART at `0x170`-`0x17c`, `pmod_uart.sv`, both Keychron receivers (`keylink_rx.sv`) and `desktop_link.v`'s unprompted keyboard and pad reports are gone, so the core accepts only none or OLEDrgb per socket or the VGA pair. In the firmware `pmodi2c` became `tangi2c`, which with the desktop clock works only while `tang_core_i2c_bus()` reports core 0x54 at ABI 1.5 or later; `tang_lora.cpp`, the MCP79410 path, the QMK patches and `scripts/apply-qmk-tang-patches.sh` were deleted. Firmware `fc2e144-dirty.c680dcc`, 653,200 bytes, SHA-256 `2c408681ff6b10a09f2c6ad001f75ecb744a9516f1c11341f3e1ae0fb6168437`, built against `~/.cache/tangcore-dev/sdk-2.3.36` and was flashed with `tools/tinytang_flash.py --yes`; the old ABI 1.4 core stays on the card as `tinytang.bin.abi14` and `tinytang.fs.gz.abi14`. The first ABI 1.5 core, `desktop.2f3ad2bd0bda.bin`, read the DS3231 but its scan found only 0x68 and the Classic Controller NACKed its wake-up writes, while with the RTC on the chain TinyNES's latest build, its entry 51 image `d6bc2a6d` with the player `0435d849` stripped from the current `nes_bench.elf` and Castlevania (USA) (Rev A) installed under `/root/cores`, `/root/ae350` and `/root/media/nes` and started from the shell, played with the controller; the game froze on exit, which the user says happens when leaving NES. Two engine faults were found and fixed against the TinyNES master: the engine drove SCL without reading it back, so it now waits for SCL to read high before timing each high half and ends a transfer, or a whole scan, as a NACK with both lines released when SCL stays low for 1 ms, after which the controller's writes ACKed (core `desktop.1ec24a701253.bin`) but its reads gave `ff`; and it read with a repeated START, so a read now sets the register, sends STOP, waits 200 us and reads separately, and scan probes became one-byte NACKed reads. `tb_i2c_master` gained a stretching slave, a Wii mode that sends `ff` after a repeated START or under 150 us from STOP to START, a dead bus, a stuck SCL and a write-probe count, and `tb_desktop_pmod` a stretching controller at 0x52; they failed on each of six planted faults (no SCL wait, no timeout, an abort leaving SDA held, a repeated START, no pause, write probes), the unit slave's ACK sampling was moved to SCL's rising edge after it raced SDA, and `tools/tests/test_desktop_core.sh` passed. An eight-seed sweep (`DESKTOP_BUILD_DIR=build/desktop-abi15w`) passed every seed, and seed 3, `desktop.bff603129045.bin` (4,479,158 bytes, `desktop.37935486286e.fs`, `tinytang.fs.gz` 389,364 bytes from `gzip -9 -n`), met timing at 74.97 MHz on `clk`, 110.67 MHz on the pixel clock and 148.92 MHz on `sys_clk` with both clocks on global resources only and no SPINE17 hop, and went on the card as `tinytang.bin`. On the board it read the controller's identity `00 00 a4 20 01 01` and data `a0 20 10 62 ff ff`, byte 5 reading `bf` while the user held B; `tangi2c scan` still finds only 0x68, for a reason not established. The DS3231 had lost its time (OSF set, 2000-01-01), was set from host UTC with `tangi2c rtc set`, and held it across reboots and the TinyNES session; the user set the zone in Date & time, saw the right desktop time across a reboot, and passed the cycle, and skipped the D+ voltage measurement. In `.ai/core-reference.md` BRD-003 now has the populated port and its pull-downs from TinyNES's record, the new BRD-011 records the USB-A bus, PMOD-007 is retired into it, PMOD-006 and PMOD-008 record the removed personalities, and BL6-005 and TOOL-017 follow, with ABI 1.5 seed 3 as the build of record. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and no settled entry was rewritten, checked that the reference's six YAML blocks parse with the index and records naming the same 147 ids in the same order, and validated this entry as number 23 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The legacy nestang and menu keyboard link, nestang patches 0004 to 0006, `boot-cart.tdsh` and the firmware's 0x08 keyboard path in `fpga_frames`, `tang_osd_desk` and `tang_key`, is to be retired in its own cycle as the user chose. Why `tangi2c scan` misses the Wii controller that `tangi2c read` reaches is open and needs a logic analyser or a slower probe rate to settle. The pin voltage with the adapter's and the DS3231's pull-ups both to 5 V is unmeasured and was estimated at about 3.75 V, above the 3.3 V bank, the same under TinyNES; the DS3231 losing its time suggests a missing or flat backup cell. TinyNES was started from the shell rather than its window, so the window's launch and return to `tinytang.bin` remain untested, and the freeze on leaving a game belongs to TinyNES. The older items stand: whether `tangini apply` to a running desktop core lights the panel, `i2s2_oled` on Phosphor and `vga` under the new key untested, TinyNES and TinyGBC driving PMOD0 where the OLED now sits, whether the pairings reached `/root/ble/bonds.bin`, a lost state send shows only on this console, a `.zip` double-clicked in Files or on the desktop still opens in the Editor, TinyGBC's Input, Video and Saves pages wait on the player, `tools/fetch-deps.sh --check` looks for fork paths this machine does not use, and `v0.1.0` remains a pre-release.

#### Files Modified:

- CMakeLists.txt
- README.md
- THIRD_PARTY.md
- fpga/desktop/README.md
- fpga/desktop/build.tcl
- fpga/desktop/desktop.cst
- fpga/desktop/desktop_link.v
- fpga/desktop/desktop_pmod.sv
- fpga/desktop/desktop_regs.sv
- fpga/desktop/desktop_top.sv
- fpga/desktop/i2c_master.sv
- fpga/desktop/keylink_rx.sv
- fpga/desktop/pmod_uart.sv
- manual/getting-started.md
- ports/bl616/phosphor/pmod_sockets.cpp
- ports/bl616/tang_clock.c
- ports/bl616/tang_clock.h
- ports/bl616/tang_i2c.cpp
- ports/bl616/tang_ini.h
- ports/bl616/tang_lora.cpp
- ports/bl616/td_desktop_bl616.c
- ports/bl616/tdsh_platform_bl616.c
- scripts/apply-qmk-tang-patches.sh
- third_party/patches/qmk/0001-keychron-k2he-tang-mode.patch
- third_party/patches/qmk/0002-keychron-k2he-tang-keymap.patch
- third_party/patches/qmk/README.md
- tools/desktop_open/synth.ys
- tools/tests/sim/tb_desktop_link.sv
- tools/tests/sim/tb_desktop_pmod.sv
- tools/tests/sim/tb_desktop_uart.sv
- tools/tests/sim/tb_i2c_master.sv
- tools/tests/test_desktop_core.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 24 COMMIT Unreleased 2026-10-23T12:18:15-07:00

#### Coming From:

Unreleased 800d11b

#### Purpose:

Replace the OLED Terminal's live shell session with a console command that prints text to the OLED once and then leaves it idle, as the user directed after the panel stayed dark and the session's memory kept the desktop from restarting.

#### Outcome:

The user reported the OLED dark on the desktop core after entry 23's deploy. With the panel dark the core read `0xc0 = 0x0010`, counted frames at `0x108` and, in one boot, carried the text frame `0xeda1179a` at `0x10c`, the frame entry 22's lit panel had, while in another the OLED terminal was stopped and the frame was the blank `0x8a258aec`; nothing at boot started the terminal, only `oledterm start` or the desktop app, and `oledterm start` lit the panel, which is the likely explanation of entry 22's dark `tangini apply`, though how the terminal came to be running in the first boot was not established. The user then asked for the panel to be dark until the OLED Terminal app opened, mirror it while open and go dark on close, choosing the window alone and no console override. Firmware `800d11b-dirty.2f58567` blanked the link's cells while the window was closed; on the board the app opened but the panel never lit, the heap stood at 16,680 bytes free of 98,296 with a largest block of 14,684 bytes after 761 refused allocations, the last 16,384 bytes for the desktop Terminal's shell task, and the desktop would not start again, because the session's roughly 31 KB is held until reboot; the link poller allocating its buffers on first use and returning silently when that failed is the probable reason the panel never lit, not confirmed. The user scrapped the desktop app and the live update and approved a console command instead. `ports/bl616/phosphor/oled_cmd.cpp` adds `oled print <line> [line...]`, `oled clear` and `oled font [standard|compact|graphics]`: after the legacy ID command finds core 0x54, it checks the identity, ABI 1.2 or later and an OLEDrgb in `0xc0`, then writes the font to `0x118`, all 384 cells to `0x200` in six 64-word blocks and a hidden cursor to `0x114`, and sends nothing after; each argument is a line, because the shell keeps only the character after a backslash, and `ports/bl616/oled_text.c` lays the lines out at the font's width with a stride of 24, grey on black, the boxed fallback for anything outside printable ASCII, and the reset cell `0x0720` everywhere else, cutting text that does not fit. It holds no task and no heap, and each register access holds the FPGA link lock across its reply. `tang_oled.c`, `oled_vterm.c`, `phosphor/oled_link.cpp`, their hooks in `tdsh_tang_flash.c`, `tang_osd_desk.c` and `td_desktop_bl616.c`, their tests, `tools/make_oled_demo.py` and `tools/check_stdout_isolation.py` were removed. `tools/tests/test_oled_text.sh` passed and failed on each of three planted faults (no blanking, a late wrap, rows at the font's width instead of the stride), and `test_tang_flash.sh`, `test_osd_desk.sh` (88), `test_terminal_lifecycle.sh`, `test_script_limits.sh`, `test_frames.sh`, `test_clock.sh`, `test_ini.sh` and `tools/check_help_width.py` (137 entries) passed. Firmware `800d11b-dirty.87aa9f4`, 644,864 bytes, SHA-256 `0fe1c5b4f8fbe13e21b3e4aa2fafeb410335c941ab604cd1708ef158f8cf8a96`, built against `~/.cache/tangcore-dev/sdk-2.3.36` with no warning in project files beyond the existing ones in `tang_jtag_programmer.c`, was flashed with `tools/tinytang_flash.py --yes` and power-cycled. At boot the panel was dark at `0x8a258aec` with 47,812 bytes of heap free and a largest block of 29,504 bytes; `oled print "Hello" "TinyTang"` from the USB console gave frame `0xd0322a95` and the user saw both lines, `oled clear` returned `0x8a258aec`, the user printed from the desktop's Terminal (`0x93f46320`), the heap read the same 47,812 and 29,504 bytes afterwards with no refused allocation, and the desktop started again with the text still on the panel; the user passed the cycle. `.ai/core-reference.md` PMOD-006 records `oled print` as what now draws on the panel. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and no settled entry was rewritten, checked that the reference's six YAML blocks parse with the index and records naming the same 147 ids in the same order, and validated this entry as number 24 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The panel is dark at boot and shows only what `oled print` writes, which a core load clears; nothing redraws it after a game returns to the desktop core, which is expected under this design. The legacy nestang and menu keyboard link remains to be retired in its own cycle, and entry 23's open items stand: `tangi2c scan` missing the Wii controller, the unmeasured pin voltage with both pull-ups to 5 V, the DS3231's backup cell, TinyNES's window launch and its freeze on leaving a game, `i2s2_oled` on Phosphor and `vga` under the new key untested, TinyNES and TinyGBC driving PMOD0 where the OLED sits, `/root/ble/bonds.bin`, the console-only lost state send, `.zip` files opening in the Editor, TinyGBC's waiting pages, `tools/fetch-deps.sh --check`'s fork paths, and `v0.1.0` as a pre-release.

#### Files Modified:

- README.md
- THIRD_PARTY.md
- fpga/desktop/README.md
- ports/bl616/oled_text.c
- ports/bl616/oled_text.h
- ports/bl616/oled_vterm.c
- ports/bl616/oled_vterm.h
- ports/bl616/phosphor/oled_cmd.cpp
- ports/bl616/phosphor/oled_link.cpp
- ports/bl616/tang_oled.c
- ports/bl616/tang_oled.h
- ports/bl616/tang_osd_desk.c
- ports/bl616/td_desktop_bl616.c
- ports/bl616/tdsh_tang_flash.c
- tools/check_stdout_isolation.py
- tools/make_oled_demo.py
- tools/tests/oled/test_oled_vterm.c
- tools/tests/sim/gen_desktop_block_frames.cpp
- tools/tests/tb_oled_link.cpp
- tools/tests/tb_oled_session.c
- tools/tests/tb_oled_text.c
- tools/tests/tb_osd_desk.c
- tools/tests/tb_tang_flash.c
- tools/tests/test_oled_link.sh
- tools/tests/test_oled_session.sh
- tools/tests/test_oled_text.sh
- tools/tests/test_oled_vterm.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 25 COMMIT Unreleased 2026-10-23T13:00:43-07:00

#### Coming From:

Unreleased 966da0d

#### Purpose:

Retire the legacy keyboard link together with the nestang NES and menu cores that carried it, as the user chose over removing the link alone, leaving the Bluetooth keyboard as the only keyboard.

#### Outcome:

The user was offered retiring only nestang patches `0004` to `0006`, which would have meant regenerating `0007` and the menu patch and rebuilding both cores with Gowin EDA without a board test, or retiring the two cores outright, since neither is on the rebuilt card and TinyNES and the desktop core replaced them; the user chose the second. The nestang series `0001` to `0007`, the menu-core patch and its README, `scripts/apply-nestang-patches.sh`, `scripts/boot-cart.tdsh`, `scripts/castlevania.tdsh`, `tools/build_nestang_core.sh`, `tools/build_menu_core.sh`, `tools/tinytang_desk_deploy.sh`, `tools/make_script_heap_probe.py` and the nestang line in `deps.conf` were removed; `third_party/patches/README.md` now covers the TinyDesk and shell series, and `THIRD_PARTY.md` keeps nestang's project, commit and licence as the desktop core's origin. In the firmware the core-to-BL616 keyboard report `0x08` is no longer decoded: `fpga_frames.c` lost its keyboard cache and `fpga_frames_keyboard()`, `tang_fpga_uart.c` lost `tang_fpga_keyboard()`, and `tang_osd_desk.c` types, repeats, watches F12 and absorbs keys from the Bluetooth keyboard alone; `tang_key.c`'s translation, its repeat timeout, now judged by the Bluetooth connection, and the firmware's left-alt pointer mode stay, as do the `0x17` keyboard command to programs under the layer and the joypad report `0x03` TinyNES sends. A core that still sends `0x08`, which no current core is known to, would have it queued as an unnamed frame and ignored. `nesload` and the legacy 32x28 `osd` page commands remain for a later cleanup, as planned. `test_osd_desk.sh` (88), `test_key.sh` (53), `test_frames.sh` (24), `test_tang_flash.sh`, `test_terminal_lifecycle.sh` and `tools/check_help_width.py` (137 entries) passed; `tools/fetch-deps.sh --check` still reports the unsatisfied entries recorded since entry 22. Firmware `966da0d-dirty.1704dab`, 644,848 bytes, SHA-256 `1ae0abf41d6a163756e2b316ba89b4fc2274287dfa14d0c6699860538f11a943`, built against `~/.cache/tangcore-dev/sdk-2.3.36` with no warning in project files beyond the existing ones in `tang_jtag_programmer.c`, was flashed with `tools/tinytang_flash.py --yes`, the first attempt finding no `/dev/ttyACM0` until the user reconnected the cable; after the user's reboot with the Classic Controller attached, `platform` read `966da0d-dirty.1704dab` and `crash` 47,864 bytes free with a largest block of 29,504 bytes. The user reported Bluetooth typing with repeat in the desktop, F12, the left-alt pointer and a TinyNES game with F12 over it all working, and passed the cycle. `.ai/core-reference.md` revises DEV-005, PROT-007, PROT-008, PROT-009, TDSH-004, TOOL-003, TOOL-010, TOOL-012, TOOL-019, BLE-006 and BLE-008 and the index lines of DEV-005, TOOL-003, TOOL-010 and TOOL-019 for the retirement. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and no settled entry was rewritten, checked that the reference's six YAML blocks parse with the index and records naming the same 147 ids in the same order, and validated this entry as number 25 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

What only nestang used remains for a cleanup cycle of its own: `nesload` and the ROM-streaming commands, the legacy 32x28 `osd` page and `tang_osd.c`'s and `tang_usbstat.c`'s references to it and to the menu core, and the reference records that describe them. Entry 23's and 24's open items stand: `tangi2c scan` missing the Wii controller, the unmeasured pin voltage with both pull-ups to 5 V, the DS3231's backup cell, TinyNES's freeze on leaving a game, `i2s2_oled` on Phosphor and `vga` under the new key untested, TinyNES and TinyGBC driving PMOD0 where the OLED sits, `/root/ble/bonds.bin`, the console-only lost state send, `.zip` files opening in the Editor, TinyGBC's waiting pages, `tools/fetch-deps.sh --check`'s fork paths, and `v0.1.0` as a pre-release.

#### Files Modified:

- BUILDING.md
- deps.conf
- docs/phosphor-memory-budget.md
- ports/bl616/fpga_frames.c
- ports/bl616/fpga_frames.h
- ports/bl616/tang_ble.c
- ports/bl616/tang_ble.h
- ports/bl616/tang_fpga_link.h
- ports/bl616/tang_fpga_uart.c
- ports/bl616/tang_key.c
- ports/bl616/tang_key.h
- ports/bl616/tang_osd_desk.c
- ports/bl616/tdsh_platform_bl616.c
- README.md
- scripts/apply-nestang-patches.sh
- scripts/boot-cart.tdsh
- scripts/boot.tdsh
- scripts/castlevania.tdsh
- scripts/phosphor.tdsh
- THIRD_PARTY.md
- third_party/patches/0001-controller-gate.patch
- third_party/patches/0002-desktop-text-layer.patch
- third_party/patches/0003-desktop-layer-wiring.patch
- third_party/patches/0004-keyboard-link.patch
- third_party/patches/0005-keyboard-typing.patch
- third_party/patches/0006-pointer-mode.patch
- third_party/patches/0007-wide-layer-alignment.patch
- third_party/patches/menu/0001-menu-core.patch
- third_party/patches/menu/README.md
- third_party/patches/README.md
- tools/build_menu_core.sh
- tools/build_nestang_core.sh
- tools/make_script_heap_probe.py
- tools/tests/tb_osd_desk.c
- tools/tinytang_desk_deploy.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 26 COMMIT Unreleased 2026-10-23T14:47:14-07:00

#### Coming From:

Unreleased 7b67c34

#### Purpose:

Hand off the questions examined after entry 25, the 8BitDo receiver, field file transfer and a console over Bluetooth, because the user's session was ending.

#### Outcome:

No file in this repository changed. With `tangi2c` on the desktop core (ABI 1.5) the 8BitDo Retro Receiver for the NES/SNES Classic, plugged into Adafruit's adapter on USB-A and paired to a Wii Remote with a Classic Controller, identified as `00 00 a4 20 01 01` and sent only data format 3; TinyNES's entry 52 changed its pad unit to read format 3 and the user passed both setups there, recorded as TinyNES WII-004, and TinyGBC's entry 22 carries the same change, built but not yet deployed. The receiver's manual excludes the Xbox Series X|S controller, which BLE-017 already closes on this board, and the user's Joy-Cons were flat. For moving files in the field the user asked about a USB SD reader or thumb drive on the OTG port and Sipeed's PMOD TF card: the OTG connector does not source VBUS (USB-003), so a drive needs a powered OTG adapter or self-powered hub, untested here, while the software exists in the SDK (CherryUSB's EHCI host and `usbh_msc.c`, FatFs's `fatfs/port/fatfs_usbh.c` and its `usb` volume, `tang_usb_role.c`'s role switch, the shell's `cp`); the PMOD TF card would need an SD controller in the core, a free socket and a FatFs driver over the 2 Mbaud link, and was advised against. A plan was proposed and not approved: a `usbdisk mount | eject | status` command that hosts the drive at `/usb`, logs to the card because the console is gone while hosting, and shows `/usb` in Files. The user also asked whether the console could run over Bluetooth to the PC; BLE-002 and the controller record show `ble1m10s1bredr0` includes the peripheral role, but nothing was designed. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed `.ai/core.md` was not changed and no settled entry was rewritten, and validated this entry as number 26 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and `None.` in Files Modified.

#### Next Steps:

Finish TinyGBC's entry 22 first: its seed-6 build was left running, and its next steps give the deploy and the test. The USB drive plan waits on the user's answers: whether a powered OTG adapter or self-powered hub is on hand, and whether the drive or reader is USB-A or USB-C. A console over Bluetooth would need a GATT serial service on the BL616 as a peripheral beside its central links, a console route like the desktop Terminal's bridge and a host-side bridge, none of it tried; whether the controller library keeps a peripheral link and the keyboard and mouse links together is untested. The fresh card at `/run/media/vash/TEST_DRIVE` holds only `test_1.txt`. Entry 25's open items stand.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 27 COMMIT Unreleased 2026-10-23T16:21:08-07:00

#### Coming From:

Unreleased ff1ccd4

#### Purpose:

Point the firmware at the three settings files the rebuilt card now carries, so the PMOD layout and both players' settings are read from /tinytang.ini, /tinynes.ini and /tinygbc.ini.

#### Outcome:

The user fixed the card's names and directed that they are the layout from now on, so the firmware followed the card rather than the card following the firmware. `ports/bl616/phosphor/pmod_sockets.cpp` opens `/sd/tinytang.ini` and its console messages, the `tangini` help line included, name it; `tang_nes_ini.h` and `tang_gbc_ini.h` point at `/sd/tinynes.ini` and `/sd/tinygbc.ini` with those shown names, so the `tang` command reports them without further change; and the settings writers in `tinynes_app.cpp` and `tinygbc_app.cpp` build their temporary and backup files from the new names, `.tmp` and `.bak` as before. Comments across `tang_ini.c`, `tang_settings_ini.c`, `tang_fpga_uart.c`, `tdsh_platform_bl616.c`, `tang_core_load.h`, `tdsh_tang_flash.c` and `phosphor/oled_cmd.cpp` were corrected in the same pass, the examples became `docs/tinytang.ini`, `docs/tinynes.ini` and `docs/tinygbc.ini`, and the host tests and the README, the manual chapters, `fpga/desktop/README.md`, `fpga/hdmi_test/hdmi_test.v` and the two scripts' comments follow. `test_ini.sh` passed, `test_nes_ini.sh` ran 45 checks and `test_fat_rename.sh` 16, all without failure, and the firmware built with `make CHIP=bl616 BOARD=bl616dk` as `ff1ccd4-dirty.9db4a4e`, 644,848 bytes, SHA-256 `2c947cb737ac50df4ed552cf24ad33417339e91b2b9c8e9fe02212758ac7b4b2`, with no warning in any changed file and only the ten pre-existing ones in the SDK's `usbh_core.c` and `tang_jtag_programmer.c`. `tinytang_flash.py` stages the image on the card with `tangput` before `tangflash` installs it, so a flash with the card out of the board could not proceed and the card went back in; the commit reported OK, the user power-cycled, and the board returned as `ffff:5454`. On the board `platform` read `tinytang ff1ccd4-dirty.9db4a4e`, `tangini` printed `tinytang.ini: layout i2s2_oled` and `core 0x54 (ABI 1.5) has 0xc0 = 0x0010, as declared` from the boot path's load, and `tang` read `/tinynes.ini` with 24 keys, the user's keymap, `aspect=1:1 palette=fceumm scale=3`, `volume=20 mute=no`, and `/tinygbc.ini` with 2 keys at `volume=20 mute=no`; the heap read 47,864 bytes free of 98,456 with a 29,504-byte largest block. The user reported PASS for the change. Two builds of the same tree produced different image hashes, `f3a18b64` and `2c947cb7`, which is the non-reproducibility already recorded and not a change here. The user asked mid-cycle for the Game Boy save, state and ROM folders to be made to work in the same build and then withdrew it on recalling that TinyGBC has no save or state path; TinyNES's own `/root/media/nes`, `/root/saves/nes` and `/root/states/nes` already matched the card, so nothing about the folders changed, and the card's `/root/saves/gb`, `/root/saves/gbc`, `/root/states/gb` and `/root/states/gbc` stay as placeholders. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and entries 1 to 26 were not rewritten, checked that the reference's six YAML blocks parse with the index and records naming the same 147 ids in the same order, and validated this entry as number 27 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The card still holds the previous firmware at `/bl616-firmware.bin`, so the Software Update window would offer to install the older image; replacing it with this build waits on the user's approval to write the card. TinyGBC's saves and states remain its own cycle: its player has no battery-save path and no card build of its state code, and TinyTang has no TinyGBC NVRAM or state transfer, so the four Game Boy folders stay empty until that work is done. On the card, `manual/getting-started.md` and `manual/tinynes.md` are empty, `manual/files.md` and `manual/troubleshooting.md` are older than the repository's copies, and `/root/scripts/boot.tdsh` is the older copy that still names the retired `boot-cart.tdsh`. A `tools/check_manual.py` to enforce the reader's subset is proposed and unapproved.

#### Files Modified:

- README.md
- docs/gbc.ini (renamed to docs/tinygbc.ini)
- docs/nes.ini (renamed to docs/tinynes.ini)
- docs/tang.ini (renamed to docs/tinytang.ini)
- fpga/desktop/README.md
- fpga/hdmi_test/hdmi_test.v
- manual/files.md
- manual/getting-started.md
- manual/tinynes.md
- manual/troubleshooting.md
- ports/bl616/phosphor/oled_cmd.cpp
- ports/bl616/phosphor/pmod_sockets.cpp
- ports/bl616/tang_core_load.h
- ports/bl616/tang_fpga_uart.c
- ports/bl616/tang_gbc_ini.c
- ports/bl616/tang_gbc_ini.h
- ports/bl616/tang_ini.c
- ports/bl616/tang_ini.h
- ports/bl616/tang_nes_ini.c
- ports/bl616/tang_nes_ini.h
- ports/bl616/tang_settings_ini.c
- ports/bl616/tang_settings_ini.h
- ports/bl616/tdsh_tang_flash.c
- ports/bl616/tinygbc_app.cpp
- ports/bl616/tinynes_app.cpp
- scripts/boot.tdsh
- scripts/phosphor.tdsh
- tools/tests/tb_fat_rename.c
- tools/tests/tb_ini.c
- tools/tests/tb_nes_ini.c
- tools/tests/test_fat_rename.sh
- tools/tests/test_ini.sh
- tools/tests/test_nes_ini.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 28 COMMIT Unreleased 2026-10-23T16:34:50-07:00

#### Coming From:

Unreleased 8471d51

#### Purpose:

Replace the card's stale firmware image at `/bl616-firmware.bin` with the build running on the board, so the Software Update window installs the current firmware rather than the previous one.

#### Outcome:

Entry 27 left the card's `/bl616-firmware.bin` holding the image built from `966da0d-dirty.1704dab` while the board ran `ff1ccd4-dirty.9db4a4e`, so the Software Update window would have staged and committed the older build over the newer one. With the user's approval the image that is actually running, `build/build_out/tinytang_bl616.bin`, SHA-256 `2c947cb737ac50df4ed552cf24ad33417339e91b2b9c8e9fe02212758ac7b4b2` at 644,848 bytes, was written over `/bl616-firmware.bin` with `tools/tinytang_put.py` through the board's own console; `tangput` reported 644,848 bytes written and the shell's `ls -l` read back 644,848. Nothing was rebuilt: the running image is the tested one, and a fresh build would have carried a different identity and a different byte stream, since the image is not reproducible. The card is in the board and was not mounted on the host during the write. No stronger check was available on the board: the shell has no hash command, `tangflash` only stages and commits with no verify-only mode, and reading 644 KB back over the console is not practical, so the card's copy is evidenced by the byte count and by the image's identity on the host; the Software Update window re-verifies the header and CRC when it stages the file. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and entries 1 to 27 were not rewritten, checked that the reference's six YAML blocks parse with the index and records naming the same 147 ids in the same order, and validated this entry as number 28 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and `None.` in Files Modified.

#### Next Steps:

The card's update slot now matches the running firmware; installing it would re-commit the same build, which is useful as recovery rather than as an upgrade. The open items of entry 27 stand unchanged: the Game Boy save and state folders wait on TinyGBC's own cycle, the card's manual chapters and `boot.tdsh` are older or empty, and `tools/check_manual.py` remains proposed and unapproved.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 29 COMMIT Unreleased 2026-10-23T17:00:28-07:00

#### Coming From:

Unreleased cf8018b

#### Purpose:

Put each game core's `.fs` beside its `.bin` on the card, as the desktop core already had, so a one-wire JTAG load has a bitstream for every core the card carries.

#### Outcome:

The user had deleted the retired ABI 1.4 pair (`tinytang.bin.abi14` and `tinytang.fs.gz.abi14`) and asked for the FS files of the two game cores beside their BINs. Both builds were on this machine and each was proved against the image already on the card before anything was written: running `tools/fs_to_bin.py` over TinyGBC's `build/ae350_soc/ae350_soc.fs` (37,362,149 bytes, SHA-256 `1f1b64e5…`) reproduces the card's `tinygbc.bin` (`78d01d47…`, 4,666,860 bytes) byte for byte, and the same over TinyNES's `build/ae350_soc/ae350_soc.fs` (38,252,773 bytes, SHA-256 `2a356060…`) reproduces the card's `tinynes.bin` (`5cd4ab00…`, 4,777,964 bytes). They were written as `gzip -9 -n` companions, matching the desktop core's `tinytang.fs.gz`: `/root/cores/console138k/tinygbc.fs.gz`, 867,585 bytes, and `tinynes.fs.gz`, 795,315 bytes, written to the card while it was mounted on the host; decompressing each from the card returned exactly the source `.fs` hashes above. Neither is read by the board — `tangload` takes the `.bin`, and the gzipped `.fs` is a companion for the one-wire path that loads the text bitstream over JTAG (TOOL-012) — so nothing on the board changed and no test applies. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and entries 1 to 28 were not rewritten, checked that the reference's six YAML blocks parse with the index and records naming the same 147 ids in the same order, and validated this entry as number 29 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and `None.` in Files Modified.

#### Next Steps:

The card now carries a `.fs.gz` beside each of its three cores; the retired ABI 1.4 pair is gone, as the user deleted it, so the rollback to the PMOD-I2C desktop core no longer exists on the card. Entry 28's items stand: the card's `/bl616-firmware.bin` is the running build, the Game Boy save and state folders wait on TinyGBC, the card's manual chapters and `boot.tdsh` are older or empty, and `tools/check_manual.py` remains proposed and unapproved.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 30 COMMIT Unreleased 2026-10-23T17:07:02-07:00

#### Coming From:

Unreleased 007f4ee

#### Purpose:

Populate `/root/media/gb` and `/root/media/gbc` on the card from the MiSTer card's Game Boy and Game Boy Color sets, in lettered folders like the NES library.

#### Outcome:

The MiSTer card was mounted at `/run/media/vash/MiSTer_Data` and its sets are `games/Gameboy` and `games/GBC`, not a `GB` folder: 600 and 543 zip files. Each was copied into a folder named for its first character — `A` to `Z` uppercased, the digit itself, and `#` for the one name beginning with a bracket — giving `/root/media/gb` with 600 files in 27 folders and `/root/media/gbc` with 543 files in 31 folders, 427 MB used on the card in total. Names were copied unchanged. `mister-boot.gb`, MiSTer's own boot ROM, and the `Palettes` folder were not copied. The counts on the card match the source sets exactly, five randomly chosen archives from each folder set passed `unzip -t`, and the largest folder holds 75 files, well inside the library's 256-entry ceiling. Two limits of the window now bear on the result and were reported to the user: `lib_choose_root()` opens only the first media root that exists and lists nothing else, so with `/root/media/gb` present the 543 GBC files are not shown at all; and `lib_listable()` drops any name at or over `LIB_NAME_BYTES` (56), which hides 57 of the GB names and 155 of the GBC names. Neither is a fault in the copy. No repository file changed. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and entries 1 to 29 were not rewritten, checked that the reference's six YAML blocks parse with the index and records naming the same 147 ids in the same order, and validated this entry as number 30 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and `None.` in Files Modified.

#### Next Steps:

The GBC library is on the card but unreachable from the window while `gb` exists, and 212 of the 1,143 names are too long to list. Both are TinyTang's `ports/bl616/tinygbc_app.cpp`: the library could list every GB root in one view instead of only the first, and the 56-byte cap could be named in the status line or raised, which costs about 40 bytes per entry across 256 entries of static memory that the board's free heap would lose. Shortening the 212 names on the card is the alternative that needs no firmware change. The user is to choose; nothing has been changed in the repository or the app.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 31 COMMIT Unreleased 2026-10-23T17:09:50-07:00

#### Coming From:

Unreleased 0284dab

#### Purpose:

Shorten the 212 Game Boy and Game Boy Color names that the TinyGBC window's 56-byte limit hides, so every cartridge in the two new libraries can be listed and started.

#### Outcome:

The user chose renaming over widening the window, noting that the NES library had been treated the same way, which its 47-byte longest name is consistent with. A deterministic rule was applied on the card: trailing parenthetical groups are dropped from the end while the name is at or over 56 bytes, then the stem is cut at the last space, hyphen, underscore or comma past 60 percent of the allowance, a cut that would leave an unbalanced parenthesis is backed off to before it, and a collision appends ` ~2`, ` ~3` and so on. 57 names in `/root/media/gb` and 155 in `/root/media/gbc` were renamed, none now exceeds 55 bytes, the longest is `World Cup USA 94 (Europe) (En,Fr,De,Es,It,Nl,Pt,Sv).zip` at 55, the zip counts remain 600 and 543, the byte totals remain 61,209,403 and 242,645,015, no folder holds two names that differ only in case, and eight randomly chosen renamed archives across the two sets pass `unzip -t`. Each set gained a `RENAMED.txt` at its root holding `folder/original -> new` for every rename; the library ignores it because it is not a `.gb`, `.gbc` or `.zip`. Losing a region or enhancement tag from the display name is the cost, and the manifest is what restores it. No repository file changed. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and entries 1 to 30 were not rewritten, checked that the reference's six YAML blocks parse with the index and records naming the same 147 ids in the same order, and validated this entry as number 31 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and `None.` in Files Modified.

#### Next Steps:

The GBC library is still unreachable from the window, because `lib_choose_root()` in `ports/bl616/tinygbc_app.cpp` opens only the first media root that exists and `/root/media/gb` wins; that needs the app to list every GB root in one view, which is a TinyTang firmware cycle to propose and approve. Entry 30's and 28's items stand otherwise: the card's `/bl616-firmware.bin` is the running build, the Game Boy save and state folders wait on TinyGBC, the card's manual chapters and `boot.tdsh` are older or empty, and `tools/check_manual.py` remains proposed.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 32 COMMIT Unreleased 2026-10-23T17:21:16-07:00

#### Coming From:

Unreleased 6c20dcf

#### Purpose:

Make the TinyGBC window's library open the media root and offer both Game Boy folders, so the monochrome and the Color library are reachable from one window.

#### Outcome:

`ports/bl616/tinygbc_app.cpp`'s `LIB_ROOTS` list, whose `lib_choose_root()` opened only the first root that existed, is gone: `GB_ROOT` is `/root/media` and `GB_DIRS` names `gb`, `gbc` and `gbcolor`, so the library opens one level higher and lists only those folders, in any case, and no loose cartridges at that level. `lib_listable()` gained an `at_root` flag that applies that filter at the media root and leaves every folder and cartridge listable below it; `lib_at_root()`, the `..` handling and the launch path are unchanged, so a cartridge starts from whichever folder it sits in. An empty media root now says `No gb, gbc or gbcolor folder` rather than `0 entries`. The alternative, merging both roots into one list, was rejected because the letter folders would have appeared twice and games shared between the two sets would have been indistinguishable. The firmware built with `make CHIP=bl616 BOARD=bl616dk` as `6c20dcf-dirty.5c5154b`, 644,848 bytes, SHA-256 `90a60ecbba81642d28eb3e3a073dac43d5b0cc3cf2b2fbe86d4937b88e3bb1d8`, with no warning in the changed file; `tinytang_flash.py` staged and committed it, the user power-cycled, and `platform` read `tinytang 6c20dcf-dirty.5c5154b`. `/root/media` held `nes`, `gb` and `gbc`, and `/root/media/gb` its `4` and `A` to `Z` folders. The user opened the TinyGBC window and reported that everything passes: `gb` and `gbc` are listed with `nes` absent, both folders open to their letter folders, a game starts from each, and `Up` returns to `/root/media`. Housekeeping followed on the card: the flash's leftover `/tinytang-upload.bin` was removed with the shell's `rm`, and `/bl616-firmware.bin` was rewritten with the running image so the Software Update window installs this build rather than the previous one; the card root then held `root`, `tinygbc.ini`, `tinytang.ini`, `tinynes.ini`, `tinydesk_tz` and `bl616-firmware.bin`. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and entries 1 to 31 were not rewritten, checked that the reference's six YAML blocks parse with the index and records naming the same 147 ids in the same order, and validated this entry as number 32 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The Game Boy save and state folders remain empty placeholders until TinyGBC gains a battery save and a card build of its state code, and its player still has no NVRAM or state protocol for TinyTang to serve. The card's `manual/getting-started.md` and `manual/tinynes.md` are empty, `manual/files.md` and `manual/troubleshooting.md` are older than the repository's copies, and `/root/scripts/boot.tdsh` is the older copy that still names the retired `boot-cart.tdsh`; `tools/check_manual.py` remains proposed and unapproved. The ABI 1.4 desktop core is no longer on the card, the user having deleted it, so there is no rollback to the PMOD-I2C desktop arrangement.

#### Files Modified:

- ports/bl616/tinygbc_app.cpp

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 33 COMMIT Unreleased 2026-10-23T19:37:12-07:00

#### Coming From:

Unreleased 248ed17

#### Purpose:

Bring TinyPhosphor back onto the rebuilt card under the new naming, and fix the two faults that stopped it playing: a lost stream acknowledgement the firmware treated as fatal, and a player image taken from the wrong project.

#### Outcome:

The card's Phosphor set followed the `tinyphosphor` naming: `ports/bl616/phosphor/ae350_play.*` now streams `/sd/root/ae350/tinyphosphor-play.tpi`, `scripts/phosphor.tdsh` became `scripts/tinyphosphor.tdsh` with its `PLAYER=` line and the app's no-core hint following, and README, the app comments and the reference were updated; the core went on as `/root/cores/console138k/tinyphosphor.bin` (TinyPhosphor `build/ae350_e22/sweep/seed-7`, 5,047,788 bytes, SHA-256 `e9460424…`, ABI 1.14) with its `.fs.gz` beside it, the seven FLACs to `/root/media/music`, and the script to `/root/scripts`. The first board tests failed three ways and each was traced. Register reads after a `tangload` timed out until a legacy core-ID frame had been sent, which `tangini` did and `phosphor peek` did not, so `fpga_debug_request` now retries once behind a probe, and three `peek`s then answered with no priming. The player-image stream stalled 215–232 KB into 845 KB at 2 Mbaud and 419 KB at 5 Mbaud, with no acknowledgement and no stream-loader overflow; a resend behind the same probe came back `status 2` with the offset already advanced, proving the frame had landed and only its acknowledgement was lost, so `fpga_file_stream` now treats the acknowledgement's next offset as authoritative and `fpga_stream` retries a frame behind a probe. The stream was pinned to 2 Mbaud while that was being understood. To decide whether the fault was firmware at all, the 2026-10-21 firmware was rebuilt from `6843692` in a worktree: it streamed the same image in 2,725 ms, which proved the transport and the core sound and the regression to be in later firmware, and it also showed the second fault, `the AE350 did not start playing (loader 0x03)` with the loader in RUN and its log holding only `ae350 boot`. The player deployed was Tang-Phosphor's qualified image, 863,764 bytes, CRC-32 `ef1502ed`, chosen on the strength of the inherited PHOS-008 record; TinyPhosphor's own log names its player as 844,964 bytes, `.tpi` SHA-256 `cb77d0d3…`, and TinyPhosphor's player on this core asks for a track and plays. With it, the final build `248ed17-dirty.f41fa49` (644,848 bytes) streamed the player in 9,117 ms and reported `playback complete: 1,439,192 samples, 0 underruns, 48000 Hz`, first sample 10,350 ms after the play, and the user opened the Phosphor window, loaded the core, played a track and reported PASS. The diagnostics were then removed, the wrong `/root/ae350/resident.tpi` and the temporary old-format `/tang.ini` deleted from the card with the flash's leftover `/tinytang-upload.bin`, and `.ai/core-reference.md` revised: EXTCTL-003 is now VERIFIED with the lost-acknowledgement behaviour, and PHOS-008 records that Tang-Phosphor's player is not TinyPhosphor's. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and entries 1 to 32 were not rewritten, checked that the reference's six YAML blocks parse with the index and records naming the same 147 ids in the same order, and validated this entry as number 33 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The cause of the missing stream acknowledgement is not established: it happens at both 2 and 5 Mbaud, roughly every 220 KB, and the retry now hides it at the cost of about a second and a half per track. The fast link is therefore still switched off in `fpga_file_stream` and should be re-tested once the loss is understood. TinyPhosphor's README still names `/root/ae350/resident.tpi`; that is a documentation fix in that repository and needs its own cycle. `/root/media/music/test.flac` is the 1.3 MB test file and stays unless the user wants it removed. The earlier open items stand: TinyGBC's save and state folders, the card's empty or older manual chapters and its older `boot.tdsh`, and `tools/check_manual.py` unapproved.

#### Files Modified:

- README.md
- ports/bl616/phosphor/ae350_play.cpp
- ports/bl616/phosphor/ae350_play.h
- ports/bl616/phosphor/fpga_debug.cpp
- ports/bl616/phosphor/fpga_file_stream.cpp
- ports/bl616/phosphor/fpga_stream.cpp
- ports/bl616/phosphor/pmod_sockets.cpp
- ports/bl616/phosphor/td_phosphor_app.cpp
- scripts/tinyphosphor.tdsh (renamed from scripts/phosphor.tdsh)

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 34 COMMIT Unreleased 2026-10-23T19:58:34-07:00

#### Coming From:

Unreleased 5db3dba

#### Purpose:

Deploy TinyPhosphor's player as an ELF like the two game players, by building the AE350 boot ROM's image header in the firmware instead of shipping a packed `.tpi`.

#### Outcome:

`fpga_file_stream_options` gained a prefix, bytes sent as the stream's first data ahead of the file's range, and `fpga_file_stream` sends it before the data loop; `options.length` then counts the prefix and the payload together. `ports/bl616/phosphor/ae350_play.cpp` reads `/sd/root/ae350/tinyphosphor-play.elf`, and when the file's first four bytes are the ELF magic it parses the image with `ports/bl616/ae350_load.c`, the same reader `ae350run` uses, requires a single 4-byte-aligned loadable segment, computes the payload's CRC-32, builds the boot ROM's 32-byte TPI1 header from the segment's address, the ELF's entry point and the section length, and streams header then payload; a packed `.tpi` still takes the old path unchanged, and a multi-segment ELF is refused with a reason rather than mis-packed, since TPI1 is a flat image. The first build was rejected by the loader with state `0x81`, `AE350_STATE_BAD_HEADER`, and the cause was mine: the magic constant was written `0x31504954`, which little-endian is `TIP1`, instead of `0x31495054`, `TPI1`. Comparing the generated header against the `.tpi` built by `tools/ae350_run.py pack` showed every other field already identical and the payload byte-identical, and with the constant corrected the two headers matched exactly. The firmware built as `5db3dba-dirty.05cbb3b`, was flashed, and after the user's power cycle `phosphor play` of a 28.7 MB FLAC in `/root/media/music/hungarian` reported `player sent in 9772 ms`, first sample at 11,475 ms, and played with 0 underruns, the user hearing it; `phosphor status` read 4,391,537 samples at 48000 Hz at 1:31. The `.tpi` was removed from the card, which now carries `tinyphosphor-play.elf` beside `tinynes-play.elf` and `tinygbc-play.elf`. `.ai/core-reference.md`'s PHOS-008 records the ELF deployment. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and entries 1 to 33 were not rewritten, checked that the reference's six YAML blocks parse with the index and records naming the same 147 ids in the same order, and validated this entry as number 34 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The user reorganized `/root/media/music` into `myst/` and `hungarian/`, and the Phosphor window's library skips directories (`td_phosphor_app.cpp` filters `AM_DIR`), so its list is now empty while the shell plays both folders; folder navigation like the TinyNES and TinyGBC libraries have is the next cycle, as the user directed. The lost stream acknowledgement of entry 33 is still unexplained and the fast 5 Mbaud link still off. TinyPhosphor's README still names `/root/ae350/resident.tpi`. The card's `/root/ae350` no longer holds the TinyCreature or TI-Emu images that were there this morning.

#### Files Modified:

- ports/bl616/phosphor/ae350_play.cpp
- ports/bl616/phosphor/fpga_file_stream.cpp
- ports/bl616/phosphor/fpga_file_stream.h

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 35 COMMIT Unreleased 2026-10-23T20:08:00-07:00

#### Coming From:

Unreleased 5aa6cdd

#### Purpose:

Give the Phosphor window's library folder navigation, so albums kept in subfolders of `/root/media/music` can be browsed and played from the window rather than only from the shell.

#### Outcome:

`ports/bl616/phosphor/td_phosphor_app.cpp`'s listing had filtered `AM_DIR` out, which suited a flat music folder and left the window empty once the user reorganized the card into `myst/` and `hungarian/`. The library now keeps a shown and a real current directory, both starting at the music root, and `load_folder()` lists folders as well as tracks: folders first, each entry carrying an `is_dir` flag, with `..` prepended below the root and suppressed at it so the library cannot wander off into the card. `open_dir()` walks into a folder, or back up by truncating the current path, and reloads; `on_activate` opens a folder or plays a track; Play with a folder selected, or with nothing selected, falls through to `first_track()`; Prev, Next and the auto-advance step use `next_track()`, which skips folders, so none of them can land on `..` or an album; `play_index()` refuses a folder and builds both paths from the current directory; `index_of()` matches against the current shown path, so the playing row is still highlighted; the header line names the current path and its item count; and a reopened window resets to the music root rather than to the album the last one was browsing. The firmware built as `5aa6cdd-dirty.f0c59e0` with no warning in the changed file, was flashed, and after the user's power cycle `platform` read it back. The user opened the Phosphor window and reported PASS: `myst/` and `hungarian/` are listed in place of an empty list, `hungarian/` opens to its seven tracks, `..` returns to the music root, a track starts from the folder, Play with a folder selected falls through to its first track, Prev and Next skip folders, and auto-advance carries on. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and entries 1 to 34 were not rewritten, checked that the reference's six YAML blocks parse with the index and records naming the same 147 ids in the same order, and validated this entry as number 35 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The lost stream acknowledgement of entry 33 is still unexplained and the fast 5 Mbaud link still off; both are worth one cycle once the cause is understood. TinyPhosphor's README still names `/root/ae350/resident.tpi` and needs its own documentation cycle, and its reference should record that its player's home on this card is `tinyphosphor-play.elf`. The earlier card items stand: TinyGBC's save and state folders, the empty or older manual chapters, the older `boot.tdsh`, and `tools/check_manual.py` unapproved.

#### Files Modified:

- ports/bl616/phosphor/td_phosphor_app.cpp

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 36 COMMIT Unreleased 2026-10-23T20:15:21-07:00

#### Coming From:

Unreleased 341802c

#### Purpose:

Repair `scripts/tinyphosphor.tdsh`, whose player check still looked for the `.tpi` deleted when the player became an ELF, so the script aborted before loading anything.

#### Outcome:

The script's only use of the player path is a presence check ahead of the load; the playback itself goes through `phosphor play`, which already resolves `/root/ae350/tinyphosphor-play.elf`. `PLAYER=` now names that file, the repo copy and the card copy were updated with `tools/tinytang_put.py` (3,119 bytes, the same size because `.tpi` and `.elf` are both four characters), and the script was then run from the console: `tangload` loaded the core with the layout declared as `0xc0 = 0x0510`, `fpga` answered `core 80`, `phosphor poke 0xac 0x13` set the scope's look, `osd off` handed the screen over, and the run ended at `phosphor: up` rather than returning at the player check as it would have done before the fix. The user asked whether the two game systems need a comparable script and they do not: both launch entirely in-process from their windows, `tang_core_load()` then `tang_ae350_run()` then the desktop core back, and neither app mentions a `.tdsh`; the console equivalent for a game is the two commands without a script. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and entries 1 to 35 were not rewritten, checked that the reference's six YAML blocks parse with the index and records naming the same 147 ids in the same order, and validated this entry as number 36 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The lost stream acknowledgement of entry 33 and the fast 5 Mbaud link still off are the remaining Phosphor engineering items. TinyPhosphor's README still names `/root/ae350/resident.tpi`. The card's other standing items are unchanged: TinyGBC's save and state folders, the empty or older manual chapters, the older `boot.tdsh`, and `tools/check_manual.py` unapproved.

#### Files Modified:

- scripts/tinyphosphor.tdsh

#### Status:

- Build: N/A
- Deployment: PASS
- User Test: NOT RUN

---

## 37 COMMIT Unreleased 2026-10-23T20:24:17-07:00

#### Coming From:

Unreleased 3138403

#### Purpose:

Rename the two game windows and the Phosphor window in the desktop's start menu so all three carry the same `Tiny*` name the rest of the card uses.

#### Outcome:

The window titles were already `TinyNES` and `TinyGBC`, but the registered app names the desktop lists read `TinyNES Control Deck` and `TinyGBC Control Deck`; they are now `TinyNES` and `TinyGBC`, and the Phosphor app, whose title and registered name were both `Phosphor`, is now `TinyPhosphor` in both. The file-header comments in `tinynes_app.cpp`, `tinygbc_app.cpp` and `td_phosphor_app.cpp` follow, as does the comment in `docs/tinynes.ini` that names the app; the only remaining mentions are historical comments in `tools/tests/tb_fat_rename.c` and `test_fat_rename.sh` describing what the app did in earlier cycles. The firmware built as `3138403-dirty.3dc7b84` with no warning in the changed files, was flashed, and after the user's power cycle the user opened the start menu and reported PASS for all three names and the Phosphor window's title bar. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and entries 1 to 36 were not rewritten, checked that the reference's six YAML blocks parse with the index and records naming the same 147 ids in the same order, and validated this entry as number 37 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

Tiny89 is the next system to bring up like the other three, starting with an exit in its player: the diagnostic runs until the board is reset, so a windowed launch could never return to the desktop. Its core already takes input from the keyboard over the link rather than a pad on PMOD0, which suits the current `i2s2_oled` layout. The Phosphor engineering items stand: the lost stream acknowledgement and the fast 5 Mbaud link still off.

#### Files Modified:

- docs/tinynes.ini
- ports/bl616/phosphor/td_phosphor_app.cpp
- ports/bl616/tinynes_app.cpp
- ports/bl616/tinygbc_app.cpp

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 38 COMMIT Unreleased 2026-10-23T20:33:49-07:00

#### Coming From:

Unreleased 6c741a0

#### Purpose:

Give the desktop a Tiny89 window that lists the card's calculator images and starts a session, so the fourth system launches the way the other three do.

#### Outcome:

`ports/bl616/tiny89_app.cpp` is the window: a fixed 50x20 frame titled Tiny89 with a library over `/root/media/ti89` that lists folders and images (` .89u`, `.89t`, `.raw` and `.rom`, case-insensitive), enters a folder and climbs back with `..`, and shows the current path and count in its status line. Launch takes the selected image and runs `tang_core_load(tiny89.bin)`, then `tang_ae350_run(tiny89-play.elf, <image>)`, then `tang_core_load(tinytang.bin)` in an 8 KiB task of the app's own, the same shape the two game windows use, and the window survives because it lives on the BL616; if the core load is busy or fails it says so rather than starting half a session. The app is registered in `td_desktop_bl616.c` and built from `CMakeLists.txt`. The card took the set in the new naming: `tiny89.bin` from Tiny89's `tiny89_soc_kbd` build (4,698,604 bytes, SHA-256 `ec9b3027…`) with `tiny89.fs.gz` (712,813), `tiny89-play.elf` (5,657,172 bytes, SHA-256 `52b77b39…`, the build carrying the exit Tiny89's entry 18 added) and `/root/media/ti89/TI89Titanium_OS.89u` (1,400,021 bytes). Tiny89's core takes its input from the keyboard over the link rather than a pad on PMOD0, so the current `i2s2_oled` layout needs no change and the OLED is not displaced. The firmware built as `6c741a0-dirty.67aa894` with no warning in the new file, was flashed, and after the user's power cycle the user ran the session and reported PASS: the library listed the image, Launch booted the calculator on HDMI after the player's long stream, the Bluetooth keyboard typed on it, F12 swapped between TinyDesk and the calculator, and Control with Alt and q returned to the desktop. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and entries 1 to 37 were not rewritten, checked that the reference's six YAML blocks parse with the index and records naming the same 147 ids in the same order, and validated this entry as number 38 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The calculator's RAM save still speaks the TinyNES NVRAM protocol, so it would land in `/root/saves/nes`; naming the product in that protocol and giving this firmware a per-system save folder is the next cycle, and Tiny89's entry 17 leaves an 18-byte loss in a large byte payload open, which is what a 4 MiB flash write depends on. The card's other standing items are unchanged: the Phosphor stream's lost acknowledgement and the fast link still off, TinyPhosphor's README naming `resident.tpi`, TinyGBC's save and state folders, the empty or older manual chapters, the older `boot.tdsh`, and `tools/check_manual.py` unapproved.

#### Files Modified:

- CMakeLists.txt
- ports/bl616/td_desktop_bl616.c
- ports/bl616/tiny89_app.cpp

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 39 COMMIT Unreleased 2026-10-24T01:16:05-07:00

#### Coming From:

Unreleased 9bda7e3

#### Purpose:

Put a real date and time on the save log, make the card's write timing visible, and give Tiny89's window a page that says whether its save verified, so a failed save stops being invisible.

#### Outcome:

Every line `ae350_log()` writes now begins with the local ISO-8601 time, `2026-10-24T00:56:43-07:00`, taken from the RTC-kept clock: UTC from `tang_clock_now()`, the zone from `/sd/tinydesk_tz` (the same file the desktop and `tangi2c` read), and a civil-date conversion in the file so no time library is needed. A clock that has never been set writes `no-clock` rather than a date it invented, which is what the log shows until the desktop has synced the RTC after a power-up. Each save now also records what the card cost: `card <path>: <n> writes, <ms> in all, max <ms>, <n> over 5 ms`, measured around `f_write`, because the longest write is the window in which arriving payload bytes are lost. The `STATE` channel needed no firmware change -- `ae350_channel_line()` and `ae350_path_for()` have served `/root/states/<system>/` since TinyNES -- and Tiny89's new state save now lands there; what the firmware did need was nothing at all, which the log confirms. `ports/bl616/tiny89_app.cpp`'s Saves page became three channels, State, RAM and Flash, filtered to the image selected in the Library because the log spans sessions and images: it shows the file sizes, the board's last verdict per channel with the stamp, and a dim line that says "Did not verify -- Ctrl+S again, then Refresh" when a save came back short. That line is the change with the most evidence behind it: the user's own missing variable turned out to be a save that failed at 00:44:10 and an older state restored afterwards, a failure the window had no way to show. The firmware built as `9bda7e3-dirty.aa03010` with no warning in either file, was flashed and power-cycled; the log then carried stamped lines for every event, a whole state save at `00:56:43` with `515 writes, 343 ms in all, max 24 ms, 3 over 5 ms`, and the user reported PASS on the cycle that followed. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and entries 1 to 38 were not rewritten, checked that the reference's YAML blocks parse with the index and records naming the same ids in the same order, and validated this entry as number 39 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The Saves page still lists the `.ram` file that Tiny89 no longer writes on a save, so it reads as stale history beside the state; either drop it or label it as the fallback. The stamps depend on the desktop having run since power-up, so a session started straight from the shell writes `no-clock`; syncing the clock from `ae350run`'s own start would close that. The save remains one-way, so the page reports the board's verdict only after the fact -- the retry is the user's.

#### Files Modified:

- ports/bl616/tang_fpga_uart.c
- ports/bl616/tiny89_app.cpp

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 40 COMMIT Unreleased 2026-10-24T03:14:00-07:00

#### Coming From:

Unreleased 159e65d

#### Purpose:

Let a session be handed one file to send to the calculator over its link, so Tiny89 can offer software that is not in the flash image.

#### Outcome:

`ae350run` takes an optional third argument naming the file to send, and `tang_ae350_set_send_file()` records it for the next run; the channel table gains `SEND`, and a program asking `Tiny89 SEND WANT <address> <room>` is served that exact file through the loader with the same block checksums every other file gets. Unlike the saves, the name is the caller's rather than a derivation, which is why it is remembered rather than built from the data file's base. The shell command's usage line and description now say so. The firmware built as `9bda7e3-dirty` with no warning in the changed file and was flashed. With it in place Tiny89 fetched and parsed `/root/media/ti89/apps/TINOTES.89k` (22,051 bytes) on the board, and the transfer that followed failed on the calculator side -- Tiny89's entry 20 records that -- so the channel itself is proven and the protocol is not. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the `.ai/` diff, confirmed `.ai/core.md` unchanged and entries 1 to 38 not rewritten, checked the reference's six YAML blocks parse with 147 index ids matching 147 records in order, and validated this entry as number 40 of 100 with six sections, prose, allowed Status values and eligible paths only.

#### Next Steps:

The Link tab in the Tiny89 window: a folder of calculator files, a Send button, and a launch that passes the chosen file as this third argument and lands on the saved Receive state. The protocol question -- what an application's RTS must say -- is Tiny89's to settle first; this channel only has to carry the file.

#### Files Modified:

- ports/bl616/tang_ae350_run.h
- ports/bl616/tang_fpga_uart.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: N/A

---

## 41 COMMIT Unreleased 2026-10-24T20:48:25-07:00

#### Coming From:

Unreleased b964e76

#### Purpose:

Put the card's USB-A I2C devices behind the PCA9548A switch the user fitted, so the RTC and the Wii adapter are each reached on their own channel without any `/tinytang.ini` key, as the user approved.

#### Outcome:

The desktop core's bus now goes through the switch. `ports/bl616/tang_i2c_mux.c` and `tang_i2c_mux.h` hold the bench's fixed map -- channel 0 the Wii adapter at 0x52, channel 1 a magnetometer, channel 6 the DS3231 at 0x68, channel 7 an SHT45 -- and the routing rule: those two mapped addresses always select their own channel, the switch's own address never does, and any other address stays on whichever channel `tangi2c mux` selected, which is how an unmapped sensor is reached. `ports/bl616/tang_i2c.cpp` probes the switch once at 0x70 and caches the answer, selects a channel ahead of every known-device transaction under the same recursive bus lock, walks all eight channels with `tangi2c scan` and removes the switch's own reply from each count, gained `tangi2c mux <0-7>|off|status`, and refuses `tangi2c auto on` while the switch is fitted because the engine's once-a-second read in `0x158` cannot select a channel; `ports/bl616/tdsh_tang_flash.c` points the switch at channel 0 before programming any core, so a game core that drives H13/G13 itself still finds its controller, and `tools/tests/tb_tang_flash.c` asserts that step's place in the load order. With no switch fitted the same firmware probes 0x70, gets no answer and talks on a direct bus, so one image serves both wirings. On the board the firmware before this cycle found 0x70 and reported no RTC at all, and the deployed one reports `pca9548a at 0x70`, scans channel 0 with 0x52, channel 1 with 0x1c, channel 6 with 0x68 and channels 2 to 5 and 7 with only the switch, reads `0x3d` at `0x1c/0x0f` on channel 1 after `tangi2c mux 1` -- the LIS3MDL's identity value, so the user's magnetometer is already fitted -- and set and read back the DS3231 through channel 6 (`2026-10-25 03:34:30` UTC, OSF clear, local `2026-10-24 20:34:31` at the `-25200` second zone) with the time surviving the flash and reboot. The first deployed build had `tangi2c mux` status write 0x00 to the switch to report it, which deselected the channel it was reporting; the switch cannot be read without being written, because the engine sends a register byte ahead of a read and the part takes that byte as its control value, so the probe now writes back the value it believes is selected, and the corrected firmware `b964e76-dirty.a70bd07`, 657,264 bytes, SHA-256 `97c81534da73b09aba85bb8445a251ba09f8fc2a0949f05b6a97bec7c5e8937e`, built with no warning in any changed file, was flashed and the user then reported PASS on the desktop clock and on a game still seeing the Wii Classic Controller. No SHT45 answers on channel 7, nor at 0x44 or 0x45 when that channel is selected, so the sensor the user has powered there is not on the bus; that is left as a bench question rather than a firmware one. `tools/tests/tb_i2c_mux.c` and `test_i2c_mux.sh` lock the map and the routing rule, and of the 26 host tests 25 pass: `tools/check_help_width.py` had been failing since entry 40 on `ae350run`'s 102-column help line, which is now 59 columns and the command's meaning unchanged, and `tools/tests/test_phosphor_catalog.sh` was already red, compiling `td_phosphor_app.cpp` without `-I ports/bl616` and, once that path is added, asserting the pre-entry-35 music root; it was left untouched for its own cycle. `.ai/core-reference.md` gained BRD-012 for the switch, its control register, the read-without-write limitation and the routing rule, BRD-011 gained the revision that the devices now hang off it, and the routing table and fast lookup index gained BRD-012. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and entries 1 to 40 were not rewritten, checked that the reference's six YAML blocks parse with the index and records naming the same 148 ids in the same order and that every record has its ten fields, and validated this entry as number 41 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The SHT45 needs isolating on the bench: move a device that already works onto channel 7, or move the SHT45 onto a chain that works, to separate a dead channel from a cable that is carrying power and no data. `tools/tests/test_phosphor_catalog.sh` needs its own cycle to be brought back to the app's current music-root behaviour. The magnetometer at 0x1c and the SHT45 are otherwise undriven: reading them means adding their commands, not extending the switch. The map stays fixed in firmware until the user asks for `/tinytang.ini` keys, and the standing items are unchanged.

#### Files Modified:

- README.md
- ports/bl616/tang_fpga_uart.c
- ports/bl616/tang_i2c.cpp
- ports/bl616/tang_i2c_mux.c
- ports/bl616/tang_i2c_mux.h
- ports/bl616/tdsh_tang_flash.c
- tools/tests/tb_i2c_mux.c
- tools/tests/tb_tang_flash.c
- tools/tests/test_i2c_mux.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 42 COMMIT Unreleased 2026-10-24T22:26:29-07:00

#### Coming From:

Unreleased 6099d54

#### Purpose:

Make the core-load path actually point the PCA9548A at the Wii adapter's channel before a game core takes the bus, correcting the no-op preselection entry 41 shipped.

#### Outcome:

The user reported that the Wii Classic Controller no longer drove a game after entry 41, and the cause was mine: `tang_i2c_mux_for_game_core()` was called after `tang_ini_core_replacing()`, whose first act is to clear `tang_core_i2c_bus()`, so the hook saw no bus, took its early return and never wrote the switch. The switch therefore stayed on the RTC's channel from the clock's last read, the game core polled 0x52 on segment 6, and the controller answered nothing. The call now sits ahead of both `*_replacing()` hooks, where the desktop core is still driving H13/G13 and can perform the select. `tools/tests/tb_tang_flash.c` could not have caught it, because its stubs recorded only that a hook ran; they now model the bus's ownership, held by the desktop core until `tang_ini_core_replacing()` gives it up and restored by `tang_ini_core_loaded()`, and the hook records `mux-select-NOBUS` instead of `mux-select` when it fires without the bus, so reinstating the old order fails three checks with that marker. The firmware built as `6099d54-dirty.aa86eb6`, 657,280 bytes, SHA-256 `44ed4fd5cfc1bf3e891fa96508a0e7ce92d524b0fc1414409c31c588e42cf7d0`, with no warning in either changed file, and was flashed; that image also carried the still-unverified Saves-page work whose own cycle follows, so the deployed binary is not this commit alone. After the power cycle the user launched a game and reported PASS on the Classic Controller. One residual consequence is recorded rather than fixed: a failed core load leaves `tang_core_i2c_bus()` cleared until the next successful load, so `tangi2c` refuses meanwhile and that next load's select is also a no-op. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and entries 1 to 41 were not rewritten, checked that the reference's six YAML blocks still parse with the index and records naming the same 148 ids in the same order, and validated this entry as number 42 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The Saves-page cycle that is already built into this image needs its user test -- a save made with HOME and L, then the TinyNES window's Saves page showing the time -- before its own entry can be written. After that the failed-load consequence above is worth a small cycle of its own: a load that does not program should restore `tang_core_i2c_bus()` for the core that is still running, or the firmware should re-derive it from the core that answers rather than from whether a load happened. The standing items are unchanged, and the SHT45 on the switch's channel 7 still answers nothing while the switch, the RTC, the magnetometer and the Wii adapter are all proven through it.

#### Files Modified:

- ports/bl616/tdsh_tang_flash.c
- tools/tests/tb_tang_flash.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 43 COMMIT Unreleased 2026-10-24T22:29:57-07:00

#### Coming From:

Unreleased f10cd4c

#### Purpose:

Give the TinyNES window's Saves page the real time of each save, read from the board's RTC-stamped save log, and share that log's parser with the Tiny89 window instead of keeping a second copy.

#### Outcome:

The page's last line had said "Created: unknown, the board has no clock" since it was written, which the switch work made false: the firmware has stamped every line of `/sd/root/saves/last.log` with the RTC-kept clock's local ISO-8601 time since entry 39, and the Tiny89 window already read that log. The parsing moved out of `ports/bl616/tiny89_app.cpp` into `ports/bl616/td_saves_log.c` and `td_saves_log.h`: one reader folds a log a chunk at a time into a per-destination record, keeping a half-read line across read boundaries, recognising the firmware's `arm <path>: ...` and `finish <path>: <got> of <want> bytes, crc <a> of <b>, <verdict>` lines only for the destination whose path ends in the caller's needle, and turning `2026-10-24T20:41:05-07:00` into `2026-10-24 20:41` while leaving `no-clock` and a line written before stamps existed as themselves rather than passing either off as a date. `ports/bl616/tinynes_app.cpp`'s `state_show()` reads the log once per page for both of a cartridge's files and replaces the hardcoded line with the state's record -- `Saved 2026-10-24 20:41, whole, 262144 B` -- distinguishing an attempt that started and never finished, one whose clock was unset, and one with no record at all, and the static "and written when the game ends" sentence under the battery-save folder became that file's own record, so both saves are dated. `tiny89_app.cpp` kept its wording and now uses the shared reader. `tools/tests/tb_saves_log.c` and `test_saves_log.sh` cover no log, a finished save, one that came back short, an arm that never finished, a newer arm superseding a verdict, two cartridges and two files interleaved, a line split across a read boundary and inside the path, `no-clock`, a legacy unstamped line, CRLF and an overlong line, and three planted faults were shown to fail it: dropping stamp recognition gives 21 failures, dropping the path filter 7, and dropping the partial line between reads 3; the overlong-line case does not discriminate and is a crash-safety assertion instead. The rest of the host suite passed except `tools/tests/test_phosphor_catalog.sh`, whose breakage predates this work and is recorded in entry 41. `tools/check_help_width.py` found no entry over 80 columns. Nothing new was built or flashed for this entry: the work was already in the image entry 42 deployed, `6099d54-dirty.aa86eb6`, 657,280 bytes, SHA-256 `44ed4fd5cfc1bf3e891fa96508a0e7ce92d524b0fc1414409c31c588e42cf7d0`, and the user then saved with HOME and L, reopened the TinyNES window's Saves page and reported PASS on the time and on the battery-save line. The README's description of that page follows. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and entries 1 to 42 were not rewritten, checked that the reference's six YAML blocks still parse with the index and records naming the same 148 ids in the same order, and validated this entry as number 43 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

Files on the card still carry no real date, because turning that on means `FF_FS_NORTC` off and a `get_fattime()` against the RTC, which would give the Editor and Files views a date too and is its own cycle. `/root/saves/last.log` is appended to and never rotated, so it grows for the life of the card; a size cap that keeps the newest lines would bound it. The failed-load consequence entry 42 recorded is still parked. The SHT45 on the switch's channel 7 still answers nothing, and the control test that would separate a dead port from a dead sensor -- a device that already answers, moved to that channel -- has not been run.

#### Files Modified:

- README.md
- ports/bl616/td_saves_log.c
- ports/bl616/td_saves_log.h
- ports/bl616/tiny89_app.cpp
- ports/bl616/tinynes_app.cpp
- tools/tests/tb_saves_log.c
- tools/tests/test_saves_log.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 44 COMMIT Unreleased 2026-10-25T14:36:13-07:00

#### Coming From:

Unreleased 462f6e2

#### Purpose:

Rename the switch's channel 7 for the MLX90614 now fitted there and drop the magnetometer from the fixed channel map, the user having replaced both that sensor and the upstream cable that was silently taking the whole bus down.

#### Outcome:

The user replaced the cable from the dock's USB-A port to the PCA9548A with one whose two centre conductors were crossed, and with it fitted nothing on the bus answered: `tangi2c mux` reported no switch at 0x70, `tangi2c scan` found nothing across 0x08 to 0x77, and direct reads of 0x70, 0x68 and 0x44 each NACKed, while `fpga` still reported core 84 (0x54, the desktop core) and `tangi2c status` showed nack at sequence 0, so the core and the engine were alive and everything behind them was silent; the crossing was what the user found on inspection, and re-terminating the pair with SCL on D- and SDA on D+ brought the switch back with no firmware change, which also establishes that the firmware cannot tell a crossed pair from an absent chain (BRD-011). The restored bus scanned channel 0 with 0x52, channel 6 with 0x68 and channel 7 with one device at 0x5a, and `tangi2c read 68 00 7` returned 35 28 21 07 25 10 26, the correct UTC. The user reported that the SHT45 which had named channel 7 is broken, that the magnetometer is disconnected, and that channel 7 now carries Adafruit product 1982, an MLX90614 IR thermometer, at its factory address 0x5A; that part ACKs its address but returns 00 for every register read (0x03, 0x06, 0x07 and 0x0e tried), which the DS3231 does not do through the same engine. The firmware change is a rename: `TANG_I2C_MUX_CH_MAG` and its `mag` name are gone so channels 1 to 5 read unassigned, `TANG_I2C_MUX_CH_SHT45` became `TANG_I2C_MUX_CH_IR` with the name `ir`, and the README's `tangi2c` paragraph follows; `tang_i2c_mux_target()` is untouched because no address ever routed to channel 1, so the Wii and RTC routing is unchanged. `tools/tests/tb_i2c_mux.c` now asserts that channel 1 is unassigned and that the three named channels are distinct, and `test_i2c_mux.sh` passes; `tools/check_help_width.py` reports 138 entries with none over 80 columns. The firmware built as `462f6e2-dirty.ec043f4`, 657,280 bytes, SHA-256 `eec58f69e373c326ca7987a0cedb54c78324f7e8dcd7777219b0419f83c536f2`, with no warning in the changed file, and its image no longer carries the strings `sht45` or `mag`; it was installed with `tools/tinytang_flash.py --port /dev/ttyACM1 --yes` and the user power-cycled the board. `platform` then reported `462f6e2-dirty.ec043f4`, `tangi2c mux 7` read `channel 7 (ir)`, `tangi2c mux 1` read `channel 1 (unassigned)`, `tangi2c scan` named every channel as above, and `tangi2c rtc` read the DS3231 at 2026-10-25 21:34:52 UTC with OSF clear, zone -25200 seconds and local time matching the host; the user reported PASS. Both host tools default to /dev/ttyACM0, which on this workstation is an Arduino NanoESP32 (2341:0070) while the board enumerates as /dev/ttyACM1 (ffff:5454), and the console guard's status probe is what kept any command from being typed into it. `.ai/core-reference.md`'s BRD-012 now carries the new map, the unreadable MLX90614 and the unassigned channels, and BRD-011 carries the crossed-pair result, with a routing row for a silent bus. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and entries 1 to 43 were not rewritten, checked that the reference's YAML blocks parse with the routing table, the index and the records naming the same ids in the same order, and validated this entry as number 44 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The MLX90614 does not read yet, and the next cycle for it is a bench test rather than code: moving the DS3231 onto channel 7, or the MLX90614 onto channel 6, would separate a fault in that segment from a fault in the part, and if the part is sound the leads are the engine's STOP, 200 us then separate read against an SMBus Read Word that expects a repeated START, which would mean changing `fpga/desktop/i2c_master.sv` and rebuilding the desktop core, and pull-ups missing on a channel that has never carried a device that answered, the SHT45 having NACKed at 0x44 and 0x45 on it too. Channel 1 stays unassigned unless a device is fitted and given a name, and a later map change is two constants plus this test. The standing items of entry 43 are otherwise unchanged: rotation for `/root/saves/last.log`, `FF_FS_NORTC` off with a `get_fattime()` for real card dates, the failed core load that leaves `tang_core_i2c_bus()` cleared, the TinyNES Saves page's stale `.ram` row, and the Tiny89 link tab with its 18-byte payload loss.

#### Files Modified:

- README.md
- ports/bl616/tang_i2c_mux.c
- ports/bl616/tang_i2c_mux.h
- tools/tests/tb_i2c_mux.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 45 COMMIT Unreleased 2026-10-26T20:53:48-07:00

#### Coming From:

Unreleased 416ba34

#### Purpose:

Establish on the bench, before any firmware is written for it, whether Adafruit's MCP23017 8xMOSFET solenoid board can be driven through the switch's channel 7 by the unchanged desktop core.

#### Outcome:

The user removed the MLX90614 from channel 7 for good and fitted the solenoid board there in its place, for a desktop app, first planned in the TinyControl repository and now to be built and logged here, that toggles the board's eight solenoid outputs and reads its channel B inputs. The board was found running `462f6e2-dirty.ec043f4` with no core answering on UART1, so `tangi2c` refused for want of an I2C bus; `tangload /root/cores/console138k/tinytang.bin`, the boot script's own load, brought core 84 back with `tinytang.ini` declaring `i2s2_oled` as `0x0010`. `tangi2c scan` then found 0x20 alone on channel 7, the MLX90614's 0x5a being gone, beside 0x68 on channel 6. Every access was preceded by `tangi2c mux 7`, since the firmware routes only 0x52 and 0x68 and the clock's RTC reads otherwise leave the switch on channel 6. The chip read its datasheet's power-on values, `ff ff 00 00 00 00 00 00` from 0x00, `00 00` at IOCON and zero elsewhere; GPPUB written `7f` read back `7f` and GPIOB then read `7f` with nothing wired; with IODIRA `00`, OLATA `55` and `aa` read back on GPIOA as `55` and `aa`; and IODIRA, GPPUB and OLATA were restored to their power-on values and read back, so register writes, multi-byte STOP-then-read reads and the pointer's auto-increment all work through the existing engine and the board needs no change to `desktop.bin`. At the user's request OLATA `01` and IODIRA `00` were then set and left, so OUT0 is on now; its LED cannot light because the schematic powers every LED from V+, which has no supply yet, and the user deferred the powered test and the grounded-input test. Microchip's current MCP23017 datasheet, DS20001952D of June 2022, makes GPA7 and GPB7 output only and requires IO7 to be set as an output, where the 2007 DS21952B in the user's manual folder calls them bidirectional, so channel B has seven inputs and GPB7 has to be driven. The user decided that outputs keep their state, which the hardware does by itself because nothing on the Tang can reset the chip. `.ai/core-reference.md` gains BRD-013 for the board's I2C side and BRD-014 for its output stages, SOURCED because no stage has conducted, with two routing rows and their index lines, and BRD-012 now records the new channel 7 contents, the stale `ir` name and the MLX90614 question closed unanswered; the reference's YAML blocks parse, and the index and the records name the same 150 ids in the same order with every routed id present. `.ai/core.md` was not touched. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was unchanged and entries 1 to 44 were not rewritten, and validated this entry as number 45 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and nothing listed in Files Modified.

#### Next Steps:

The next cycle is the firmware the user has seen proposed and not yet approved: route 0x20 to channel 7 and rename that channel `gpio` in `ports/bl616/tang_i2c_mux.c` with `tools/tests/tb_i2c_mux.c` to match; add a driver that configures a chip found at its power-on state with latches zero, port A as outputs, GPB0 to GPB6 as pulled-up inputs and GPB7 as an output, and that adopts the latches of a chip it finds already configured; add a `tangio` shell command and a TinyDesk window with eight output toggles, seven input indicators and a not-connected state; then build the BL616 firmware alone, flash it and have the user test it. The powered test of the output stages and the grounded-input test remain for the user once a V+ supply and solenoid ratings are chosen, and BRD-014 stays SOURCED until then. The standing items of entry 44 are unchanged apart from the MLX90614, which no longer applies.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: NOT RUN

---

## 46 COMMIT Unreleased 2026-10-26T21:42:14-07:00

#### Coming From:

Unreleased bf084e1

#### Purpose:

Add a shell command that sets UART1's rate to match a core built for a faster link, so TinyMaps could measure what the BL616 link can actually carry.

#### Outcome:

TinyMaps found its 2 Mbaud link the ceiling on everything it wants to do next and measured it at 190 kB/s, 95% of that rate's wire time; the question was what a faster rate would deliver, and nothing in this firmware could ask for one. The rate can only be changed by `fpga_debug_set_baud`, which accepts 2 or 5 Mbaud only, lives in Phosphor's file-streamer path, and negotiates with the core through the `0x10` extension family before either end moves. For a bench measurement none of that is wanted: the core is built for one rate and the command should simply match it. `fpgabaud <rate>` therefore calls `tang_fpga_set_baud` directly, offering 2, 5, 12 and 16 Mbaud, and the comment says plainly what the risk is -- changing the rate under a core that did not expect it leaves the link silent until the core is loaded again, which is a `tangload` or a power cycle and not a fault. It was built with the xpack RISC-V compiler against the Bouffalo SDK at its pinned commit, the T-Head toolchain being unnecessary, and flashed over USB with `tinytang_flash.py`; the first two attempts appeared to hang because that tool asks `install ... on /dev/ttyACM0? [y/N]` before it does anything and has no terminal to answer with, and `--yes` is the flag for it. After a power cycle `platform` reported `416ba34-dirty.a98ea86` where it had been `462f6e2-dirty.ec043f4`, and the command answered its usage, accepted `fpgabaud 2000000`, and refused `fpgabaud 3000000` as unoffered. The switch and its recovery were then verified against a real core rather than assumed: TinyMaps loaded and answering at 2 Mbaud went silent on `fpgabaud 12000000` and answered again after `fpgabaud 2000000`, so a mis-set rate costs one command. The rate that matters is 5 Mbaud, and with a TinyMaps build for it the link carried 4 MiB in 9.7 s, 431 kB/s against 190. 12 Mbaud is in the command's list but not in reach of the FPGA side yet: its PHY cannot be clocked fast enough on the clocks TinyMaps has, which TinyMaps records in its own log. The user has not flown this firmware himself, which is why the user test is not claimed. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was unchanged and entries 1 to 45 were not rewritten, and validated this entry as number 46 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and one file listed.

#### Next Steps:

Nothing outstanding here; the command is a bench tool and stays deliberately unnegotiated. If a core ever negotiates its own rate, the `0x10` extension's `FPGA_EXT_SET_BAUD` is still the mechanism to implement on both sides, and this command is not a substitute for it -- it exists so a measurement can be taken without it. The Xuantie toolchain was not needed and `tools/fetch-deps.sh --check` still reports the pinned `xuantie-900-gcc` and `oss-cad-suite` entries absent at the paths the manifest expects, while both are present at the paths this build used, so a release cut from this tree would still report those gaps.

#### Files Modified:

- ports/bl616/tang_fpga_uart.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 47 COMMIT Unreleased 2026-10-26T23:26:04-07:00

#### Coming From:

Unreleased a879a92

#### Purpose:

Give the TinyGBC window the Saves page TinyNES's has, for the save states and battery saves TinyGBC's player now keeps on the card.

#### Outcome:

The transport needed no change: `ae350_channel_line` already maps `TinyGBC` to the `gbc` system, so `TinyGBC NVRAM WANT` and `STATE WANT` are answered from `/root/saves/gbc/` and `/root/states/gbc/`, and `NVRAM SAVE` and `STATE DATA` are written there through the verified replace, as TinyGBC's entries 24 and 25 used them. `ports/bl616/tinygbc_app.cpp`'s Saves tab, a placeholder until now, is TinyNES's page with TinyGBC's paths and header: the selected cartridge's `/root/states/gbc/<cartridge>.state` with its size and CRC-32, its `TGS1` header's game CRC-32, frame and gambatte commit, when it was saved and when the cartridge's battery save was last written, both read from `/root/saves/last.log` through `td_saves_log`, and a note that L saves and R loads with HOME. The page is filled whenever its tab is shown, and its widgets are cleared when the window closes. The comments in `ports/bl616/tang_fpga_uart.c` that called the NVRAM and STATE channels TinyNES's alone now say they serve every product, and README's cartridge section names TinyGBC's folders and page. The firmware was built with the default toolchain and SDK as `a879a92-dirty.3c5e535`, 657,328 bytes, SHA-256 `d07c8574793d52e8c7660d0e36bf7092c4157cd4ea824e2ed3f07a77a45b2f6a`, the dirty part including the user's uncommitted changes in `third_party/tinydesk`, which this cycle neither made nor commits; `tools/tinytang_flash.py --yes` staged and committed it, and after the user's power cycle `platform` reported `a879a92-dirty.3c5e535`. The user played TinyGBC with saves and states, opened the Saves page and passed the cycle; the card then held `/root/states/gbc/Pokemon Pinball (USA, Australia) (Rumble Version).state` at 59,661 bytes and two `.sav` files in `/root/saves/gbc/`. The shell's `cat` returned only the first 3,962 bytes of the 24,749-byte `last.log`, and there is no `tail` or `grep`, so the log's newest lines were not read from the host. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was unchanged and entries 1 to 46 were not rewritten, and validated this entry as number 47 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

TinyGBC's next cycle is the cartridge clock for MBC3 and HuC3 games, which needs the date this firmware's RTC keeps handed to the player at launch, likely beside the settings blob, and a place for the clock's `baseTime` beside the battery save. `cat` stopping at about four kilobytes leaves `last.log` unreadable from the host past its start, and a `tail` command, or a fix to `cat`, would let a save be checked from the console. Entry 45's MCP23017 firmware and entry 46's standing items are unchanged.

#### Files Modified:

- README.md
- ports/bl616/tang_fpga_uart.c
- ports/bl616/tinygbc_app.cpp

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 48 COMMIT Unreleased 2026-10-27T00:40:34-07:00

#### Coming From:

Unreleased b10ae68

#### Purpose:

Carry the clock of MBC3 and HuC3 Game Boy cartridges for TinyGBC's player: give it the date at launch, keep the clock on the card beside the battery save, and show it on the TinyGBC window's Saves page.

#### Outcome:

TinyGBC's player counts a cartridge's clock against `time()`, which on its CPU counts from reset unless it is told the date (TinyGBC entry 26). `ports/bl616/tang_fpga_uart.c` answers `TinyGBC SETTINGS WANT` with sixteen bytes instead of two: the volume and the mute from `/tinygbc.ini`, six zero bytes, and `tang_clock_now`'s UTC seconds little-endian, or 0 when the clock has never been set; a player that asks for two bytes still gets the first two, and the console line now ends `clock sent` or `clock not set`. `ae350_channel_line` gained an `RTC` channel, which `ae350_path_for` maps to `/root/saves/<system>/<cartridge>.rtc` through the battery save's verified replace, and a new `ae350_channel_kind` names its file a clock in the console's words, replacing two inline state-or-save tests. `ports/bl616/tinygbc_app.cpp`'s Saves page reads the `.rtc`'s last record from `/root/saves/last.log` with the other two and shows `Clock <when>, <verdict>, <bytes> B`, or that none is kept, on row 12, and the heading on row 10 now names clocks, 47 of the window's 52 columns. README's cartridge section names the clock. The firmware was built with the default toolchain and SDK as `b10ae68-dirty.886b72a`, 657,328 bytes, SHA-256 `cce78dee3e3cb3eb3530e551b74859e8a88628dafe5a9eb88e74901a72793f22`, the dirty part again including the user's uncommitted `third_party/tinydesk` changes, flashed with `tinytang_flash.py --yes`, and `platform` reported it after the power cycle. A launch with `ae350run` from the shell, recorded over the console, logged `clock not set` and stopped relaying after a few seconds; the exit then froze and the board needed a power cycle, which the user identified as a known effect of holding the console during a run, so that run is not counted. From the TinyGBC window the user set Pokemon Gold's clock, saved in the game and exited, the Saves page showed the battery save and the clock each written whole at 00:31, 32,768 and 8 bytes, and after a wait the relaunched game's clock had moved from 10:07 to 10:09; the user passed the cycle. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was unchanged and entries 1 to 47 were not rewritten, and validated this entry as number 48 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The recorded launch found the clock unset after the board was replugged, so a game can start without a date and keep no clock for that session without saying so; syncing the RTC before the settings reply, or noting an unset clock in the TinyGBC window, would close that. Holding the console during an AE350 run still breaks the core unload at exit and deserves its own investigation. Entry 47's `cat` limit and entry 45's MCP23017 firmware stand.

#### Files Modified:

- README.md
- ports/bl616/tang_fpga_uart.c
- ports/bl616/tinygbc_app.cpp

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 49 COMMIT Unreleased 2026-10-27T09:01:04-07:00

#### Coming From:

Unreleased 0f896ca

#### Purpose:

Give the TinyGBC window a working Input page, as TinyNES has one, so the player's buttons are rebound from the desk and the mapping is kept in `/tinygbc.ini` instead of being fixed at build time.

#### Outcome:

The mapping is one `<button> = <controller>` key per Game Boy button under an `[input]` section, written by the window's Save and read back when the window opens. `ports/bl616/tang_gbc_ini.h` gained `TANG_GBC_BUTTONS`, the Game Boy's buttons in the order the player's `pad_joypad[]` holds them, the fourteen controller buttons in the order the player's `pad_source[]` holds them, a `pad_map[]` in the settings struct and `tang_gbc_default_map()` so the page's Restore resets the buttons without moving the volume; `tang_gbc_ini.c` parses the section, names both sides and defaults to what the player did before the mapping existed. `ports/bl616/tang_fpga_uart.c` answers `TinyGBC SETTINGS WANT` with twenty-four bytes instead of sixteen, the mapping last so a player built before it keeps its default, still cut to what the player asked for, and its console line now names the A binding as well as the volume and the clock. `ports/bl616/tinygbc_app.cpp`'s Input tab replaces the "not implemented yet" placeholder with the page TinyNES has: eight rows showing each button and what answers to it, a Change that steps the selected row to the next controller button, Restore, and Save, whose writer rewrites only `[input]` and `[audio]` and keeps every other byte of the file. Two faults belong with it. The first build, `0f896ca-dirty.8e08ee8`, drew an empty rectangle: `td_list` draws nothing until `td_list_set_count()` is called, which the page did not do, as TinyNES's does; the count is in, and a window invalidation now clears the rows' change marks after a save. The firmware was rebuilt as `0f896ca-dirty.ad50850`, 661,456 bytes, SHA-256 `d2ddc5bdb9bcf8978cec42dff7cfde6dcfa022cba944600708a7b3cae73f45bb`, flashed with `tinytang_flash.py --yes` and reported by `platform` after a power cycle. On the board the launcher answered a request of `00000018` bytes with `ae350run: settings /tinygbc.ini, volume=20, clock not set, a=x` and the player reported `TinyGBC GB PADMAP=1,8,3,2,4,5,6,7`, so A answered to X; the user then opened the Input page, changed the mapping to one of their own, saved it and passed the cycle, and the file read back with the shell's `cat` holds `b = y`, `a = b`, the four directions and `select`/`start` unchanged, `volume = 20` as it was, and the comment line above them. Before that fix the mapping could arrive as nothing at all: TinyGBC entry 30 records the head of the request line being lost on the wire one launch in five, leaving the player on its defaults silently, and the repair is in that repository, which now settles the line before every request and asks again for the settings when no answer lands. The board left the USB bus during the cleanup afterwards, with the desktop core not yet reloaded, so the last core the FPGA was given may still be the TinyGBC one; the card holds the player this cycle deployed, 3,244,664 bytes, SHA-256 `40bde651750768c988aa66916cca15d99aa1fa75d293826ed66a8db5bd0a6551`. The user's uncommitted `third_party/tinydesk` changes were left alone. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was unchanged and entries 1 to 48 were not rewritten, and validated this entry as number 49 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The Video page is the last placeholder in the TinyGBC window, and it has nothing to set until the player offers a picture setting. The TinyGBC player's requests for the battery save, the clock and the state file are settled but not retried, so a lost request line there still starts a game silently without its save. Carrying the mapping to the left stick, which TinyGBC's player still does not decode, would add eight axes as sources once the player measures them.

#### Files Modified:

- README.md
- ports/bl616/tang_fpga_uart.c
- ports/bl616/tang_gbc_ini.c
- ports/bl616/tang_gbc_ini.h
- ports/bl616/tinygbc_app.cpp

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 50 COMMIT Unreleased 2026-10-27T09:13:20-07:00

#### Coming From:

Unreleased 57391f3

#### Purpose:

Establish whether the CherryUSB host stack is still required to enumerate on SDK 2.3.36, and reclaim its RAM if it is not, as the first step of the memory work.

#### Outcome:

`CONFIG_CHERRYUSB_HOST` was disabled in `defconfig`, `ports/bl616/tang_usb_role.c` was deleted along with its three-line registration in `tdsh_platform_bl616.c`, and the two settings were documented; on the same tree the ELF went from `text` 631,186, `data` 10,976 and `bss` 348,040 to `text` 615,990, `data` 10,912 and `bss` 300,804, a saving of 47,236 bytes of static RAM and 15,196 of flash, and the `.bin` from 661,456 to 644,784 bytes. The saving is larger than the 22,244 bytes of the two globals that were the original target because the host branch also carried `usb_hc_ehci.c`: `g_usbhost_bus` 11,148, `g_hub_class` 11,096, `g_framelist` 16,534, `ehci_qtd_pool` 6,432, `ehci_qh_pool` 2,400 and `ep0_request_buffer` 1,298 all went. The hypothesis was then tested on the board: the image was flashed with `tools/tinytang_flash.py --yes`, the board was power-cycled, and it enumerated as `ffff:5454`, answered `platform` with the new identity and reported in `crash` a heap of 82,740 bytes free of 133,320 with a 39,212-byte largest block; `usbstat` still read the OTG block and `usbrole` was gone as intended. That falsifies TOOL-001, which held that the flag was required because the host branch brought in the FreeRTOS OSAL: on 2.3.36 `CONFIG_CHERRYUSB_OSAL` has its own Kconfig default of `freertos` whenever FreeRTOS is on, and with the host stack out the image contains zero `usb_osal_*` symbols and still enumerates, so the OSAL was only ever referenced by the host stack and the requirement described the forked SDK in `proj.conf`. `proj.conf`, `README.md` and the reference were corrected accordingly, TOOL-001 was revised in place and USB-005's consequence now records the three-bit device-mode sequence itself, since the file that held it is gone; the user then opened the desktop and reported it good. `tools/check_help_width.py` (140 entries), `tools/tests/test_tang_flash.sh`, `test_terminal_lifecycle.sh` and `test_script_limits.sh` all passed. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and no settled entry was rewritten, checked that the reference's six YAML blocks parse with the index and records naming the same ids in the same order, and validated this entry as number 50 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The memory work continues: read the BL616's efuse device information to establish whether this die supports PSRAM at all, which decides whether the 4 MB `ram_psram` region the SDK and linker already describe is available or whether the remaining work is limited to the display buffers and boot-time fragmentation. Two things are outstanding from this cycle: the board is running the pre-comment build of this code, so a rebuild and one more power cycle would put the committed identity on it, and `third_party/tinydesk` still carries five modified files that keep every build identity `-dirty`.

#### Files Modified:

- README.md
- defconfig
- proj.conf
- ports/bl616/tang_usb_role.c
- ports/bl616/tdsh_platform_bl616.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 51 COMMIT Unreleased 2026-10-27T09:19:11-07:00

#### Coming From:

Unreleased 353556e

#### Purpose:

Establish whether the BL616 supports PSRAM, which decides whether the memory work can add RAM or can only spend less of what it has.

#### Outcome:

`ports/bl616/tang_psram.c` adds a `psram` command that reads the efuse device information and prints it without configuring anything, plus an opt-in `psram init` that refuses to run unless the efuse reports support, because `board_psram_x8_init()` spins in a `while(1)` on a part without PSRAM and does so before the console exists. The command was registered beside `usbstat`, documented in the README, and built clean at `text` 619,582 and `bss` 301,308, 504 bytes more than the previous image, with `tools/check_help_width.py` reporting 141 entries and none over 80 columns. It was flashed with `tools/tinytang_flash.py --yes`, the board was power-cycled, and `psram` answered `package 0 QFN40`, `flash 2 4MB`, `psram 0 NO`, `version 0`: this die has no PSRAM support, which is a capability of the part rather than an unfitted chip, so the SDK's PSRAM driver and the linker's 4 MB `ram_psram` region are unusable here and `CONFIG_PSRAM` must stay off permanently. The heap was unchanged by this cycle at 82,740 bytes free of 133,320 with a 39,212-byte largest block, and the probe itself costs 504 bytes of `.bss`. BL6-002 was revised from an inference about the SDK's `mem.h` into a measured efuse read, and its consequence now records why the option is closed rather than merely absent; the README gained fact 18 for the same reason, and its fact 1 was corrected, which entry 50 had left asserting that the USB host stack is required. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and no settled entry was rewritten, checked that the reference's six YAML blocks parse with the index and records naming the same 150 ids in the same order, and validated this entry as number 51 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

PSRAM is closed for this board, so the memory work is now only about spending less, and the static buffers are the target: `s_front` and `s_back` at 28,808 bytes each for a 120x30x8 desktop, `s_shadow` at 14,400 for the same grid, and two 18,364-byte `td_vterm_t` terminal buffers, together about 109 KB of the 301 KB `.bss`. Note that moving a buffer from `.bss` to the heap is now net-neutral -- it frees one and consumes the other -- so only shrinking a buffer or not having it at all gains capacity, which makes the desktop geometry and the second terminal buffer the first questions to answer rather than lazy allocation. Boot-time fragmentation remains the other item, with the largest free block at 39,212 bytes against 82,740 free before anything has run, and `third_party/tinydesk` still carries five modified files that keep every build identity `-dirty`.

#### Files Modified:

- README.md
- ports/bl616/tang_psram.c
- ports/bl616/tdsh_platform_bl616.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: N/A

---

## 52 COMMIT Unreleased 2026-10-27T09:35:09-07:00

#### Coming From:

Unreleased 5867dc3

#### Purpose:

Reclaim the static memory this port sizes itself, now that PSRAM is closed and the heap is the only budget that can be moved.

#### Outcome:

Four allocations were sized down without touching the display grid, the FPGA core or a vendored source file. `TD_VT_SCROLLBACK` went from 12 to 4, because two terminals each held 12 scrollback rows at 3,840 bytes and four was the floor that keeps `td_vterm.h`'s scrollback array non-empty. `TD_MAX_WIDGETS` went from 96 to 64, 32 slots at 148 bytes, accepting that a non-modal window refused slots opens with `incomplete` set rather than failing loudly. The Help app's `char s_text[12288]` became a pointer allocated in `open_chapter()` and freed in `on_close()`, with the view state cleared alongside the free so `list_item()` cannot read a released buffer, which returns the buffer whenever the window is shut instead of reserving it from boot. The Bluetooth HID diagnostic log went from 128 lines to 64. The ELF moved from `text` 619,582 and `bss` 301,308 to `text` 619,758 and `bss` 272,844, a net 28,464 bytes of static RAM for 176 bytes of code, and on the board `crash` reports a heap of 82,740 free of 133,320 with a 39,212-byte largest block before and 111,028 free of 161,608 with a 67,500-byte largest block after -- the largest block being the figure that decides whether a 16 KB task stack can be placed. `tools/check_help_width.py` (141 entries), `tools/tests/test_osd_desk.sh` (88 checks), `test_terminal_lifecycle.sh`, `test_frames.sh` (24 checks) and `test_key.sh` (53 checks) passed, and the image was installed with `tools/tinytang_flash.py --yes` and run after the user's power cycle. The user then opened the desktop with the Bluetooth mouse connected and reported 92 KB free after boot and PASS, which is the test that covers both risky changes: the smaller widget pool across the TinyGBC, TinyNES and Tiny89 windows, and the Help reader's allocate-and-free path across a close and a reopen. TDESK-002 was revised in the same cycle because it described a 100 by 30 desktop at 24,008 bytes a buffer, whereas this port builds 80 by 45 to match `textdisp_wide.sv` and its buffers are 28,808 bytes each. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and no settled entry was rewritten, checked that the reference's six YAML blocks parse with the index and records naming the same 150 ids in the same order, and validated this entry as number 52 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The remaining static memory worth moving is the desktop's own grid, and it is a joint firmware and FPGA decision rather than a flag: 80 by 45 costs 57,616 bytes in the two screen buffers, 14,400 in the desk shadow and 36,728 in two terminal grids, so trimming rows to 30 would return about 33,600 bytes and also drop the core's block RAM, at the price of a core rebuild and a board test. Two smaller questions stand beside it: whether the desk layer's terminal and the Terminal app's can share one grid, worth 18,364 bytes if they never need independent buffers, and whether the widget pool at 64 is now near its peak, which the pass above cannot answer because a refused slot is silent. The build identity remains `-dirty` because `third_party/tinydesk` still carries five modified files.

#### Files Modified:

- CMakeLists.txt
- ports/bl616/td_help_app.cpp
- ports/bl616/tang_ble.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 53 COMMIT Unreleased 2026-10-27T09:36:18-07:00

#### Coming From:

Unreleased b8768a6

#### Purpose:

Give the TinyGBC window's Video page the two picture settings that do something for this machine -- the palette of a monochrome cartridge and the colour correction of a colour one -- and send them to the player.

#### Outcome:

The aspect setting was deliberately not built: for a 160x144 picture the video unit's `ASPECT` register only picks a width, five columns for 800x720 with square pixels or six for 960x720, so there is no aspect to choose. `ports/bl616/tang_gbc_ini.h` gained the two lists, `TANG_GBC_PAL_*` and `TANG_GBC_COLOUR_*`, whose numbers are the player's own (`TinyGBC firmware/include/tinygbc_video.h`) because they travel in the settings blob, and `ports/bl616/tang_gbc_ini.c` parses them from a new `[video]` section as `palette = off|auto|dmg|pocket|light|gbc` and `colour = off|accurate|fast`; zero means Off in both, which is what a card with no file gets and what the core does anyway. `ports/bl616/tang_fpga_uart.c` fills blob bytes 6 and 7 from them -- the two taken out of the six that were reserved, so the blob stays twenty-four bytes and the clock and the mapping keep their offsets -- and its console line now names the palette and the correction as well as the volume, the clock and the A binding. `tang_ae350_run()` gained `tang_ae350_cartridge_cgb()`, which reads ` CGB=` out of the player's CORE line, is reset at the start of every run and stays -1 when no run reached the core. `ports/bl616/tinygbc_app.cpp`'s Video tab replaces its placeholder with two rows, a Change that steps the selected row and a Save, and `save_settings()` now rewrites `[input]`, `[video]` and `[audio]` while keeping every line outside them byte for byte. The palette row is greyed with `td_theme()->dim` and its Change refuses with a line of explanation when the cartridge selected in the Library is the one the last run reported as a colour cartridge, because a colour cartridge draws its own colours and the player's core returns before reading the palette option at all; a cartridge nothing is known about, which is everything not yet launched and every `.zip`, is left enabled rather than guessed at, since the deck cannot open an archive to read a header. The firmware built as `b8768a6-dirty.e0c2ba0`, 648,960 bytes, SHA-256 `6f4af972de548f02c19b11c34550a9f61f30e3d864491a07a961109db8eab472`, on top of the three commits another agent had just landed on this branch (CherryUSB host stack dropped, PSRAM probed, static memory resized); the first build overlapped that agent's entry 52 commit and was rebuilt once the tree was still, which changed the image hash but not its size. By the user's instruction nothing was flashed and nothing was deployed, so the board still runs `0f896ca-dirty.ad50850` and no part of this cycle has been on hardware. README's window section describes the page, the two keys and why the palette is greyed. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was unchanged and entries 1 to 52 were not rewritten, and validated this entry as number 53 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

Deployment is the open step and waits on the user: flash this firmware and the player TinyGBC entry 31 built, then judge the six palettes and three corrections on a monitor. Until a colour cartridge has been launched once, its palette row stays enabled, so the greyed state is best checked after playing a `.gbc` and then selecting the same file again. The Video page has no row for the picture's size, by design.

#### Files Modified:

- README.md
- ports/bl616/tang_ae350_run.h
- ports/bl616/tang_fpga_uart.c
- ports/bl616/tang_gbc_ini.c
- ports/bl616/tang_gbc_ini.h
- ports/bl616/tinygbc_app.cpp

#### Status:

- Build: PASS
- Deployment: NOT RUN
- User Test: NOT RUN

---

## 54 COMMIT Unreleased 2026-10-27T10:02:24-07:00

#### Coming From:

Unreleased e3af2df

#### Purpose:

Deploy entry 53's Video page and settings reply, and record what the board showed.

#### Outcome:

`b8768a6-dirty.e0c2ba0`, 648,960 bytes, SHA-256 `6f4af972de548f02c19b11c34550a9f61f30e3d864491a07a961109db8eab472`, was flashed with `tinytang_flash.py --yes` and `platform` reported that identity after the power cycle. The settings reply carried the new bytes: with `[video]` holding `palette = off` the console said `ae350run: settings /tinygbc.ini, volume=20, clock not set, a=b, palette off, colour off`, and with `palette = dmg` it said `palette dmg`, both launching a monochrome cartridge, and in each case the player answered `TinyGBC GB VIDEO PALETTE=Off COLOUR=Off` or `PALETTE=DMG COLOUR=Off`. The settings request itself is now answered before the player's core line, which is what the ordering the player moved to requires. Two faults belong with the cycle. `ae350run` did not return to the shell after a run whose program stopped without exiting, twice in one session, each time needing a power cycle; the second left `/tinygbc.ini` holding the test value `colour = accurate` and `/root/ae350/tinygbc-carttest.elf`, a frame-limited player used only for these console runs, on the card with the TinyGBC core still loaded, so the board was not handed back in its normal state. And a run of RetroRegen's test cartridge with `colour = accurate` completed over 300 frames, `VIDEO=859e8f71`, but its console capture lost the player's picture line, so a non-zero colour byte has not been seen arriving on the board; on the PC the same setting changes the picture, `e921f6ea` against the uncorrected `aaddaa41`. The user has not yet judged the page or the palettes on a monitor. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was unchanged and entries 1 to 53 were not rewritten, and validated this entry as number 54 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and `None.` in Files Modified.

#### Next Steps:

The board needs a power cycle and then its normal state put back: the test player deleted, `/tinygbc.ini` returned to the user's own values without the test one, and the desktop core loaded. Then the user judges the two rows and the greyed palette row. `ae350run` not returning after a run that stops without exiting is now the first fault of its own to chase, ahead of the memory work, because it has cost two power cycles in a day.

#### Files Modified:

None.

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 55 COMMIT Unreleased 2026-10-27T10:47:46-07:00

#### Coming From:

Unreleased d10ec9f

#### Purpose:

Record that the firmware entry 54 deployed no longer starts a Game Boy game from the TinyGBC window, so that its log does not read as a working deployment.

#### Outcome:

Launching a cartridge from the TinyGBC window shows the fabric's colour bars and returns to TinyDesk without a game. The player is what dies: `/root/saves/last.log` shows launches at 10:33 to 10:35 reaching the player's save and state requests, which is what a working launch does, and launches from 10:37 onward logging `--- run` and nothing more. Two causes are established and one of them is mine. I started `tangload` twice and let my console reader close the port at twenty to twenty-five seconds, when this image takes about thirty-five, so both loads were cut short and a half-loaded FPGA gives exactly this symptom; but after a power cycle and a clean load, which reported `tangload: core loaded` and left `fpga` answering `core 84`, one console `ae350run` still logged `--- run` and stopped, so the configuration is not the whole story. TinyGBC entry 33 carries the detailed account, including that the player's error line never reached the console: from about 10:36 `ae350run` output nothing but its echoed command, where an hour earlier it printed its whole boot sequence. Nothing in this repository changed after entry 54; the firmware and the card's contents are as that entry left them, and the user took the board back and asked that nothing connect to it, which is honoured. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was unchanged and entries 1 to 54 were not rewritten, and validated this entry as number 55 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and `None.` in Files Modified.

#### Next Steps:

Diagnosis belongs to TinyGBC's entry 33: the benchmark with uCity compiled in through `tools/load_ae350.py --tinytang` first, the CART player second, and the settings ordering as the thing to move if both fail. On this side there is nothing to change until the cause is known; the Video page and the settings reply are in the tree and deployed, and they are not yet exercised.

#### Files Modified:

None.

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: FAIL

---

## 56 COMMIT Unreleased 2026-10-27T11:12:20-07:00

#### Coming From:

Unreleased 8f81f73

#### Purpose:

Record that the build entry 55 carried as failing is accepted, so this log does not leave the deployment recorded as broken.

#### Outcome:

The user launched games from the TinyGBC window and passed the cycle: the launch works again and the Video page sets the picture. `/tinygbc.ini` read back with the shell's `cat` holds `[video]` with `palette = light` and `colour = accurate` between `[input]` and `[audio]`, written there by the page's Save, with the mapping and the volume at 20 untouched and the sections in their original order. The cause of the failure entry 55 recorded was in the player, not here: it put its twenty-four byte settings request on the tail of the cartridge's megabyte, and TinyGBC entry 34 moved it ahead of the cartridge, after which entry 35 saw the launch pass every step and this cycle's play accepted it. No file here changed after entry 55. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was unchanged and entries 1 to 55 were not rewritten, and validated this entry as number 56 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and `None.` in Files Modified.

#### Next Steps:

Nothing here. The window's Video page and the settings reply are exercised and accepted; the greyed palette row for a colour cartridge is the one part of the page the user has not needed to use. The fault that made this cycle expensive to verify, a paced console run making no frames and blocking the shell, belongs to TinyGBC's player and is its next item.

#### Files Modified:

None.

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 57 COMMIT Unreleased 2026-10-27T12:07:10-07:00

#### Coming From:

Unreleased 48f632d

#### Purpose:

Read this board's clock at the only moments it can be read, so a game started after a power cycle keeps its cartridge's clock, and say on the Saves page when it cannot.

#### Outcome:

The clock is an Adafruit DS3231 at 0x68 on the USB-A port reached through the desktop core's I2C engine over the UART link (tang_clock.h), so it can only be read while that core is loaded and the link is free -- and a launch replaces that core. A game started soon after a power cycle therefore went without a date, and the player kept nothing: `/root/saves/last.log` has stencilled such runs `no-clock` all along. `ports/bl616/tinygbc_app.cpp` gained `board_clock_check()`, which calls `tang_clock_sync()` -- the call that returns *why* it failed -- stores the result and writes a new line on the Saves page: `Board clock: set, so cartridge clocks are kept.` or `Board clock: not set (<reason>), so a cartridge's clock is not kept.` with the reason being no RTC answering, the RTC holding no date, or the clock not being readable. It is called when the window opens, which is while the desktop core is loaded, and in `lib_start` before the launch task swaps that core, which is the last moment the bus exists; when the date is missing the Library status says `Starting ... (board clock not set: its clock is not kept)` rather than the bare `Starting ...`. `tang_clock_sync()` sets the running clock as a side effect, so a successful read here also gives the settings reply its date, which is the fix rather than only a report. The firmware built as `48f632d-dirty.26b9ec3`, 648,960 bytes, SHA-256 `9efa097c9e5f931eddaf57240d3b77d0a19b42973bc3e93f282e0238d57df26f`, and `tools/tests/test_clock.sh` passes with 35,930 checks and none failed. Nothing was flashed: the board is in use, so this is built and waiting. The player's half, which names a session that cannot keep a clock, is TinyGBC entry 37. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was unchanged and entries 1 to 56 were not rewritten, and validated this entry as number 57 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible path in Files Modified.

#### Next Steps:

Flash it with the player TinyGBC entry 37 built, then test the case that failed: power-cycle, open the TinyGBC window without launching anything else first, and read the Saves page -- it should report the clock set if an RTC is fitted and reachable, and a clock cartridge's `.rtc` should then be written at exit where before nothing was. If it reports not set, the reason tells the two hardware faults apart: no RTC answering is an absent module or a wrong socket, and the RTC holding no date is the DS3231's oscillator-stop flag, which a write through `tang_clock_set` clears. Whether the RTC is physically fitted on this dock has never been recorded, and that is what the new line will settle.

#### Files Modified:

- ports/bl616/tinygbc_app.cpp

#### Status:

- Build: PASS
- Deployment: NOT RUN
- User Test: NOT RUN

---

## 58 COMMIT Unreleased 2026-10-27T12:25:14-07:00

#### Coming From:

Unreleased 86d2424

#### Purpose:

Record the deployment of the clock read and the user's acceptance of it, and the note writer that made the result readable from the console.

#### Outcome:

The firmware deployed was `86d2424-dirty.455db9f`, 648,960 bytes, SHA-256 `d050eec1184842cedd1fde88278ecbfb1b758cf9e61890c94e59be68dabbcc43`, and it reads this board's clock where it can be read. Two changes are in it beyond entry 57. `ports/bl616/tang_ae350_run.h` and `tang_fpga_uart.c` gained `tang_ae350_note()`, which appends one stamped line to `/root/saves/last.log` through the same writer an AE350 run uses, so a window can leave a fact where a console cannot be read. `tinygbc_app.cpp`'s `board_clock_check()` calls it with its verdict. On the board, the user opened the window's Saves tab and the log recorded `2026-10-27T12:21:11-07:00 clock: set, a cartridge's clock is kept` with a real local time, where every earlier run in that file had said `no-clock`; they then launched Pokemon Gold and exited at the title screen, and the log shows the run at `12:23:28` reading the battery save (32,768 bytes), the state (84,237) and the clock (8 bytes), and the clock written back at `12:23:47` as `8 of 8 bytes, crc 9eb5c4ef of 9eb5c4ef, whole`, one write in 1 ms. The clock is therefore kept across a session started after a power cycle, which is what failed before. The RTC itself was already recorded here in BRD-011 and BRD-012, so this confirms a recorded fact rather than establishing one, and nothing in `.ai/core-reference.md` needed revising. The board reports the console on a `/dev/ttyACM*` that changes between boots -- this cycle it moved from 0 to 2 and back to 0 -- so the tools' `--port` was needed and is worth remembering. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was unchanged and entries 1 to 57 were not rewritten, and validated this entry as number 58 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

Nothing here. The console port moving between boots is worth a line in the tooling: `tinytang_put.py`, `tinytang_flash.py` and `tinytang_console.py` default to `/dev/ttyACM0` and refuse safely when the board is elsewhere, which cost a few minutes of confusion; picking the port by asking the candidates in turn, as this cycle's checks did by hand, would remove that. TinyGBC's entry 38 records the player's half of the acceptance.

#### Files Modified:

- ports/bl616/tang_ae350_run.h
- ports/bl616/tang_fpga_uart.c
- ports/bl616/tinygbc_app.cpp

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 59 COMMIT Unreleased 2026-10-27T14:49:37-07:00

#### Coming From:

Unreleased e3ce7c3

#### Purpose:

Give the Tang a working UART on a PMOD socket so the RYLR998 LoRa module moves off the bench Pico and onto the board it belongs to.

#### Outcome:

`pmod_uart.sv` came back from `800d11b^` unchanged and was wired into the socket mux as a new personality, 8, rather than the 7 it had before, because 5 is the Phosphor core's I2S2 (PMOD-004) and the same word must not mean two things across cores. The desktop core reached ABI 1.6 with the register window at 0x170/0x174/0x178/0x17c, the capability word at 0x08 reading 0x10f and `supported_word` accepting only 0x0080 or 0x0800; the firmware gained `tang_fpga_read32`/`tang_fpga_write32` on the existing extended endpoint, a `lora` command with at/ctrl/loop/read/flush/status/reset/ver, and a `lora` layout in `/tinytang.ini` that declares PMOD0. Two RTL defects were found only by hardware and each cost a rebuild: `supported_word` refused the new word outright with status 6, and the combinational write validator ended in `else status=4`, so writes to 0x170 and 0x17c were rejected before they could be applied even though the FSM branches existed and reads worked; the read-back test (write the loopback bit, read 0x17c back as zero) is what proved the write never landed. The module then sat across pins 1-6 instead of 7-12, which put its TXD where the core does not read, and the receiver filled with 512 bytes of noise; the internal loopback returned `AT` byte for byte and so ruled out the RTL in one command. With the module on 7-12, `lora ver` answered +VER=RYLR998_REYAX_V1.2.4, `lora status` read 0x00000000, the module still held address 2, network 6, +PARAMETER=7,7,1,12 and 915 MHz from its time on the Pico, and one `AT+SEND` of a framed 24-character packet moved TinyNode's counters from rx 7 frames 16 to rx 8 frames 17 with rssi -32 dBm, so the Tang now transmits to the node over the radio. `tools/tests/test_ini.sh` passes with new coverage for the lora layout and its word, and `tools/check_help_width.py` passes at 144 entries. PMOD-009 and TOOL-022 record the personality and the fact that the desktop core's source list is kept in both `build.tcl` and `synth.ys`; PMOD-006 was revised for ABI 1.6. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and no settled entry was rewritten, checked that the reference's six YAML blocks parse with the index and records naming the same 152 ids in the same order, and validated this entry as number 59 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The link exists but carries nothing but AT commands, so the next cycle is the framing port and then the application protocol: `lora_frame` moves from TinyNode almost unchanged, and the protocol above it needs its own length and checksum because a lost frame currently reassembles into a silently short message (TinyNode's FRM-008). The core load is still manual after each boot, since the boot script loads `tinytang.bin` rather than `tinytang-lora.bin`; the old core remains on the card as the rollback. Two small debts stand: the ini's `lora` layout releases PMOD1 and gives up that socket's OLED by design, and the read path has no back-pressure, so a chatty peer would overrun the 512-byte FIFO the way the floating line did.

#### Files Modified:

- fpga/desktop/build.tcl
- fpga/desktop/desktop_link.v
- fpga/desktop/desktop_pmod.sv
- fpga/desktop/desktop_regs.sv
- fpga/desktop/desktop_top.sv
- fpga/desktop/pmod_uart.sv
- ports/bl616/tang_fpga_link.h
- ports/bl616/tang_fpga_uart.c
- ports/bl616/tang_ini.c
- ports/bl616/tang_ini.h
- ports/bl616/tang_lora.c
- ports/bl616/tdsh_platform_bl616.c
- tools/desktop_open/synth.ys
- tools/tests/tb_ini.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: N/A

---

## 60 COMMIT Unreleased 2026-10-27T16:19:13-07:00

#### Coming From:

Unreleased ecc0ca7

#### Purpose:

Let the Tang ask TinyNode questions over the LoRa link, as a client rather than as a Reticulum peer.

#### Outcome:

`ports/bl616/tang_proto.{c,h}` is the node's codec again, moved to C so this firmware can build it, and `tang_lora.c` gained the client around it: it encodes one application message, sends it in frames addressed to the node's module, and waits for a reply whose sequence matches the one it sent. The wait discards anything that is not that reply, because the node shares the radio with its own Reticulum traffic, and a request is tried ten times before failing, 1.2 s apart and 4 s per attempt, reusing the same sequence so a late answer still counts. Three defects of mine made first contact look like a dead radio and each needed a flash to find: the command was first called `tang`, which this firmware already uses for the game ini settings, so it never registered; the command table entry then silently failed to match its search text, shipping a build without the command at all; and the receive loop cleared the reassembly buffer before decoding it, which timed out every request. The rule taken from that is to check the binary for a new command's strings before flashing rather than trusting the build. On the board, with the lora core loaded and the module on PMOD0 across pins 7-12, `node status` returned the node's full text with its own stack reporting 35 known paths, and status, env, gps, ping, time, help and an unknown word all answered with valid CRCs and matching sequences, six of six without a timeout once the retries were in. PROT-011 records the client, its retry policy and why the wait must discard. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and no settled entry was rewritten, checked that the reference's six YAML blocks parse with the index and records naming the same 153 ids in the same order, and validated this entry as number 60 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The Tang can ask but has no screen: `node` is a console command, so the next cycle is a Messages app that polls on a heartbeat and shows the answers, followed by serving the `messages`, `body` and `send` request types that both ends already define and the node currently answers with "not implemented yet". The core still has to be loaded by hand after every boot, because the boot script names the old one. Ten attempts with a 1.2 s gap means a failing request takes about 50 s to give up, which is fine at a prompt and too long for a UI, so a screen should shorten the attempt count or move the retries off the main loop.

#### Files Modified:

- ports/bl616/tang_lora.c
- ports/bl616/tang_proto.c
- ports/bl616/tang_proto.h

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: N/A

---
## 61 COMMIT Unreleased 2026-10-27T18:41:00-07:00

#### Coming From:

Unreleased dfc7bdc

#### Purpose:

Answer a program's request for the board's clock at launch, so an emulated calculator with no clock chip of its own can show the real date.

#### Outcome:

`ports/bl616/tang_clock.h` and `tang_clock.c` gained `tang_clock_zone()`, which reads the desktop's zone from `/tinydesk_tz` once with the bound check the existing readers use, answers 0 when the file is missing and on the host build, and is now the one reader: `ae350_zone()` in `tang_fpga_uart.c` returns it rather than parsing the file a third time, so the time stamps on the card's save log and the clock handed to a core cannot disagree. `tang_fpga_uart.c` gained a `Tiny89 SETTINGS WANT <address> <room>` answer in `tang_ae350_run()`'s line loop, sending six bytes over the boot ROM's load protocol the way the BIOS and a save state already travel: UTC seconds since 1970 as a little-endian 32-bit value, then the desktop's zone as a signed little-endian 16-bit count of minutes east of UTC. It follows TinyGBC's clock in that same table exactly, including the rule that a board which does not know the time sends zero rather than a date it invented, and the launch logs `ae350run: settings, clock sent|not set, zone <n> minutes` so the console says which happened. The first design was a desk-layer frame instead, `FPGA_CMD_TIME 0x18` sent beside the keyboard's `0x17`, and it was written and host-tested; it was dropped because carrying it needs three new read-only registers in the core's fabric, and that netlist change pushed the pixel clock onto clock-bridge spines nobody has measured, so the core would not pack at any of sixteen swept seeds (Tiny89's core-log entry 21 has the seeds and the gate). The frame, its sender in `tang_osd_desk.c` and the stand-in clock in `tools/tests/tb_osd_desk.c` were removed, leaving that suite at its 88 checks and `test_clock.sh` at 35930. The firmware builds with the local SDK 2.3.36 and toolchain to 657,232 bytes at SHA-256 `78020302b370da6289da70b08aefd3d6a6d988f0494dcf9e82c96ef2ffcb8a9d`, and entry 60's lesson was applied rather than restated: `strings` finds `Tiny89 SETTINGS WANT ` and the clock log line in the image, so the handler is in the build rather than only in the source. It was deployed: `tangflash` staged the image from the card after a `tinytang_put`, committed it and the board came back on its own, with `platform` reporting `dfc7bdc-dirty.1fc8e8c`, the identity the tree generated. On the board the handler answered a real request (`ae350run: settings, clock sent, zone -420 minutes`, then Tiny89's program printed `Tiny89 CLOCK 1793152387 -420`), the RTC having been read first with `tangi2c rtc` because the DS3231 is reached through the desktop core's I2C engine and a launch that replaces that core before reading it leaves `tang_clock_now()` with no offset. The user reports the cycle passing. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and no settled entry was rewritten, and validated this entry as number 61 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The blob is now an ABI between this firmware and Tiny89's program, and the fields after the first six bytes are free for a later cycle to use, so anything added must be appended rather than inserted; the six bytes themselves must not move, since a program that asks for them reads the UTC seconds little-endian at offset zero. The zone is read once and kept, so a change to `/tinydesk_tz` reaches a running program at its next launch rather than during a session, which matches how the desktop's own clock treats it. If a core ever needs the clock corrected while it runs, the frame in this entry's history is the shape to come back to, and its blocker is a measurement of the `SPINE19` and `SPINE27` clock bridges rather than anything in this repository. The next cycle that touches a launch path should confirm on the board that `ae350run` reports `clock sent` with the desktop's own zone, which is the one half of this that no host test covers.

#### Files Modified:

- ports/bl616/tang_clock.c
- ports/bl616/tang_clock.h
- ports/bl616/tang_fpga_uart.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 62 COMMIT Unreleased 2026-10-27T18:49:13-07:00

#### Coming From:

Unreleased dfc7bdc

#### Purpose:

Add a native TinyCreature app that starts its matched Celtic player without a shell worker and restores TinyDesk when the player exits.

#### Outcome:

`ports/bl616/tinycreature_app.cpp` is a 54 by 18 TinyDesk window with Launch and Controls pages: it checks the installed core, player and fixed `celtic.tcl`, starts an 8 KiB task that calls the same `tang_core_load()` and `tang_ae350_run()` implementations as the other apps, and reloads `tinytang.bin` after every run that reached the TinyCreature core; the page names missing artifacts, the controls record the three mouse gestures and F12 or Escape, and no arbitrary land browser is offered because TinyCreature still compiles Celtic's plant table into this player. The app is registered with the desktop and the build, its manual chapter and README record the four card paths and the fact that F12 exits rather than overlays a core with no desktop layer, and the generic AE350 line parser now accepts `TinyCreature WANT`, `TinyCreature LINK` and `TinyCreature EXIT` alongside the three existing product names without adding TinyCreature to the save channels it does not use. The local SDK 2.3.36 build with the Xuantie toolchain passed, compiled the new C++ unit without a warning, produced a 661,376-byte `tinytang_bl616.bin` at SHA-256 `389b8a149a470d0deb0325239d014f6eb42ca84046e67036355acd75986a2e6f`, and strings in the image confirm the app title and its player and scene paths. The AE350 loader, frame, keyboard, desktop-input, core-load, clock and carried TinyDesk suites pass at 26, 24, 53, 88, all, 35,930 and 10 checks respectively; the clock result also confirms the pending entry 61 changes underneath this cycle were preserved. That firmware was flashed, power-cycled and confirmed as `dfc7bdc-dirty.c00cc02`; TinyCreature's core, compressed bitstream, player and Celtic scene were installed with matching sizes. The diagnostic launch reached `TinyCreature WANT` and `TinyCreature LINK`, and F12 stopped the player, but its first build reset before the EXIT text crossed the link; TinyCreature's rebuilt drain fix was installed and work stopped before it or this native app was exercised, so this entry records deployment but not a user-test pass. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff including entry 61, confirmed `.ai/core.md`, `.ai/core-reference.md` and all settled entries are unchanged, and validated this entry as number 62 of 100 with exactly six sections, one-sentence Purpose, prose, eligible paths and allowed Status values.

#### Next Steps:

For handoff, the firmware and final four-file bundle are already installed. Wake one paired keyboard once, then check the app's ready state, launch, live mouse input, Escape exit, desktop restoration and repeat launch; also confirm the rebuilt player now delivers `TinyCreature EXIT 0` without the five-minute silence timeout. TinyCreature's existing flickering-band and sticky-grab defects remain outside this firmware cycle.

#### Files Modified:

- CMakeLists.txt
- README.md
- manual/desktop.md
- manual/index.md
- manual/tinycreature.md
- ports/bl616/tang_ae350_run.h
- ports/bl616/tang_fpga_uart.c
- ports/bl616/td_desktop_bl616.c
- ports/bl616/tinycreature_app.cpp

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---
## 63 COMMIT Unreleased 2026-10-28T00:28:25-07:00

#### Coming From:

Unreleased 238296a

#### Purpose:

Make the Wii nunchuck on the board's USB-A adapter drive TinyDesk's pointer, with Z and C as its buttons and the stick as the shell's history at the prompt.

#### Outcome:

The part was characterized on the bench before any code: `tangi2c read 52 00 6` returned `ff ff ff ff ff ff` until 0x55 was written to 0xF0 and 0x00 to 0xFB, after which the same read returned live data, so a Wii extension controller answers 0xff until it is woken rather than when it is absent; BRD-011's Classic Controller had sent 0xff after its wake-up ACKed on the older repeated-START engine, so that was the read shape and not this, and the record now separates the two. Holding one state per run of reads measured the part: center 0x7b/0x83 (123/131), full left 0x18, full right 0xdd, full down 0x21 and full up 0xe5, about 98 counts each way with Y growing upward, and Z as bit 0 and C as bit 1 of byte 5, both active low. `ports/bl616/tang_nunchuck.{c,h}` is the driver: it sends the two wake-up writes on the first poll after the layer starts and again whenever a read comes back all 0xff, reads the identity at 0xFA once per wake-up and refuses a part that is not a nunchuck because a Classic's 6-bit stick would read as hard over and its d-pad as the buttons, learns the center from the first resting sample with an 88 to 168 bound, and maps each axis with an 8-count dead zone and a squared response to 96 counts a poll -- six cells on the 80x45 grid at 16 counts to a cell -- with Y negated so a push away moves the pointer up. Z and C merge into the left and right buttons of the same `tang_mouse_t` the Bluetooth mouse fills, and at the shell prompt the vertical axis becomes wheel detents, one per 140 ms, so the stick walks the shell's history as the mouse's wheel does. `tang_i2c.cpp` supplies the read, write and millisecond hooks on the same `transaction()` path as the RTC, which routes 0x52 to the switch's channel 0 by address; `tang_osd_desk.c` polls it beside the Bluetooth mouse in both the shown and the hidden path and resets it when the layer starts; `tdsh_platform_bl616.c` registers `nunchuck status|raw|center`. `tools/tests/tb_nunchuck.c` and `test_nunchuck.sh` pass at 51 checks -- the decode, the identity, the dead zone, all four directions, the Y inversion, the buttons and their edges, the console wheel gate, center learning and its refusal, the wake-up and 0xff retry, and a Classic being refused -- and `tb_osd_desk.c` gained three wiring checks for 92. `test_pad`, `test_i2c_mux`, `test_clock`, `test_ini`, `test_key` and `test_frames` pass and `check_help_width.py` reports 147 entries with none over 80 columns. The firmware built with the local SDK 2.3.36 and the Xuantie toolchain at 661,424 bytes, SHA-256 `35dd962cd5d9a887f536801cdda6d083b9d5efb949131107997fe848f29eaa9a`, with no warning outside `tang_jtag_programmer.c`, and `strings` finds the command and its text in the image. It was put to the card and flashed with `tangflash`, the board was power-cycled and `platform` reported `238296a-dirty.c12ea87`; `nunchuck status` then read ready with center 123/130 and report `7b 82 b1 85 8f 83`, pushing the stick at the prompt recalled the shell's history in order, and the user reports the pointer, its direction, the dead zone, Z dragging a window and C working, and passes the cycle. `.ai/core-reference.md` gained the WII topic with WII-001 (the wake-up, VERIFIED), WII-002 (the report and the values measured on this part, VERIFIED) and WII-003 (the Classic's format, SOURCED), and BRD-011 and BRD-012 carry dated revisions. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and no settled entry was rewritten, checked that the reference's six YAML blocks parse with the index and records naming the same 156 ids in the same order and the catalog listing 18 topics, and validated this entry as number 63 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

A Classic Controller would need its own decode, its format now recorded in WII-003, and a bench pass for the centers of its 6-bit axes; the accelerometer is unused and should stay so, because tilt is too noisy to aim with. The dead zone, the 98-count full scale and the 96-count ceiling are the tunables if the feel wants changing, all in `tang_nunchuck.h`. Nothing else is outstanding from this cycle: the RTC on channel 6 and the nunchuck on channel 0 leave each other alone, and the standing items of entry 62 are unchanged.

#### Files Modified:

- README.md
- manual/desktop.md
- ports/bl616/tang_i2c.cpp
- ports/bl616/tang_nunchuck.c
- ports/bl616/tang_nunchuck.h
- ports/bl616/tang_osd_desk.c
- ports/bl616/tdsh_platform_bl616.c
- tools/tests/tb_nunchuck.c
- tools/tests/tb_osd_desk.c
- tools/tests/test_nunchuck.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 64 COMMIT Unreleased 2026-10-28T01:08:36-07:00

#### Coming From:

Unreleased 92f2004

#### Purpose:

Make the Classic Controller drive the same pointer as the nunchuck, its left stick at the nunchuck's scale, its right stick at half, and its L and R shoulders as the buttons.

#### Outcome:

The part was measured on the bench before the decode was written: `tangi2c read 52 FA 6` returned `00 00 a4 20 01 01`, the identity and format code WII-003 recorded, and `tangi2c read 52 00 6` at rest returned `a0 20 10 61 ff ff`, which under format 0x01 is a left stick at 32/32, a right stick at 16/16 and every button released. Holding each state for a run of reads gave the left stick 5 at full left, 54 at full right, 58 at full up and 6 at full down, the right stick 28 and 4 on its X and 28 and 3 on its Y, and the shoulders byte 4 bit 5 and bit 1 with the digital click closing at about 29 of 31 on the analog trigger. `ports/bl616/tang_nunchuck.{c,h}` became `ports/bl616/tang_wiipad.{c,h}` because it now serves two parts: the identity at 0xFA chooses the packing, the nunchuck's 8-bit stick or the Classic's format 0x01 with a 6-bit left stick, a 5-bit right stick split across bytes 0 to 2 and the shoulders in byte 4, and the driver refuses a part it does not know rather than decoding one as the other. The left stick keeps the nunchuck's dead zone and 96-count ceiling, the right stick adds at 48 counts with its X negated, and both add into the one pointer; the left stick's vertical axis remains the console wheel and the right stick is left out of it. The identity is re-read every five seconds while a part reports and an unsupported part is retried every two, so a controller swapped under power is picked up without a reboot. The console command is `wiipad status|raw|center`, with `nunchuck` kept as an alias, and status prints which part is fitted and both centers. `tools/tests/tb_wiipad.c` and `test_wiipad.sh` pass at 89 checks -- the nunchuck and Classic decodes against the recorded bytes, the identities including a guitar being refused, both mappings, the analog-trigger threshold, the buttons and their edges, the console wheel, center learning for both sticks, and the swap and unsupported paths -- while `tb_osd_desk.c` stays at 92 with the renamed hooks and `check_help_width.py` reports 148 entries. The firmware built at 661,440 bytes, SHA-256 `393c77a999fe72ff43d660ec57f10c646f47a1bd8c41bbff196e726a27445684`, with no warning outside `tang_jtag_programmer.c`, was put to the card and flashed with `tangflash`, and the user power-cycled and tested it: the controller works, with the left stick, the right stick at half speed, the shoulders and the console wheel, and the user asks for two changes, that the right stick's X is inverted in use and that the left and right buttons move from the L and R shoulders to A and B. WII-003 is now VERIFIED with these readings, WII-001, WII-002 and BRD-012 follow the rename, and README and `manual/desktop.md` describe both parts. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and no settled entry was rewritten, checked that the reference's six YAML blocks parse with the index and records naming the same 156 ids in the same order and the catalog listing 18 topics, and validated this entry as number 64 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

Two corrections the user asked for come first: the Classic's right stick X moves the pointer the wrong way and its sense has to be flipped, and the left and right buttons move from the L and R shoulders to A and B. The buttons the remap frees, and the spare face and d-pad buttons, invite a small key map for the desktop -- Tab and Shift+Tab for focus, Enter, Escape, the arrows, Page Up and Page Down and F6 are all understood by TinyDesk (WM and widgets) -- which is its own cycle once the user picks the mapping. The standing items of entry 63 are otherwise unchanged.

#### Files Modified:

- README.md
- manual/desktop.md
- ports/bl616/tang_i2c.cpp
- ports/bl616/tang_wiipad.c
- ports/bl616/tang_wiipad.h
- ports/bl616/tang_osd_desk.c
- ports/bl616/tdsh_platform_bl616.c
- tools/tests/tb_osd_desk.c
- tools/tests/tb_wiipad.c
- tools/tests/test_wiipad.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 65 COMMIT Unreleased 2026-10-28T02:02:16-07:00

#### Coming From:

Unreleased eb87f67

#### Purpose:

Correct the Classic Controller's pointer -- its right stick's sense, and A and B as the buttons -- put the driver in the data format a wireless receiver needs, and give the controller's spare buttons the desktop keys the user chose.

#### Outcome:

The user's use of the pointer established that the Classic's right stick X was inverted, so its sign was flipped, and the left and right buttons moved from the L and R shoulders to A and B. A third-party receiver then showed a fault worth more than the two: an 8BitDo receiver for the NES/SNES Classic, with an SN30 Pro paired to it, identified as a Classic Controller and returned a fixed `84 86 86 86 00 00` in data format 1 across a dozen reads and a replug, which the format-1 decode read as a permanently deflected stick with every button down -- the console's history being walked continuously. Writing 0x03 to 0xFE was accepted and the format-3 report followed the pad, so the driver now selects format 3 when it adopts a Classic, reads eight bytes, accepts an identity whose format byte is 0x01 or 0x03, and decodes format 3's eight bytes with 8-bit axes; TinyNES's `wii_pads.sv` has selected format 3 for every controller all along, which is why the pads worked under the game cores and not here. The Classic's button bits were then confirmed live with the new `wiipad trace` command, which samples at the poll rate and prints a report only when it changes: A, B, X, Y, +, -, L, R, ZL, ZR, Home and all four d-pad directions each appeared on their own bit, and the d-pad's diagonals landed on the right bit pairs. The key map the user approved is in: Y is Tab, ZL Shift+Tab, X Escape, plus is Enter, minus the start menu, L and R Page Up and Down, ZR closes the focused window, and Home is the screen switch; the d-pad moves the pointer instead of sending arrows, one cell a press and accelerating while held to the nunchuck's six cells a poll, because the project's game-controller D-pad has always moved the pointer and because a pad whose stick the receiver reports as a d-pad would otherwise have no pointer at all; at the shell prompt the d-pad is still the arrow keys. A stick-only trace settled the analog question in the negative for this hardware: moving the SN30 Pro's left stick changed the d-pad bits and left all four axis bytes at 132/134, so the receiver presents the pad as a digital Classic and the sticks never reach the bus as axes. The firmware built at 665,552 bytes, SHA-256 `4fe986beff03a470e951de09142a8c36b8df3709a32fe7922ff2de5be7f7ca58`, was put to the card and flashed, and the user reports the button map correct, the d-pad moving the pointer, and the stick still feeling digital, which is what the receiver makes it. `test_wiipad` passes at 108 checks -- both decodes, the format-3 selection, the key map, the d-pad's pointer step and ramp, the console arrows, and both swap directions -- `test_osd_desk` at 92 and `check_help_width.py` at 149 entries. WII-003 and WII-004 carry the format-3 and receiver findings, and README and `manual/desktop.md` describe both parts and the key map. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and no settled entry was rewritten, checked that the reference's six YAML blocks parse with the index and records naming the same 157 ids in the same order and the catalog listing 18 topics, and validated this entry as number 65 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The one untested way this receiver could still carry analog stick data is data format 0x02, the nine-byte report with 10-bit axes and 8-bit triggers (WII-003), which the firmware cannot see today because `tang_i2c.cpp` copies at most eight bytes out of the engine's sixteen-byte read window and `tangi2c` refuses a length over eight; widening the window and trying format 0x02 against this receiver is the next experiment, and if its axes are flat there too the receiver is digital by design and WII-004 says so for good. The key map's ZR (Ctrl+Q) and the start-menu keys were confirmed from the table and the trace but not each exercised in the desktop. The standing items of entry 64 are otherwise unchanged.

#### Files Modified:

- README.md
- manual/desktop.md
- ports/bl616/tang_osd_desk.c
- ports/bl616/tang_wiipad.c
- ports/bl616/tang_wiipad.h
- tools/tests/tb_osd_desk.c
- tools/tests/tb_wiipad.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---
## 66 COMMIT Unreleased 2026-10-28T02:22:25-07:00

#### Coming From:

Unreleased dda50fe

#### Purpose:

Make the Classic Controller's d-pad its only pointing input -- the arrow keys normally and the pointer while the right shoulder is held -- drop the sticks the receiver never reports, and correct the diagonals.

#### Outcome:

The user's research into the 8BitDo receiver confirmed what the trace had shown: it does not support analog sticks at all, and mirrors them onto the d-pad, so the four axis bytes are never anything but their resting values and no firmware can recover a position that is not sent. Before that was accepted, the one untested format was tried: `tang_i2c.cpp`'s read path was widened from the engine's eight-byte copy to the whole sixteen-byte window (the read window, the transaction copy, the `tangi2c` length cap and the wiipad read hook), a firmware was built and flashed to ask the receiver for data format 0x02's nine-byte report, and no nine-byte reading was ever taken -- the pad stopped answering after that flash and the change was reverted uncommitted at the user's request, so whether the receiver carries the sticks in format 2 is not established either way, and the pad's failure was not separated from the receiver having lost its Bluetooth link on the power cycle. With the analog path closed, the driver stopped reading the Classic's sticks entirely: its d-pad is the pointing input and the arrows are what it sends, the arrow keys normally, repeating as the keyboard does (500 ms, then 50 ms), and with the right shoulder held -- the keyboard's Alt -- the pointer instead, one cell a poll, with no arrow; at the shell prompt, where there is no pointer, the d-pad is the arrows and the shoulder takes it nowhere, which is what Alt+arrows do there. ZL, ZR, L and R were unmapped at the user's request and left for a later decision, and the shoulders were then swapped so that R is the modifier and L is free. A diagonal fault was ours and not the pad's: the cursor path latched only the direction bit that was new in the poll and dropped the one already held, so adding up while holding right lost the right unless both landed inside one 40 ms poll, which is why two buttons had to be slammed together; the whole held mask is used now, and a test presses right and then adds up. The firmware built at 665,552 bytes, SHA-256 `5aaeb473c57f0d864bff4ec6e68dbed0d7e0492d02004b6b09af14205ef29c85`, was put to the card and flashed, and the user reports the pad working smoothly, the d-pad and the R-held pointer both right, and the diagonals fixed. `test_wiipad` passes at 111 checks -- both decodes including the d-pad and every button bit from the trace, the sticks moving nothing, the key map, the d-pad's two modes, the diagonal, the console arrows, and both swap directions -- and `test_osd_desk` at 92, with `check_help_width.py` at 149 entries. WII-003 and WII-004 record the digital sticks and the user's confirmation, and README and `manual/desktop.md` describe the d-pad's two modes and the buttons left free. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and no settled entry was rewritten, checked that the reference's six YAML blocks parse with the index and records naming the same 157 ids in the same order and the catalog listing 18 topics, and validated this entry as number 66 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

Data format 0x02 remains untested, and the widening that would let the firmware see its nine bytes is reverted; if it is tried again it must be flashed on its own, with no pointer change beside it, so a silent pad can only mean the wider read, and the receiver's link should be checked before the result is believed. ZL, ZR and L are free and wait on the user's decision. The entry-62 TinyCreature test and the standing items of entry 65 are otherwise unchanged.

#### Files Modified:

- README.md
- manual/desktop.md
- ports/bl616/tang_wiipad.c
- ports/bl616/tang_wiipad.h
- tools/tests/tb_wiipad.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 67 COMMIT Unreleased 2026-10-28T11:15:16-07:00

#### Coming From:

Unreleased d1f40df

#### Purpose:

Add TinyKiwix, a desktop reader for an offline encyclopedia pack that a host tool converts from a Kiwix/openZIM archive, with the tool that builds the pack and the one that checks it.

#### Outcome:

The cycle added TinyKiwix, a desktop window that reads an offline encyclopedia from `/root/wiki`, and the two host tools that make and check that pack. A Kiwix `.zim` cannot be read on the board: openZIM stores entries in clusters of about a megabyte compressed with Zstandard or XZ, and decoding one needs that much back-reference window against 320 KB of OCRAM with no PSRAM (BL6-002) and a largest free heap block under 70 KB after entry 52, so `tools/make_wiki_pack.py` reads the archive on a host -- standard library only, `compression.zstd` for current archives and `lzma` for older ones -- keeps namespace C's `text/html` entries, folds them into wrapped ASCII at 78 columns, drops tables, MathML and reference superscripts, resolves the redirect pages libzim writes as `<meta http-equiv="refresh">` by re-pointing the redirect's title at its target's body, and writes `wiki.meta`, an `wiki.idx` of 80-byte records sorted by folded title, and body chunks cut at 3 GiB for FAT32's 4 GiB file limit. Three defects of mine were found by measurement rather than by reading the code back: the cluster offset base was one byte off, which fed every article the previous blob's last byte and was pinned down by the invariant that the first offset equals the offset table's size; visiting entries in archive order reached a cluster different from the previous one for 99.8% of them, a fresh megabyte of decompression each, so entries are now sorted by cluster through an external sort before any body is read; and the resolved-redirect stream leaves the join in target order, not its own title order, which left six titles out of order and broke the binary search for them, so it is re-sorted by its own title before the final merge and a test with two redirects sorting opposite to their targets covers it. `tools/check_wiki_pack.py` validates a whole folder -- sortedness, duplicate titles, chunk bounds, offsets inside chunks, printable ASCII, line counts and column widths -- and caught headings running 79 to 81 columns because the `## ` marker was added after the text had been truncated to the column budget. `ports/bl616/td_kiwix_app.cpp` binary-searches the index by title, which costs a handful of card reads at any archive size, opens a title, and pages through the body 12 KB at a time in a buffer allocated when an article is first opened and freed when the window closes, the same cost as TinyHelp; the user's first finding was that a page could not be turned backwards, so the paging controls are Page Up and Page Down in both views -- of titles and of an article, in the same two places -- with a Back button to the list, and the reader remembers where each page began, 32 pages being more than the converter's 256 KB article cap can produce at 12 KB a page. The measured content: the 1,094,936,257-byte simple-English archive, whose own MD5 was verified before conversion, became 544,372,949 bytes of text and 289,768 records with 4,732 redirects resolved, and the checker passed every record and every body in 14 seconds. `tools/tests/test_wiki_pack.sh` runs 30 checks on the record contract and 28 on the converter over a synthetic archive built byte for byte in the test, and the redirect order, the heading width and the MathML leakage were each shown to fail it once reintroduced, while the cluster order and the offset base were found against the published archives and are not covered by the synthetic archive; 27 of the 28 host suites pass, the exception being `tools/tests/test_phosphor_catalog.sh`, red since entry 41 and untouched here. The firmware built as 669,744 bytes at SHA-256 `33be5da0e2341a7966390e05a084d1ce2505fd8e5ad09c89b9239ff0e195da5b`, `tinytang_flash.py --yes` staged and committed it as `tangflash: OK committing`, the user power-cycled, opened TinyKiwix, searched, read and paged, and reported it working; a post-cycle `platform` read was not taken because the board had left USB, so the deployed identity rests on the staged image's hash and on the user's test of a user interface that exists only in that build. `.ai/core-reference.md` gained the KIWI topic with four records -- the container and its cluster cost, the redirect pages, the published trims and sizes, and the FAT32 chunking -- and the routing table and fast lookup index name them.

#### Next Steps:

The 49 GB English archive is the stated goal and is not converted: it needs about 75 GB of scratch on the host for the archive and its output and several hours of conversion, and its pack would be the first to use a second body chunk, a path no pack has exercised -- a two-chunk pack whose article is read from `wiki.001` on the board is the thing to prove before that run. Redirects stored as directory entries rather than refresh pages are skipped, non-Latin articles convert to almost nothing and survive only if they clear `--min-chars`, images and SVGs cannot be shown on a character display at all, and search is by title prefix only, a full-text index having no design yet. `tools/tests/test_phosphor_catalog.sh` remains red from entry 41 and is still its own cycle, and the standing items of entry 66 are otherwise unchanged.

#### Files Modified:

- CMakeLists.txt
- README.md
- manual/desktop.md
- manual/index.md
- manual/kiwix.md
- ports/bl616/td_desktop_bl616.c
- ports/bl616/td_kiwix_app.cpp
- ports/bl616/td_wiki_pack.h
- tools/check_wiki_pack.py
- tools/make_wiki_pack.py
- tools/tests/check_wiki_convert.py
- tools/tests/tb_wiki_pack.c
- tools/tests/test_wiki_pack.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 68 COMMIT Unreleased 2026-10-28T12:14:15-07:00

#### Coming From:

Unreleased 6315b05

#### Purpose:

Give TinyKiwix a Library of the packs on the card, opened one at a time, so a pack is a folder under `/root/wiki` rather than the single fixed location it was.

#### Outcome:

TinyKiwix now opens on a Library, as the TinyNES and TinyGBC windows open on their libraries: one row per pack, showing the title the pack names itself with and, when it has one, its folder in brackets, with `Refresh` to re-read the folder, `Page Up`/`Page Down` to move through it, Enter to open the pack under the cursor, and `Find...` offered only once a pack is open, because a title search belongs to a pack and not to the list of packs. Packs are discovered as the pack sitting at `/root/wiki` itself -- which is where the first one was left, so the card needed no surgery -- and then every folder there holding a `wiki.meta`, sorted by folder name with the root pack first so the list does not depend on the order FAT returns entries; `wiki_pack_dir_ok()` in `td_wiki_pack.h` rejects `.`, `..`, hidden names and names too long to store without becoming another folder's name. One pack is open at a time and `Back` walks titles to Library, article to titles; the merged search I had proposed was dropped at the user's direction after looking at what Kiwix actually publishes -- about 3,000 archives across 16 collections, in which one project appears as four depths, many languages and topic cuts, so a merged index would answer every search twice over. The converter gained nothing structural but its licence default now names the Wikimedia project it recognises (`Wikivoyage`, `Wiktionary`, `Wikibooks`, and the rest of the family) instead of falling back to "see the source archive". Wikivoyage Europe was converted as the second pack and placed at `/root/wiki/voyage-europe`: 80 MB of archive became 10,268 index records and 93 MB of text with 1,062 redirects resolved, and `tools/check_wiki_pack.py` passed every record and body, as it does for the 289,768-record simple-English pack beside it. Tests: `tools/tests/test_wiki_pack.sh` now runs 40 checks on the pack contract -- the folder-name rule included -- and 29 on the converter, all passing, and 27 of 28 host suites pass with `tools/tests/test_phosphor_catalog.sh` still red from entry 41. The firmware built as 669,776 bytes at SHA-256 `3d815ce542cbe9cc4cec360ff4dfecdc12e74c01c1cb7f64bffb9c2949dcd109`, `tinytang_flash.py --yes` committed it as `tangflash: OK committing`, the user power-cycled with the card in the board, opened both packs, moved between them and between pages, and reported the build a pass. Two findings for later were established while answering questions about the archive catalogue: the reader remembers only 32 page starts, so a body of about 85 pages or more would make `Page Up` past that point read a stale entry and jump to the article's beginning while labelling it a later page -- unreachable at the converter's 256 KB article cap, which is about 22 pages, but reachable the moment that cap is raised -- and Kiwix's Project Gutenberg archives hold each book as a `text/html` entry beside an EPUB and a cover page inside a 206 GB web application, so a Gutenberg pack would need a cover-page filter and a much larger article cap, which the user chose to take as a separate app rather than as part of this reader.

#### Next Steps:

Gutenberg becomes its own app in a later cycle, at the user's direction, and the work it needs is now understood: skip the `_cover` HTML entries each book carries, raise `--max-article-bytes` to about 2 MB because a novel is 300 to 700 KB of text, and expect 25 to 40 GB from the 80,656 English books or one Library of Congress class at a time. Before any cap increase, the 32-page history must be fixed -- a larger table or a re-walk from the start of the body -- or `Page Up` will lie. Increasing the cap costs no board RAM (a page is 12 KB whatever the article's length) but makes a 1 MB book 85 page turns, so `PAGE_BYTES` and a "line X of Y" footer are worth revisiting with it. zimgit packs, PhET and TED are not article corpora and stay unusable here: each is a web application of WASM, JSON and PDFs behind two HTML shells. The standing items of entry 67 are otherwise unchanged.

#### Files Modified:

- README.md
- manual/kiwix.md
- ports/bl616/td_kiwix_app.cpp
- ports/bl616/td_wiki_pack.h
- tools/make_wiki_pack.py
- tools/tests/check_wiki_convert.py
- tools/tests/tb_wiki_pack.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 69 COMMIT Unreleased 2026-10-28T12:47:31-07:00

#### Coming From:

Unreleased 892b071

#### Purpose:

Give a pack the two tables a book reader needs -- a chapter list and an author index -- and teach the converter to build them from a Gutenberg archive and the checker to verify them.

#### Outcome:

A pack may now carry `wiki.parts` and `wiki.auth` beside `wiki.idx`, both optional and additive, so TWP1 is unchanged and TinyKiwix ignores them. A chapter row is 56 bytes -- the book's record number, the byte offset of the chapter inside its body, and a 48-byte title -- sorted numerically by (book, offset) so a reader finds a book's chapters with one binary search and a sequential read. An author row is 52 bytes -- record number and a 48-byte name -- ordered by surname, derived at sort time, so a reader can find Melville without the row storing the key. The converter gained: stripping of Project Gutenberg's own header block and of the footer that starts at whichever comes first of the `*** END ***` marker and the visible "End of Project Gutenberg's ..." line; chapter detection from `## ` headings and from CHAPTER, BOOK, PART, ACT, SCENE, CANTO and SECTION lines, capped at 500 a book; the author read from the cover page's `data-author-name` attribute, which is the only place a Gutenberg archive names it; and a title taken from the entry's URL less its `.html` and its ebook number when the entry carries none, which is every Gutenberg entry. The tables are resolved against the finished index, because a book's record number exists only after the title sort: rows are collected against the book's URL and the place its body landed, then joined to the index's (chunk, offset) once it is written. Chapters are written only for an archive that carries cover pages and only for an entry that has one, so the Wikipedia packs gain no table. Three defects were found by the tests rather than by reading the code back: the chapter table was sorted by the raw bytes of little-endian integers, which puts 256 before 1 and leaves a table a binary search cannot read; the footer loop took the last marker rather than the first and left the visible Gutenberg footer line in every book; and every article in a pack got a Start row until the cover gate was added. The checker now validates both tables -- counts against `wiki.meta`, rows in order, record numbers inside the index, chapter offsets inside their own book, names printable and ordered by the same surname rule -- and refuses a chapter that starts past its book, which a planted corruption in the test proves. `tools/tests/test_wiki_pack.sh` runs 40 pack checks and 45 converter checks; on the five-book Gutenberg sample the converter produced one book and one author from six HTML entries, dropping four navigation pages under `--min-chars 2000`, with 31 chapters and "Charles A. Lee" attached to the right record, and the checker passed it. The same two tools also gained progress reporting for the long conversions they are now used for: the converter prints the references processed against the archive's total with its rate, elapsed time and estimate every 5000, and the checker prints the percentage of records it has validated. No firmware was built or deployed and nothing on the card changed.

#### Next Steps:

The reader is next: a TinyGutenberg window over these tables -- a Library of shelves, a Contents button reading `wiki.parts`, browsing by surname from `wiki.auth`, and a saved reading position -- and with it the fix for the 32-page history, which still stands as a defect for any article long enough to reach it. Chapter detection needs a real class archive to judge: the sample book is a dictionary, so its "chapters" are the letters of the alphabet, and a shelf of novels will say whether the 48-byte titles and the 500-row cap are right. The full English Wikipedia download is in progress on the GIT drive under `/run/media/vash/GIT/tinytang-scratch` and is unaffected by this cycle; it will be the first pack to write more than one body chunk.

#### Files Modified:

- tools/check_wiki_pack.py
- tools/make_wiki_pack.py
- tools/tests/check_wiki_convert.py

#### Status:

- Build: PASS
- Deployment: N/A
- User Test: N/A

---
