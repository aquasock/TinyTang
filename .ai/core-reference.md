# TINYTANG TECHNICAL REFERENCE

> **Project:** TinyTang
> **Purpose:** The authoritative lookup for externally defined facts this project depends on: the board's wiring and fitted parts, the BL616's registers and memory, the interface contract of the FPGA cores we boot, the contracts of the external projects we embed, and the behaviour of the vendor tools.
> **Authority:** Per `.ai/core.md`, consult this file before looking anything up online. A record is authoritative to the extent of its `status` and cited `sources`. When a better or newer source is found online, add or supersede a record and notify the user.

This file records what an outside source (a datasheet, schematic, vendor SDK, vendor tool, or external repository) says, plus hardware observations that confirm or refute it. It does not record project history, build results, or discussion; those belong in `core-log.md`. It does not record project design choices unless an external fact forces them.

Three of the records here are negative results — facts about what this hardware *cannot* do. They are recorded because each cost a session to establish and each would otherwise be re-litigated: see USB-002, USB-003, and NEST-002.

---

## 1. AI operating rules

```yaml
schema_version: 1

record_kinds:
  - DEVICE      # FPGA silicon: part, revision, identification
  - BOARD       # Tang Console 138K board: pins, parts, clocks, wiring, power
  - SOC         # BL616 microcontroller: address map, USB block, power domains
  - USB         # USB behaviour on this board: roles, VBUS, speeds, stack lifecycle
  - FLASH       # BL616 flash layout and boot image format
  - PROTOCOL    # the BL616 <-> loaded FPGA core UART contract
  - EXTERNAL    # interface contracts owned by other projects
  - TOOLCHAIN   # Bouffalo SDK, CherryUSB, Gowin EDA, RISC-V toolchain

statuses:
  VERIFIED:   "Primary source cited AND confirmed on this project's hardware or tools."
  SOURCED:    "Primary source cited; not yet confirmed on this project's hardware or tools."
  INFERRED:   "Derived from secondary sources, reverse inspection, or consistent observation; no primary source. Flag before relying on it."
  SUPERSEDED: "Replaced by the record named in superseded_by."

rules:
  - "One fact or one tightly coupled fact group per record."
  - "Every record cites at least one source with enough detail to re-check it: repository path plus commit, file, symbol, register name, or tool version."
  - "Separate what the source says (statement) from what it means for this project (consequence)."
  - "Diagnostic or implementation limits chosen by this project are never recorded as standard or device limits."
  - "Do not rewrite a settled record's statement. Correct it by adding a new record and marking the old one SUPERSEDED with superseded_by."
  - "An INFERRED record must say how it was inferred, in verification."
  - "A negative result is a fact. Record what the hardware cannot do with the same rigour as what it can, and say how many attempts established it."
  - "When an online lookup was needed because this file had no answer, add the finding here with the existing syntax, then notify the user if the source is newer or more valid than a record already here."
  - "Use technical names (GW5AST-138, BL616, CDC, OTG). The user's conversational names are not used in this file."
```

---

## 2. Topic catalog

Topic IDs are the `record_id` prefix. An entry reserves a name; it does not claim coverage.

```yaml
- topic_id: DEV
  name: "GW5AST-138 FPGA device"
  description: "The FPGA fitted to the Tang Console 138K: identification and the bitstreams built for it."

- topic_id: BRD
  name: "Tang Console 138K board"
  description: "Board wiring, fitted parts, clocks, power inputs, and the FPGA pin assignments this firmware depends on."

- topic_id: PMOD
  name: "PMOD sockets and the tang.ini contract"
  description: "The dock's two PMOD sockets, the seating-orientation rule, and the tang.ini contract that tells the core what is attached; the pin map and the personality registry are kept in Tang-Phosphor."

- topic_id: BL6
  name: "BL616 microcontroller"
  description: "The board's companion MCU: peripheral base addresses, the USB OTG block, the allocator, and the always-on domain."

- topic_id: USB
  name: "USB behaviour on this board"
  description: "What the BL616's USB port does and does not do here: roles, VBUS, negotiated speeds, and the lifecycle of the stacks."

- topic_id: FLS
  name: "BL616 flash layout and boot image"
  description: "Where the application lives, how a boot header is validated, and what a soft reset leaves behind."

- topic_id: PROT
  name: "BL616 to FPGA core UART contract"
  description: "The frame format and command set a loaded Tang core expects on the board's UART1, and the on-screen text page it exposes."

- topic_id: NEST
  name: "nestang (external project)"
  description: "nand2mario's NES core for Tang boards: its FPGA-side USB host, its board targets, and the outputs it leaves unconnected."

- topic_id: TCTL
  name: "Tang-Control (external project)"
  description: "The reference BL616 firmware for this board: how it drives the OSD, and how it configures the OTG connector."

- topic_id: TDSH
  name: "TinyDesk Shell (external project)"
  description: "The shell this firmware runs: version, scripting language, and the terminal control sequences it emits."

- topic_id: TDESK
  name: "TinyDesk (external project)"
  description: "The desktop environment embedded in this firmware: its port surface, memory model, app set, and host contracts."

- topic_id: EXTCTL
  name: "Extended FPGA control protocol"
  description: "The versioned 0x10 register channel, the 0x12 validated block write, and the 0x11 stop-and-credit stream, as defined by the Tang-Control family."

- topic_id: PHOS
  name: "Phosphor audio loader"
  description: "The SD-card layout, playlist and metadata contract for the Phosphor core, and the handover and artwork rules its streaming depends on."

- topic_id: PROV
  name: "Provenance and attribution"
  description: "The family lineage this project descends from, and the licence and notice obligations that follow from the code it inherits or vendors."

- topic_id: PSX
  name: "Tang-PSX (external project)"
  description: "Facts harvested from Tang-PSX's reference as that project is archived: the device revision and resources, the FPGA end of the core link, the module's JTAG and UART header, the dock's SDRAM, and the measured core-state behaviour of the transport."

- topic_id: TOOL
  name: "Toolchain behaviour"
  description: "Bouffalo SDK, CherryUSB, Gowin programmer, and RISC-V toolchain behaviour that affects correctness, plus the provenance and licence of code vendored into this project."
```

---

## 3. Active routing

| Question | Consult first | Fast records |
|---|---|---|
| Which FPGA is on this board, and how is it identified on the wire? | DEV | DEV-001 |
| Which core image belongs on this board? | DEV | DEV-002 |
| Does the FPGA keep its core across a power cycle? | DEV | DEV-003 |
| Does this project run AE350 RISC-V code, and how does it get there? | DEV | DEV-004 |
| Which FPGA pins are the JTAG programmer's? | BRD | BRD-001 |
| Why does the SD card fail to mount? | BRD | BRD-002 |
| Where do the two USB-A controller ports go? | BRD | BRD-003 |
| What is the onboard USB debug bridge? | BRD | BRD-004 |
| Does the board need a particular power input? | BRD | BRD-005 |
| Where are the board's PMOD sockets, and how are their pins numbered? | PMOD | PMOD-001 |
| How does the core learn what is seated in the PMOD sockets? | PMOD | PMOD-002 |
| Which register selects a PMOD personality, and what are the values? | PMOD | PMOD-003 |
| Where are the USB OTG registers, and which bit is which? | BL6 | BL6-001 |
| How much RAM does the BL616 have? | BL6 | BL6-002 |
| How do I read free heap? | BL6 | BL6-003 |
| Why do xPortGetFreeHeapSize() and friends fail to link? | BL6 | BL6-004 |
| Is there any real-time clock? | BL6 | BL6-005 |
| How do I read the CPU clock? | BL6 | BL6-006 |
| What speed does the console negotiate? | USB | USB-001 |
| Can the OTG block tell me what is on the other end of the cable? | USB | USB-002 |
| Can the OTG connector power a keyboard? | USB | USB-003 |
| Is it safe to re-register the console's endpoints? | USB | USB-004 |
| Why is there no host deinit, and what must be done by hand? | USB | USB-005 |
| How do I tell whether a host is attached? | USB | USB-006 |
| Where does the application live in flash, and what does a soft reset do? | FLS | FLS-001 |
| What is the frame format a loaded core expects? | PROT | PROT-001 |
| Which commands does a loaded core understand? | PROT | PROT-002 |
| Which UART and rate reach a loaded core? | PROT | PROT-003 |
| What is the layout of the core's on-screen text page? | PROT | PROT-004 |
| Why does the screen go black when the OSD is on? | PROT | PROT-005 |
| How could a controller navigate a menu? | PROT | PROT-006 |
| Why can a ROM load end in a black screen with no error? | PROT | PROT-007 |
| Why can't a modern keyboard work on the USB-A ports? | NEST | NEST-001, NEST-002 |
| How does Tang-Control draw and navigate its OSD? | TCTL | TCTL-001 |
| How does Tang-Control configure the OTG connector? | TCTL | TCTL-002 |
| What is Tang-Control's device identity, and is it stock firmware? | TCTL | TCTL-004 |
| What are 1-wire and 2-wire, and which port is the CDC? | TCTL | TCTL-005 |
| Which cores exist, and where do their images live? | TCTL | TCTL-006 |
| What commands did the retired host client offer? | TCTL | TCTL-007 |
| How was the BL616 reflashed without a BOOT button? | TCTL | TCTL-008 |
| What diagnostic counters did the RX path keep? | TCTL | TCTL-009 |
| Which helper scripts existed, and what did each do? | TCTL | TCTL-010 |
| What is the versioned register protocol over the core UART? | EXTCTL | EXTCTL-001 |
| What is the validated block write? | EXTCTL | EXTCTL-002 |
| What is the stop-and-credit stream, and how does it back-pressure? | EXTCTL | EXTCTL-003 |
| Do peek and poke work with nand2mario's cores? | EXTCTL | EXTCTL-004 |
| How must a request/response transaction be serialised on the shared link? | EXTCTL | EXTCTL-005 |
| Where do audio files go, and which formats play? | PHOS | PHOS-001 |
| What playlist syntax and limits apply? | PHOS | PHOS-002 |
| How is track metadata chosen, and how is non-ASCII text handled? | PHOS | PHOS-003 |
| How is a FLAC track sent, and how do tracks join without a gap? | PHOS | PHOS-004 |
| How does the Phosphor core receive cover art? | PHOS | PHOS-005 |
| Which shell revision is this, and what can a script do? | TDSH | TDSH-001 |
| Which sequences must a console mirror understand? | TDSH | TDSH-002 |
| What does TinyDesk need from a port? | TDESK | TDESK-001, TDESK-003 |
| How much RAM does the desktop need? | TDESK | TDESK-002 |
| Why does the Terminal window say "No shell backend in this build."? | TDESK | TDESK-004 |
| How is a shell hosted in the Terminal window? | TDESK | TDESK-005 |
| Why does Ctrl+S freeze the screen? | TDESK | TDESK-006 |
| Why are System Monitor and Task Manager blank? | TDESK | TDESK-007 |
| Which apps exist, and which are compiled here? | TDESK | TDESK-009 |
| Why is CONFIG_CHERRYUSB_HOST set with a device-only firmware? | TOOL | TOOL-001 |
| Which IDCODEs does the Gowin programmer accept? | TOOL | TOOL-002 |
| What versions are the build made from? | TOOL | TOOL-003 |
| Why must a bitstream be device-specific? | TOOL | TOOL-004 |
| Where is the USB-enumeration bisection kept? | TOOL | TOOL-005 |
| Which host tools exist, and what do they need? | TOOL | TOOL-006 |
| What code is vendored into this project, and under what licence? | TOOL | TOOL-007 |
| Which SDK components does this firmware link, and under what licences? | TOOL | TOOL-008 |
| Who is upstream of this project, and in what order? | PROV | PROV-001 |
| Does this project carry the licences and notices it owes? | PROV | PROV-004 |
| What licence is this project under, and what is the exception? | PROV | PROV-004 |
| Where is colibri, and what are its terms? | PROV | PROV-003 |
| Which silicon revision is this board, and which revision is the core image? | PSX | PSX-001 |
| How much logic and block RAM does the device have? | PSX | PSX-002 |
| Which FPGA pins carry the core UART, and where is the board's clock? | PSX | PSX-003 |
| What is on the FPGA module's JTAG and UART header? | PSX | PSX-004 |
| What memory is on this board? | PSX | PSX-005 |
| How do I tell whether a core is loaded, and what is a core ID? | PSX | PSX-006 |

---

## 4. Fast lookup index

