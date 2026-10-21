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