```yaml
DEV-001: "The FPGA is a GW5AST-138; its JTAG IDCODE is 0x0001081b, reported as ID=0001081b on every tangload"
DEV-002: "cores/console138k/nestang.bin is 4,593,044 bytes and byte-identical to nand2mario's generated console138k artifact (GW5AST-138B)"
DEV-003: "The FPGA keeps no configuration across a power cycle, so a core has to be reloaded with tangload on every boot before a ROM will run"
DEV-004: "A core bitstream can carry AE350 RISC-V software: the program is compiled to a hex file and read into a boot ROM inside the design with $readmemh, so it is synthesised into the bitstream and arrives with tangload; the firmware loads whole images and never AE350 code as an artifact of its own"
BRD-001: "BL616 to FPGA JTAG: TMS GPIO0, TCK GPIO1, TDO GPIO2, TDI GPIO3"
BRD-002: "SD is gated behind GPIO 16 held high; without it f_mount returns FR_NOT_READY (3)"
BRD-003: "The two USB-A controller ports are FPGA pins: usb1_dp/dn H13/G13, usb2_dp/dn M15/M16, all IO_TYPE=LVCMOS33"
BRD-004: "The onboard debug bridge is a SIPEED FT2232 (0403:6010, product 'USB Debugger'), a separate USB path from the BL616's CDC"
BRD-005: "The board has two power inputs and runs on either; a power cycle is unplugging both and restoring power first"
PMOD-001: "Two PMOD sockets: PMOD1 beside HDMI on W19 W20 F19 F20 E22 D22 E21 D21 and PMOD0 on V18 V19 G21 G22 F18 E18 C22 B22, all LVCMOS33; Sipeed interleaves the rows, so IO0/2/4/6 are Digilent pins 1-4 and IO1/3/5/7 are pins 7-10, and flipping a module swaps pins 1-4 with 7-10"
PMOD-002: "/tang.ini at the SD root is the socket contract (pmod0/pmod1 plus _flip, flat under [tang]); a missing file or absent entry releases the socket and unknown modules are refused; modules carry no ID pins, so presence can never be detected, and the parser is firmware work - formerly Tang-Control's, now this project's"
PMOD-003: "Socket control register 0x10: bit 0 renderer, bits 4-7 PMOD0 personality and 8-11 PMOD1, bits 12/13 upside-down; personalities 0 none, 1 oledrgb, 2 vga J1, 3 vga J2; 0x14 is scratch"
BL6-001: "USB_BASE 0x20072000; OTG_CSR +0x80 (ID 21, CROLE 20, SPD 23:22, VBUS_VLD 19, A_SESS 18, B_SESS 17, A_BUS_DROP 5, A_BUS_REQ 4); PDS usb_ctl 0x2000E500 (IDDIG 5, DRVBUS_POL 4)"
BL6-002: "OCRAM is 320 KB at 0x20FC0000; the PSRAM window is declared but this board has no external RAM"
BL6-003: "The allocator is TLSF: mem.h exposes g_kmemheap, kfree_size(), and heapsize; PMEM_HEAP is the same heap unless the chip is a BL618"
BL6-004: "xPortGetFreeHeapSize() and xPortGetMinimumEverFreeHeapSize() do not exist in this build; using them fails at link time"
BL6-005: "There is an HBN always-on RTC counter (HBN_Enable_RTC_Counter, HBN_Get_RTC_Timer_Val) with a selectable 32 kHz source: a counter, not a calendar, and no battery"
BL6-006: "bflb_clk_get_system_clock(BFLB_SYSTEM_CPU_CLK) returns the CPU clock"
USB-001: "The CDC console negotiates USB 2.0 High Speed, 480 Mbps (lsusb -t); CONFIG_USB_HS sets CDC_MAX_MPS 512, which is legal only at HS"
USB-002: "No role signal follows the cable: OTG_CSR's ID bit tracks the forced configuration (PDS IDDIG), not the connector"
USB-003: "The OTG connector does not source VBUS: A_BUS_REQ is set and nothing comes out, in both DRVBUS_POL polarities, and a device that reacts to power stayed dark for 60 s"
USB-004: "usbd_add_endpoint() assigns by endpoint index, not by appending, and usbd_deinitialize() resets intf_offset and calls usb_dc_deinit(), so tdsh_bl616_console_init() is safe to call again"
USB-005: "There is no host deinit in the SDK: usbh_deinitialize() is software-only, and usb_hc_low_level_init() has no counterpart, so the port must be returned to device mode by hand"
USB-006: "CherryUSB fires USBD_EVENT_CONFIGURED on enumeration and USBD_EVENT_DISCONNECTED when the host goes away: the usable host-present signal"
FLS-001: "Application at 0x40000, staging at 0x100000, commit runs from .tcm_code with interrupts off; a soft reset lands in the vendor loader, so a reflash needs a power cycle"
PROT-001: "Frames are 0xAA len_hi len_lo type payload[len-1]; length is big-endian and counts the type byte; a length high byte >= 8 drops the core back to hunting for magic"
PROT-002: "Commands: 01 core ID, 02 config string, 03 joypad (core to BL616), 04 cursor, 05 text, 06 loading state, 07 ROM data, 08 overlay, 09 HID, 0a/0b floppy, 0c PS/2"
PROT-003: "BL616 UART1, TX GPIO 28, RX GPIO 27, 2,000,000 baud, 8N1"
PROT-004: "The text page is 32 columns by 28 rows of 8x8 cells from a full ASCII font (FONT[0:127][0:7]); column 0 draws in the cursor colour; the core's logo sits at LOGO_X 92, LOGO_Y 201"
PROT-005: "overlay selects the whole picture at the mixer (nes2hdmi.sv: if (overlay) rgb <= overlay_color), and a core comes out of reset with it on"
PROT-006: "The core sends its joypad state as response 0x03 every 20 ms when it changes, unconditionally, whether or not anyone asked"
PROT-007: "A ROM stream into a core that is not running is discarded with no error on either side and surfaces as a black screen; the fpga ID probe between the loads is the only detection"
NEST-001: "nestang's FPGA USB host is low-speed only, 1.5 Mbps, on two GPIO wires with external 15K pull-downs and a 12 MHz clock; its signalling engine is a 1072-byte ROM program"
NEST-002: "nestang wires only .game_snes from the FPGA host; the keyboard outputs key_modifiers and key1..key4 are unconnected, so a keyboard enumerates and is discarded"
TCTL-001: "Tang-Control's OSD is the BL616's work: overlay_cursor and overlay_printf are commands 0x04 and 0x05, and navigation is literal bit tests on the joypad word"
TCTL-002: "Tang-Control's default makes the OTG connector a USB host; TANG_USB_CDC_CONSOLE, supported only on console138k, makes it a CDC console instead"
TCTL-003: "Tang-Control documents the low-speed host's 15K pull-downs and its DS2-adapter support, and normalises extended baud rates to 2 Mbps before JTAG"
TCTL-004: "Tang-Control is a fork of nand2mario's firmware-bl616; its CDC command console and the transport protocols behind it are additions on a feature branch, and its USB identity is 0xFFFF:0x6160"
TCTL-005: "Three wiring modes: one-wire user (normal core use), one-wire diag (JTAG and UART straight to the FPGA through the FT2232 debug cable), two-wire debug (power plus the CDC cable in the board's bottom-left USB-C port)"
TCTL-006: "Cores: 1 NES/nestang.bin, 2 SNES/snestang.bin, 3 GBA/gbatang.bin, 4 MegaDrive/mdtang.bin, 5 SMS/smstang.bin, 6 PC-XT/pctang.bin, 0x50 Phosphor/phosphortang.bin; an image is looked for at cores/<board>/<name> then cores/<name>"
TCTL-007: "The retired host client offered ping, status, rxstats, caps, peek, poke, baud, stream, bench, put, get, ls, rm, mkdir and firmware; there is no rename"
TCTL-008: "No-BOOT update: patch the boot header (body length at 0x84, CRC-32 of the first 252 bytes at 0xFC), send it as a firmware command over CDC, power-cycle, then confirm app_sha256"
TCTL-009: "The RX task kept counters for bytes, joypad frames, FIFO overflows, FIFO high water, resync bytes, unknown frame types and longest poll gap; the rework adding interrupt-driven RX and a TX mutex fixed gamepad and OSD stutter"
TCTL-010: "Helper scripts being retired: tangctl.py, liveuart.py, liveuart_draw.py, print_uart.py, jtag.py, tdi_compare.py, crc16.sh and fs.py, which converts a Gowin .fs to .bin"
TCTL-011: "Tang-Control is the architecture of record for the Phosphor core and the cores after it, and is hardware-verified as a whole because this project was built from it; a record's own status still reports whether that specific fact was confirmed here"
TDSH-001: "TinyDesk Shell v0.1.3 at 232a39f; uScript 1.1.1 with if/while/for/function, pipes, redirection; Linux and Windows host ports"
TDSH-002: "The shell emits a closed set: CR, LF, ESC[2K, ESC[2J, ESC[H, ESC[<n>C, ESC[<n>D and SGR colour, and nothing else"
TDESK-001: "TinyDesk pins tinydesk-shell at 232a39f, the same revision as this project's submodule; its port surface is td_hal_t: read_byte, write, millis, sleep_ms"
TDESK-002: "Screen memory is TD_MAX_COLS x TD_MAX_ROWS x 8 bytes, twice; the ESP32-C6 uses 80x25 or 256x96, and 100x30 costs 48 KB for the pair"
TDESK-003: "TinyDesk's filesystem is ports/common/td_fs_stdio.c, written against stdio, dirent.h and sys/stat.h, so it lands on this port's FatFS syscall layer unmodified"
TDESK-004: "The Terminal app draws 'No shell backend in this build.' exactly when s_backend is NULL; td_terminal_set_backend() supplies it"
TDESK-005: "A Terminal backend is td_term_backend_t: start, read, write, user, set_user; the ESP32-C6 reference runs the shell in a task over two FreeRTOS stream buffers"
TDESK-006: "The Editor saves with Ctrl+S; the host HAL clears IXON so it arrives, and a board port has no termios, so the user's terminal eats it as XOFF and freezes the display"
TDESK-007: "td_run() returns after td_quit(); td_shutdown() restores the terminal"
TDESK-008: "Network, MQTT, Modbus and Software Update are built on proto/td_sock.c and proto/td_tls.c: POSIX sockets, esp_timer.h, and mbedTLS with esp_crt_bundle.h"
TDESK-009: "Nine start-menu apps plus a clock window: About, Counter, Editor, Files, Log Viewer, Settings, System Monitor, Task Manager, Terminal, and Date & time"
TOOL-001: "CONFIG_CHERRYUSB_HOST is required for the CDC to enumerate with FreeRTOS enabled; CONFIG_NEWLIB stops enumeration"
TOOL-002: "The Gowin programmer's accepted IDCODEs: GW5A-25 0x0001281b, GW5AT-60 0x0001481b, GWAST-138 0x0001081b, GW5AT-138 0x0001181b, GW2A-18 0x0000081b"
TOOL-003: "Bouffalo SDK 2.0.0 at ~/.cache/tangcore-dev/sdk with the T-Head RISC-V GCC 10.2.0 toolchain"
TOOL-004: "A Gowin bitstream names its device: nestang's console138k project is GW5AST-138B, and build.tcl generates the project from the device name rather than checking one in"
TOOL-005: "The USB-enumeration bisection is kept as proj.min.conf, proj.nonewlib.conf and proj.rtos.conf plus ref/, selected by TINYTANG_MIN, TINYTANG_RTOS, TINYTANG_NONEWLIB, TINYTANG_REF, TINYTANG_USB_ONLY, TINYTANG_NO_FS and TINYTANG_NO_SHELL"
TOOL-006: "tools/ holds tinytang_flash.py (reflash over CDC), tinytang_put.py (file onto the card) and tinytang_run.py (run a shell command), all needing Python with pyserial"
TOOL-007: "ports/bl616/tang_jtag_programmer.c is nand2mario's Apache-2.0 Gowin GPIO JTAG programmer from Tang-Control's fpga/programmer.cpp, based on openFPGALoader, vendored unmodified apart from its include list"
TOOL-008: "Linked out of the SDK, each under its own licence rather than the SDK's: FreeRTOS V10.4.6 (MIT, (C) 2021 Amazon.com) via CONFIG_FREERTOS, CherryUSB (Apache-2.0) for the CDC console and its FreeRTOS OSAL, and FatFs R0.15 w/patch3 (ChaN, source-redistribution condition only); LVGL, TJpgDec, mbedTLS, littlefs and the codecs are not linked"
EXTCTL-001: "Tang-Control's extended channel is legacy frame type 0x10: version, opcode, sequence, address, data, CRC-16; opcodes 0x00 capabilities, 0x01 read32, 0x02 write32, 0x03 set baud (2 or 5 Mbps, both ends switch only after the response), 0x04 block write"
EXTCTL-002: "Frame type 0x12 writes 1 to 64 consecutive 32-bit words and applies none of them unless CRC, version, opcode, count, length and alignment all validate; the reply is a 0x10 response with opcode 0x84 and the word count"
EXTCTL-003: "Frame type 0x11 is a stop-and-credit stream: flags start, data, end and cancel, at most 1024 data bytes per frame, and the FPGA acknowledges each frame with the next expected offset and receive credit"
EXTCTL-004: "Only the Phosphor core implements the extended protocol; nestang's iosys consumes an unknown frame type and ignores it, so caps, peek, poke and baud need a Phosphor core loaded"
EXTCTL-005: "Hold the shared link across a whole request and its response, not just the transmit bytes; protecting only the send lets another packet overtake the outstanding reply"
PHOS-001: "Phosphor: the core image sits at cores/console138k/phosphortang.bin and audio under music/; single files decode on the AE350 through Rockbox codecs, while playlists are WAV and FLAC only"
PHOS-002: "Playlists accept VLC-style .m3u and UTF-8 .m3u8: at most 255 tracks, 512-byte source lines, 255-byte resolved paths, paths relative to the playlist, and rejection of URLs, HLS, nested playlists, missing files and unsupported formats"
PHOS-003: "Track metadata prefers FLAC ALBUM, ALBUMARTIST (over ARTIST), ARTIST and TITLE comments, then WAV RIFF LIST/INFO IPRD, IART and INAM, then playlist-name and #EXTINF fallbacks; display text is UTF-8 reduced to the core's ASCII font, unsupported code points becoming one '?'"
PHOS-004: "FLAC is sent as fLaC with STREAMINFO marked as the last metadata block plus the unchanged frames, so large PICTURE or PADDING blocks do not delay the first frame; gapless handover needs core capability bit 7 and the player's draining state 7, queued up to one PCM FIFO (about 0.4 s) ahead"
PHOS-005: "Cover art is a baseline JPEG centre-fitted to 92x92 RGB332 on the BL616 and uploaded to an inactive FPGA bank before one atomic commit; the audible-stream register 0xa4 gates the display change"
PROV-001: "Lineage as the user states it: nand2mario's TangCore is the origin for Tang-Phosphor and Tang-PSX; Tang-Control is a fork of the same repo for peek/poke and the 1-wire and 2-wire debug arrangements; the family also uses a DDR3 IP block, TinyDesk, and CERN's colibri as a reference; the remembered memory module turned out to be nand2mario's JTAG bit-bang programmer, and Tang-PSX is being archived rather than deleted"
PROV-002: "Superseded by PROV-004. Recorded the licence and notice files when they landed, at which point the project licence was Apache-2.0"
PROV-004: "TinyTang's own code is MIT, in LICENSE; the single exception is ports/bl616/tang_jtag_programmer.c, which stays Apache-2.0 as nand2mario's file with its text at LICENSES/Apache-2.0.txt and a note added to its header. Apache-2.0 was never required - it is permissive, and there is no copyleft in the tree"
PROV-003: "colibri is CERN's vendor-independent, fully verified, open-source VHDL common library at gitlab.cern.ch/colibri/colibri, with an unofficial SystemVerilog port at github.com/kavierim/colibri-sv pinned to upstream 3fa78412; read the port when working in SystemVerilog since it ships an AGENTS.md and targets Verilator and free tooling, but treat the original as the authority; licensed CERN-OHL-W-2.0 (weakly reciprocal), not MIT, with no code copied here"
PSX-001: "This board is GW5AST-138 revision C (package mark 2518CA0N), while the console138k core image it loads is built for revision B and works anyway"
PSX-002: "GW5AST-138: 138,240 LUTs, 139,095 registers, 340 BSRAM blocks of 18 Kbit, about 765 KB"
PSX-003: "50 MHz oscillator on FPGA pin V22; the BL616 control UART reaches the FPGA on V14 (FPGA RX) and U15 (FPGA TX), LVCMOS33 - the other end of the link PROT-003 describes from the BL616 side"
PSX-004: "Module connector U1201, 8-pin JST SH: 1 5V0 via diode (~4.4 V), 2 TMS T13, 3 TDO U13, 4 TCK V12, 5 TDI R13, 6 RX V14, 7 TX U15, 8 GND; the JTAG nets are shared with the BL616, so an external adapter must be released during a tangload, pins 6 and 7 stay unconnected, and pin 1 must not reach a 3.3 V adapter"
PSX-005: "Dock SDRAM is Winbond W9825G6KH-6, 32 MB x16 at 166 MHz, on connectors J9 and J10, separate from the SOM's 1 GiB x32 DDR3; neither is the BL616's memory, which is 320 KB of on-chip OCRAM (BL6-002)"
PSX-006: "A loaded core is indicated by active_core, the low byte of its CORE_ID (0x01 nestang, 0x50 Phosphor, 0x51 Gate 1), not by core_running, which reads no while a core answers; uploads are refused while a core runs; the link is 2 Mbaud with a 5 Mbaud fast mode and needs an iosys clock at least 8x the baud"
```

---

## 5. Records

```yaml
- record_id: DEV-001
  kind: DEVICE
  topic_id: DEV
  title: "The board's FPGA is a GW5AST-138, identified by JTAG IDCODE 0x0001081b"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "The Tang Console 138K carries a GW5AST-138. Its JTAG IDCODE is 0x0001081b, and the acknowledged IDCODE constant for that part is IDCODE_GWAST_138."
  consequence: "Any bitstream loaded by tangload must be built for GW5AST-138B. The programmer checks the detected IDCODE against its table of known parts, and every load here reports ID=0001081b."
  sources:
    - "Tang-Control, fpga/programmer.cpp: #define IDCODE_GWAST_138 0x0001081b (local checkout, /run/media/vash/GIT/Tang-Control)"
    - "Tang-Control, fpga/programmer.cpp: the accepted-IDCODE test in fpga_program()"
  verification: "Reported by the programmer on every tangload run against this board, for example 'Writing 4593044 bytes...ID=0001081b'. Consistent across the whole session."

- record_id: DEV-002
  kind: DEVICE
  topic_id: DEV
  title: "cores/console138k/nestang.bin is nand2mario's generated console138k artifact"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "The NES core on the card is 4,593,044 bytes with MD5 808b1f14b19d2d2db670ba95b8fa9ebf, byte-identical to the artifact produced by the nestang tree's console138k target. The place-and-route report for that build names GW5AST-138, device version B, part GW5AST-LV138PG484AC1/I0. The console60k image is 2,321,194 bytes."
  consequence: "The image is the right one for this board; there is no 60K/138K ambiguity to resolve. Note that the 138K project is generated from the device name by build.tcl rather than checked in as a .gprj, so searching for a project file will not find it."
  sources:
    - "nestang, impl/gwsynthesis/nestang_console138k_ds2.prj: Device id GW5AST-138B, partNumber GW5AST-LV138PG484AC1/I0 (local checkout /run/media/vash/GIT/tangcore/nestang)"
    - "nestang, impl/pnr/nestang_console138k_ds2.rpt.txt: Device GW5AST-138, Device Version B"
    - "nestang, build.tcl console138k branch: set_device GW5AST-LV138PG484AC1/I0 -device_version B, plus src/boards/console138k.v"
    - "nestang commit 976c326 'add console 138k build files', 2025-03-29"
  verification: "MD5 of the card's image compared against the built artifact; sizes compared against the console60k image. The card's copy is dated 2025-05-02, matching the TangCore 0.9 release the card was built from."

- record_id: DEV-003
  kind: DEVICE
  topic_id: DEV
  title: "The FPGA keeps no configuration across a power cycle, so a core is reloaded every boot"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "Nothing on this board reconfigures the GW5AST-138 on power-up: the fabric comes up empty and the BL616 has to program a core image over JTAG. On this bench a core was never present after a power cycle until tangload ran, and the load was what brought the HDMI output and the UART link to life."
  consequence: "Any procedure that spans a power cycle must reload the core, which is why a cartridge takes three commands in order - tangload to put the core in, fpga to confirm it answers, nesload to stream the ROM - rather than assuming a core is there. It also means there is no persisted fabric state to reason about at boot."
  sources:
    - "This project's session behaviour: a core appeared only after tangload on every boot"
    - "BRD-001: the JTAG pins the load drives"
  verification: "Exercised on this board across many power cycles; each began with an unconfigured FPGA until tangload ran. Not tested against a hypothetical on-board reconfiguration path, because none was found."

- record_id: DEV-004
  kind: DEVICE
  topic_id: DEV
  title: "A core bitstream can carry AE350 RISC-V software, and tangload is how it reaches the board"
  status: SOURCED
  verified_date: 2026-10-03
  statement: "The GW5AST contains an AE350 hard RISC-V core, and a design may run software on it. That software is not a separate artifact at load time: it is compiled, written to a hex file, and read into a boot ROM inside the design with $readmemh, so it is synthesised into the bitstream and travels with it. Tang-Phosphor's AE350 images are built that way - scripts/build-ae350-ddr3.sh builds the boot ROM from software/ae350 alongside the gateware, and src/ae350/ae350_boot_rom.sv takes its INIT_FILE as ae350_boot.hex and fills the ROM from it at elaboration."
  consequence: "Deploying a core is therefore also how AE350 software reaches this board, and this project does run it: phosphortang.bin decodes audio on the AE350 (PHOS-001), so its RISC-V program arrived here inside that image by way of tangload. The boundary that matters is worth stating because it is easy to state wrongly in either direction - this firmware loads whole bitstreams and never loads AE350 code as an artifact of its own, yet the RISC-V code it runs is there because of what it loads. A description that omits the AE350 understates what the board is doing; one that says the firmware loads AE350 code implies a load path that does not exist."
  sources:
    - "Tang-Phosphor, src/ae350/ae350_boot_rom.sv: the INIT_FILE parameter defaulting to 'ae350_boot.hex' and the initial $readmemh(INIT_FILE, rom) that fills the ROM"
    - "Tang-Phosphor, scripts/build-ae350-ddr3.sh: 'Build the AE350 + DDR3 image: the boot ROM (software/ae350), the Gowin DDR3 IP ...'"
    - "Tang-Phosphor, src/ae350/ae350_subsystem.sv: the ae350_boot_rom instantiation"
  verification: "Read from Tang-Phosphor's AE350 design this session: the ROM init file, the $readmemh, and the build script that produces it. Not exercised on this bench, and no AE350 software was built here."

- record_id: BRD-001
  kind: BOARD
  topic_id: BRD
  title: "BL616 to FPGA JTAG pins are GPIO 0 through 3"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "The board's JTAG link from the BL616 to the FPGA uses TMS = GPIO 0, TCK = GPIO 1, TDO = GPIO 2, TDI = GPIO 3."
  consequence: "ports/bl616/tang_jtag_glue.h defines these four, and the vendored Gowin programmer drives them for its fast bit-bang path. Every core load depends on this mapping."
  sources:
    - "Tang-Control, utils/init.cpp: the GPIO_HIGH_Z register writes annotated reg_gpio_tms/tck/tdo/tdi"
    - "This project's ports/bl616/tang_jtag_glue.h, reproduced from the working build's preprocessed source"
  verification: "tangload programs the FPGA successfully and repeatedly on this board."

- record_id: BRD-002
  kind: BOARD
  topic_id: BRD
  title: "The SD card is power-gated behind GPIO 16, held high"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "The card does not come up unless GPIO 16 is driven high. Without it f_mount returns FR_NOT_READY (3). The SDK's dev-board SD init does not touch that pin."
  consequence: "ports/bl616/tdsh_fs_bl616.c drives GPIO 16 before mounting; sdh_power_enable() is the only board-specific line in the mount path."
  sources:
    - "Tang-Control, utils/init.cpp: 'Set GPIO 1 (physical pin 15) to high to enable SDMMC' followed by bflb_gpio_set(gpio_dev, GPIO_PIN_16)"
  verification: "Measured on this board: FRESULT 3 before, 0 after. The shell reads and writes the card."

- record_id: BRD-003
  kind: BOARD
  topic_id: BRD
  title: "The two USB-A controller ports are FPGA pins, not BL616 pins"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "The Tang Console's two USB-A ports reach the FPGA on four pins, all IO_TYPE=LVCMOS33: usb1_dp H13 and usb1_dn G13 (the left port), usb2_dp M15 and usb2_dn M16. There is no PHY or transceiver on them."
  consequence: "Nothing the BL616's firmware does can change those ports. They are driven by the FPGA design loaded into the fabric, which is why the keyboard question is a core question and not a firmware one."
  sources:
    - "nestang, src/boards/console.cst: 'USB1 and USB2 (usb1 is on the left)' with the four IO_LOC/IO_PORT lines"
    - "nestang, src/nestang_top.sv: usb_hid_host and usb_hid_host2 instantiated on usb1_dn/dp and usb2_dn/dp under ifdef USB1/USB2"
  verification: "Read from the constraint file and the instantiation, and consistent with the observed behaviour of the ports under nestang."

- record_id: BRD-004
  kind: BOARD
  topic_id: BRD
  title: "The onboard debug bridge is a SIPEED FT2232, a separate USB path"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "The board carries an onboard debug bridge enumerating as 0403:6010 FT2232C/D/H Dual UART/FIFO, manufacturer SIPEED, product 'USB Debugger', two vendor-class interfaces. It is a distinct USB device from the BL616's CDC, and it appears on the host only when the host is connected to that path."
  consequence: "Its presence in lsusb and the absence of ffff:5454 is the signature of the host being on the debug path rather than the BL616's OTG connector, which is a useful diagnostic when the console is unexpectedly missing."
  sources:
    - "Observed in lsusb on this workstation: 'Bus 001 Device NNN: ID 0403:6010 Future Technology Devices International, Ltd FT2232C/D/H Dual UART/FIFO IC', with sysfs manufacturer SIPEED and product USB Debugger"
  verification: "Seen directly during a port-mode change that removed the BL616 from the bus."

- record_id: BRD-005
  kind: BOARD
  topic_id: BRD
  title: "The board has two power inputs and runs on either"
  status: SOURCED
  verified_date: 2026-10-03
  statement: "The Tang Console has a dedicated USB power input in addition to the OTG connector, and the board runs from either supply. The user's power-cycle procedure is to unplug both and restore power first, then the OTG cable."
  consequence: "Host mode on the OTG connector would not need to power the board, and the power cycle needed after a tangflash is a physical one rather than a reset. Playback of this fact should not be assumed beyond that."
  sources:
    - "User report, 2026-10-03: 'my tang has both the power and otg USB cable plugged in. they both supply power and the tang can run on either'"
  verification: "Not instrumented. Consistent with the board surviving removals and reinsertions of the OTG cable with the console session intact, but the power routing was not measured."

- record_id: BL6-001
  kind: SOC
  topic_id: BL6
  title: "USB register block addresses and the bits that describe the port's role"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "The BL616's USB register block is at USB_BASE 0x20072000. OTG_CSR is at +0x80 and carries USB_ID_HOV_POV (bit 21), USB_CROLE_HOV_POV (bit 20), USB_SPD_TYP_HOV_POV (bits 23:22), USB_VBUS_VLD_HOV (19), USB_A_SESS_VLD (18), USB_B_SESS_VLD_POV (17), USB_A_BUS_DROP_HOV (5) and USB_A_BUS_REQ_HOV (4). The power-domain USB control register is at BFLB_PDS_BASE 0x2000E000 + 0x500, carrying PDS_REG_USB_IDDIG (bit 5) and PDS_REG_USB_DRVBUS_POL (bit 4)."
  consequence: "ports/bl616/tang_usbstat.c reads OTG_CSR and usb_ctl to report these; ports/bl616/tang_usb_role.c writes A_BUS_DROP, A_BUS_REQ, IDDIG and DRVBUS_POL. OTG_ISR is deliberately not read: its bits include pulse-or-value flags the running stack may depend on."
  sources:
    - "Bouffalo SDK, drivers/lhal/include/hardware/usb_v2_reg.h: the 0x80 : OTG_CSR block and USB_OTG_CSR_OFFSET 0x80"
    - "Bouffalo SDK, drivers/lhal/src/bflb_usb_v2.c (lines 10-31): the local defines BFLB_USB_BASE, BFLB_PDS_BASE, PDS_USB_CTL_OFFSET, PDS_REG_USB_DRVBUS_POL, PDS_REG_USB_IDDIG"
    - "Bouffalo SDK, drivers/lhal/config/bl616/bl616_memorymap.h: USB_BASE 0x20072000"
  verification: "Read on this board: usbstat prints OTG_CSR 0x00be0020 in device mode and 0x000e0010 in host mode, and usb_ctl 0x00000023 in device mode and 0x00000003 with IDDIG cleared in host mode. The bit interpretations match those values."

- record_id: BL6-002
  kind: SOC
  topic_id: BL6
  title: "The BL616 has 320 KB of OCRAM and this board fits no external RAM"
  status: SOURCED
  verified_date: 2026-10-03
  statement: "The BL616's on-chip OCRAM is 320 KB at base 0x20FC0000. A PSRAM window is declared in the memory map, but the SDK's mem.h maps PMEM_HEAP to the same internal heap unless the chip is a BL618, so on this part there is a single heap."
  consequence: "The desktop's 100x30 geometry was chosen to fit: 24,008 bytes per screen buffer against a 320 KB budget. The desktop port reports no external RAM to System Monitor rather than a zero."
  sources:
    - "Bouffalo SDK, drivers/lhal/config/bl616/bl616_memorymap.h: BL616_OCRAM_BASE 0x20FC0000, BL616_OCRAM_END + 320 * 1024"
    - "Bouffalo SDK, components/mm/mem.h: '#if defined(CONFIG_PSRAM) && defined(BL616) // only for bl618' guards PMEM_HEAP"
  verification: "Read from the SDK headers. The build's linker output is consistent: g_kmemheap at 0x62fcb63c with a heap region of about 251 KB, and no separate PSRAM heap in use."

- record_id: BL6-003
  kind: SOC
  topic_id: BL6
  title: "The allocator is TLSF, and its free figure is queryable"
  status: SOURCED
  verified_date: 2026-10-03
  statement: "The SDK's allocator is TLSF, exposed through components/mm/mem.h as 'struct mem_heap_s { void *priv; void *heapstart; size_t heapsize; size_t free_bytes; }' with the global g_kmemheap and the functions kfree_size() and pfree_size(). bflb_tlsf.c updates free_bytes on every allocation, free and realloc, and sets heapsize to the region size less tlsf_size()."
  consequence: "ports/bl616/td_desktop_bl616.c supplies System Monitor's heap readings from kfree_size() and g_kmemheap.heapsize. It tracks its own low-water mark, because the SDK keeps none; that is a minimum of its own samples, not of every allocation."
  sources:
    - "Bouffalo SDK, components/mm/mem.h: the struct, the externs, and kfree_size"
    - "Bouffalo SDK, components/mm/tlsf/bflb_tlsf.c: heap->heapsize = heapsize - tlsf_size(); heap->free_bytes adjustments in bflb_malloc/bflb_free/bflb_realloc"
    - "Bouffalo SDK, components/mm/mem.c: kfree_size() returns g_kmemheap.free_bytes"
  verification: "Read from the SDK. The API resolves and links, so it exists in this build, but the values it returns have not been read on hardware - System Monitor was reported blank before this was added and its result was not separately confirmed afterward."

- record_id: BL6-004
  kind: SOC
  topic_id: BL6
  title: "FreeRTOS's own heap queries do not exist in this build"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "xPortGetFreeHeapSize() and xPortGetMinimumEverFreeHeapSize() are declared in FreeRTOS's portable.h and defined in heap_1.c and heap_5.c, but are not present in this firmware: the SDK allocates through TLSF, and the FreeRTOS heap implementations are not linked."
  consequence: "Reaching for the familiar FreeRTOS heap API fails at link time, not at runtime. Use mem.h's kfree_size() and g_kmemheap instead; see BL6-003."
  sources:
    - "Link error on this project's build: 'undefined reference to xPortGetFreeHeapSize' and '... xPortGetMinimumEverFreeHeapSize' from ports/bl616/td_desktop_bl616.c"
    - "Bouffalo SDK, components/os/freertos/include/portable.h and portable/MemMang/heap_5.c: the declarations and heap_5 definitions"
  verification: "Observed directly as a build failure, then resolved by switching to mem.h."

- record_id: BL6-005
  kind: SOC
  topic_id: BL6
  title: "An always-on RTC counter exists, but it is a counter, not a clock"
  status: SOURCED
  verified_date: 2026-10-03
  statement: "The BL616's HBN (always-on) domain provides HBN_Enable_RTC_Counter(), HBN_Clear_RTC_Counter(), HBN_Set_RTC_Timer() and HBN_Get_RTC_Timer_Val(), the last reading HBN_RTC_TIME_H with a latch. The 32 kHz source is selectable among HBN_32K_RC (internal RC), HBN_32K_XTAL (external crystal) and HBN_32K_DIG. There is no calendar and no battery."
  consequence: "It can measure elapsed time independently of the CPU clock and would plausibly survive a soft reset, but a wall clock still has to be told the time once per power-up. This project currently reports no clock and the taskbar says so. Whether this board fits a 32.768 kHz crystal is not established."
  sources:
    - "Bouffalo SDK, drivers/soc/bl616/std/include/bl616_hbn.h: the RTC counter and timer prototypes, and the HBN_32K_* type definitions"
    - "Bouffalo SDK, drivers/soc/bl616/std/src/bl616_hbn.c line 1781: the implementation of HBN_Get_RTC_Timer_Val"
  verification: "Read from the SDK, including the implementation. Not exercised on hardware: no command reads the counter yet."

- record_id: BL6-006
  kind: SOC
  topic_id: BL6
  title: "The CPU clock is readable through bflb_clk_get_system_clock"
  status: SOURCED
  verified_date: 2026-10-03
  statement: "bflb_clk_get_system_clock(BFLB_SYSTEM_CPU_CLK) returns the CPU clock in Hz; BFLB_SYSTEM_CPU_CLK is defined as 1."
  consequence: "ports/bl616/td_desktop_bl616.c converts it to MHz for System Monitor's CPU figure."
  sources:
    - "Bouffalo SDK, drivers/lhal/include/bflb_clock.h: '#define BFLB_SYSTEM_CPU_CLK 1' and 'uint32_t bflb_clk_get_system_clock(uint8_t type)'"
  verification: "Resolves and links in this build. The value it returns has not been read on hardware."

- record_id: USB-001
  kind: USB
  topic_id: USB
  title: "The CDC console negotiates USB 2.0 High Speed, 480 Mbps"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "The firmware's CDC device enumerates at High Speed. The kernel reports the two CDC interfaces at 480M, and the build sets CONFIG_USB_HS, which makes CDC_MAX_MPS 512 - a bulk packet size legal only at High Speed."
  consequence: "Frame rate and transfer size for anything streamed over the console (the desktop's ANSI output, tool uploads) are bounded by USB and not by the serial framing. The desktop's redraws over this link are far cheaper than they would be over a UART."
  sources:
    - "lsusb -t on this workstation: 'Port 004: Dev NNN, If 0, Class=Communications, Driver=cdc_acm, 480M' and the paired CDC Data interface, also 480M"
    - "Bouffalo SDK, components/usb/cherryusb/CMakeLists.txt: if(CONFIG_USB_HS) sdk_add_compile_definitions(-DCONFIG_USB_HS)"
    - "This project's ports/bl616/usb_cdc_bl616.c: '#ifdef CONFIG_USB_HS #define CDC_MAX_MPS 512'"
  verification: "Measured on the wire with lsusb, not inferred from configuration."

- record_id: USB-002
  kind: USB
  topic_id: USB
  title: "No role signal follows the cable; the ID bit tracks forced configuration"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "Across a swap of the OTG cable from a PC to a keyboard and back, including a period with nothing attached, OTG_CSR's USB_ID_HOV_POV, USB_CROLE_HOV_POV, USB_VBUS_VLD_HOV, USB_A_SESS_VLD and USB_B_SESS_VLD_POV never changed. Only the speed field moved, from high to full and back, which tracks the link rather than the attachment. In device mode USB_ID reads 1 and in host mode it reads 0, following the PDS_REG_USB_IDDIG bit that the host bring-up clears."
  consequence: "Role detection cannot be built on the OTG block's status. Use the device stack's own enumeration events instead - see USB-006. Ports/bl616/tang_usbwatch.c and tang_usbstat.c are the instruments."
  sources:
    - "This project's /usbrole.log, produced by usbwatch 0.1.3: 'go_host', 'probe start, budget 60s', then the same csr=000e0010 pds=00000003 repeated, then 'probe result: host: nothing enumerated in 60s'"
    - "Bouffalo SDK, drivers/lhal/src/bflb_usb_v2.c usb_hc_low_level_init(): 'enable device-A for host' by clearing PDS_REG_USB_IDDIG"
  verification: "Measured on this board by moving the cable with a probe running, twice; the second run added the DRVBUS_POL phase and the timestamps."

- record_id: USB-003
  kind: USB
  topic_id: USB
  title: "The OTG connector does not source VBUS, so it cannot host"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "With the port switched to host, the SDK's host bring-up runs and requests power - OTG_CSR ends with USB_A_BUS_REQ_HOV set and USB_A_BUS_DROP_HOV clear - but no power appears on the connector. A device that visibly reacts to power stayed dark through a full 60-second window, and the OTG status never changed. Flipping PDS_REG_USB_DRVBUS_POL, which the SDK defines and never writes anywhere, made no difference. It is not a rail that is always live either: in device mode, with no PC attached, a device gets nothing, which is correct for a peripheral and shows the port's VBUS is switched rather than shared with the board's supply."
  consequence: "No keyboard, gamepad or storage device can be hosted on this connector. The role switching in ports/bl616/tang_usb_role.c works; there is no hardware behind the host half. The practical workaround for powering a device would be a self-powered hub in the port, which is untested."
  sources:
    - "This project's /usbrole.log: 'phase 1: DRVBUS_POL=0, csr=000e0010 pds=00000003', 'phase 2: DRVBUS_POL=1, pds=00000013', 'probe result: host: nothing enumerated in 60s'"
    - "Bouffalo SDK, drivers/lhal/src/bflb_usb_v2.c usb_hc_low_level_init(): the drop-then-request VBUS sequence"
    - "nestang, src/nestang_top.sv comment on the console138k usage: the Console has onboard SD and FPGA-side controller ports"
  verification: "Two hardware runs with the cable moved by the user, plus a separate test in device mode with no host attached. The DRVBUS_POL flip is proven to have executed by the pds value in the log."

- record_id: USB-004
  kind: USB
  topic_id: USB
  title: "The console's endpoints can be re-registered safely"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "usbd_add_endpoint() assigns into the endpoint arrays by index rather than appending, so registering the same endpoint twice overwrites rather than duplicating. usbd_deinitialize() sets g_usbd_core.intf_offset = 0 and calls usb_dc_deinit(). usbd_initialize() is just usb_dc_init()."
  consequence: "tdsh_bl616_console_init() is safe to call again after a deinit, which is what makes the way back from host mode a plain re-init in ports/bl616/tang_usb_role.c."
  sources:
    - "Bouffalo SDK, components/usb/cherryusb/core/usbd_core.c: usbd_add_endpoint(), usbd_deinitialize(), usbd_initialize()"
  verification: "Read in the SDK before relying on it, then exercised: the device-to-host-to-device sequence ran and the device stack came back up. It did not produce a working console on its own - that required USB-005's fix - so this record establishes that re-registration is legal, not that the round trip works unaided."

- record_id: USB-005
  kind: USB
  topic_id: USB
  title: "There is no host deinit; the port must be returned to device mode by hand"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "The SDK provides usb_hc_low_level_init(), which initialises the PHY, attaches USBH_IRQHandler to interrupt 37, clears PDS_REG_USB_IDDIG to force A-device, and drives USB_A_BUS_DROP_HOV then USB_A_BUS_REQ_HOV to request VBUS. It provides no counterpart. usbh_deinitialize() is only usbh_hub_deinitialize(), a software teardown, so those register settings survive it."
  consequence: "Bringing the device stack back up after a host session succeeds but leaves the port insisting it is a host, and no host ever enumerates the board. ports/bl616/tang_usb_role.c's port_back_to_device() restores the three bits explicitly: set A_BUS_DROP, clear A_BUS_REQ, set IDDIG."
  sources:
    - "Bouffalo SDK, drivers/lhal/src/bflb_usb_v2.c: usb_hc_low_level_init() and the absence of any matching deinit"
    - "Bouffalo SDK, components/usb/cherryusb/core/usbh_core.c: usbh_deinitialize() calling usbh_hub_deinitialize()"
  verification: "Established the hard way: a host session left the board dark on USB even with the cable in a PC, and recovered only on a power cycle. The bit-restoring fix is in the source and the sequence now returns cleanly, though the fix itself was not separately re-measured after the port-mode change that followed."

- record_id: USB-006
  kind: USB
  topic_id: USB
  title: "CherryUSB's configured/disconnected events are the usable host-present signal"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "The device stack's usbd_event_handler() receives USBD_EVENT_CONFIGURED when a host has enumerated the device and configured it, and USBD_EVENT_DISCONNECTED when the host goes away."
  consequence: "This is the signal any role switching should be built on, given USB-002. ports/bl616/usb_cdc_bl616.c already turns them into s_configured, which tdsh_bl616_console_connected() exposes."
  sources:
    - "This project's ports/bl616/usb_cdc_bl616.c: usbd_event_handler(), setting and clearing s_configured"
  verification: "In use for the whole session: the configured flag is what the console's writes are gated on and what the OSD watch reports through."

- record_id: FLS-001
  kind: FLASH
  topic_id: FLS
  title: "Application at 0x40000, staging at 0x100000, and a soft reset lands in the vendor loader"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "The application occupies flash from 0x40000 with a maximum of 0x80000 bytes. A new image is staged in an erased region at 0x100000, verified against the file, and only then copied into the application slot. The copy must run from .tcm_code with interrupts disabled, because once the first application sector is erased no instruction may be fetched from the application's XIP flash. The boot header carries magic at 0x00, 0x08 and 0x64, a CRC-32 of the first 252 bytes at 0xFC, and a body length at 0x84 counting bytes after the 4 KiB header region. A soft reset lands in the vendor loader."
  consequence: "tangflash validates the header and the file's length against it before erasing anything, and the user power-cycles afterwards. The vendor loader below 0x40000 is never touched."
  sources:
    - "Tang-Control, utils/firmware_update.cpp: FW_APP_BASE, FW_STAGING_BASE, the sector helpers and the erase/write/verify loops"
    - "This project's ports/bl616/tdsh_tang_flash.c: the constants and check_boot_header()"
  verification: "tangflash has been run roughly a dozen times this session, each time followed by a power cycle and a working console."

- record_id: PROT-001
  kind: PROTOCOL
  topic_id: PROT
  title: "Frame format between the BL616 and a loaded core"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "Frames in both directions are 0xAA, a 16-bit length high byte first, a length low byte, a type byte, then payload. The length counts the type byte, so a frame carrying n bytes of data has length n+1. A frame whose length high byte is 8 or more drops the receiver back to hunting for the next 0xAA."
  consequence: "ports/bl616/tang_fpga_link.h documents this and FPGA_FRAME_MAX is 2047; tang_fpga_frame() refuses longer frames rather than sending them. The reference firmware's fpga_tx_header() writes len >> 8 first, confirming the byte order."
  sources:
    - "nestang, src/iosys/iosys_bl616.v: the protocol comment block above the receiver, and the RECV_IDLE/RECV_LEN1/RECV_LEN2 state machine with its 'max frame length 2047' check"
    - "Tang-Control, utils/utils.cpp: fpga_tx_header() writing 0xAA, len >> 8, len & 0xFF, cmd"
  verification: "Implemented and working: the core-ID probe, the cartridge stream and the OSD writes all use it."

- record_id: PROT-002
  kind: PROTOCOL
  topic_id: PROT
  title: "The command set a loaded core understands"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "Commands to the core: 0x01 get core ID, 0x02 get core config string, 0x03 set core config, 0x04 move the OSD cursor to x,y, 0x05 display a string from the cursor, 0x06 set loading state, 0x07 load data to the ROM path, 0x08 overlay on/off, 0x09 send USB joystick state, 0x0a send a floppy sector, 0x0b write a disk management register, 0x0c send a PS/2 scancode, 0x0d debug printf. Responses from the core: 0x01 core ID, 0x02 config string, 0x03 joypad state every 20 ms, 0x04 a floppy sector write, 0x05 a floppy sector read."
  consequence: "ports/bl616 covers 0x01, 0x04, 0x05, 0x06, 0x07 and 0x08, which is what the core-ID probe, the cartridge loader and the OSD need. Not implemented: the floppy, PS/2 and HID paths, which belong to other cores."
  sources:
    - "nestang, src/iosys/iosys_bl616.v: the command list in the header comment and the case statements in RECV_PARAM"
  verification: "Six of these commands are exercised on hardware: the core answers 0x01, casts a 131,088-byte ROM through 0x07, and draws through 0x04, 0x05 and 0x08."

- record_id: PROT-003
  kind: PROTOCOL
  topic_id: PROT
  title: "The core link is UART1 at 2 Mbaud"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "The BL616 reaches a loaded core on UART1, transmit GPIO 28 and receive GPIO 27, at 2,000,000 baud, 8N1. The core side is iosys_bl616's async receiver and transmitter, both parameterised with BAUD_RATE 2_000_000."
  consequence: "ports/bl616/tang_fpga_uart.c configures exactly this RP2350-free arrangement: GPIO 28 as UART1_TX, GPIO 27 as UART1_RX, and the RX side drained from an interrupt because the BL616's 32-byte FIFO cannot hold a burst at this rate."
  sources:
    - "Tang-Control, utils/init.cpp: bflb_gpio_uart_init for GPIO_PIN_28 as UART1_TX and GPIO_PIN_27 as UART1_RX in the non-Primer, non-Nano branch"
    - "nestang, src/iosys/iosys_bl616.v: 'localparam BAUD_RATE = 2_000_000' and the async_receiver/async_transmitter instantiations"
  verification: "fpga reports 'core 1 answering on UART1 at 2000000 baud'; a 131,088-byte ROM streams without a dropped byte."

- record_id: PROT-004
  kind: PROTOCOL
  topic_id: PROT
  title: "The core's on-screen text page: 32 by 28 cells of 8x8"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "A loaded core carries a text layer of 32 columns by 28 rows of 8x8 cells, drawn from a full ASCII bitmap font declared as FONT[0:127][0:7]. Each glyph is eight bytes, one per row, with the low bit the leftmost column; the eighth row is blank for most glyphs. Column 0 of every row draws in the cursor colour rather than the text colour, and cannot be switched off. The core draws its own logo into the same layer at LOGO_X 92 and LOGO_Y 201, 72 by 14 pixels, and it survives a clear because clearing writes the character buffer while the logo comes from a separate table."
  consequence: "ports/bl616/tang_osd.c clips text at the row end and its menu uses column 0 for the selection marker, which is the only highlight the page offers. ports/bl616/tang_osd_term.c stops at 25 rows to keep the scroll region clear of the logo, which occupies rows 25 and 26 across columns 11 to 20."
  sources:
    - "nestang, src/iosys/textdisp.v: the module comment '32x28 text display in 8x8 font', the LOGO_X/LOGO_Y constants, and the is_cursor and logo comparisons in the pixel logic"
    - "nestang, src/assets/font.vh: 'localparam [7:0] FONT[0:127][0:7]'"
    - "nestang, src/iosys/iosys_bl616.v: the comment on command 0x05, 'display string from cursor', and the cursor_x < 32 test"
  verification: "The user confirmed the page, the menu marker and the blinking cursor on hardware. The glyph table was decoded and checked: 'A', '0', '/', '_' and '.' render as expected."

- record_id: PROT-005
  kind: PROTOCOL
  topic_id: PROT
  title: "The overlay is a whole-picture layer, and a core starts with it on"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "The overlay is not a per-pixel text mask: the HDMI mixer selects it wholesale with 'if (overlay) rgb <= overlay_color'. A core comes out of reset with the overlay asserted, since iosys_bl616 declares 'reg overlay_reg = 1'."
  consequence: "A cartridge loaded without clearing the overlay plays its music behind a black screen carrying only the core's logo, which reads as a video fault and is not one. This cost a session to find, with the game audible the whole time. ports/bl616/tang_fpga_uart.c sends command 0x08 with 0 before releasing the core, through tang_osd_set() so the module's idea of the state stays true."
  sources:
    - "nestang, src/hdmi2/nes2hdmi.sv line 210: 'if (overlay) rgb <= {overlay_color...}'"
    - "nestang, src/iosys/iosys_bl616.v: 'reg overlay_reg = 1; assign overlay = overlay_reg;' and the command 0x08 handling"
  verification: "Observed: the game ran with music and no picture until the overlay was cleared, then the intro screen appeared. Confirmed by the user."

- record_id: PROT-006
  kind: PROTOCOL
  topic_id: PROT
  title: "The core reports its joypad state unprompted every 20 ms"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "iosys_bl616 sends response 0x03 unprompted, with a frame length of 5, whenever the joypad state changes and at most every 20 ms: joy1 high byte, joy1 low byte, joy2 high byte, joy2 low byte. It does this whether or not the BL616 asked for anything. Tang-Control's menu is navigated by literal bit tests on that word - joy1 & 0x10 is up, 0x20 is down, 0x100 or 0x1 is choose - and its selection marker is a '>' written to column 0 of the active row."
  consequence: "A controller-driven menu needs no core change and no encoder: read the frame and redraw the marker. This project currently parses response 0x03 only to keep the reader in sync and discards it, so the capability is available and unbuilt. osd menu <n> is the seam it would drive."
  sources:
    - "nestang, src/iosys/iosys_bl616.v: the SEND_IDLE branch selecting SEND_JOYPAD on change with JOY_UPDATE_INTERVAL, and resp_frame_len <= 5"
    - "Tang-Control, ui/menu_manager.cpp: the joy1 bit tests and the column-0 marker writes"
  verification: "Read from both sources. The frames demonstrably arrive: the transport's frame parser has to skip them to stay synchronised."

- record_id: PROT-007
  kind: PROTOCOL
  topic_id: PROT
  title: "Streaming a ROM into a core that is not listening fails silently"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "The ROM stream is one-way: the BL616 sends command 0x07 frames and the core consumes them, so with no core running - or with one running but its overlay still up - that second of UART traffic is discarded and nothing reports an error on either side. Nothing fails and nothing times out, and the visible result is a black screen that cannot be told from a video fault. The fpga probe between the two loads is what makes it detectable: it asks the core for its ID with command 0x01, proving something is answering before the ROM is committed. scripts/boot-cart.tdsh runs that check between its loads and exits non-zero when it fails."
  consequence: "The probe is not decoration but the only error detection in the cartridge path, so any reimplementation of the load sequence should keep a check between programming the core and streaming the ROM. Without it the failure surfaces later as an unexplained black screen."
  sources:
    - "This project's scripts/boot-cart.tdsh: the sequence and the check between the two loads"
    - "PROT-002: command 0x07 for ROM data and 0x01 for the core ID"
  verification: "Exercised on this board. scripts/boot-cart.tdsh has been run on its happy path and on its guard path (TDSH-001), and the black-screen-with-audio symptom was seen before the overlay clearing of PROT-005."

- record_id: NEST-001
  kind: EXTERNAL
  topic_id: NEST
  title: "nestang's FPGA-side USB host is low-speed only, with its protocol in a ROM"
  status: SOURCED
  verified_date: 2026-10-03
  statement: "The USB host in the FPGA fabric supports low-speed USB (1.5 Mbps) only. It expects D+, D-, VBUS and GND with two external 15K pull-downs, and a 12 MHz clock on usbclk. There is no full-speed or high-speed handling in the design. Its low-level signalling engine is not pure logic: usb_hid_host.v instantiates usb_hid_host_rom and fetches instructions from a 1072-byte program loaded by $readmemh."
  consequence: "No full-speed or high-speed device can be used on the board's USB-A ports, whatever firmware is written, and raising the host's speed class would mean rewriting that ROM program as well as re-checking the electrical layer. Instruments: see NEST-002 for what the host does with what it reads."
  sources:
    - "nestang, src/usb_hid_host.v: the module comment 'This should support keyboard, mouse and gamepad input out of the box, over low-speed USB (1.5Mbps). Just connect D+, D-, VBUS (5V) and GND, and two 15K resistors between D+ and GND, D- and GND. Then provide a 12Mhz clock through usbclk.'"
    - "nestang, src/usb_hid_host.v: 'usb_hid_host_rom ukprom(.clk(usbclk), .adr(pc), .data(inst))' and the S_OPCODE/branch/jmppc decode"
    - "nestang, src/pll/pll_12.v and its instantiation feeding usbclk"
  verification: "Read from the source. Consistent with the observed device type classification in the same file, which checks bInterfaceClass == 3 and bInterfaceSubClass == 1 for keyboard and mouse and calls anything else a gamepad."

- record_id: NEST-002
  kind: EXTERNAL
  topic_id: NEST
  title: "nestang wires only the gamepad output, so keyboard input is discarded"
  status: SOURCED
  verified_date: 2026-10-03
  statement: "usb_hid_host.v parses full keyboard reports, producing key_modifiers and key1 through key4 when a device identifies as a boot keyboard. In nestang_top.sv only .game_snes is connected; the keyboard outputs are left unconnected, and a search of the top level and iosys_bl616 for those signals finds nothing. The device type goes only to the LED."
  consequence: "A keyboard plugged into a USB-A port enumerates and is read, and every keystroke is then dropped. No keyboard can reach this firmware through those ports regardless of its speed. Adding the ability would mean a protocol extension and a rebuilt core, which this project has deliberately avoided."
  sources:
    - "nestang, src/nestang_top.sv: the usb_hid_host instantiation with only .usbclk, .usbrst_n, .usb_dm, .usb_dp, .game_snes, .typ and .conerr connected"
    - "nestang, src/usb_hid_host.v: the typ == 1 keyboard report layout with key_modifiers and key1..key4"
  verification: "Read from the source: a search for key1, key2, key_modifiers and 'keyboard' across nestang_top.sv and iosys_bl616.v returns nothing."

- record_id: TCTL-001
  kind: EXTERNAL
  topic_id: TCTL
  title: "Tang-Control draws and navigates its OSD from the BL616"
  status: SOURCED
  verified_date: 2026-10-03
  statement: "Tang-Control's OSD is entirely the BL616's work. overlay_cursor(x, y) and overlay_printf() are protocol commands 0x04 and 0x05. Navigation is a bit test on the joypad state the FPGA relays. Its file chooser, menu manager and on-screen console all draw this way, and the console's line editor prints an underscore as its caret and reads keys from key_buf[4], filled by a HID parser."
  consequence: "The OSD contract is fully reproducible from this project, which is what ports/bl616/tang_osd.c and tang_osd_term.c do. Tang-Control's menu and the menu this project draws independently converged on the same column-0 marker idiom."
  sources:
    - "Tang-Control, ui/menu_manager.cpp: overlay_cursor(0, options[active]) then overlay_printf('>')"
    - "Tang-Control, ui/console.cpp: the command console, the '_ ' caret, and key_input() over key_buf"
    - "Tang-Control, main.cpp: the receiver parsing response 0x03 into joy1_state and joy2_state"
  verification: "Read from the source, and the equivalents in this project were then built and confirmed on hardware by the user. Tang-Control itself was not run."

- record_id: TCTL-002
  kind: EXTERNAL
  topic_id: TCTL
  title: "Tang-Control treats the OTG connector's role as a build-time choice"
  status: SOURCED
  verified_date: 2026-10-03
  statement: "Tang-Control's CMake offers TANG_USB_CDC_CONSOLE, described as 'Use the Console 138K DEBUG/OTG port as a CDC console', defaulting OFF and permitted only when the board is console138k. With it off, main.cpp calls usbh_initialize(), fatfs_usbh_driver_register() and usb_gamepad_init(); with it on, usb_cdc_console_init() instead. The accompanying comment reads: 'The Console 138K retail board has onboard SD and FPGA-side controller ports, so dedicate the BL616 OTG connector to a PC-facing debug link.'"
  consequence: "The reference firmware does not attempt runtime role detection, and its own comment characterises this connector as a PC-facing debug link. That is consistent with USB-003's finding and is the strongest prior evidence that the connector is device-only by design."
  sources:
    - "Tang-Control, CMakeLists.txt: the TANG_USB_CDC_CONSOLE option, its console138k-only guard, and the conditional CONFIG_CHERRYUSB_DEVICE settings"
    - "Tang-Control, main.cpp: the #ifdef TANG_USB_CDC_CONSOLE branch choosing between usb_cdc_console_init() and usbh_initialize()"
  verification: "Read from the source. Not exercised: this project never ran Tang-Control's host build."

- record_id: TCTL-003
  kind: EXTERNAL
  topic_id: TCTL
  title: "Tang-Control normalises the core link's baud before JTAG"
  status: SOURCED
  verified_date: 2026-10-03
  statement: "Before programming a core, Tang-Control checks the FPGA UART's baud and returns both ends to 2 Mbps if an extended session had negotiated faster, using fpga_debug_set_baud then fpga_uart_set_baud. It also comments that all boards other than the Console pair use a 26 MHz crystal and scale the rate accordingly."
  consequence: "Its JTAG path assumes the core link is at 2 Mbps when it starts, which is the rate this project leaves it at. The 26 MHz scaling does not apply here: this board is a Console."
  sources:
    - "Tang-Control, fpga/programmer.cpp: the baud normalisation at the top of fpga_program()"
    - "Tang-Control, utils/init.cpp: the TANG_CONSOLE60K/TANG_CONSOLE138K branch selecting 2000000, and the else branch scaling by 40/26"
  verification: "Read from the source only. This project's tang_jtag_glue.h keeps the same constant behaviour as stubs."

- record_id: TDSH-001
  kind: EXTERNAL
  topic_id: TDSH
  title: "TinyDesk Shell v0.1.3, and the scripting language it provides"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "The shell is TinyDesk Shell v0.1.3 at commit 232a39fa3375f2c8eb2560cdc69440f7095f8a25, consumed here as the third_party/tinydesk-shell submodule. Its portable core is C11 and MIT licensed. The script language, uScript 1.1.1, provides variables, quoting, command substitution, arithmetic, if/elseif/else/endif, while/endwhile, for/endfor, functions with return statuses, pipes up to eight stages, and redirection. tdsh run passes no arguments to a script; a script inherits the caller's variables. It also ships POSIX and Windows host ports."
  consequence: "scripts/boot-cart.tdsh is written in this language and needed nothing beyond if, $?, and two variables. Its four shell commands are registered through tdsh_register_commands()."
  sources:
    - "third_party/tinydesk-shell/docs/SCRIPTING.md: the language reference, the run forms, and the limits table"
    - "third_party/tinydesk-shell/VERSION and .git (v0.1.3, 232a39f)"
  verification: "The shell runs on this board; boot-cart.tdsh has been run repeatedly, on its happy path and on its guard path."

- record_id: TDSH-002
  kind: EXTERNAL
  topic_id: TDSH
  title: "The shell's terminal output is a small, closed set of sequences"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "The line editor and builtins emit: carriage return, line feed, ESC [ 2 K to erase a line, ESC [ 2 J and ESC [ H to clear and home, ESC [ <n> C and ESC [ <n> D to move the cursor, and SGR colour sequences. The editor redraws a line by emitting CR, erase-line, then the prompt and buffer, then a cursor-left by the number of characters past the cursor. Nothing else is emitted."
  consequence: "A console mirror needs to understand only that set, which is what makes ports/bl616/tang_osd_term.c small. It treats CR as the start of the logical line rather than the row, because on a 32-column page a row-based CR would leave the tail of the previous render behind on every keystroke of a wrapped line."
  sources:
    - "third_party/tinydesk-shell/src/core/tdsh_terminal.c: redraw(), and the ESC [ C / ESC [ D writes in the arrow-key handling"
    - "third_party/tinydesk-shell/src/core/tdsh_builtin.c: the clear builtin's ESC [ H ESC [ 2 J"
  verification: "The mirror was built against this set and the user confirmed the result on the core's screen."

- record_id: TDESK-001
  kind: EXTERNAL
  topic_id: TDESK
  title: "TinyDesk's port surface is four functions, and it pins the same shell revision"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "TinyDesk is a terminal desktop: it draws overlapping text-mode windows with ANSI escape sequences, reads the keyboard and mouse back from the terminal, and has no display hardware. Its whole port surface is td_hal_t, four function pointers - read_byte returning the next byte or -1 without blocking, write returning the number of bytes accepted, millis, and sleep_ms - plus a context pointer. It consumes tinydesk-shell as a submodule at 232a39fa, the same revision this project uses, and its core is portable C11."
  consequence: "The port in ports/bl616/td_desktop_bl616.c is mostly a table of pointers over console calls this project already had. There is no second copy of the shell to keep in step."
  sources:
    - "third_party/tinydesk/include/tinydesk/td_hal.h: the td_hal_t definition and the comment 'This is the only thing a port has to provide.'"
    - "third_party/tinydesk/README.md: the terminal-desktop description and the four-functions claim"
    - "third_party/tinydesk/.gitmodules: submodule third_party/tdsh at 232a39fa3375f2c8eb2560cdc69440f7095f8a25"
  verification: "Built into this firmware and running: the desktop draws, opens windows, and its Terminal runs the shell."

- record_id: TDESK-002
  kind: EXTERNAL
  topic_id: TDESK
  title: "Screen memory is columns times rows times eight bytes, doubled"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "td_config.h sizes every static pool. Each screen buffer costs TD_MAX_COLS x TD_MAX_ROWS x 8 bytes and there are two of them. The PC defaults are 400 by 150; the ESP-IDF builds set smaller limits, with one ESP32-C6 variant using 256x96 and another 80x25 with TD_VT_SCROLLBACK 12 and TD_MAX_WIDGETS 96. The smallest usable desktop is 40 by 12."
  consequence: "100x30 costs 24,008 bytes per screen buffer against the BL616's 320 KB, measured in the link map as s_front and s_back. The geometry must divide evenly and the terminal's reported size is clamped to it."
  sources:
    - "third_party/tinydesk/include/tinydesk/td_config.h: the TD_MAX_COLS/TD_MAX_ROWS comment and the pool definitions"
    - "third_party/tinydesk/ports/esp32c6/components/tinydesk/CMakeLists.txt: the two TD_SCREEN_LIMITS settings"
  verification: "The linker reports s_front and s_back at 0x5dc8 = 24,008 bytes each, which is 100x30x8 exactly."

- record_id: TDESK-003
  kind: EXTERNAL
  topic_id: TDESK
  title: "TinyDesk's filesystem is written against stdio and deploys unmodified"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "TinyDesk's Files and Editor reach the disk through a td_fs_ops_t vtable supplied by ports/common/td_fs_stdio.c, which is implemented on top of stdio, dirent.h and sys/stat.h. The ESP-IDF port uses the same file through its VFS. Its root string is copied at construction, and all paths are formed below it."
  consequence: "This project already provides that API over FatFS as a newlib syscall layer, so td_fs_stdio.c compiles here unchanged - no filesystem port was written. This was the piece expected to be the bulk of the work and it turned out to be free."
  sources:
    - "third_party/tinydesk/ports/common/td_fs_stdio.c: the file comment 'td_fs_ops_t on top of the C library and <dirent.h>. Used by the desktop hosts and by the ESP-IDF port (its VFS provides the same calls for LittleFS).'"
    - "This project's ports/bl616/tdsh_fs_bl616.c: _open_r, _read_r, _write_r, _lseek_r, _stat_r, _fstat_r, _unlink_r, _mkdir_r, _rename_r, and opendir/readdir/closedir over FatFS"
  verification: "The user confirmed the file created from the shell with touch and echo appears in the desktop's Files window, and that editing and saving it in the Editor reaches the card."

- record_id: TDESK-004
  kind: EXTERNAL
  topic_id: TDESK
  title: "The Terminal app requires a td_term_backend_t and says so when it has none"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "The Terminal app draws 'No shell backend in this build.' and a second line naming td_terminal_set_backend() exactly when its static s_backend pointer is NULL. With a backend installed it starts it on first draw, forwards keystrokes through backend->write, polls backend->read into its terminal emulator each tick, resizes the emulator to the window, and offers scrollback."
  consequence: "That message is a precise indicator, not a fault: it means the port has not installed a backend. ports/bl616/td_desktop_bl616.c installs one from td_bridge_bl616.c."
  sources:
    - "third_party/tinydesk/apps/terminal.c: on_draw()'s 'if (!s_backend)' branch, and the start/read/write calls in the tick and key paths"
    - "third_party/tinydesk/apps/td_apps.h: 'void td_terminal_set_backend(const td_term_backend_t *backend);'"
  verification: "Seen on hardware before the bridge existed, and gone after it was installed."

- record_id: TDESK-005
  kind: EXTERNAL
  topic_id: TDESK
  title: "A Terminal backend is start/read/write/user, and the reference runs the shell in a task"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "td_term_backend_t has name, start(ctx, cols, rows), read(ctx, buf, cap), write(ctx, buf, len), user(ctx) and set_user(ctx, user), with resize optional. The ESP32-C6 implementation starts a shell in its own FreeRTOS task whose stdin and stdout are a funopen stream over two stream buffers: keys go in through write, output comes back through read, and a break check pulls Ctrl+C out of the input buffer for long-running commands."
  consequence: "ports/bl616/td_bridge_bl616.c follows this shape with plain rings rather than stream buffers, since this build has no stream-buffer configuration, and adds a shutdown path the reference does not need: the inner shell must be ended when the desktop exits, or it would read the console alongside the outer one."
  sources:
    - "third_party/tinydesk/ports/esp32c6/main/tdsh_bridge_esp.c: the whole file, including the stream buffer sizes and the shell task"
    - "third_party/tinydesk/ports/common/tdsh_bridge.h: the host backend's interface"
  verification: "Built and running: tdsh run /scripts/boot-cart.tdsh typed into the Terminal window boots a core and starts a cartridge, and the shell's logging appears in the vterm."

- record_id: TDESK-006
  kind: EXTERNAL
  topic_id: TDESK
  title: "The Editor saves with Ctrl+S, which a board port cannot guarantee arrives"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "The Editor's key handler saves on Ctrl+S, closes on Ctrl+W or Ctrl+Q or Esc, and offers Save|Discard|Cancel when closing with unsaved changes. On a host, the HAL puts the local terminal into raw mode and clears IXON before starting, with a comment noting that ISIG is cleared so Ctrl+C arrives as 0x03."
  consequence: "A board port cannot do that: there is no termios on the BL616, so software flow control on the user's terminal is untouched. With IXON on, Ctrl+S is swallowed as XOFF and stops the display, which reads exactly like a hang; Ctrl+Q restores it. The Editor's other save path through Ctrl+W avoids it. Ctrl+A is also screen's command prefix, so select-all does not arrive under screen."
  sources:
    - "third_party/tinydesk/apps/editor.c: the ctrl test and the 's' case, and the unsaved-changes message box"
    - "third_party/tinydesk/ports/posix/hal_posix.c: 'raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON)' and 'raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG); /* Ctrl+C arrives as 0x03 */'"
    - "third_party/tinydesk/src/input.c: the 0x01 to 0x1A mapping to a key with TD_MOD_CTRL"
  verification: "Observed on hardware: Ctrl+S appeared to freeze the desktop and saved nothing; Ctrl+W saved successfully. The IXON explanation is from the source, not from an instrumented measurement of the terminal."

- record_id: TDESK-007
  kind: EXTERNAL
  topic_id: TDESK
  title: "td_run() returns after td_quit(); System Monitor and Task Manager read sysinfo"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "The documented sequence is td_init(&hal), td_run(), td_shutdown(), and td_run() returns after td_quit(); the System menu's last item calls td_quit(). td_shutdown() restores the terminal. System Monitor reads free_heap, min_free_heap, total_heap, task_count and cpu_mhz; Task Manager reads tasks(out, max) filling name, state, priority, core, stack_free and cpu_tenths, with -1 documented as 'unknown'."
  consequence: "The desktop can be launched from a shell command and the prompt returns when the user quits it, which is how ports/bl616/td_desktop_bl616.c's desktop command is built. Leaving those sysinfo members NULL is what left both apps drawing empty frames; both are now supplied."
  sources:
    - "third_party/tinydesk/include/tinydesk/td.h: 'td_run(); // returns after td_quit()'"
    - "third_party/tinydesk/include/tinydesk/td_sysinfo.h: the heap, task and cpu_mhz members and the td_task_info_t definition"
    - "third_party/tinydesk/src/wm.c: the td_quit() call on the System menu's last item"
  verification: "Quitting the desktop returns 'desktop: exited' and the shell prompt, confirmed by the user. The heap and task members were added after both apps were reported blank; their effect was not separately re-confirmed."

- record_id: TDESK-008
  kind: EXTERNAL
  topic_id: TDESK
  title: "Four apps cannot be built here: they need sockets and mbedTLS"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "Network, MQTT, Modbus and Software Update, together with their shared proto/ layer, are built on proto/td_sock.c and proto/td_tls.c: the first includes netdb.h, sys/socket.h, arpa/inet.h and esp_timer.h; the second includes mbedtls/net_sockets.h and esp_crt_bundle.h. Nothing resembling that stack exists on the BL616."
  consequence: "This project compiles a subset of apps and supplies its own td_apps_register_all(), which registers Terminal, Files, Editor, System Monitor, Task Manager, Log Viewer, Settings, Counter and About, plus the clock and the session. apps/apps.c, mqtt.c, modbus.c, network.c and update.c are left out of the build."
  sources:
    - "third_party/tinydesk/proto/td_sock.c and td_tls.c: the includes"
    - "third_party/tinydesk/apps/apps.c: td_apps_register_all() and td_proto_service_start()"
  verification: "The build fails without this exclusion and succeeds with it; the excluded apps have no entry point that would work here in any case."

- record_id: TDESK-009
  kind: EXTERNAL
  topic_id: TDESK
  title: "The desktop's app set, as named in the start menu"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "Each app registers a td_app_t of name and icon: About, Counter, Editor, Files, Log Viewer, Settings, System Monitor, Task Manager and Terminal. Date & time is not a start-menu app: it is opened by clicking the taskbar clock, and the desktop itself - taskbar, start menu, window management and desktop icons - is core. Desktop icons come from the current user's Desktop folder, and a right-click on a .tdsh file offers Run, which opens the Terminal and runs the script."
  consequence: "Nine menu entries plus the clock. For the root user the desktop folder resolves below the filesystem root, which here is /sd, so the shell reaches it as /root/Desktop."
  sources:
    - "third_party/tinydesk/apps/*.c: the s_app definitions and td_app_register() calls"
    - "third_party/tinydesk/apps/session.c: compute_paths(), giving root the home '<root>/root'"
    - "third_party/tinydesk-shell/docs/SCRIPTING.md: 'TinyDesk desktop: right-click, Run, opens the Terminal window and runs tdsh run'"
  verification: "The user worked through the desktop and found the filesystem, the Editor, the Terminal and window management working. The desktop-icon path was not separately exercised."

- record_id: TOOL-001
  kind: TOOLCHAIN
  topic_id: TOOL
  title: "CONFIG_CHERRYUSB_HOST is required to enumerate; CONFIG_NEWLIB prevents it"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "With FreeRTOS enabled and only the device stack built, the CDC never enumerates on this board; enabling CONFIG_CHERRYUSB_HOST, which brings in the FreeRTOS OSAL, is what fixes it. Separately, the SDK's newlib port stops the device enumerating, with or without its FatFS file layer."
  consequence: "Both settings in proj.conf look wrong for a device-only firmware and are deliberate. The shell's stdio is therefore implemented over FatFS in ports/bl616 rather than taken from newlib."
  sources:
    - "Bouffalo SDK, components/usb/cherryusb/CMakeLists.txt: the host branch adding osal/usb_osal_freertos.c"
    - "This project's proj.conf: the comments recording the bisection that isolated both facts"
  verification: "Established by bisection on this board earlier in the project, and recorded in proj.conf; reconfirmed by the firmware running throughout this session."

- record_id: TOOL-002
  kind: TOOLCHAIN
  topic_id: TOOL
  title: "The Gowin programmer's accepted IDCODEs"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "The vendored programmer accepts five parts: GW5A-25 0x0001281b, GW5AT-60 0x0001481b, GWAST-138 0x0001081b, GW5AT-138 0x0001181b and GW2A-18 0x0000081b. Anything else is reported as an unknown board and programming stops."
  consequence: "This is a second, independent way to confirm which silicon is on the bench, alongside the bitstream's own device declaration. It also means a load into the wrong board fails loudly rather than silently misconfiguring."
  sources:
    - "Tang-Control, fpga/programmer.cpp: IDCODE_GW5A_25, IDCODE_GW5AT_60, IDCODE_GWAST_138, IDCODE_GW5AT_138, IDCODE_GW2A_18, and the test in fpga_program()"
  verification: "The board reports 0x0001081b, which is the GWAST-138 entry and the only one that matches."

- record_id: TOOL-003
  kind: TOOLCHAIN
  topic_id: TOOL
  title: "Build versions in use"
  status: SOURCED
  verified_date: 2026-10-03
  statement: "The firmware is built against the Bouffalo SDK reporting PROJECT_SDK_VERSION 2.0.0, with the T-Head RISC-V GCC 10.2.0 toolchain for riscv64-unknown-elf, driven by CMake and the SDK's project.build. The FPGA images are built elsewhere, with Gowin EDA, and are consumed here as finished bitstreams."
  consequence: "The SDK lives at ~/.cache/tangcore-dev/sdk and the toolchain at ~/.cache/tangcore-dev/toolchain by default, both overridable in the environment. Nothing in this project builds FPGA logic; the Gowin toolchain is not invoked here."
  sources:
    - "This project's build/generated/sdk_version.h: PROJECT_SDK_VERSION \"2.0.0\""
    - "This project's Makefile: BL_SDK_BASE and TOOLCHAIN_BIN defaults"
    - "Build output: the toolchain path resolving to riscv64-unknown-elf/10.2.0"
  verification: "Read from the build tree. The Gowin version is not established here; it belongs to the core images, not to this firmware."

- record_id: TOOL-004
  kind: TOOLCHAIN
  topic_id: TOOL
  title: "A Gowin bitstream names its device, so images are not interchangeable"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "A Gowin project binds the device at generation time. nestang's build.tcl takes a device name and calls set_device accordingly, and for console138k that is GW5AST-LV138PG484AC1/I0 with device version B, using the 138K-specific PLL sources. The corresponding synthesis project records Device id GW5AST-138B."
  consequence: "The 60K and 138K console images are not interchangeable, and the difference is visible in the artifact rather than only in a build script. Comparing a bitstream's size and its project declaration is a reliable way to tell which device it is for."
  sources:
    - "nestang, build.tcl: the console138k branch and its set_device line"
    - "nestang, impl/gwsynthesis/nestang_console138k_ds2.prj: the Device element"
  verification: "Read from the build files; the 138K artifact is 4,593,044 bytes against the 60K image's 2,321,194, and the loaded image runs on this board."

- record_id: TOOL-005
  kind: TOOLCHAIN
  topic_id: TOOL
  title: "The USB-enumeration bisection is kept in-tree, selected by environment variable"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "The configurations used to isolate TOOL-001 survive as three SDK config files - proj.min.conf, proj.nonewlib.conf and proj.rtos.conf - alongside the SDK's own device example vendored under ref/ as cdc_acm_template.c, ref_main.c and ref_rtos_main.c and built from inside this project. CMakeLists.txt picks them from the environment: TINYTANG_MIN, TINYTANG_RTOS and TINYTANG_NONEWLIB choose a config file, TINYTANG_REF selects the reference build, and TINYTANG_USB_ONLY, TINYTANG_NO_FS and TINYTANG_NO_SHELL add compile definitions that strip the firmware to a USB-only, no-filesystem or no-shell build. The README describes the three config files as the ones that produced TOOL-001's result."
  consequence: "The negative results in TOOL-001 have a reproducible witness rather than a recollection: setting an environment variable and rebuilding re-runs the bisection. Anyone removing these files or guards should know they are the evidence for why CONFIG_CHERRYUSB_HOST is set and CONFIG_NEWLIB is not."
  sources:
    - "This project's CMakeLists.txt: the TINYTANG_* guards and the config selection"
    - "This project's proj.min.conf, proj.nonewlib.conf, proj.rtos.conf and ref/"
  verification: "The files and the guards are present in the tree and were read directly. That these particular configs produced TOOL-001's result is this project's own account, not something re-run here."

- record_id: TOOL-006
  kind: TOOLCHAIN
  topic_id: TOOL
  title: "This project's host-side tools, and what they need"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "Three Python scripts in tools/ do the host-side work and all need pyserial: tinytang_flash.py writes a firmware image to the BL616 over the CDC with no BOOT button involved, tinytang_put.py puts a file onto the SD card, and tinytang_run.py runs a shell command on the board and streams its output. This is the live set; the retired Tang-Control helpers are recorded separately in TCTL-010. A build machine therefore needs Python with pyserial in addition to the SDK and the RISC-V toolchain of TOOL-003."
  consequence: "These are the only supported ways to reflash the board and to place files on the card. tinytang_flash.py needs the CDC port free and a power cycle afterwards (FLS-001), and a live screen session holds the port."
  sources:
    - "This project's tools/: tinytang_flash.py, tinytang_put.py, tinytang_run.py"
    - "This project's README.md: 'Python with pyserial for the tools in tools/'"
  verification: "All three were used against this board: the firmware was flashed, files were put on the card, and shell commands were run and their output captured."

- record_id: TOOL-007
  kind: TOOLCHAIN
  topic_id: TOOL
  title: "The vendored Gowin JTAG programmer is Apache-2.0, from nand2mario via Tang-Control"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "ports/bl616/tang_jtag_programmer.c is nand2mario's bit-banged GPIO JTAG programmer for Gowin GW5A and GW2A, (c) 2025.2, licensed Apache-2.0, itself based in part on openFPGALoader by Gwenhael Goavec-Merou. It was taken from Tang-Control's fpga/programmer.cpp and used unmodified apart from its include list. ports/bl616/tang_jtag_glue.h supplies what it expected from its own tree - the console, the card file, the GPIO device handle, and stubs for the FPGA UART bookkeeping - and documents that provenance."
  consequence: "This is working code inherited from the previous firmware rather than reimplemented, so a claim that only board knowledge was carried over is not quite right: the programmer is a translation unit. Its licence is Apache-2.0 and travels with the file, which is a different licence from the MIT TinyDesk shell submodules (TDSH-001, TDESK-001)."
  sources:
    - "This project's ports/bl616/tang_jtag_programmer.c: the Apache-2.0 header, the 2025.2 nand2mario copyright and the openFPGALoader attribution"
    - "This project's ports/bl616/tang_jtag_glue.h: the provenance comment"
  verification: "Read from the file header and the glue header in the tree. The programmer is exercised on every tangload, which reports ID=0001081b (DEV-001)."

- record_id: TOOL-008
  kind: TOOLCHAIN
  topic_id: TOOL
  title: "The SDK components this firmware actually links, and their licences"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "The Bouffalo SDK is Apache-2.0 for its own code but bundles third-party components under their own licences, and three of those are linked into this firmware. FreeRTOS Kernel V10.4.6, MIT, Copyright (C) 2021 Amazon.com, Inc. or its affiliates, selected by CONFIG_FREERTOS in proj.conf. CherryUSB, Apache-2.0, Copyright (C) 2006 Bertrik Sikken, (c) 2016 Intel Corporation and (c) 2022 sakumisu, which is the device CDC console and the FreeRTOS OSAL hosting it. And FatFs R0.15 w/patch3, Copyright (C) 2022 ChaN, whose condition obliges only a redistribution of source to retain its notice. Not linked into this firmware: the SDK's LVGL, TJpgDec, mbedTLS, littlefs and multimedia codecs."
  consequence: "This is the part PROV-002's list does not reach: naming the SDK is not naming what the SDK carries, and FreeRTOS's MIT text has to travel with copies and substantial portions, which is what this firmware's binary is. THIRD_PARTY.md now names all three. Anyone enabling a further SDK component - mbedTLS for the network apps, LVGL for graphics - inherits that component's licence at the moment they enable it."
  sources:
    - "Bouffalo SDK components/os/freertos/tasks.c: 'FreeRTOS Kernel V10.4.6 / Copyright (C) 2021 Amazon.com, Inc. or its affiliates. / SPDX-License-Identifier: MIT'"
    - "Bouffalo SDK components/usb/cherryusb/core/usbd_core.c: the Apache-2.0 SPDX line and the three copyright holders"
    - "Bouffalo SDK components/fs/fatfs/ff.c: 'FatFs - Generic FAT Filesystem Module R0.15 w/patch3' and ChaN's condition"
    - "This project's proj.conf: set(CONFIG_FREERTOS 1)"
  verification: "Read from the SDK's own source headers at ~/.cache/tangcore-dev/sdk, and the FreeRTOS selection confirmed in proj.conf. The not-linked list was checked against this project's configuration and sources rather than the SDK's inventory."

- record_id: TOOL-009
  kind: TOOLCHAIN
  topic_id: TOOL
  title: "The console's input is exclusive, and a raw upload's bytes are keystrokes"
  status: VERIFIED
  verified_date: 2026-10-04
  statement: "tangput takes raw bytes from the same CDC stream the desktop reads its typed input from, and on that wire a file and a burst of typing are the same thing: nothing distinguishes them, because the console is one byte stream with no framing of its own. The desktop, when it is running, is the reader holding it. So a transfer is safe only while the console is at a shell prompt, and tangput itself cannot tell the difference - it will accept whatever arrives and write it."
  consequence: "A file sent while the desktop is up is delivered to whichever window has focus instead of to tangput. Nothing reports an error and tangput never sees a short write, so the failure is silent until the card is looked at. Observed on 2026-10-04: a 131088-byte ROM sent this way produced a root directory of new.txt and New folder entries with binary data where filenames belong, and damaged the allocation table badly enough that /cores, /scripts and /roms became unreachable. The board then booted a stock core (fpga reports core 0; the patched image reports 1) and boot.tdsh could not find its core. The recovery is fsck.vfat from a PC, before any reformat. tools/tinytang_put.py now refuses to send unless it sees a shell prompt, checking for the desktop's own markers ([Start], Terminal - tdsh, or the alternate-screen sequence) first; the check belongs in the tool because the alternative is remembering, and this was done twice."
  sources:
    - "This project's tools/tinytang_put.py: require_shell() and DESKTOP_MARKERS"
    - "This project's ports/bl616/tang_osd_desk.c: the desktop layer reads the console's input and forwards it to the desktop"
    - "Observed on this board, 2026-10-04"
  verification: "Observed directly: the damaged directory listing was read back from the board, the card stopped accepting writes with 'cannot create', and the same send succeeded once the desktop was exited. The guard's refusing path has not itself been exercised."

- record_id: TOOL-010
  kind: TOOLCHAIN
  topic_id: TOOL
  title: "A patch is no longer recognised once later work edits the lines it added"
  status: VERIFIED
  verified_date: 2026-10-04
  statement: "scripts/apply-nestang-patches.sh decides whether a carried patch is already applied by asking git to reverse-apply it. That works while the patch's surroundings move, and it stops working the moment a later cycle edits the lines the patch itself added, because the reverse check requires those lines to be present verbatim and the forward check requires them to be absent. Neither is true, so the patch can be neither applied nor recognised."
  consequence: "This happened on the pointer-mode cycle, which rewrote an assign that patch 0004 had introduced, and it stopped the core build outright. The script now falls back to presuming a series is applied when the tree has local changes, while still failing hard on a clean tree where a non-applying patch is a genuine fault. The guarantee that the series is correct is therefore not this check but the reconstruction test: apply every patch in order to a fresh clone and compare the result byte-for-byte with the tree, which is what is run before each patch is committed."
  sources:
    - "This project's scripts/apply-nestang-patches.sh: the strict pass, the -C1 fallback, and the local-changes presumption"
    - "third_party/patches/0004-keyboard-link.patch, whose added lines 0006 rewrote"
  verification: "Bitten once and fixed: the build failed with 'does not apply cleanly' on patch 0004 while the same series applied cleanly to a fresh clone. The reconstruction test then passed for all six patches with the tree reproduced byte-for-byte."

- record_id: TCTL-004
  kind: EXTERNAL
  topic_id: TCTL
  title: "Tang-Control is a fork of nand2mario's firmware, and its USB identity is 0xFFFF:0x6160"
  status: SOURCED
  verified_date: 2026-10-03
  statement: "Tang-Control is a fork of nand2mario's firmware-bl616, the stock TangCore firmware for this board. Its CDC command console and the transport protocols behind it - the extended debug channel, the stream protocol and no-BOOT firmware update - are additions on its feature/usb-cdc-file-transfer branch, not stock behaviour. It identifies itself to the host as USB vendor 0xFFFF, product 0x6160."
  consequence: "A host client keyed to 0xFFFF:0x6160 will not find TinyTang, which presents 0xFFFF:0x5454. Command names this project inherited, such as tangput and tangflash, come from that console and deliberately match it. The stock firmware it forks has neither the console nor the transports, so its capabilities are not a floor TinyTang was standing on."
  sources:
    - "Tang-Control, README.md: 'This repo is a fork of nand2mario's firmware-bl616' and the list of what the fork adds over stock"
    - "Tang-Control, scripts/tangctl.py: USB_VID = 0xFFFF, USB_PID = 0x6160"
  verification: "Read from the README and the client. Tang-Control was never run by this project; the record is a statement about that firmware, not about ours."

- record_id: TCTL-005
  kind: EXTERNAL
  topic_id: TCTL
  title: "The three wiring modes, and which connector carries the CDC"
  status: SOURCED
  verified_date: 2026-10-03
  statement: "Tang-Control names three wiring arrangements. One-wire user mode runs TangCore normally. One-wire diag mode gives JTAG to the FPGA and the UART straight to the FPGA, for core load and execute, reached through the FT2232 debug cable. Two-wire debug mode connects the power cable and the CDC cable together, giving CDC file management, JTAG programming through a Pico 2, and no-BOOT BL616 flashing. The CDC arrives on the board's bottom-left USB-C port. Its client prints this explanation when it finds an FT2232 instead of a CDC device."
  consequence: "This is the vocabulary behind every '1-wire' and '2-wire' remark in this project's history, including the several sessions where the console was missing because the host was on the wrong path. When ffff:5454 is absent and 0403:6010 is present, the host is on the debug path, which is the one-wire arrangement."
  sources:
    - "Tang-Control, README.md: the three mode diagrams and their captions"
    - "Tang-Control, scripts/tangctl.py: the find_port() hint naming the FT2232, one-wire mode, and the bottom-left USB-C port for the CDC"
  verification: "Read from the README and the client. The FT2232 half is independently confirmed by BRD-004, which observed that device on this workstation while the CDC was absent."

- record_id: TCTL-006
  kind: EXTERNAL
  topic_id: TCTL
  title: "The TangCore core inventory and where an image is looked for"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "TangCore's core table is: id 1 NES, directory nes, image nestang.bin; id 2 SNES, snes, snestang.bin; id 3 Game Boy Advance, gba, gbatang.bin; id 4 MegaDrive or Genesis, genesis, mdtang.bin; id 5 Sega Master System, sms, smstang.bin; id 6 IBM PC/XT, pc, pctang.bin; and id 0x50 Phosphor, music, phosphortang.bin. An image is looked for first at cores/<board>/<name> and then at cores/<name>, taking the first that exists and is non-empty."
  consequence: "This is the naming every core image on the card follows, and why /cores/console138k/ is the directory that matters here. It also explains the core IDs the UART reports: nestang answers 1."
  sources:
    - "Tang-Control, core/cores.cpp: init_core_list() and find_core_for_board()"
  verification: "Confirmed against this board's own card: ls /cores/console138k lists nestang.bin, snestang.bin, gbatang.bin, mdtang.bin, smstang.bin, pctang.bin and phosphortang.bin among others, matching the table's names."

- record_id: TCTL-007
  kind: EXTERNAL
  topic_id: TCTL
  title: "The retired host client's command surface"
  status: SOURCED
  verified_date: 2026-10-03
  statement: "tangctl.py offers ping, status, rxstats with an optional reset, caps, peek, poke, baud, stream, bench, put, get, ls, rm, mkdir and firmware, run over the CDC as line commands with raw byte phases for transfers. caps, peek, poke and baud need a Phosphor core. There is no rename: rename is get, put under the new name, then rm. The firmware command patches and installs a BL616 image and the client waits for the device to disappear and return."
  consequence: "This is the capability being retired, and the list to consult when deciding what TinyTang should grow next. Two of these already have equivalents here - put and the firmware install, as tangput and tangflash - and the rest do not: TinyTang has a shell rather than a command protocol, so a client of this shape cannot talk to it unchanged."
  sources:
    - "Tang-Control, README.md: the tangctl.py command table and the note that rename does not exist"
    - "Tang-Control, scripts/tangctl.py: the command implementations"
  verification: "Read from the README and the client. None of these commands were exercised on hardware by this project."

- record_id: TCTL-008
  kind: EXTERNAL
  topic_id: TCTL
  title: "The no-BOOT firmware update procedure, in full"
  status: SOURCED
  verified_date: 2026-10-03
  statement: "A build intended for installation has its boot header rewritten before transfer: the body length is written little-endian at 0x84 as the file size less 0x1000, and a CRC-32 of the first 252 bytes is written little-endian at 0xFC. The patched image is then sent over CDC with the firmware command, and the board is power-cycled afterwards, because the BL616 resets into its vendor loader and TangCore only returns on power-on. The client verifies the transfer by reading back the application's SHA-256 and comparing it with the local build, and it requires the transfer to sustain at least a megabyte per second while doing so."
  consequence: "This is the same header layout this project validates in FLS-001, now with the procedure and the verification step that surround it. The power-cycle requirement matches this project's experience exactly. Note that tangflash here does not patch a header: it expects an image already carrying a valid one."
  sources:
    - "Tang-Control, README.md: the inline header-patching script and the two-case flashing section"
    - "Tang-Control, scripts/tangctl.py: FW_APP_MAX_SIZE, FW_BOOT_HEADER_SIZE, FW_HEADER_REGION, check_boot_image(), read_status(), CRC_VERIFY_MIN_BYTES_PER_SECOND"
  verification: "Read from the README and the client. The header layout it depends on is confirmed independently by FLS-001."

- record_id: TCTL-009
  kind: EXTERNAL
  topic_id: TCTL
  title: "The FPGA UART receive path's design, and the counters it kept"
  status: SOURCED
  verified_date: 2026-10-03
  statement: "Tang-Control reworked the FPGA UART receive path to interrupt-driven RX with a TX mutex, reporting that this fixed gamepad and OSD stutter. Its receive task maintains counters for bytes drained, complete joypad frames, hardware FIFO overflows, the most bytes found waiting when the task woke, bytes skipped while searching for 0xAA, frames dropped for an unrecognised type, and the longest interval between polls. Its client exposes them through an rxstats command and can reset them."
  consequence: "This project reached the same conclusion independently - tang_fpga_uart.c drains the RX ring from an interrupt because the 32-byte FIFO cannot hold a burst at 2 Mbaud - and the counter set is worth adopting: resync_bytes and fifo_overflows are the two that would have diagnosed the framing and rate problems faster than the logs did."
  sources:
    - "Tang-Control, utils/utils.h: struct fpga_rx_stats and fpga_rx_get_stats/fpga_rx_reset_stats"
    - "Tang-Control, README.md: 'FPGA UART RX rework - interrupt-driven RX + a TX mutex (fixes gamepad/OSD stutter)'"
  verification: "Read from the header and the README. This project's own ISR-drained ring is confirmed working by every UART operation it performs; the counters are not implemented here."

- record_id: TCTL-010
  kind: EXTERNAL
  topic_id: TCTL
  title: "The helper scripts that are being retired"
  status: SOURCED
  verified_date: 2026-10-03
  statement: "Alongside the client, Tang-Control carries liveuart.py and liveuart_draw.py to decode and visualise BL616 to FPGA UART traffic, print_uart.py for a raw dump, jtag.py with tdi_compare.py and crc16.sh as JTAG programming verification helpers, and fs.py to convert a Gowin .fs bitstream to the .bin this board loads."
  consequence: "liveuart is the most directly useful of these: it is a decoder for the exact protocol PROT-001 describes, and its visualiser would make the joypad and OSD traffic visible in a way neither project's text logs do. fs.py matters because this board consumes .bin, not .fs."
  sources:
    - "Tang-Control, README.md: the debug scripts section"
    - "Tang-Control, scripts/: the file listing"
  verification: "Read from the README and the directory listing. None of these scripts was run by this project."

- record_id: EXTCTL-001
  kind: EXTERNAL
  topic_id: EXTCTL
  title: "The versioned 0x10 register channel: layout, opcodes and statuses"
  status: SOURCED
  verified_date: 2026-10-03
  statement: "Legacy frame type 0x10 carries a versioned request and response channel. Requests are 15 bytes including the command byte: version 1, opcode, a 16-bit sequence, a 32-bit address, a 32-bit data field, and a CRC-16. Responses are 16 bytes including the command byte: version, the request's opcode with bit 7 set, a status byte, the sequence, the address copied from the request, a 32-bit data field, and a CRC-16. Opcodes are 0x00 capability query, 0x01 32-bit read, 0x02 32-bit write and 0x03 negotiated baud change; block writes use opcode 0x04 over frame type 0x12. Statuses are 0 success, 1 unsupported version, 2 unsupported opcode and 3 bad CRC. Capability bits are read32, write32, streaming, negotiated baud and validated block writes. Every field is big-endian, and the CRC is CRC-16/CCITT-FALSE - polynomial 0x1021, initial value 0xFFFF, no reflection, no final XOR - taken over the command byte and every payload byte before the CRC."
  consequence: "No part of this exists in the stock firmware or in nand2mario's cores; see EXTCTL-004. It is recorded here because it is the register-access contract of the Tang-Control family, and because an implementation of it is the only thing that would give TinyTang peek and poke."
  sources:
    - "Tang-Control, docs/extended-control-protocol.md: the version 1 request and response tables and the capability list"
    - "Tang-Control, utils/fpga_debug.h: FPGA_EXT_VERSION, FPGA_EXT_COMMAND, the opcode and capability enumerations, and the fpga_debug_result structure"
    - "Tang-Control, utils/fpga_ext_frame.h: fpga_ext_crc16_byte() and fpga_ext_packet_crc()"
  verification: "Read from the protocol document and the two headers. Not exercised: this project has no Phosphor core loaded and no implementation of the channel."

- record_id: EXTCTL-002
  kind: EXTERNAL
  topic_id: EXTCTL
  title: "Frame type 0x12: a validated multi-word write"
  status: SOURCED
  verified_date: 2026-10-03
  statement: "Frame type 0x12 writes between 1 and 64 consecutive 32-bit registers in one transaction. Its frame length is 12 plus 4 times the count, including the command byte, and its payload is version 1, opcode 0x04, a 16-bit sequence, a word-aligned first address, a count byte, the register values in address order, and a CRC-16 over the command byte and every preceding payload byte. The FPGA applies none of the words unless the CRC, version, opcode, count, frame length and alignment are all valid, and then writes them in address order. The reply is an ordinary 0x10 response whose opcode field is 0x84 and whose data field carries the word count; status 2 reports an invalid opcode, count, length or alignment."
  consequence: "This is the mechanism for updating a block of registers atomically, which is what a display bank commit needs. The absolute rule that nothing is applied unless everything validates is the part worth carrying forward: a partially applied register block would be a rendering fault with no error to point at."
  sources:
    - "Tang-Control, docs/extended-control-protocol.md: the block-write section"
    - "Tang-Control, utils/fpga_debug.h: FPGA_BLOCK_COMMAND 0x12 and FPGA_EXT_BLOCK_MAX_WORDS 64"
    - "Tang-Control, utils/fpga_ext_frame.h: FPGA_EXT_BLOCK_HEADER_LENGTH and fpga_ext_block_payload()"
  verification: "Read from the document and the headers. Not exercised here."

- record_id: EXTCTL-003
  kind: EXTERNAL
  topic_id: EXTCTL
  title: "Frame type 0x11: a stop-and-credit byte stream"
  status: SOURCED
  verified_date: 2026-10-03
  statement: "Frame type 0x11 carries a byte stream with explicit flow control. A request payload is version, flags, a stream ID, a 32-bit byte offset, a 16-bit data length, the data, and a CRC-16. Flags are start 0x01, data 0x02, end 0x04 and cancel 0x08; data frames carry at most 1024 bytes. After consuming each frame the FPGA replies with a status, the echoed flags and stream ID, the next expected byte offset, a receive credit value and a CRC-16. The sender does not transmit the next frame until that acknowledgement arrives. Start resets the expected offset to zero, and end and cancel carry no data."
  consequence: "This is how a host feeds a core that cannot keep up, without extra wiring: the core's own acknowledgement provides back-pressure. It is the mechanism a file-streaming command needs, and note that the acknowledgement carries the next expected offset, so a lost or reordered frame is detectable rather than silently absorbed."
  sources:
    - "Tang-Control, docs/extended-control-protocol.md: the stream frames section"
    - "Tang-Control, README.md: the stream and bench commands, and utils/fpga_stream.cpp and fpga_file_stream.cpp as their implementation"
  verification: "Read from the document and the README. Not exercised here."

- record_id: EXTCTL-004
  kind: EXTERNAL
  topic_id: EXTCTL
  title: "Only the Phosphor core implements the extended protocol"
  status: SOURCED
  verified_date: 2026-10-03
  statement: "Tang-Control states that caps, peek, poke and baud require a Phosphor core loaded, because the extended 0x10 protocol is implemented only by that core. Its README lists this among its gotchas. For comparison, this project's nestang tree handles frame types 0x01 through 0x0d and sends every other type to a default branch that consumes the payload and returns without action."
  consequence: "The register-access channel is not a property of the board's UART; it is a property of the FPGA design. TinyTang loads nand2mario's cores, which do not implement it, so peek and poke are unavailable here regardless of BL616 firmware - and would become available by loading a Tang-Control-family core rather than by writing anything in TinyTang."
  sources:
    - "Tang-Control, README.md: gotcha 2, 'caps/peek/poke/baud need a Phosphor core loaded'"
    - "nestang, src/iosys/iosys_bl616.v: the RECV_PARAM case covering 3 through 0x0c and the default branch that consumes unknown commands"
  verification: "The nestang half is read directly in this project's tree. The Phosphor half is Tang-Control's own statement and was not reproduced here."

- record_id: EXTCTL-005
  kind: EXTERNAL
  topic_id: EXTCTL
  title: "A transaction must hold the shared link across its response"
  status: SOURCED
  verified_date: 2026-10-03
  statement: "Tang-Control's debug header states the rule directly: serialise complete request and response transactions across every protocol that shares the FPGA UART, and hold the lock until the matching response arrives. It notes that protecting only the transmit bytes permits another packet to overtake the outstanding response. Its implementation drains any stale response before sending, assigns a sequence number from 1 upward and never 0, and matches version, opcode, sequence and address before accepting a reply."
  consequence: "This project's transport releases its lock between sending and waiting, which is safe today only because a single command waits for a response while every other sender is fire-and-forget, and because the response parser discards non-matching frames. That is a property of the current callers rather than of the transport, so it is written down here before someone adds a second responder. The sequence and match-before-accept rules are also worth adopting if the register channel is ever implemented."
  sources:
    - "Tang-Control, utils/fpga_debug.h: the comment above fpga_link_acquire()"
    - "Tang-Control, utils/fpga_debug.cpp: transaction_locked(), next_sequence, and the drain of response_ready before each send"
  verification: "Read from the header and the implementation. This project's own lock discipline is as described, which is why the rule is recorded rather than assumed satisfied."

- record_id: PHOS-001
  kind: EXTERNAL
  topic_id: PHOS
  title: "The Phosphor audio loader: layout, codecs and scope"
  status: SOURCED
  verified_date: 2026-10-03
  statement: "Tang-Control provides the filesystem and playlist layer for the Phosphor core. The core image goes at cores/console138k/phosphortang.bin and audio and playlists under music/. Phosphor appears in the main menu with the submenu header 'Phosphor - Audio Player'. Single-file playback decodes on the AE350 using Rockbox's codecs, taking the format from the file's contents rather than its name and covering MP3, WAV, FLAC, MP2, Ogg Vorbis, Opus, AAC, ALAC, WavPack, WMA, AC-3 and TTA. Playlists remain limited to WAV and FLAC."
  consequence: "This is the contract a music player on this board would follow, and it is the one place where the AE350 soft core does the work rather than the fabric. It also explains what music/ and phosphortang.bin are doing on the card this project runs from."
  sources:
    - "Tang-Control, docs/phosphor-loader.md: the SD-card layout and codec sections"
    - "Tang-Control, core/tangpsx.cpp and core/phosphor.cpp: the loader and menu implementations"
  verification: "Read from the loader document. Not exercised here; the card does carry a music/ directory and phosphortang.bin, which is consistent but not a test."

- record_id: PHOS-002
  kind: EXTERNAL
  topic_id: PHOS
  title: "The playlist profile: syntax, normalisation and hard limits"
  status: SOURCED
  verified_date: 2026-10-03
  statement: "Both .m3u and UTF-8 .m3u8 are accepted in the VLC extended form - an optional #EXTM3U line, #EXTINF with a duration and title, then the track path. Entries may mix WAV and FLAC and extension matching is case-insensitive. Track paths resolve relative to the playlist's own directory; forward and backward slashes, dot components and bounded parent components are normalised without allowing a path to escape the filesystem root; repeated paths stay repeated. Blank lines, comments, LF or CRLF endings and an optional UTF-8 byte-order mark are accepted. A playlist holds at most 255 tracks, a source line is limited to 512 bytes and a resolved path to 255 bytes. URLs, HLS playlists, nested playlists, missing files and unsupported formats are rejected before playback begins. #EXTINF supplies display metadata only: advancement follows the player's reported state, never the tagged duration."
  consequence: "A bounded, pre-validated playlist is what lets the loader start playing without discovering a problem mid-track. The rule that #EXTINF never drives advancement is the one most likely to be got wrong by a reimplementation."
  sources:
    - "Tang-Control, docs/phosphor-loader.md: the VLC M3U compatibility profile section"
    - "Tang-Control, core/m3u_playlist.cpp and tests/m3u_playlist_test.cpp"
  verification: "Read from the document and the implementation's test coverage. Not exercised here."

- record_id: PHOS-003
  kind: EXTERNAL
  topic_id: PHOS
  title: "Metadata precedence, and how display text is reduced to ASCII"
  status: SOURCED
  verified_date: 2026-10-03
  statement: "A track's displayed album, artist and title prefer FLAC comments - ALBUM, then ALBUMARTIST with ARTIST as its fallback, then ARTIST, then TITLE - falling back to the playlist name and #EXTINF text. WAV files use the RIFF LIST/INFO fields IPRD, IART and INAM when present. The panel shows exactly those three values. Display text is decoded as UTF-8 and reduced to the core's bounded ASCII font: typographic quotes, apostrophes, dashes, non-breaking spaces and ellipses are normalised to readable ASCII, and any other unsupported code point becomes a single question mark."
  consequence: "The one-to-one substitution rule matters: an unsupported code point must not change the character count, because the FPGA's text layout is positional. This is the same problem this project faces in its own text page, where the font is a fixed 128-entry bitmap table."
  sources:
    - "Tang-Control, docs/phosphor-loader.md: the metadata and artwork paragraph"
    - "Tang-Control, core/phosphor_metadata.cpp and tests/phosphor_metadata_test.cpp"
  verification: "Read from the document and the test coverage. Not exercised here."

- record_id: PHOS-004
  kind: EXTERNAL
  topic_id: PHOS
  title: "How a FLAC track is sent, and how tracks join without a gap"
  status: SOURCED
  verified_date: 2026-10-03
  statement: "A native FLAC track is transmitted as the fLaC marker, then STREAMINFO marked as the last metadata block, then the unchanged audio frames. The core skips every other metadata block, so large PICTURE or PADDING blocks do not delay a track's first frame over the UART, while the display side still reads them from the SD file. Each playlist entry is its own stream session. When the core advertises gapless append in capability bit 7, the loader starts the next entry as soon as the player reports its draining state - state 7 - rather than waiting for the FIFO to empty, and the core plays the new session's first sample on the sample period after the previous track's last, so same-rate tracks are sample-contiguous. The last entry still waits for completion, and older cores fall back to a completion handover."
  consequence: "Two separable ideas worth carrying forward: reordering a container's metadata so the payload can start sooner, which applies to any streaming format, and using a capability bit to decide between a fast handover and a safe one, which is how a new protocol stays compatible with an older core."
  sources:
    - "Tang-Control, docs/phosphor-loader.md: the FLAC and gapless paragraphs"
    - "Tang-Control, README.md: capability bit 7 as gapless append in the extended protocol's documented set"
  verification: "Read from the document. Not exercised here."

- record_id: PHOS-005
  kind: EXTERNAL
  topic_id: PHOS
  title: "Cover art: baseline JPEG, 92x92 RGB332, committed atomically"
  status: SOURCED
  verified_date: 2026-10-03
  statement: "Album art is taken from a FLAC front-cover PICTURE block when present and otherwise from a placeholder. Baseline JPEG covers are centre-fitted to 92x92 pixels in RGB332 on the BL616 and uploaded to an inactive FPGA bank before a single atomic artwork commit. Metadata and artwork work runs independently of the audio stream. The relevant registers named in the document are the audible-stream register 0xa4, and playback is driven by the FPGA's own player state."
  consequence: "This is why Tang-Control's build pulls in the SDK's TJpgDec sources and sets LV_USE_SJPG: the decoder is the only part of LVGL it uses. A reimplementation would need a baseline JPEG decoder and would still have to respect the inactive-bank-then-commit rule."
  sources:
    - "Tang-Control, docs/phosphor-loader.md: the artwork paragraph"
    - "Tang-Control, CMakeLists.txt: TJPGD_DIR into the SDK's lvgl/extra/libs/sjpg, and the LV_USE_SJPG definition"
  verification: "Read from the document and the build file. Not exercised here."

- record_id: PMOD-001
  kind: BOARD
  topic_id: PMOD
  title: "The dock's two PMOD sockets, and Sipeed's interleaved pin numbering"
  status: SOURCED
  verified_date: 2026-10-03
  statement: "The dock carries two PMOD sockets. PMOD1, beside the HDMI port, exposes PMOD1_IO0-IO7 on FPGA balls W19 W20 F19 F20 E22 D22 E21 D21, and PMOD0 exposes PMOD0_IO0-IO7 on V18 V19 G21 G22 F18 E18 C22 B22, all LVCMOS33. Sipeed's IO numbering interleaves the socket rows: IO0, IO2, IO4 and IO6 are Digilent module pins 1-4 and IO1, IO3, IO5 and IO7 are pins 7-10, so a connector index is Sipeed's IO number and not the linear pin order. Turning a module over swaps pins 1-4 with pins 7-10 while GND stays on pins 5/11 and VCC on 6/12."
  consequence: "This is the board half of the PMOD story, and it is kept in Tang-Phosphor's own reference, where the constraint file (src/boards/console138k_pmod.cst) and the personalities (src/pmod/) live; this project cites it rather than copying it, so the dock does not acquire a second, driftable source. It matters here only because the firmware chooses which core image to load, and a personality built into that image has to match what the user has seated."
  sources:
    - "Tang-Phosphor, .ai/core-reference.md, the 'Dock PMOD Sockets' record"
    - "Tang-Phosphor, src/pmod/pmod_slot.sv: the socket and orientation comment"
  verification: "Read from Tang-Phosphor's reference and pmod_slot.sv. Not measured on this bench; the underlying dock facts were confirmed during Tang-Phosphor's own OLED bring-up, which this project has not repeated."

- record_id: PMOD-002
  kind: EXTERNAL
  topic_id: PMOD
  title: "The tang.ini socket contract, and which project owns its parser"
  status: SOURCED
  verified_date: 2026-10-03
  statement: "PMOD modules carry no identification pins, so physical seating cannot be detected and the gateware can only be told what is attached. Tang-Phosphor therefore makes /tang.ini at the SD-card root the contract between what is seated and what the core drives: flat keys under one [tang] section, pmod0 = <module> and pmod0_flip = yes or no, and the same for pmod1. A missing file, or a socket with no entry, leaves that socket released, so the absent file is the safe state. Unknown module names, a vga_j1 without its vga_j2 partner, and flip for a module that is not flip-safe are refused rather than guessed. Tang-Phosphor's reference assigns the parser to the firmware that owns the SD card and the transport, naming Tang-Control there, while the registry of supported personalities lives beside the RTL that implements it."
  consequence: "That assignment lands here. Tang-Control is being retired into TinyTang, so the /tang.ini parser is inherited work in this project rather than in Tang-Phosphor, and a record elsewhere that still points at Tang-Control for it now points here. Nothing in this firmware reads /tang.ini today, which is safe only because no personality is selected unless the user writes one down."
  sources:
    - "Tang-Phosphor, .ai/core-reference.md, the 'PMOD configuration file (tang.ini)' record"
  verification: "Read from Tang-Phosphor's reference. The contract is documented and not implemented here: this tree contains no /tang.ini parsing, and none was exercised."

- record_id: PMOD-003
  kind: EXTERNAL
  topic_id: PMOD
  title: "Register 0x10 is the socket control register the tang.ini parser would write"
  status: SOURCED
  verified_date: 2026-10-03
  statement: "In Tang-Phosphor the PMOD socket selection is register 0x10: bit 0 holds the renderer, bits 4-7 are the PMOD0 personality, bits 8-11 the PMOD1 personality, bit 12 marks PMOD0 seated upside down and bit 13 PMOD1. Personality numbering is 0 none, 1 oledrgb, 2 vga J1 and 3 vga J2. Register 0x14 is scratch. Power-on defaults select the panel on PMOD0 and nothing on PMOD1, and the host is the only party that validates a declaration, because it is the only party that knows what the user wrote."
  consequence: "This is the seam the tang.ini contract would write through, and so the register a TinyTang-side parser would target. Note the name collision: extended-protocol legacy frame type 0x10 (EXTCTL-001) is a transport opcode and not this register, and the two must not be conflated."
  sources:
    - "Tang-Phosphor, .ai/core-reference.md, the 0x10 socket control register record"
  verification: "Read from Tang-Phosphor's reference. Not exercised here, and no register write of this kind exists in this tree."

- record_id: TCTL-011
  kind: EXTERNAL
  topic_id: TCTL
  title: "Tang-Control's standing: the architecture of record for Phosphor and later cores"
  status: SOURCED
  verified_date: 2026-10-03
  statement: "Stated by the user on 2026-10-03: Tang-Control is the architecture of record for the Phosphor core and for the cores that follow it, and it is hardware-verified as a whole because this project was built starting from it. The lineage is visible in this tree, where the board layer, the JTAG path, the SD gate, the UART1 contract and the flash layout all descend from Tang-Control."
  consequence: "This is why the TCTL records are worth carrying past retirement, and why a board-layer question should be taken to Tang-Control rather than to nand2mario's stock firmware. It does not make any individual claim verified: a record's own status still reports whether that specific fact was confirmed on this hardware or only read from a source."
  sources:
    - "Project statement recorded with the user, 2026-10-03"
  verification: "The lineage is visible in this tree and the inherited board layer has been exercised here. Tang-Control's hardware verification as a whole is the user's assertion and is not something this project's own runs can establish."

- record_id: PROV-001
  kind: EXTERNAL
  topic_id: PROV
  title: "The family lineage, as the user states it"
  status: SOURCED
  verified_date: 2026-10-03
  statement: "Stated by the user on 2026-10-03 as the official lineage. nand2mario's TangCore project is the origin: Tang-Phosphor and Tang-PSX were started from it. Tang-Control is a fork of that same repo, made to add peek and poke and the one-wire and two-wire debug arrangements. On top of that the family uses a DDR3 IP block, TinyDesk, CERN's colibri library as a reference, and what the user first recalled as nand2mario's memory module."
  consequence: "This is the attribution chain to check against, and it is why the TCTL records look past Tang-Control to nand2mario rather than treating the fork as an origin. Both open links were closed the same day: the remembered memory module is nand2mario's bit-banged GPIO JTAG programmer, already vendored here and recorded as TOOL-007, not memory code; and colibri's licence is CERN-OHL-W-2.0, not MIT (PROV-003). Tang-PSX is also being archived - backed up and kept, not deleted - which leaves Tang-Phosphor as the surviving core project in day-to-day use and makes Tang-PSX's own reference historical rather than live; the records that bear on this firmware are harvested as PSX-001 through PSX-006."
  sources:
    - "Project statement recorded with the user, 2026-10-03"
    - "nand2mario/tangcore README.md: 'TangCore firmware is licensed under Apache 2.0. Each core maintains its original license - please check individual core directories for details.'"
    - "Tang-Control README.md line 3: 'This repo is a fork of nand2mario's firmware-bl616' (https://github.com/nand2mario/tangcore)"
  verification: "The TangCore origin and the Tang-Control fork are confirmed in the trees, and the split licence (firmware Apache-2.0, cores per-directory) is in nand2mario's README. The DDR3 configuration is Apache-2.0 values from Sipeed's TangMega-138K-example design while the generated controller is Gowin EDA vendor IP, and TinyDesk is the two MIT submodules. The memory module resolved to the JTAG programmer by the user's correction of 2026-10-03, and colibri's licence was read from the port's own NOTICE and LICENSES/ directory."

- record_id: PROV-002
  kind: TOOLCHAIN
  topic_id: PROV
  title: "The licence and third-party notice, and what they cover"
  status: SUPERSEDED
  verified_date: 2026-10-03
  statement: "TinyTang carries a LICENSE and a THIRD_PARTY.md at the repository root, both added on 2026-10-03. The licence is Apache-2.0, which is the right one because this project descends from TangCore's Apache-2.0 firmware by way of Tang-Control. THIRD_PARTY.md names TangCore's firmware as the lineage for the board knowledge, Tang-Control as the intermediate, the vendored Gowin JTAG programmer with its retained nand2mario copyright and openFPGALoader behind it, both TinyDesk submodules at their pinned MIT commits, nestang's iosys_bl616.v read as a specification but not copied (GPL-3.0), the Bouffalo SDK (Apache-2.0), and colibri (PROV-003). Before this the tree held neither file and only usb_config.h carried an SPDX tag, and that is a vendor file."
  consequence: "The obligation this project had is discharged. The one claim in THIRD_PARTY.md that has to stay true is the one about the core UART protocol: it was read from a GPL-3.0 source as a specification and reimplemented, and that is the sentence that keeps a GPL-3.0 reading from becoming an unlicensed copy. Any file vendored from here on needs a line in that document and a record here."
  sources:
    - "This project's LICENSE and THIRD_PARTY.md, added 2026-10-03"
    - "nand2mario/tangcore README.md: the Apache-2.0 firmware licence"
    - "Tang-Phosphor, LICENSE and THIRD_PARTY.md: the model followed"
    - "TOOL-007, TDSH-001 and TDESK-001: the vendored programmer and the two submodules"
  verification: "Both files exist in the tree, and each entry in THIRD_PARTY.md was written from a licence read directly: tangcore's README, the programmer's own header, Tang-Control's LICENSE, the two submodules' VERSION and commit, the SDK's LICENSE, and colibri's NOTICE and LICENSES/."
  superseded_by: "PROV-004"

- record_id: PROV-003
  kind: EXTERNAL
  topic_id: PROV
  title: "colibri: CERN's VHDL common library, and the SystemVerilog port of it"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "colibri is CERN's open-source, vendor-independent, fully verified VHDL common library, developed by the EP department to standardise FPGA designs across the organisation and released publicly in 2026. It lives at gitlab.cern.ch/colibri/colibri, mirrored at gitlab.com/colibri-cern/colibri. For SystemVerilog work there is an unofficial port at github.com/kavierim/colibri-sv, which CERN has not endorsed and which pins itself to upstream commit 3fa784121ccea86d9e65b2e0dc08d2a3327f5f2f on colibri-cern/colibri. That port is written for Verilator and free EDA tooling and ships an AGENTS.md, a CONVENTIONS.md, docs with a module template and playbooks, sim testbenches, fv/ formal material, a sysml/ requirements model, Python behavioural models, and a Yosys ASIC smoke script; it is new and small, with two stars and a last push of 2026-09-30. The licence is CERN-OHL-W-2.0, the weakly reciprocal CERN Open Hardware Licence, not MIT, held in LICENSES/; the port's NOTICE declares itself a modification of the library's Covered Source made under section 3 of that licence."
  consequence: "Read the port, not the VHDL, when working on this project: it is in the language this family writes, it targets the free toolchain already in use here (Verilator, Yosys, the OSS CAD Suite), and it is the only one of the two aimed at agents at all - it ships an AGENTS.md and a conventions document. The original stays the authority on intent and currency, because the port is unofficial and pinned, so it lags upstream and carries one translator's choices. The licence covers both and is unchanged by any of this: no colibri code is in this tree, nothing is owed today, and the weak reciprocity applies from the first copied module - including from the port, which is itself Covered Source modified under section 3. CERN-OHL-W-2.0 is not interchangeable with the Apache-2.0 and MIT licences this project otherwise runs on."
  sources:
    - "CERN colibri project: https://gitlab.cern.ch/colibri/colibri (mirror https://gitlab.com/colibri-cern/colibri)"
    - "kavierim/colibri-sv: README.md, AGENTS.md, and its pinned upstream line naming commit 3fa784121ccea86d9e65b2e0dc08d2a3327f5f2f"
    - "kavierim/colibri-sv: LICENSES/CERN-OHL-W-2.0.txt and NOTICE, the port's own licence declaration"
    - "CERN EP department announcement and the FPGA Developers' Forum material on colibri, 2024-2026"
    - "Project statement recorded with the user, 2026-10-03: that future agents will reference it"
  verification: "The licence and the upstream pin were read from the port's own NOTICE, LICENSES/ directory and AGENTS.md, and the project, mirror and port were all confirmed to exist. The library is not present in this tree: no colibri source is vendored here."

- record_id: PSX-001
  kind: DEVICE
  topic_id: PSX
  title: "This board's FPGA is revision C, while the core image it runs is revision B"
  status: SOURCED
  verified_date: 2026-10-03
  statement: "Sipeed identifies the device revision from the fifth character of the second marking line on the package. The installed part is marked GW5AST-LV138PG484AC1/I0, 2518CA0N, TS0E44.00, so it is revision C, and Gowin targets it as GW5AST-138C. The core image this project loads, nand2mario's console138k artifact, is built for revision B (DEV-002)."
  consequence: "A revision-B bitstream is what actually runs here and it works: the cartridge boots and the UART link answers. Worth knowing rather than assuming, because revision is what Gowin keys IP generation on and Tang-PSX regenerated all of its IP for revision C. Nothing in this firmware builds gateware, so the choice belongs to whoever builds the core images; this record exists so that a core which fails to configure is not blamed on the loader first."
  sources:
    - "Tang-PSX, .ai/core-reference.md record DEV-001, harvested as that project is archived"
    - "Sipeed wiki, How to Identify Device Version, as cited by TangMega-138K-example ddr_memory/README.md, commit 06e7d8b118d345915ab6f257b7c22226f81575cd"
  verification: "Read from Tang-PSX's reference, which verified it from a user photograph of the package on 2026-09-28. Not re-measured here; this project's own evidence is indirect, that a revision-B console138k image configures and runs on this board."

- record_id: PSX-002
  kind: DEVICE
  topic_id: PSX
  title: "GW5AST-138 logic and block-RAM resources"
  status: SOURCED
  verified_date: 2026-10-03
  statement: "The device provides 138,240 LUTs, 139,095 registers and 340 BSRAM blocks. Each block is 18 Kbit, about 765 KB in total."
  consequence: "This project builds no gateware, so the figures bound only what can be asked of the cores it loads: on-chip block RAM cannot hold a PSX-class machine's working set, which is why the cores on this card depend on DDR3 or the dock's SDRAM rather than on BSRAM."
  sources:
    - "Tang-PSX, .ai/core-reference.md record DEV-002, harvested as that project is archived"
    - "Gowin EDA 1.9.11.03 place-and-route resource report for GW5AST-LV138PG484AC1/I0"
  verification: "Read from Tang-PSX's reference, which took it from Gowin's device totals. Not re-read from a datasheet here."

- record_id: PSX-003
  kind: BOARD
  topic_id: PSX
  title: "The FPGA end of the core UART link, and the board's 50 MHz clock"
  status: SOURCED
  verified_date: 2026-10-03
  statement: "A 50 MHz oscillator drives FPGA pin V22 at LVCMOS33. The BL616 control UART reaches the FPGA on V14 (FPGA receive) and U15 (FPGA transmit), also LVCMOS33."
  consequence: "This is the far end of the link PROT-003 describes from the BL616 side, TX GPIO 28 and RX GPIO 27 at 2 Mbaud. When the link goes silent either end can be at fault, and these are the pins to probe on the FPGA side; the 50 MHz clock is the reference the Tang-Control-derived transports time themselves against."
  sources:
    - "Tang-PSX, .ai/core-reference.md record BRD-003, harvested as that project is archived"
    - "litex-boards commit e4307929c38a, litex_boards/platforms/sipeed_tang_console.py (clk50, serial)"
    - "Sipeed TangMega-138K-example commit 06e7d8b, ddr3_1v4_hs.cst (clk, uart_tx, uart_rx)"
  verification: "Read from Tang-PSX's reference. Not probed here; this project's evidence is indirect, in that its own transport over these pins works."

- record_id: PSX-004
  kind: BOARD
  topic_id: PSX
  title: "The module's 8-pin JTAG and UART header, U1201"
  status: SOURCED
  verified_date: 2026-10-03
  statement: "The Tang Mega 138K module's 8-pin JST SH connector U1201 carries, by pin: 1 5V0 through diode D13, 2 TMS (T13), 3 TDO (U13), 4 TCK (V12), 5 TDI (R13), 6 RX into the FPGA (V14), 7 TX out of the FPGA (U15), 8 GND. On these docks the same JTAG nets run straight to the BL616 debugger, with a 0-ohm resistor on TDO. Pin 1 measures about 4.4 V."
  consequence: "Three rules follow, and together they explain the debugging arrangements this project inherited. An external JTAG adapter shares TCK, TMS and TDI with the BL616, which drives them while it loads a core, so the adapter must be released or unplugged during a tangload. Pins 6 and 7 must stay unconnected, because they are the core UART link. Pin 1 must not reach a 3.3 V adapter. It is also the configuration JTAG, not the AE350's debug JTAG. A Raspberry Pi Pico 2 running lonehog/JTAGprobe under OpenOCD found the TAP (IDCODE 0x0001081B) where openFPGALoader 0.13.1 did not, because its cmsisdap driver needs CMSIS-DAP v1 HID and the probe speaks v2."
  sources:
    - "Tang-PSX, .ai/core-reference.md record BRD-006, harvested as that project is archived"
    - "Sipeed tang_mega_138k_30353_Schematics.pdf, sheet JTAG Connector, U1201"
  verification: "Read from Tang-PSX's reference, where it was confirmed by unloaded voltage readings on the connector and by a working OpenOCD connection. Not re-measured here."

- record_id: PSX-005
  kind: BOARD
  topic_id: PSX
  title: "The dock's SDRAM add-on, and which memory belongs to whom"
  status: SOURCED
  verified_date: 2026-10-03
  statement: "The dock exposes two 40-pin SDRAM connectors, J9 and J10, each able to carry one 16-bit SDR SDRAM; the add-on module is a Winbond W9825G6KH-6, 256 Mbit (32 MB) x16, 166 MHz grade, 3.3 V I/O, with CLK on connector pin 20. Separately the SOM fits two Hynix H5TQ4G63EFR-RDC x16 DDR3 devices forming a 1 GiB x32 array on a shared address and command bus."
  consequence: "Recorded because the memory question keeps coming up: the RAMs on this bench are distinct and only one belongs to the firmware. The BL616 has 320 KB of on-chip OCRAM and no external RAM (BL6-002); the FPGA has the DDR3 array and optionally these SDRAM modules on the dock. Nothing in this firmware touches either FPGA-side memory."
  sources:
    - "Tang-PSX, .ai/core-reference.md records SDR-001 and BRD-001, harvested as that project is archived"
    - "litex-boards commit e4307929c38a, sipeed_tang_console.py: sdram0/1_connector and the W9825G6KH6 module selection"
  verification: "Read from Tang-PSX's reference, which confirmed the part by photograph only. Which connectors are populated on this bench, and the connector pin mapping, are not verified."

- record_id: PSX-006
  kind: EXTERNAL
  topic_id: PSX
  title: "Reading a loaded core's state, and the core ID's low byte"
  status: SOURCED
  verified_date: 2026-10-03
  statement: "Tang-PSX's measurements of the Tang-Control transport: with a core loaded and answering, the status line reported active_core 81 (0x51) together with core_running: no, and at the main menu active_core was 0. So the reliable indicator of a loaded core is active_core, and it holds the low byte of the core's own CORE_ID, because the legacy core-ID response carries only that byte. Uploads were refused while a core was running, and cores are looked for at cores/<board>/ then cores/, with console138k as the board name. The link runs at 2,000,000 baud with a 5,000,000 fast mode, and the iosys clock must be at least eight times the baud rate."
  consequence: "Two things worth carrying. The low byte explains every core ID seen on this card - 0x01 nestang, 0x50 Phosphor, 0x51 Gate 1 - and it is exactly what this project's fpga probe reads back. And the eight-times rule is a precondition on the core, not on the loader, so it belongs to whoever builds a core image rather than to this firmware. Tang-PSX's TCTL records and this project's EXTCTL and TCTL records already cover the protocol and the search path; this record adds the state reading and the id byte."
  sources:
    - "Tang-PSX, .ai/core-reference.md records TCTL-001, TCTL-002 and TCTL-003, harvested as that project is archived"
    - "Tang-Control commit 26e975bef22b: utils/fpga_debug.h and scripts/tangctl.py"
  verification: "Read from Tang-PSX's reference, where the status lines were observed repeatedly while a core answered. This project's own fpga probe returning core 1 for nestang is consistent with the low-byte rule, but is not a measurement of the byte."

- record_id: PROV-004
  kind: TOOLCHAIN
  topic_id: PROV
  title: "The project licence is MIT, with one file left Apache-2.0"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "TinyTang's own code is MIT, in LICENSE at the repository root, which supersedes the Apache-2.0 choice PROV-002 recorded. The one exception is ports/bl616/tang_jtag_programmer.c, which stays Apache-2.0 as nand2mario's file (TOOL-007): the Apache-2.0 text is reproduced at LICENSES/Apache-2.0.txt, and that file's header gained a comment saying so, because its original sentence pointed at the root LICENSE of the tree it was written in."
  consequence: "Apache-2.0 was never required, which is the point worth keeping: it is permissive, not copyleft, so an Apache-2.0 file may sit inside an MIT project provided that file keeps its notices and its licence text. The cost of MIT is a mixed-licence repository rather than a uniform one, which is ordinary here - Tang-Phosphor is GPL-3.0 with MIT, BSD-2-Clause and Apache-2.0 files inside it. Two consequences follow and should be known rather than discovered: the express patent grant Apache-2.0 carries is given up, and GPL-3.0 gateware from Tang-Phosphor can no longer be copied into this tree, which a GPL project could do and a permissive one cannot."
  sources:
    - "This project's LICENSE, LICENSES/Apache-2.0.txt, THIRD_PARTY.md and ports/bl616/tang_jtag_programmer.c, all changed 2026-10-03"
    - "Project statement recorded with the user, 2026-10-03: a preference for MIT unless Apache-2.0 turned out to be required"
    - "PROV-002, which this record supersedes"
  verification: "Checked that nothing forces a copyleft licence: outside third_party and build, the only file in the tree carrying a licence of its own is the vendored programmer, so the project's inbound licensing is one file wide. Both licence texts are present and were read back after being written."
```

---

## 6. Record template

```yaml
- record_id: <TOPIC>-<NNN>
  kind: DEVICE | BOARD | SOC | USB | FLASH | PROTOCOL | EXTERNAL | TOOLCHAIN
  topic_id: <TOPIC>
  title: "Short noun phrase"
  status: VERIFIED | SOURCED | INFERRED | SUPERSEDED
  verified_date: YYYY-MM-DD
  statement: "What the source says, with exact values and units"
  consequence: "What it means for this project, or where it is implemented"
  sources:
    - "Document or repository, revision/commit/path/symbol, URL"
  verification: "How and when it was confirmed, or why it is not yet confirmed"
  superseded_by: "<record_id>"   # only on a SUPERSEDED record
```

---

## 7. Maintenance boundary

- Keep external facts, their sources, and their verification status here.
- Board faces shared with another project are cited, not copied. The PMOD socket pins, the seating rule and the personality registry live in Tang-Phosphor's `.ai/core-reference.md` beside the constraint file and the RTL that implement them; this file records only the part that bounds this firmware, so the same board does not acquire two sources that can drift.
- The exception is a project being archived. Tang-PSX is backed up and kept, but stops being a live source, so the records that bear on this firmware were copied here as PSX-001 through PSX-006. Copying is for reach, not for preservation: nothing is lost when that project is archived, but its reference stops being the thing anyone consults day to day.
- Keep build results, failures, rationale, and chronology in `core-log.md`; cite a log entry number in `verification` instead of repeating it.
- Do not add speculative records for topics that have not been looked up. An unanswered question stays out of this file until a source answers it.
- When a record's source is superseded by a newer document or tool version, add a new record rather than editing the old statement.
- A `core-syntax.md` audit is required whenever this file changes, per `.ai/core.md`.

```yaml
last_reviewed: 2026-10-03
```
