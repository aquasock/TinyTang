# TINYTANG TECHNICAL REFERENCE

> **Project:** TinyTang
> **Purpose:** The authoritative lookup for externally defined facts this project depends on: the board's wiring and fitted parts, the BL616's registers and memory, the interface contract of the FPGA cores we boot, the contracts of the external projects we embed, and the behaviour of the vendor tools.
> **Authority:** Per `.ai/core.md`, consult this file before looking anything up online, and open the record behind any hardware fact before acting on it. A record is authoritative to the extent of its `status` and cited `sources`. When a better or newer source is found online, add or revise a record and notify the user.

This file records what an outside source (a datasheet, schematic, vendor SDK, vendor tool, or external repository) says, plus hardware observations that confirm or refute it. It does not record project history, build results, or discussion; those belong in `core-log.md`. It does not record project design choices unless an external fact forces them.

Some records are negative results — facts about what this hardware *cannot* do, or an open defect's ruled-out causes. They are recorded because each cost a session to establish and each would otherwise be re-litigated: see USB-002, USB-003, NEST-002, BLE-007 and BLE-017.

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

rules:
  - "One fact or one tightly coupled fact group per record."
  - "Every record cites at least one source with enough detail to re-check it: repository path plus commit, file, symbol, register name, or tool version."
  - "Separate what the source says (statement) from what it means for this project (consequence)."
  - "Diagnostic or implementation limits chosen by this project are never recorded as standard or device limits."
  - "A record holds what is true now. When a fact changes or proves wrong, edit the record in place: revise statement and consequence, update verified_date and sources, and end verification with 'Revised <date> (core-log entry N): <what changed>'. Git and core-log.md keep the history; this file does not."
  - "Before acting on a record, check that the code paths and symbols it cites still exist; fix the record in the same cycle if they do not."
  - "Never reuse a record_id. When a record is folded into another, or split, move its id to the retired-id table in section 8 with the record that now holds its facts, after checking that every fact still true has been carried across."
  - "An INFERRED record must say how it was inferred, in verification."
  - "A negative result is a fact. Record what the hardware cannot do with the same rigour as what it can, and say how many attempts established it. An open defect may be recorded as its established facts and ruled-out causes, with any lead labelled as a lead."
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

- topic_id: BLE
  name: "Bluetooth LE on the BL616"
  description: "The BL616's own radio: the board's antenna path, the SDK's controller libraries and host stack, how its GATT client behaves against a real HID device, and which input devices can be used at all."

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
| Does the FPGA keep its core across a power cycle? | DEV | DEV-006 |
| What is stored in the FPGA's own configuration flash, and who put it there? | DEV | DEV-007 |
| Does this project run AE350 RISC-V code, and how does it get there? | DEV | DEV-004 |
| Is there a core with no emulator for TinyDesk to sit on? | DEV | DEV-005 |
| Which FPGA pins are the JTAG programmer's? | BRD | BRD-001 |
| Why does the SD card fail to mount? | BRD | BRD-002 |
| Where do the two USB-A controller ports go? | BRD | BRD-003 |
| What is the onboard USB debug bridge? | BRD | BRD-004 |
| Does the board need a particular power input? | BRD | BRD-005 |
| What decides whether the board comes up one-wire or two-wire? | BRD | BRD-006 |
| Can one-wire and two-wire run together? | BRD | BRD-007 |
| Can a Raspberry Pi HAT go on the dock's 40-pin headers? | BRD | BRD-008 |
| How is the BL616 recovered if a firmware stops it before USB comes up? | BRD | BRD-009 |
| Where are the board's PMOD sockets, and how are their pins numbered? | PMOD | PMOD-001 |
| How does the core learn what is seated in the PMOD sockets? | PMOD | PMOD-002 |
| Which register selects a PMOD personality, and what are the values? | PMOD | PMOD-004 |
| Which declarations passed on this dock, including the encoder's lower row and PmodVGA header order? | PMOD | PMOD-006 |
| How does this firmware read /tang.ini, which module names does it accept, and is I2S2 a personality? | PMOD | PMOD-006 |
| Which socket pins carry the desktop core's I2C bus, and why does a scan find 112 devices? | PMOD | PMOD-007 |
| Where are the USB OTG registers, and which bit is which? | BL6 | BL6-001 |
| How much RAM does the BL616 have? | BL6 | BL6-002 |
| How do I read free heap? | BL6 | BL6-003 |
| Why do xPortGetFreeHeapSize() and friends fail to link? | BL6 | BL6-004 |
| Is there any real-time clock? | BL6 | BL6-005 |
| How do I read the CPU clock? | BL6 | BL6-006 |
| Why is the FPGA link slower than its baud rate? | BL6 | BL6-007 |
| Why did a crash or assert freeze the board silently, and what catches it now? | BL6 | BL6-008 |
| What happens now when the heap runs out, and why did loading cores use it up? | BL6 | BL6-009 |
| Why does an allocation fail with free memory left, and why did phosphor.tdsh stop loading? | BL6 | BL6-010 |
| What speed does the console negotiate? | USB | USB-001 |
| Can the OTG block tell me what is on the other end of the cable? | USB | USB-002 |
| Can the OTG connector power a keyboard? | USB | USB-003 |
| Is it safe to re-register the console's endpoints? | USB | USB-004 |
| Why is there no host deinit, and what must be done by hand? | USB | USB-005 |
| How do I tell whether a host is attached? | USB | USB-006 |
| Why is TinyDesk on HDMI laggy with the computer attached? | USB | USB-007 |
| Why does the console vanish when the Bluetooth radio starts? | USB | USB-008 |
| Where does the application live in flash, and what does a soft reset do? | FLS | FLS-002 |
| What does a watchdog reset do, and where is a crash record kept across it? | FLS | FLS-003 |
| Which BL616 loader does the board have, and where are the full flash backups? | FLS | FLS-004 |
| Where is the Bluetooth antenna, is one fitted, and how well does the radio work without one? | BLE | BLE-001 |
| Which SDK Bluetooth library does this firmware need, and what does Bluetooth cost? | BLE | BLE-002 |
| Why does a UUID-filtered GATT discovery find nothing? | BLE | BLE-003 |
| Why does a subscription's notify callback get a NULL report? | BLE | BLE-004 |
| Why does the BLE host hang partway through setting up a device? | BLE | BLE-005 |
| What does the Logitech K950 expose over Bluetooth LE? | BLE | BLE-006 |
| Which Bluetooth input devices can the board use? | BLE | BLE-007 |
| Why does a Bluetooth keyboard need its own liveness rule and its own pointer mode? | BLE | BLE-008 |
| What does the Logitech M750 mouse expose over Bluetooth LE? | BLE | BLE-009 |
| Why does a GATT subscribe fail with -EALREADY after a bonded device reconnects? | BLE | BLE-010 |
| How does the host reconnect a paired device, and why is there no bt_le_set_auto_conn? | BLE | BLE-011 |
| Where are Bluetooth pairings kept, and how do devices come back after a reset? | BLE | BLE-012 |
| What does a failed re-encryption mean, and when is a stored key dropped? | BLE | BLE-012 |
| How much heap does the running Bluetooth stack take? | BLE | BLE-014 |
| What does the Xbox Wireless Controller expose, and how is it put in pairing mode? | BLE | BLE-015 |
| What is in the Xbox controller's input report? | BLE | BLE-016 |
| Why does the Xbox controller not work here, and what has been ruled out? | BLE | BLE-017 |
| Why does a Bluetooth connection assert and reset the board? | BLE | BLE-018 |
| What is the frame format a loaded core expects? | PROT | PROT-001 |
| Which commands does a loaded core understand? | PROT | PROT-002 |
| Which UART and rate reach a loaded core? | PROT | PROT-003 |
| What is the layout of the core's on-screen text page? | PROT | PROT-004 |
| Why does the screen go black when the OSD is on? | PROT | PROT-005 |
| How could a controller navigate a menu? | PROT | PROT-006 |
| Why can a ROM load end in a black screen with no error? | PROT | PROT-007 |
| What shows on screen between a core loading and the desktop painting? | PROT | PROT-008 |
| Why are there marks down the left edge of the desktop layer? | PROT | PROT-009 |
| How does ae350run load a program and a file into a TinyNES core's AE350? | PROT | PROT-010 |
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
| Does the merged Phosphor core decode WAV or FLAC in the FPGA? | PHOS | PHOS-006 |
| Why does the resident AE350 player hang before its first decode? | PHOS | PHOS-007 |
| Which resident player image is the qualified one, and how is it rebuilt? | PHOS | PHOS-008 |
| How is a track stopped on the merged core, and what do the sink's counters mean? | PHOS | PHOS-009 |
| Why does a song take many seconds to start, and how big a file can Phosphor play? | PHOS | PHOS-010 |
| Which register routes the stream to the AE350, and which Phosphor core does the firmware need? | PHOS | PHOS-011 |
| How would a resident AE350 player be handed tracks and stopped, and where does it publish its state? | PHOS | PHOS-012 |
| What did the unqualified resident gapless player teach about tracking and stopping tracks on the BL616? | PHOS | PHOS-013 |
| Which shell revision is this, and what can a script do? | TDSH | TDSH-004 |
| What rules does upstream set for this port: names, commands, patches, pull requests? | TDSH | TDSH-005 |
| Why did desktop scripts return without running, and how are their memory limits set now? | TDSH | TDSH-008 |
| Which sequences must a console mirror understand? | TDSH | TDSH-002 |
| What does TinyDesk need from a port? | TDESK | TDESK-013, TDESK-003 |
| How much RAM does the desktop need? | TDESK | TDESK-002 |
| Why does the Terminal window say "No shell backend in this build."? | TDESK | TDESK-004 |
| How is a shell hosted in the Terminal window? | TDESK | TDESK-005 |
| Why did the Terminal stop accepting input after reopening the desktop? | TDESK | TDESK-014 |
| How does TinyDesk launch the Network app, and how does this port put a Bluetooth window there? | TDESK | TDESK-015 |
| Why does the Editor take so much heap, and what limits does this port set? | TDESK | TDESK-016 |
| Why does Ctrl+S freeze the screen? | TDESK | TDESK-006 |
| Why are System Monitor and Task Manager blank? | TDESK | TDESK-007 |
| Which apps exist, and which are compiled here? | TDESK | TDESK-009 |
| Can the Files app delete a file with two keystrokes? | TDESK | TDESK-011 |
| Why is CONFIG_CHERRYUSB_HOST set with a device-only firmware? | TOOL | TOOL-001 |
| Which IDCODEs does the Gowin programmer accept? | TOOL | TOOL-002 |
| What versions are the build made from? | TOOL | TOOL-003 |
| Why must a bitstream be device-specific? | TOOL | TOOL-004 |
| Where is the USB-enumeration bisection kept? | TOOL | TOOL-005 |
| Which host tools exist, and what do they need? | TOOL | TOOL-006 |
| What code is vendored into this project, and under what licence? | TOOL | TOOL-007 |
| Which SDK components does this firmware link, and under what licences? | TOOL | TOOL-016 |
| Why was the open-flow core's HDMI dark, how is it fixed, and which flow builds the desktop core? | TOOL | TOOL-017 |
| Why did open-flow cores whose clk took SPINE17 or SPINE18 load dead? | TOOL | TOOL-021 |
| Is it safe to send a file to the board, or is the desktop holding the console? | TOOL | TOOL-011 |
| How is the board debugged one-wire, and what of TinyTang and TinyDesk is reachable then? | TOOL | TOOL-012 |
| How is a core deployed, debugged and exercised entirely over one-wire? | TOOL | TOOL-013 |
| Is there a JTAG path to the FPGA other than the BL616 and the FT2232? | TOOL | TOOL-014 |
| Can a FatFS call be made inside a critical section? | TOOL | TOOL-015 |
| Does the Bluetooth stack need attribution, and where are its notices? | TOOL | TOOL-016 |
| Why does the patch applier stop recognising a patch? | TOOL | TOOL-010 |
| Who is upstream of this project, and in what order? | PROV | PROV-001 |
| Does this project carry the licences and notices it owes? | PROV | PROV-005 |
| What licence is this project under, and what is the exception? | PROV | PROV-005 |
| Where is colibri, and what are its terms? | PROV | PROV-003 |
| Which silicon revision is this board, and which revision is the core image? | PSX | PSX-001 |
| How much logic and block RAM does the device have? | PSX | PSX-002 |
| Which FPGA pins carry the core UART, and where is the board's clock? | PSX | PSX-003 |
| What is on the FPGA module's JTAG and UART header? | PSX | PSX-004 |
| What memory is on this board? | PSX | PSX-005 |
| How do I tell whether a core is loaded, and what is a core ID? | PSX | PSX-006 |

---

## 4. Fast lookup index

One line per live record, in the order of section 5. Retired ids are in section 8.

```yaml
DEV-001: "The FPGA is a GW5AST-138; its JTAG IDCODE is 0x0001081b, reported as ID=0001081b on every tangload"
DEV-002: "cores/console138k/nestang.bin is 4,593,044 bytes and byte-identical to nand2mario's generated console138k artifact (GW5AST-138B)"
DEV-004: "A core bitstream can carry AE350 RISC-V software: the program is compiled to a hex file and read into a boot ROM inside the design with $readmemh, so it is synthesised into the bitstream and arrives with tangload; the firmware loads whole images and never AE350 code as an artifact of its own"
DEV-005: "The menu core is the console138k design with nestang's NES machine not instantiated: MENU_CORE in src/boards/console138k_menu.v, selected by build.tcl's third `menu` argument; it answers CORE_ID 1, the same value the patched NES core answers, and is about four fifths smaller in logic while its bitstream is only ~2.5 percent smaller"
DEV-006: "The FPGA is not empty at power-up: with no tangload it is already running a core that answers core ID 0 on the UART and shows a TangCore splash on HDMI, so it configures itself from storage of its own; boot.tdsh replaces it within seconds, and every boot reloads this project's core (tangload, fpga, nesload for a cartridge)"
DEV-007: "The FPGA's 16 MB XTX configuration flash (JEDEC 0b 40 18) holds one image at offset 0, 1,084,246 B, a compressed GW5AST-138 bitstream matching no TangCore 0.9 core; TangCore's installer and firmware never write that flash, so it is most likely the factory splash core"
BRD-001: "BL616 to FPGA JTAG: TMS GPIO0, TCK GPIO1, TDO GPIO2, TDI GPIO3"
BRD-002: "SD is gated behind GPIO 16 held high; without it f_mount returns FR_NOT_READY (3)"
BRD-003: "The two USB-A controller ports are FPGA pins: usb1_dp/dn H13/G13, usb2_dp/dn M15/M16, all IO_TYPE=LVCMOS33"
BRD-004: "The onboard debug bridge is a SIPEED FT2232 (0403:6010, product 'USB Debugger'), a separate USB path from the BL616's CDC"
BRD-005: "The board has two power inputs and runs on either; a power cycle is unplugging both and restoring power first"
BRD-006: "Which firmware the BL616 runs is chosen at a cold power-up: with the power input it runs TinyTang and the CDC (two-wire); on the OTG cable alone it comes up as the vendor's FT2232 USB Debugger (one-wire) and TinyTang does not run; removing the power cable from a running board changes nothing, since the OTG cable keeps it powered"
BRD-007: "One-wire and two-wire cannot be combined: by the user's account the board's modules are powered from one input or the other, never both, and making the two work together was tried at length in the earlier projects without success; no written record of that work was found"
BRD-008: "The dock's two 2x20 2.54 mm headers are the SDRAM connectors J9 (SDRAM0) and J10 (SDRAM1), MiSTer-style: FPGA I/O on pins 1-10, 13-28 and 31-40, +5 V on pin 11, ground on pin 12; they are not Raspberry Pi headers, and a Pi HAT fitted directly would put the board's 5 V on a HAT signal and short eight FPGA pins through the HAT's ground pins"
BRD-009: "The BL616 has a BOOT button inside the case for its ROM bootloader, which then enumerates as 349b:6160 on /dev/ttyACM0; a first install or a recovery writes only the application at 0x40000 with BLFlashCommand --write, never make flash, which would overwrite Sipeed's loader at 0x0"
PMOD-001: "Two PMOD sockets: PMOD1 beside HDMI on W19 W20 F19 F20 E22 D22 E21 D21 and PMOD0 on V18 V19 G21 G22 F18 E18 C22 B22, all LVCMOS33; Sipeed interleaves the rows, so IO0/2/4/6 are Digilent pins 1-4 and IO1/3/5/7 are pins 7-10, and flipping a module swaps pins 1-4 with 7-10"
PMOD-002: "/tang.ini at the SD root is the socket contract (pmod0/pmod1 plus _flip, flat under [tang]); a missing file or absent entry releases the socket and unknown modules are refused; modules carry no ID pins, so presence can never be detected; this firmware parses it (tang_ini.c) and, by the user's choice, does not enforce flip safety"
PMOD-004: "On the merged Phosphor core the socket control register is 0xc0: hold 0, PMOD0 personality 7:4, PMOD1 11:8, flips 12/13; personalities 0 none, 1 oledrgb, 2 vga J1, 3 vga J2, 4 rotary encoder; both sockets power up released, so a host must declare them; the bring-up core has the same layout at 0x10, which is not extended frame type 0x10"
PMOD-006: "tangload declares /tang.ini to core 0x50 at ABI 1.8+ and reads it back; names none, oledrgb, encoder, i2s2 (personality 5, ABI 1.10), vga_j1, vga_j2; qualified: I2S2 on PMOD0 0x0050; encoder face up on PMOD1 pins 7-12 needs flip=yes, 0x2400, or 0x2410 with OLED on PMOD0; PmodVGA face up has J1 on PMOD1 and J2 on PMOD0, 0x0230; missing-file release and unknown-module refusal pass; the desktop core 0x54 takes none/oledrgb per socket, i2c in one socket beside none/oledrgb (desktop ABI 1.3), or the VGA pair, refusing any other word whole; its OLED on PMOD0 is qualified at 0x0010, and its I2C bus beside the OLED in either socket, at 0x0160 and 0x0610 (PMOD-007)"
PMOD-007: "The desktop core's I2C personality (6) drives SCL on socket pins 3 and 9 and SDA on pins 4 and 10 as open-drain with no FPGA pull-up, and reads SDA from pin 4; a module must bring its own pull-ups: an Adafruit DS3231 on STEMMA QT answers at 0x68 and reads and writes on PMOD0 at 0x0160 and on PMOD1 at 0x0610, while a Digilent Pmod RTCC, which has none, scans 112 devices"
BL6-001: "USB_BASE 0x20072000; OTG_CSR +0x80 (ID 21, CROLE 20, SPD 23:22, VBUS_VLD 19, A_SESS 18, B_SESS 17, A_BUS_DROP 5, A_BUS_REQ 4); PDS usb_ctl 0x2000E500 (IDDIG 5, DRVBUS_POL 4)"
BL6-002: "OCRAM is 320 KB at 0x20FC0000; the PSRAM window is declared but this board has no external RAM"
BL6-003: "SDK 2.3.36's mm.h: TLSF heaps OCRAM and WRAM in g_mem_manager; kfree_size(0), kmax_free_size(0) and kmin_free_size() (a true low-water mark, tracking on in defconfig); tang_heap_info() sums total_size"
BL6-004: "xPortGetFreeHeapSize() and xPortGetMinimumEverFreeHeapSize() do not exist in this build; using them fails at link time"
BL6-005: "There is an HBN always-on RTC counter (HBN_Enable_RTC_Counter, HBN_Get_RTC_Timer_Val) with a selectable 32 kHz source: a counter, not a calendar, and no battery"
BL6-006: "bflb_clk_get_system_clock(BFLB_SYSTEM_CPU_CLK) returns the CPU clock"
BL6-007: "bflb_uart_putchar() reads the millisecond timer before every byte for its 100 ms timeout, costing about 7.5 us of CPU per byte here, which capped UART1 near 133 KB/s at 5 Mbaud; filling the TX FIFO from its free count reaches the wire rate"
BL6-008: "On SDK 2.3.36 exception_entry prints to an unconnected UART and spins, the FreeRTOS trap handler passes it masked mcause, the weak assert/stack/malloc hooks abort, an unknown device name asserts and the controller's platform_reset jumps to address 0; tang_crash.c wraps exception_entry and platform_reset and defines the hooks. The fork's allocator spun with interrupts off; 2.3.36's returns NULL"
BL6-009: "kmalloc and krealloc are wrapped with --wrap to count refused requests (tang_heap.c; 2.3.36 already returns NULL); every tangload leaked 4 KB (the JTAG programmer's unused fbuf_cached) until that was fixed; script workers are capped at 16 KB of stack (TDSH_SCRIPT_TASK_STACK=16384), and a cartridge script uses 10,728 B; a krealloc its own heap refuses is moved to the other (BL6-010)"
BL6-010: "SDK 2.3.36 splits the heap into OCRAM and a 32 KB WRAM heap; krealloc stays in its block's heap and FreeRTOS stacks come only from OCRAM, so phosphor.tdsh's 1,056 B line table was refused with WRAM free; tang_heap.c moves such a block and wraps pvPortMalloc to take any heap, so a script's 16 KB stack fits with the desktop open, and bletrace allocates its 16 KB only while capturing"
USB-001: "The CDC console negotiates USB 2.0 High Speed, 480 Mbps (lsusb -t); CONFIG_USB_HS sets CDC_MAX_MPS 512, which is legal only at HS"
USB-002: "No role signal follows the cable: OTG_CSR's ID bit tracks the forced configuration (PDS IDDIG), not the connector"
USB-003: "The OTG connector does not source VBUS: A_BUS_REQ is set and nothing comes out, in both DRVBUS_POL polarities, and a device that reacts to power stayed dark for 60 s"
USB-004: "usbd_add_endpoint() assigns by endpoint index, not by appending, and usbd_deinitialize() resets intf_offset and calls usb_dc_deinit(), so tdsh_bl616_console_init() is safe to call again"
USB-005: "There is no host deinit in the SDK: usbh_deinitialize() is software-only, and usb_hc_low_level_init() has no counterpart, so the port must be returned to device mode by hand"
USB-006: "CherryUSB fires USBD_EVENT_CONFIGURED on enumeration and USBD_EVENT_DISCONNECTED when the host goes away: the usable host-present signal"
USB-007: "An IN packet leaves only when the host reads, which it does only while a program has the port open; enumerated but unread, the console's old flush spun a million yields per packet and stalled every writer, including the desktop on HDMI. Writes now require DTR and wait at most 50 ms"
USB-008: "Starting the Bluetooth radio takes the BL616's USB device down while the CPU keeps running, and re-asserting its clock with GLB_Set_USB_CLK_From_WIFIPLL plus a device re-init brings the console back as a fresh ffff:5454 with no power cycle (tdsh_bl616_console_restart())"
FLS-002: "Application at 0x40000 up to 0xE0000 bytes, staging at 0x120000 up to the vendor data record at 0x200000; boot header magic at 0x00/0x08/0x64, body length at 0x84, CRC-32 of the first 252 bytes at 0xFC; the commit runs from .tcm_code with interrupts off; the vendor loader boots images over 0x80000; a soft reset lands in the vendor loader, so a reflash needs a power cycle"
FLS-003: "A watchdog reset, like a software reset, lands in the vendor loader and comes up as the FT2232, so only a power cycle (which scrambles RAM) brings TinyTang back; the crash recorder keeps its record in the last flash sector, 0x3FF000"
FLS-004: "The BL616 loader at 0x0 is not TangCore 0.9's bl616_fpga_partner_console138k.bin (60,741 of 88,672 bytes differ); full backups of the BL616 and FPGA flashes are in /run/media/vash/GIT/Tang-Console-138K-backups with SHA256SUMS.txt"
PROT-001: "Frames are 0xAA len_hi len_lo type payload[len-1]; length is big-endian and counts the type byte; a length high byte >= 8 drops the core back to hunting for magic"
PROT-002: "Commands: 01 core ID, 02 config string, 03 joypad (core to BL616), 04 cursor, 05 text, 06 loading state, 07 ROM data, 08 overlay, 09 HID, 0a/0b floppy, 0c PS/2"
PROT-003: "BL616 UART1, TX GPIO 28, RX GPIO 27, 2,000,000 baud, 8N1"
PROT-004: "The text page is 32 columns by 28 rows of 8x8 cells from a full ASCII font (FONT[0:127][0:7]); column 0 draws in the cursor colour; the core's logo sits at LOGO_X 92, LOGO_Y 201"
PROT-005: "overlay selects the whole picture at the mixer (nes2hdmi.sv: if (overlay) rgb <= overlay_color), and a core comes out of reset with it on"
PROT-006: "The core sends its joypad state as response 0x03 every 20 ms when it changes, unconditionally, whether or not anyone asked"
PROT-007: "A ROM stream into a core that is not running is discarded with no error on either side and surfaces as a black screen; the fpga ID probe between the loads is the only detection"
PROT-008: "A loaded core comes out of reset with the overlay asserted, and while the desktop's wide layer is still disabled the compositor shows the core's legacy 32x28 text page instead - whose store, gowin_dpb_menu.v, is never initialised, so its power-on contents are what appears; the font is initialised, which is why the garbage reads as glyphs; osd clear blanks the page (28 rows of 32 spaces) and is issued after the fpga probe in boot.tdsh and boot-cart.tdsh"
PROT-009: "hdmi.sv's cx/cy name the pixel sent on the next clock and the stock rgb path is one register deep to match; the wide layer's deeper pipeline must address the store that many pixels ahead, wrapping at frame_width, or pixels 0-2 of each line show the previous line's blanking, which reads cell 0, and cell (0,0)'s glyph column is painted down the left edge"
PROT-010: "A TinyNES core's AE350 boot ROM takes a program over UART1 at 2 Mbaud: header TNL1, address, length, entry, image CRC-32 and header CRC-32, then 8 KiB blocks each with a CRC-32, each answered ACK 0x06 or NAK 0x15 and a reason; a program asks for a file with TinyNES WANT <addr> <bytes> and takes TNR1, the length and the file the same way; ae350run sends both; after TinyNES LINK the core speaks 0xAA frames, its text in type 0x20, and ae350run hands the link to the desk layer"
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
TDSH-002: "The shell emits a closed set: CR, LF, ESC[2K, ESC[2J, ESC[H, ESC[<n>C, ESC[<n>D and SGR colour, and nothing else; a narrow-page renderer must treat CR as the start of the logical line"
TDSH-004: "TinyDesk Shell v0.1.5 at 8456dd1 (tinydesk-project/tinydesk-shell), C11 and MIT: uScript 1.1.1 (variables, substitution, arithmetic, if/while/for, functions, pipes, redirection; no script arguments); optional columns() and read_byte_timeout() in tdsh_terminal_io_t, 80 columns without either; board.conf numbers are decimal or 0x hex, never octal"
TDSH-005: "Upstream STANDARDS.md (v0.1.5) binds ports: one prefix for every owned name (this port's is tang, still recorded upstream as not chosen), interface files named tdsh_platform_<platform>.c and the like, no edits inside submodules (carried patches only), pins at release tags, both repositories' host tests passing at the pin, prefixed or subcommand-style command names, and PRs that state the user-visible change, the platforms run on and passing tests"
TDSH-008: "Upstream's 64-variable 19 KB session and 32 KB stack made desktop scripts fail silently; a carried patch makes the limits overridable and TinyTang uses 48 variables, 32-byte names, 128-byte values and a 16 KB worker stack (8,328 B session, 8,584 B job), with independent copies per script; upstream issue #2"
TDESK-002: "Screen memory is TD_MAX_COLS x TD_MAX_ROWS x 8 bytes, twice; the ESP32-C6 uses 80x25 or 256x96, and 100x30 costs 48 KB for the pair"
TDESK-003: "TinyDesk's filesystem is ports/common/td_fs_stdio.c, written against stdio, dirent.h and sys/stat.h, so it lands on this port's FatFS syscall layer unmodified"
TDESK-004: "The Terminal app draws 'No shell backend in this build.' exactly when s_backend is NULL; td_terminal_set_backend() supplies it"
TDESK-005: "A Terminal backend is td_term_backend_t: start, read, write, user, set_user; the ESP32-C6 reference runs the shell in a task over two FreeRTOS stream buffers"
TDESK-006: "The Editor saves with Ctrl+S; the host HAL clears IXON so it arrives, and a board port has no termios, so the user's terminal eats it as XOFF and freezes the display"
TDESK-007: "td_run() returns after td_quit(); td_shutdown() restores the terminal"
TDESK-008: "Network, MQTT, Modbus and Software Update are built on proto/td_sock.c and proto/td_tls.c: POSIX sockets, esp_timer.h, and mbedTLS with esp_crt_bundle.h"
TDESK-009: "Nine start-menu apps plus a clock window: About, Counter, Editor, Files, Log Viewer, Settings, System Monitor, Task Manager, Terminal, and Date & time"
TDESK-011: "TinyDesk's delete confirmations, in the Files app and on desktop icons, focus their Delete button, so Delete then Enter (or Space) removes the selected file with no further step; deleting /root/scripts/boot.tdsh leaves the board on the FPGA's own core"
TDESK-013: "TinyDesk v0.1.5 at feaf841 (tinydesk-project/tinydesk), C11: the port surface is td_hal_t (read_byte, write, millis, sleep_ms plus context); the Terminal backend is start, read, write, resize, user, set_user; it pins the shell at 8456dd1, the same as this project"
TDESK-014: "Terminal retains its started flag across td_shutdown(); a port that stops its shell at desktop exit must explicitly restart it on reentry, since reinstalling the same backend does not call start again"
TDESK-015: "TinyDesk launches apps by registered name: Settings' Network... button and the taskbar's network tray both call td_app_launch(\"Network\"), which does nothing when no app has that name, and the tray is drawn only when the port supplies td_net_ops_t; TinyTang's carried patch makes the button Bluetooth... launching its own Bluetooth app"
TDESK-016: "TinyDesk's Editor allocates TD_EDITOR_MAX + 1 bytes of text and an undo history of TD_EDITOR_UNDO bytes plus 128 records when it opens, about 25 KB at the 16 KB and 6 KB defaults, freed on close; TinyTang sets 8 KB and 2 KB, measured at 12 KB on the board, and larger files open read-only"
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
PHOS-006: "The merged Phosphor core's FPGA player is pcm_sink, a raw-PCM sink with no WAV or FLAC parser; it takes its rate from the AE350's last play_rate, latched at stream start, so a file streamed straight to it plays its bytes as samples at that rate, and every file must go through the resident AE350 player"
PHOS-007: "A resident AE350 player hangs at the start of its first decode depending on its image layout: a null jump, RAM-bridge ERROR responses from address 0 and no return; the streamed player needs 16 bytes where an input would go (0 and 32 hang, 16 and 48 play), which Tang-Phosphor's Makefile now reserves; the cause is not found"
PHOS-008: "The qualified resident player is 863764 bytes, CRC-32 ef1502ed, built by make -C software/rbhost bench-universal BENCH_NAME=resident with ~/.cache/tangcore-dev/toolchain/bin (Xuantie GCC 10.2.0) on PATH; the image left in Tang-Phosphor's build/rbhost/bench was the 863748-byte 3d762d13 that hangs (PHOS-007), so a player is rebuilt and its CRC checked before use"
PHOS-009: "The merged core's pcm_sink has no stop: an AE350 restart (0x43f0) ends the decode and pause (0x78 bit 0) silences it, but the sink stays in its playing state, and the stopped track's samples (up to 2048 in the sink and 512 in the AE350 stream queue) stay queued ahead of the next START, so a sink kept paused never takes that START; underruns (0x6c) clear only with the core, 0x30 counts STARTs, and elapsed and duration (0x8c, 0x90) read 0"
PHOS-010: "The whole file is streamed to the AE350 before the first sample plays, about 3.7 s per MB (4,387,971 B in 16,064 ms), so a song loads silently for many seconds; files up to 5.8 MB play, and where the AE350's memory limits file size is not known"
PHOS-011: "Since Tang-Phosphor register ABI 1.8 (0x04 reads 0x00010008) cpu_mode is its own readable word at 0xa8; before it, it was bit 0 of the socket control word 0xc0, so selecting the AE350 released both PMOD sockets; ae350_play selects the CPU at 0xa8 on every play and refuses an older core"
PHOS-012: "Unqualified resident-player contract: a length-0 mailbox request is a question chosen by its offset, 0 SIZE, 1 NEXT (queued track's size, 0 when none, 0xffffffff STOP) and 2 POLL (1 when a stop is pending), each answered as a four-byte session least significant byte first; the player publishes RES1 (0x52455331) at 0x4040, tracks begun 0x4044, tracks finished 0x4048, last track's samples 0x404c and decode status 0x4050"
PHOS-013: "Lessons from the discarded resident gapless player experiment: detect residency by loader RUN plus the RES1 marker, number a track as tracks begun plus one and treat it as over when tracks finished reaches that number, stop softly through the mailbox with a halt fallback, re-take the mailbox baseline whenever the BL616 has not itself started the resident player, and settle PHOS-007 and the RAM bridge before qualifying a player that never returns"
PROV-001: "Lineage as the user states it: nand2mario's TangCore is the origin for Tang-Phosphor and Tang-PSX; Tang-Control is a fork of the same repo for peek/poke and the 1-wire and 2-wire debug arrangements; the family also uses a DDR3 IP block, TinyDesk, and CERN's colibri as a reference; the remembered memory module turned out to be nand2mario's JTAG bit-bang programmer, and Tang-PSX is being archived rather than deleted"
PROV-003: "colibri is CERN's vendor-independent, fully verified, open-source VHDL common library at gitlab.cern.ch/colibri/colibri, with an unofficial SystemVerilog port at github.com/kavierim/colibri-sv pinned to upstream 3fa78412; read the port when working in SystemVerilog since it ships an AGENTS.md and targets Verilator and free tooling, but treat the original as the authority; licensed CERN-OHL-W-2.0 (weakly reciprocal), not MIT, with no code copied here"
PROV-005: "TinyTang is MIT with thirteen Apache-2.0 files: nand2mario's JTAG programmer, and ten Phosphor transport files and two host tests written by this project's author in Tang-Control; it descends from nand2mario's TangCore firmware-bl616 through Tang-Control, a fork of it, and copies no other TangCore source; THIRD_PARTY.md must keep saying the UART protocol was read from GPL-3.0 nestang as a specification, not copied"
PSX-001: "This board is GW5AST-138 revision C (package mark 2518CA0N), while the console138k core image it loads is built for revision B and works anyway"
PSX-002: "GW5AST-138: 138,240 LUTs, 139,095 registers, 340 BSRAM blocks of 18 Kbit, about 765 KB"
PSX-003: "50 MHz oscillator on FPGA pin V22; the BL616 control UART reaches the FPGA on V14 (FPGA RX) and U15 (FPGA TX), LVCMOS33 - the other end of the link PROT-003 describes from the BL616 side"
PSX-004: "Module connector U1201, 8-pin JST SH: 1 5V0 via diode (~4.4 V), 2 TMS T13, 3 TDO U13, 4 TCK V12, 5 TDI R13, 6 RX V14, 7 TX U15, 8 GND; the JTAG nets are shared with the BL616, so an external adapter must be released during a tangload, pins 6 and 7 stay unconnected, and pin 1 must not reach a 3.3 V adapter"
PSX-005: "Dock SDRAM is Winbond W9825G6KH-6, 32 MB x16 at 166 MHz, on connectors J9 and J10, separate from the SOM's 1 GiB x32 DDR3; neither is the BL616's memory, which is 320 KB of on-chip OCRAM (BL6-002)"
PSX-006: "A loaded core is indicated by active_core, the low byte of its CORE_ID (0x01 nestang, 0x50 Phosphor, 0x51 Gate 1), not by core_running, which reads no while a core answers; uploads are refused while a core runs; the link is 2 Mbaud with a 5 Mbaud fast mode and needs an iosys clock at least 8x the baud"
BLE-001: "The BL616's antenna pin runs through L9 (0 ohm) to U35, a U.FL jack marked ANT on the dock's underside, with no antenna from the factory; bare, the radio connects only close up with frequent HCI 0x3E failures; since 2026-10-10 a 2.4 GHz 6 dBi antenna is fitted on an RP-SMA pigtail, and a device that read -70 dBm bare reads -33 dBm"
BLE-002: "Scanning and connecting need the SDK's ble1m10s1bredr0 controller library (central, observer, 10 links); ble1m0s1bredr0 has neither role; Bluetooth costs 236,096 B of image, 32 KB of RAM the linker reserves and 52.6 KB of boot heap, and the controller cannot be shut down once started; the stack starts on first use or at boot when pairings are saved"
BLE-003: "Against the K950, bt_gatt_discover by service UUID (0x1812) and descriptor discovery by UUID (0x2902) found nothing, while unfiltered discovery found the same attributes; discover everything in range and filter in the callback"
BLE-004: "With BFLB_BLE_PATCH_NOTIFY_WRITE_CCC_RSP on, the SDK calls a subscription's notify callback with data NULL when the CCC write enabling it succeeds, the same signal gatt.h documents as the subscription being removed; a NULL report there is not an unsubscribe"
BLE-005: "Issuing many ATT requests at once from host callbacks blocks the host on ATT TX buffer allocation for ever; issue one request at a time, each from the previous one's completion (hid_setup_next() in tang_ble.c)"
BLE-006: "Logitech K950 in Bluetooth mode is HID over GATT on BLE: a random address that changes each time it enters pairing mode, Just Works pairing, boot protocol supported (Boot Keyboard Input value handle 0x23, CCC 0x24, Protocol Mode 0x4D) and the standard 8-byte boot report"
BLE-007: "Only Bluetooth LE HID over GATT devices can be used: the SDK's Classic profiles are A2DP, AVRCP, HFP hands-free, RFCOMM and SDP, with no HID, so a Classic-only device such as the Rii K06 (Bluetooth 3.0) cannot connect"
BLE-008: "A BLE keyboard notifies only when its key state changes, with no heartbeat, so it counts as live while connected and is zeroed on disconnect; its reports bypass the core, so left-alt pointer mode is applied in firmware by tang_key_pointer()"
BLE-009: "Logitech M750 over BLE: random address that increases by one per pairing, Just Works, Boot Mouse Input at value handle 0x23 (CCC 0x24), Protocol Mode 0x35; its boot report is 4 bytes with the wheel in byte 3, and its 7-byte report-protocol report is 16-bit buttons, 12-bit X and Y packed in 3 bytes, wheel and pan"
BLE-010: "The SDK host keeps a bonded peer's GATT subscriptions on disconnect unless BT_GATT_SUBSCRIBE_FLAG_VOLATILE is set, and re-sends them on reconnect; reusing those params structs for a new subscription rewrites a node still in its list and returns -EALREADY"
BLE-011: "This SDK's config.h defines CONFIG_BT_WHITELIST, which compiles out bt_le_set_auto_conn and the host's reconnect-after-disconnect; reconnection is one whitelist initiator (bt_conn_create_auto_le) that connects the first whitelisted device to advertise and stops, cannot run beside an explicit scan or connect, and needs the whitelist unchanged while it runs"
BLE-012: "Pairings are kept on the SD card in /sd/root/ble/bonds.bin (keys unencrypted) and restored at boot; a failed first re-encryption (level 1, error 8, then HCI 0x3E) is retried by the next connection and must not drop the key; a device that forgot its key gives error 2, on which the key is dropped, or a silent 'requesting encryption' then 0x3E, which only forget clears"
BLE-014: "Starting the Bluetooth stack takes about 15.8 KB of heap and a connection about 700 B (heap_3, read with kfree_size(); no lowest-ever counter); with both devices connected about 64.7 KB stays free (largest block 40.5 KB) across cartridge loads and desktop sessions, and reconnects do not leak"
BLE-015: "Xbox Wireless Controller: BLE HID only, public address, PnP 045E:0B13; fast blink is pairing mode; pairs LE Secure Connections Just Works wanting the host's IdKey; prefers 7.5-15 ms interval; sends reports unsolicited from link start; HID service 0x0016-0x0023 (input 0x001E, CCC 0x001F); vendor service 00000001-5F60-4C4F-9C83-A7953298D40D; works on BlueZ"
BLE-016: "Xbox Wireless Controller input Report ID 1 is 16 bytes over GATT: four 16-bit axes 0-65535, Brake and Accelerator 10-bit 0-1023 padded to 16, a 4-bit hat padded to a byte, fifteen buttons padded to 16 and one Consumer bit; its 283-byte Report Map must be read with offsets, 22 bytes at a time"
BLE-017: "Xbox Wireless Controller on this host: closed as unsupported on this board. No report has ever arrived; with this board's host traffic matching BlueZ's packet for packet it still stops answering as pairing completes, and the newer controller library in Bouffalo SDK 2.3.36 behaves identically, so the fault is the BL616's binary controller library rather than the host, the SDK version or the radio path"
BLE-018: "2.3.36's bt_conn_unref ends with BT_ASSERT(old > 0) where the fork was only atomic_dec, and bt_conn_create_le() returns a caller reference only for a newly created connection; releasing one that was not taken asserts in recv_thread and resets the board"
TOOL-001: "CONFIG_CHERRYUSB_HOST is required for the CDC to enumerate with FreeRTOS enabled; CONFIG_NEWLIB stops enumeration"
TOOL-002: "The Gowin programmer's accepted IDCODEs: GW5A-25 0x0001281b, GW5AT-60 0x0001481b, GWAST-138 0x0001081b, GW5AT-138 0x0001181b, GW2A-18 0x0000081b"
TOOL-003: "Upstream Bouffalo SDK 2.3.36 (8a3df34c), the fork 7f44f9e kept for rollback; Xuantie V2.6.1 GCC 10.2.0 from Bouffalo's toolchain release archive (its old git repository was rewritten 2026-10-12), oss-cad-suite 20261008 and xPack GCC 15.2.0-1, all three pinned by archive digest in deps.conf; desktop.bin from yosys 0.69+260, nextpnr at 8be8657b and apicula at 04bc3c5, at the script's default seed 3; Gowin EDA 1.9.11.03 for the NES and menu cores"
TOOL-004: "A Gowin bitstream names its device: nestang's console138k project is GW5AST-138B, and build.tcl generates the project from the device name rather than checking one in"
TOOL-005: "The USB-enumeration bisection is kept as proj.min.conf, proj.nonewlib.conf and proj.rtos.conf plus ref/, selected by TINYTANG_MIN, TINYTANG_RTOS, TINYTANG_NONEWLIB, TINYTANG_REF and TINYTANG_USB_ONLY; the config files reproduce it only on the 7f44f9e fork"
TOOL-006: "tools/ holds tinytang_flash.py (reflash over CDC), tinytang_put.py (file onto the card), tinytang_run.py (run a shell command) and their shared guard tinytang_console.py, all needing pyserial, plus ble_trace_to_btsnoop.py for bletrace dumps (`bletrace clear` allocates the 16 KB buffer, `bletrace free` returns it); one command's console output is cut after about 4.5 KB, so long dumps are fetched in ranges"
TOOL-007: "ports/bl616/tang_jtag_programmer.c is nand2mario's Apache-2.0 Gowin GPIO JTAG programmer from Tang-Control's fpga/programmer.cpp, based on openFPGALoader, vendored unmodified apart from its include list"
TOOL-010: "A carried patch stops being recognised once a later cycle edits the lines it added: the reverse check wants those lines present verbatim and the forward check wants them absent, so scripts/apply-nestang-patches.sh presumes the series applied in a tree with local changes, and the guarantee is the fresh-clone reconstruction test rather than the applier's check"
TOOL-011: "The console's input is exclusive, so the host tools ask the board before sending: the probe ESC [ ? 7 7 n is taken out of the USB input by the firmware, unseen by the shell or the desktop, and answered ESC [ ? 7 7 ; 1 n at the shell prompt, 2 with the desktop running, and not at all while a command runs; tools/tinytang_console.py sends nothing unless the answer is 1; a raw upload into the desktop once wrecked the card's FAT (recover with fsck.vfat)"
TOOL-012: "One-wire debugging reaches the FPGA only: JTAG on FT2232 interface 0 (openFPGALoader -c ft2232; --detect reads 0x0001081b and changes nothing; an SRAM load must be .fs, not .bin) and the FPGA UART on interface 1 (/dev/ttyUSB1, 2 Mbaud, the same 0xAA frames as the BL616 link); no shell, desktop, boot script or TinyTang tool runs, and reaching it takes a cold power-up that also restarts the FPGA, so it cannot inspect a state reached under TinyDesk"
TOOL-013: "A core can be developed entirely over one-wire: SRAM-load its .fs over JTAG with Tang-Phosphor's scripts/flash-otg.sh (17 s for the merged Phosphor image), then identify, peek, poke and stream to it on /dev/ttyUSB1 at 2 Mbaud with tools/fpga_uart.py and scripts/play_stream.py; an MP3 played through the AE350 this way gave 441000 samples, 0 underruns, 44100 Hz"
TOOL-014: "A third debug path: a Raspberry Pi Pico 2 CMSIS-DAP probe (2e8a:000c) on the module's U1201 header is its own USB device on the host, set up and validated by the user; Tang-Phosphor's scripts/flash-pico.sh drives it with openFPGALoader -c cmsisdap at 2 MHz or less (a full .fs takes 10-20 min), its real use is GAO and independent JTAG checks, and it must be idle during a tangload"
TOOL-015: "With FF_FS_REENTRANT 1 the SDK's FatFS R0.15 takes a FreeRTOS mutex (xSemaphoreTake with FF_FS_TIMEOUT ticks) on entry to every file call and gives it on exit, and FreeRTOS forbids API calls inside taskENTER_CRITICAL, so no f_* call may run inside a critical section; the JTAG programmer's did and failed tangload under TinyDesk"
TOOL-016: "Linked out of the SDK under their own licences: FreeRTOS V10.4.6 (MIT, (C) 2021 Amazon.com), CherryUSB (Apache-2.0), FatFs R0.15 w/patch3 (ChaN), and the Bluetooth stack: the Zephyr-derived host (Apache-2.0; Intel, Nordic, Wind River, Oticon, Chettimada), TinyCrypt (BSD-3-Clause, Intel 2017), micro-ecc (BSD-2-Clause, Kenneth MacKay 2014) and ctr_prng (BSD-2-Clause, Chris Morrison 2016), whose binary-form notices are reproduced in LICENSES/, plus the binary-only RivieraWaves-based controller and PHY libraries under the SDK's Apache-2.0 alone; LVGL, TJpgDec, mbedTLS, littlefs and the codecs are not linked"
TOOL-017: "Open-flow HDMI works: the apicula fork at 1c2b6cb+ writes the OSER10 aux half, HWL, lane-0 FCLKSEL0/3 and ODDR LSROMUX_0 like Gowin, and at 93761f4+ its io2hclk map splits each long side at the block midpoint as the vendor does, so the build adds no fuses; the clock network was never the fault. A timing-clean placement can still load with a dead clk (desktop ABI 1.2 seeds 6 and 16, whose clk took SPINE17 <- CBRIDGEOUT_TOP2, both taken to the board), so a changed netlist needs a seed sweep and a board test, and step 2d refuses a clk route through SPINE17; seed 3 of desktop ABI 1.3 (24a11e81a46c) is the build of record"
TOOL-018: "The `help` command list renders each line as printf(\"  %-10s %s\\n\", name, help), so a command's help text must fit TERMINAL_COLS - 13 characters -- 67 at the 80 columns the USB console and the desktop terminal use -- and tools/check_help_width.py gates it, reading the registered tables in ports/ and both vendored projects and failing with every overflowing entry named"
TOOL-019: "The keyboard link translates HID usages into the ANSI bytes TinyDesk's input parser decodes (ports/bl616/tang_key.c): F1-F11, usages 0x3a-0x44, are sent as ESC [ 11~ through ESC [ 23~, which input.c's tilde_key() maps to TD_KEY_F1-F11, while F12 (usage 0x45) is TANG_KEY_USAGE_TOGGLE -- reserved for the desktop/core screen swap and skipped before translation, so no layer ever sees it -- and the arrow and editing keys already used the same CSI forms"
TOOL-020: "Output printed before tangload has no raster to appear on, because the board draws nothing until a core is programmed, and osd desk on mirrors the console from that point forward rather than replaying it, so boot narration the user must read has to come after the core is loaded -- a line placed before tangload is reachable only by someone already attached to the USB console"
TOOL-021: "On the GW5AST-138C, SPINE17-21 and SPINE23 clock only while select ports in the logic tiles at rows 27/81, columns 92-94 hold VSS: the vendor ties SPINE19's (27,94)/(81,94) CLK0 and CLK1 to VSS, apicula asked for VCC, which the .fse cannot express, and the placer put logic there, so every open-flow clk through those spines was dead until apicula 04bc3c5 and nextpnr 8be8657b; an open-flow core with clk on SPINE18 now runs"
```

---

## 5. Records

Records are grouped in the order of the topic catalog, and numbered within each topic.

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

- record_id: DEV-005
  kind: DEVICE
  topic_id: DEV
  title: "The menu core is the console138k core with the NES machine not instantiated"
  status: VERIFIED
  verified_date: 2026-10-04
  statement: "third_party/patches/menu/0001-menu-core.patch builds the console138k bitstream with nestang's NES machine left out. The board file src/boards/console138k_menu.v defines MENU_CORE, build.tcl takes a third `menu` argument to select that board file and a second SDC, and nestang_top.sv carries its NES cluster -- the NES instance, sdram_nes, GameLoader, the autofire/joypad shift registers and the clkref/reset block -- inside `ifndef MENU_CORE`, with the four signals the survivors read (color, scanline, cycle, sample, all consumed by nes2hdmi) tied off in the else. iosys_bl616, both textdisp layers, nes2hdmi and the keyboard link are unchanged. The result answers CORE_ID 1, the same value the patched NES core answers, so the fpga probe reports one for both and cannot tell them apart."
  consequence: "The menu core exists so TinyDesk has a host of its own rather than riding on an emulator, and it is the base a later separate project grows from. It builds to impl/pnr/nestang_console138k_ds2_menu.bin. Its logic is roughly a fifth of the patched NES core's (2781 LUT against about 12400, BSRAM 13 of 340 against 43) but its bitstream is only about 2.5 percent smaller, because Gowin frames and routing rather than logic dominate the image -- so it is a much smaller design, not a meaningfully faster tangload. The NES core is untouched: the same series with the menu patch present still builds nestang_console138k_ds2.bin byte-for-byte."
  sources:
    - "This project's third_party/patches/menu/0001-menu-core.patch, third_party/patches/menu/README.md and tools/build_menu_core.sh"
    - "src/nestang_top.sv and build.tcl as the menu patch leaves them"
  verification: "Built with Gowin 1.9.11.03 for console138k: timing met, worst setup +3.443 ns and worst hold +0.144 ns. A fresh clone at the pinned commit, the common series, then the menu patch reproduces the working tree byte-for-byte. On this board tangload loaded it, fpga reported 'core 1 answering on UART1 at 2000000 baud', and osd desk on followed by desktop brought TinyDesk up with no emulator instantiated."

- record_id: DEV-006
  kind: DEVICE
  topic_id: DEV
  title: "The FPGA comes up running a stock core of its own at power-up, not empty"
  status: INFERRED
  verified_date: 2026-10-04
  statement: "With /root/scripts/boot.tdsh absent, so that nothing ran tangload after power-up, the board's FPGA was already running a core 80 seconds into uptime: fpga reported 'core 0 answering on UART1 at 2000000 baud', where every core this project builds answers 1, the desk layer was off, and the HDMI output showed a TangCore splash on more than one monitor and capture device. The same core-0 answer was seen once before, on 2026-10-04 in core-log cycle 12, with boot.tdsh renamed away. The FPGA therefore configures itself at power-up from configuration storage of its own, with a stock TangCore image, before the BL616 programs anything over JTAG."
  consequence: "The fabric is not empty at power-up; it looked empty only because boot.tdsh's tangload replaces the stock core within seconds, before most displays have locked. tangload programs SRAM and leaves that storage alone, which is why the stock core returns on every power cycle, and this project's cores live on the card, so every boot still reloads one: a cartridge is three commands in order, tangload to put the core in, fpga to confirm it answers, and nesload to stream the ROM. A core answering 0 is the sign that the boot chain did not run, which is how a missing /root/scripts/boot.tdsh shows up; restoring it is a tangput of scripts/boot.tdsh (TDESK-011 records how the desktop's Files app can delete it in two keystrokes). Core ID 0 is not in TangCore's core table (TCTL-006); DEV-007 identifies the stored image as most likely the factory splash core."
  sources:
    - "This board's console on 2026-10-04: fpga reporting core 0 with no tangload since power-up, and osd desk status reporting the layer off"
    - "The user's report of a TangCore splash at boot on several displays while boot.tdsh was missing"
    - "core-log cycle 12: the first core-0 observation"
    - "ports/bl616/tdsh_tang_flash.c: tangload calls fpga_program(), which loads SRAM ('Load SRAM' in its output)"
  verification: "Observed twice on this board, both times with boot.tdsh absent; restoring boot.tdsh returns the board to core 1 at boot. The mechanism -- which storage and by what configuration mode the FPGA loads at power-up -- is inferred from that behaviour and not traced to a schematic or a Gowin document, which is why this is INFERRED rather than VERIFIED. Revised 2026-10-11 (core-log entry 10): folded in the three-command cartridge sequence from the retired DEV-003, whose claim that the fabric comes up empty this record corrects. Revised 2026-10-13 (core-log entry 47): the card's folders moved under the shell's home, /root (cores, scripts, music, ae350, ble and demos, with ROMs in /root/data/nes), and the firmware's paths followed; the paths above are the new ones."

- record_id: DEV-007
  kind: BOARD
  topic_id: DEV
  title: "What the FPGA's configuration flash holds"
  status: VERIFIED
  verified_date: 2026-10-06
  statement: "Read back in one-wire mode with openFPGALoader 0.13.1 (-c ft2232 --detect -f, then --dump-flash --file-size 16777216), the FPGA's configuration flash is a 16 MB SPI part with JEDEC ID 0b 40 18 (XTX, 128 Mbit) and no block protection. It holds exactly one image, at offset 0: 1,084,246 bytes (MD5 d298b31689d1c4927b2f4b4cd49d7629) carrying the same Gowin preamble and device ID 0x0001081b as this project's cores, stored in the same byte order; everything above 0x109000 is erased. At about a quarter of an uncompressed GW5AST-138 core's size it is a compressed bitstream, and it matches none of the seven console138k cores in the TangCore 0.9 release, whole or block by block. Bitstreams carry no readable text, so the 'TangCore' the splash draws is not findable in it. Reaching the flash erased the FPGA's SRAM; the flash itself was only read."
  consequence: "This is the splash core DEV-006 saw at power-up. TangCore 0.9's installation (its flash_console138k.ini) writes only the BL616, at 0x0 and 0x40000, and TangCore's firmware has no code that writes the FPGA's flash, and the user does not recall writing it, so it was most likely programmed at the factory; a published Sipeed factory image or a word from Sipeed or nand2mario would settle it. TinyTang never writes this flash, so the splash returns at every power-up and in one-wire mode, which README fact 14 now says. The dump is kept with the BL616 backup (FLS-004)."
  sources:
    - "openFPGALoader 0.13.1 detect and dump on this board, 2026-10-06"
    - "TangCore 0.9 release (tangcore-0.9 (1).zip on the user's Desktop): cores/console138k/*.bin, firmware-bl616/flash_console138k.ini, installation.pdf"
    - "Tang-Control (a fork of TangCore's firmware-bl616): no FPGA-flash write path in fpga/, core/ or main.cpp"
  verification: "Dump and comparisons made on 2026-10-06 (core-log entry 39)."

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

- record_id: BRD-006
  kind: BOARD
  topic_id: BRD
  title: "One-wire or two-wire is chosen at a cold power-up by which input powers the board"
  status: INFERRED
  verified_date: 2026-10-05
  statement: "The two modes TCTL-005 names are two different programs on the BL616, and only one runs at a time. A cold power-up with the dedicated power input connected, with or without the OTG cable, runs this project's firmware, and the host sees the TinyTang CDC (ffff:5454). A cold power-up with the OTG cable alone comes up instead as SIPEED's 'USB Debugger' FT2232 (0403:6010, serial 2025041420), with interface 0 as /dev/ttyUSB0 and interface 1 as /dev/ttyUSB1, and the CDC absent. Removing the power cable from a board already running TinyTang changes nothing: the OTG cable keeps it powered (BRD-005), the CDC stays enumerated under the same device number, and the console keeps answering. The same FT2232 is what a soft reset reaches after tangflash (FLS-002)."
  consequence: "Switching between the modes is a full power cycle -- both cables out, then the power input for two-wire or the OTG cable alone for one-wire -- and it restarts the FPGA as well as the BL616, so the FPGA comes back on the core it loads by itself (DEV-006) whichever mode follows. In one-wire nothing of TinyTang runs: no shell, no boot.tdsh, so no menu core, no desktop layer, no desktop, no keyboard handling and no F12, and HDMI shows the TangCore splash. Which signal the vendor loader reads to make the choice is not known; the power input's VBUS is the obvious candidate and has not been tested."
  sources:
    - "Observed on this board, 2026-10-05: lsusb and udevadm on the host after each change; the TinyTang status probe (TOOL-011) before and after the power cable was removed"
    - "TCTL-005: Tang-Control's names for the modes"
  verification: "Each path observed once on 2026-10-05: power cable removed while running (CDC stayed as device 36, probe answered 1); cold start on the OTG cable alone (FT2232 enumerated, CDC absent, user saw the TangCore splash); cold start with both cables (CDC back as device 38, probe answered 1). Two-wire cold starts with both cables have come back as TinyTang throughout this project. Inferred rather than verified because the selection mechanism has no source and each one-wire start was seen once."

- record_id: BRD-007
  kind: BOARD
  topic_id: BRD
  title: "One-wire and two-wire are exclusive; they cannot be made to work together"
  status: INFERRED
  verified_date: 2026-10-05
  statement: "By the user's account, the Tang Console's modules are powered from one input or the other and never from both at once, so the board is in exactly one of the two modes BRD-006 describes, and combining them -- the CDC console and the FT2232 debugger at the same time -- is a power-delivery limit rather than a firmware one. The user recalls trying at length in the earlier projects to make one-wire and two-wire work together, without success."
  consequence: "Do not spend a cycle trying to run TinyTang and the FT2232 debugger together, or to reach the FPGA's JTAG or UART from the host while TinyTang runs. Pick the mode for the job: two-wire for anything involving TinyTang, TinyDesk or the card; one-wire for a core on its own (TOOL-013)."
  sources:
    - "User statement, 2026-10-05: 'we spend a lot of time trying to figure out how to get 1 and 2 wire to work together already. its a no go. Its a power deliver issue if i recall ... the actually devices/modules are powered 1 or the other, not both.'"
  verification: "Not instrumented, and no written record was found: Tang-Control's README and .ai files, Tang-Phosphor's and Tang-PSX's core-reference.md and core-log.md were searched for the power finding on 2026-10-05 and hold nothing on it. Consistent with BRD-006, where every observed state was one mode or the other."

- record_id: BRD-008
  kind: BOARD
  topic_id: BRD
  title: "The 40-pin headers are SDRAM connectors, not Raspberry Pi headers"
  status: SOURCED
  verified_date: 2026-10-06
  statement: "The Tang Console dock carries two 2x20 2.54 mm headers, one on each long edge. The schematic (docs/Tang_Mega_138K_Console_32001C__Schematics.pdf, sheet FPGA_EXT_CONN, Rev 1.3) names them J9 'SDRAM0 CONN.' and J10 'SDRAM1 CONN.' (with a note that the second can carry a GBA cartridge for a gamepad application): MiSTer-style SDRAM-module connectors whose pins 1-10, 13-28 and 31-40 go straight to FPGA I/O (SDRAM data, address and control, with two extra I/O on 29 and 30), with +5 V on pin 11 and ground on pin 12. A Raspberry Pi header instead has +5 V on pins 2 and 4, +3.3 V on 1 and 17, and ground on 6, 9, 14, 20, 25, 30, 34 and 39. One header holds the SDRAM module; the other is free."
  consequence: "A Raspberry Pi HAT fits the free header mechanically but must not be fitted to it: the HAT would take its supplies from FPGA I/O pins, its ground plane would short eight FPGA signals together, and the dock's +5 V on pin 11 would land on a HAT signal line, which on PiTFT HATs is a button that would then short 5 V to ground. A HAT can be used only through a pin-remapping adapter. The free header is otherwise usable as general FPGA I/O, with 5 V and ground on pins 11 and 12, provided the loaded core does not also claim those pins for SDRAM. This came up with the Adafruit 2.2-inch PiTFT HAT (ILI9340, SPI, 320x240), which the user decided not to pursue with an adapter; the dock's 40-pin FPC parallel RGB LCD connector (sheet LCD_DPI_24BIT) is the intended display path and has not been set up."
  sources:
    - "Sipeed schematic, sheet FPGA_EXT_CONN: J9 and J10, SMD_PIN_2x20_2.54mm (that sheet was `docs/Tang_Mega_138K_Console_32001C__Schematics.pdf`, removed from this repository on 2026-10-10; obtain a copy from Sipeed's share)"
    - "this project's own photograph of the dock's front edge, which showed the two headers (`images/circuit_boards/dock_front.jpg`, removed from this repository on 2026-10-10)"
    - "Adafruit, 2.2-inch PiTFT HAT guide: ILI9340 over the Pi's SPI with GPIO 25 and four buttons"
  verification: "Read from the schematic text and the board photograph on 2026-10-06; nothing was fitted or measured (core-log entry 41). The HAT's own pin assignments beyond those Adafruit's guide states were not checked against its schematic."

- record_id: BRD-009
  kind: BOARD
  topic_id: BRD
  title: "The BL616 has a BOOT button for its ROM bootloader, inside the case"
  status: VERIFIED
  verified_date: 2026-10-14
  statement: "The board's BL616 has a BOOT button that puts it into its ROM bootloader at power-up; it is reached by taking the console apart. Held at power-up, the BL616 presents its own USB CDC, 349b:6160 'Bouffalo CDC DEMO', on /dev/ttyACM0, with no FT2232 and no sudo needed. Bouffalo's BLFlashCommand (tools/bflb_tools/bouffalo_flash_cube in the SDK) then reads and writes any flash range: --read or --write with --start, --len and --file, after the --interface=uart --baudrate --port --chipname arguments the SDK's own flash rule passes (tools/bflb_tools/project_flash.build, BAUDRATE 2000000 by default). The SDK's standard make flash writes a complete image from 0x0, where Sipeed's loader lives (FLS-004)."
  consequence: "This is both the first install of TinyTang and the recovery path if a firmware image stops the board before its USB console comes up, so that tinytang_flash.py has nothing to talk to. Either writes only the application, the same image tinytang_flash.py installs, at 0x40000 with BLFlashCommand --write, and never uses make flash, which would replace Sipeed's loader and with it the one-wire and two-wire choice and the FT2232 debugger (BRD-006, FLS-004); the README's quick start gives that form. The full-flash backup of FLS-004 restores everything if the loader is ever damaged."
  sources:
    - "The user's account, 2026-10-11: the BOOT button exists, is inside the case, and was used to write firmware before"
    - "Bouffalo SDK tools/bflb_tools/bouffalo_flash_cube/BLFlashCommand-ubuntu --help (read, write, start, len, file); tools/bflb_tools/project_flash.build command_flash"
    - "core-log entries 22 to 25: ROM bootloader enumeration as 349b:6160, a 4 KiB --read at 0x40000 recorded as Tang-Console-138K-backups/rom_read_20261011_0x40000_len0x1000.bin, and --write of the application at 0x40000 verified by the device's SHA-256"
  verification: "Not exercised when first written: the user declined a recovery test as unnecessary, the console being awkward to open (core-log entry 21). Revised 2026-10-14 (core-log entry 63): status raised to VERIFIED, since core-log entries 22 to 25 used the path repeatedly on 2026-10-11 -- with BOOT held the BL616 enumerated as 349b:6160 on /dev/ttyACM0, a 4 KiB --read at 0x40000 came back byte-identical to the saved image, every --write of the application at 0x40000 was verified by the device's own SHA-256 (f8cb3455 for the restored 32299f4-dirty.17f81b2 image), and after a power cycle platform reported the written identity; chip UID e8ca50f49d69, flash c86016, 4 MiB. The full command lines those entries ran were not recorded, so the README's form is the SDK flash rule's arguments plus --write --start --file, not a line copied from a run; the record now also names this as the first-install path."

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
  consequence: "That assignment lands here: Tang-Control was retired into TinyTang, and the parser is ports/bl616/tang_ini.c, applied after every tangload by ports/bl616/phosphor/pmod_sockets.cpp (PMOD-006 has its behaviour and the qualified layouts). TinyTang follows the contract with one deliberate exception: flip is applied as declared for any module, without the flip-safety refusal, because the user chose to defer that check."
  sources:
    - "Tang-Phosphor, .ai/core-reference.md, the 'PMOD configuration file (tang.ini)' record"
    - "This project's ports/bl616/tang_ini.c and ports/bl616/phosphor/pmod_sockets.cpp"
  verification: "The contract was read from Tang-Phosphor's reference; this firmware's implementation of it is hardware-qualified in PMOD-006. Revised 2026-10-11 (core-log entry 10): the consequence said nothing here reads /tang.ini, which stopped being true on 2026-10-07."

- record_id: PMOD-004
  kind: EXTERNAL
  topic_id: PMOD
  title: "On the merged Phosphor core the socket control register is 0xc0, and both sockets power up released"
  status: VERIFIED
  verified_date: 2026-10-06
  statement: "In Tang-Phosphor's merged player core, the one TinyTang loads as phosphortang.bin, the socket control word is 0xc0: bit 0 holds the renderer, bits 4-7 are the PMOD0 personality, bits 8-11 the PMOD1 personality, bit 12 marks PMOD0 seated upside down and bit 13 PMOD1, and a read adds the renderer's frame selector at 18:16. Personalities are 0 none, 1 oledrgb, 2 vga J1, 3 vga J2 and 4 rotary encoder (PMOD-006 adds 5, I2S2). Both sockets power up released, which is that project's safe state, so nothing is driven until a host declares it, and the host is the only party that validates a declaration, because it is the only party that knows what the user wrote. Tang-Phosphor's bring-up core uses the same layout at register 0x10, with 0x14 as scratch and a power-on default of the panel on PMOD0."
  consequence: "TinyTang declares the sockets from /tang.ini after every tangload of core 0x50 (PMOD-006); scripts/phosphor.tdsh no longer declares them. Bit 0 must not be used for stream routing, which moved to 0xa8 (PHOS-011). The bring-up core's register 0x10 is not extended-protocol frame type 0x10 (EXTCTL-001), which is a transport opcode; the two must not be conflated."
  sources:
    - "Tang-Phosphor b7ec367: src/debug/debug_regs.sv, docs/debug-registers.md, and its .ai/core-reference.md records on the 0x10 socket control register and on socket control and stream routing in the merged player map"
    - "Tang-Phosphor src/pmod_mirror_core.sv: PERS_NONE through PERS_ENC"
  verification: "On this board on 2026-10-06 phosphor.tdsh declared 0x2410, 0xc0 read 0x2410 before, during and after two tracks, the panel frame counter at 0xd8 advanced throughout and the panel signature matched the source at 0x76491800; the user confirmed the OLED lit (core-log entry 45). Revised 2026-10-11 (core-log entry 10): folded in the bring-up core's 0x10 map from the retired PMOD-003, and replaced the consequence's 'until the parser exists' with the parser that now exists."

- record_id: PMOD-006
  kind: EXTERNAL
  topic_id: PMOD
  title: "How TinyTang applies /tang.ini, and the hardware-qualified PMOD declarations"
  status: VERIFIED
  verified_date: 2026-10-14
  statement: "Tang-Phosphor's socket word at 0xc0 selects personalities 0 none, 1 OLEDrgb, 2 PmodVGA J1, 3 PmodVGA J2, 4 encoder and, when compiled in, 5 I2S2 stereo PCM playback, which the merged core enables from register ABI 1.10 (0x04 reads 0x0001000a). The socket flip bit swaps the two signal rows; for a single-row Pmod ENC it also selects row 7-12 instead of row 1-6 without requiring the component side to face down. PmodVGA J1 carries red and blue, and J2 carries green and sync; the component-side-up seating exercised on this dock puts J1 on PMOD1 and J2 on PMOD0. Both sockets reset released, and the firmware must declare the fitted modules."
  consequence: "ports/bl616/tang_ini.c parses /tang.ini with no card or core access, so tools/tests/test_ini.sh checks it on the host, including that docs/tang.ini parses to 0x2410. It accepts none, oledrgb, encoder, i2s2, vga_j1 and vga_j2; the contract (PMOD-002) names only oledrgb, vga_j1 and vga_j2, and encoder and i2s2 are TinyTang's names for personalities 4 and 5, kept as TinyTang's own copy of Tang-Phosphor's numbering until the core reports its personalities. After every successful tangload, ports/bl616/phosphor/pmod_sockets.cpp asks the core for its ID and, for ID 0x50 at ABI 1.8 or later or ID 0x54 (TinyTang's desktop core) at ABI 1.0 or later, writes the word to 0xc0 and reads it back, masking the frame selector at 18:16; other cores, including the bring-up core whose 0x04 is not an ABI, get nothing. The firmware sends the desktop core the same word without filtering it, and the core accepts it only when each socket is 0 or 1 (none or OLEDrgb), one socket is 6 (i2c, since desktop ABI 1.3) and the other 0 or 1, or the pair is VGA J1 and J2 in either order (fpga/desktop/desktop_regs.sv supported_word, personality bitmap 0x0000004f at 0x08); any other word, one naming encoder or i2s2 included, is refused whole with status 6 and the sockets keep their previous state, released after a load, and the console prints 'tang.ini: the core did not take the socket declaration'. A missing file or entry releases the socket; an unknown module, a flip other than yes or no, or one VGA half without the other releases that socket with a line-numbered console warning; unknown keys, other sections and keys outside [tang] are warned about and ignored. Flip is applied as declared for any module, without a flip-safety check, as the user directed. tangini shows the declaration and the loaded core's sockets, and tangini apply resends it. Verified layouts are I2S2 normally seated on PMOD0 with PMOD1 released (0x0050), encoder face up on PMOD1 row 7-12 with PMOD0 released (0x2400), OLED normally seated on PMOD0 with that encoder (0x2410), and PmodVGA with PMOD0 vga_j2 and PMOD1 vga_j1, neither flipped (0x0230); on the desktop core, OLED normally seated on PMOD0 with PMOD1 none (0x0010), driving the OLED Terminal app's 24x16 display. The first VGA declaration, 0x0320, did not match that physical seating; module names must follow the actual headers."
  sources:
    - "Tang-Phosphor 7cf9ede: docs/debug-registers.md (0x04 ABI 1.10, 0x00c0 personality 5), src/pmod/pmod_slot.sv (row permutation), src/pmod/pmod_enc.sv (input pins and count), src/pmod/pmod_vga.sv (J1/J2), src/pmod_mirror_core.sv (personality registry, PERS_I2S2), docs/i2s2-bringup.md (I2S2) and .ai/core-reference.md (single-row encoder flip meaning)"
    - "Tang-PSX 61974ca: .ai/core-reference.md BRD-004 and BRD-005, including the verified J1-on-PMOD1 placement"
    - "TinyTang 42e9f9b: ports/bl616/tang_ini.c, ports/bl616/tang_ini.h, ports/bl616/phosphor/pmod_sockets.cpp, docs/tang.ini and tools/tests/test_ini.sh; firmware 969e6ff-dirty.71fe989"
    - "TinyTang 8ce6641: ports/bl616/phosphor/pmod_sockets.cpp s_socket_cores (0x54 at ABI 1.0), fpga/desktop/desktop_regs.sv supported_word, fpga/desktop/README.md (register ABI 1.1, OLED terminal)"
  verification: "The parser passes its host test under AddressSanitizer and UBSan, which failed when the VGA pairing rule and the bad-flip refusal were each removed. On this board on 2026-10-07, entries 58-60 exercised all four listed layouts through automatic core-load declaration and read-back. Entry 58 records a full 3:19 track at 44.1 kHz with zero underruns and user acceptance of I2S2 audio and the oscilloscope. Entry 59 records encoder counts of exactly four per detent in both directions, button and switch transitions, matching OLED/source signatures with advancing panel frames and user acceptance of the image, reapplication and unknown-module rejection. Entry 60 records no VGA picture at 0x0320, mirrored HDMI and CRT images accepted by the user at 0x0230 without moving the module, then a matching /tang.ini and automatic declaration after reload. Missing-file release was verified in entry 58. No hardware test of every invalid-flip, section or VGA-pair parser case is claimed. Revised 2026-10-11 (core-log entry 10): folded in the parser's behaviour from the retired PMOD-005. On 2026-10-10 a 14-load soak of desktop.bin and phosphortang.bin declared the sockets on every load after pmod_sockets.cpp's register reads gained a retry (core-log entry 2). Revised 2026-10-14 (core-log entry 66): added the desktop core, its accepted words and its whole-word refusal, and the OLED layout the user qualified on it, the release firmware dba104c-dirty.17f81b2 with /tang.ini declaring pmod0 = oledrgb, flip no, and pmod1 = none, the OLED Terminal app drawn on the panel and accepted by the user; the panel had been dark before that declaration, which is consistent with the refusal, though the earlier file's contents were not read back. Revised 2026-10-18 (core-log entry 92): the desktop core's accepted words include the i2c personality added in core-log entry 87, and its bitmap reads 0x0000004f; the I2C layout is PMOD-007. Revised 2026-10-18 (core-log entry 96): the desktop core's I2C bus is qualified beside the OLED in both arrangements, 0x0160 and 0x0610."

- record_id: PMOD-007
  kind: BOARD
  topic_id: PMOD
  title: "The desktop core's I2C bus is on socket pins 3/4 and 9/10 with no FPGA pull-up, so the module supplies the pull-ups"
  status: VERIFIED
  verified_date: 2026-10-18
  statement: "Digilent's I2C Pmod header carries SCL, SDA, GND and VCC on its pins 1 to 4 (repeated on 5 to 8), so a module seated with its VCC on the host's VCC puts SCL on host pin 3 and SDA on host pin 4 (and 9 and 10 on the other row). The Pmod RTCC (MCP79410) carries no I2C pull-up resistors, and Digilent states the bus will not work without them, suggesting 2.2 kOhm; a 2011 revision of its manual shows a different header order, so the schematic is the authority for that module. Adafruit's DS3231 breakout carries its own pull-ups and uses the STEMMA QT colours red 3.3 V, black GND, blue SDA, yellow SCL. The DS3231 answers at 7-bit address 0x68; its registers 0x00 to 0x06 are BCD seconds, minutes, hours, day, date, month and year, and bit 7 of 0x0F is OSF, set from power-up until software clears it."
  consequence: "fpga/desktop/desktop_pmod.sv drives the i2c personality (6) as open-drain on lanes 2 and 6 (SCL, pins 3 and 9) and lanes 3 and 7 (SDA, pins 4 and 10), lane n being Digilent pin n+1 as the OLED's CS on lane 0 and SCK on lane 3 prove, and reads SDA back from pin 4; the socket flip bit swaps SCL and SDA. Every PMOD pin is PULL_MODE=NONE in fpga/desktop/desktop.cst, so the bus works only with a module that brings pull-ups: with none, SDA reads low at every acknowledge, a scan of 0x08 to 0x77 reports 112 devices first at 0x08, and every read returns zeros, which is the signature to recognise. The I2C engine runs on sys_clk at 50 MHz (I2C_CLK_HZ), giving 100 kHz. The status word at 0x140 reports busy from the control clock the command is written in until the result is latched, and clears scan_done on every command, so a poller never takes the previous result; ports/bl616/tang_i2c.cpp treats a write or scan as done when busy falls and a read when its sequence number moves. Before core-log entry 92 the bus was on pins 1/7 and 2/8, which no I2C module uses."
  sources:
    - "Digilent Pmod RTCC reference (reference.digilentinc.com/pmod/pmod/rtcc): pins 1 and 5 SCL, 2 and 6 SDA; and the Digi-Key forum write-up 'Real-Time Clock MCP79410 Pmod Controller (VHDL)': no on-board pull-ups, 2.2 kOhm suggested; both read 2026-10-18 through a web search, the Digilent manual page itself refusing the fetch"
    - "This project's fpga/desktop/desktop_pmod.sv (i2c_enables, i2c_sda_raw, cmd_pending), fpga/desktop/i2c_master.sv, fpga/desktop/desktop_oled.sv lane_o, fpga/desktop/desktop.cst, ports/bl616/tang_i2c.cpp wait_idle, and tools/tests/sim/tb_desktop_pmod.sv"
  verification: "On this board on 2026-10-18 (core-log entry 92), with the Gowin validation core desktop-i2c-pins-gowin.bin (e72b8898) and /tang.ini declaring pmod0 i2c, pmod1 oledrgb (0x0160): a seated Pmod RTCC scanned 112 devices first at 0x08; the Adafruit DS3231 on a STEMMA QT cable to PMOD0 scanned one device at 0x68, read its running counter, took a written time and an OSF clear (0x0F = 08) and read both back, and kept time across a board power cycle. tb_desktop_pmod checks the pin map against a pulled-up bus with pin 1 held low and a device at 0x68, and failed on both the old pin map and the old status logic. Revised 2026-10-18 (core-log entry 96): on the desktop core of record, desktop.24a11e81a46c.bin, with the OLED seated on PMOD0 and the DS3231 on PMOD1 and /tang.ini declaring pmod0 oledrgb, pmod1 i2c (0x0610), the core read back 0x0610 at boot, the scan found 0x68, the RTC read a running time across a reboot, and the user confirmed the OLED."

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
  title: "The memory manager is mm.h over TLSF, with free, largest-block and low-water queries"
  status: VERIFIED
  verified_date: 2026-10-11
  statement: "In SDK 2.3.36 allocation goes through components/mm: mm.h's mem_manager_t g_mem_manager holds up to CONFIG_MM_HEAP_COUNT (8) mm_heap_t entries, each with a heap_id, name, start and end address, total_size and is_active flag, and kmalloc(size, flags), krealloc() and kfree() serve them. The backend is still TLSF (MEM_TLSF_EN, tlsf/tlsf_allocator.c). bsp/board/bl616dk/board.c registers two TLSF heaps, OCRAM from __HeapBase and WRAM from _heap_wifi_start. kfree_size(heap_id) and kmax_free_size(heap_id) return the free bytes and the largest free block, heap_id 0 meaning every active heap, and take the mm lock themselves. kmin_free_size() returns the lowest free figure reached, tracked on every allocation when CONFIG_MM_ENABLE_MIN_FREE_TRACKING is set, which defconfig does; kmin_free_size_reset() restarts it. mm_lock_save() is weak and defaults to bflb_irq_save(). The fork's mem.h, g_kmemheap and pfree_size() do not exist in 2.3.36."
  consequence: "ports/bl616/tang_heap.c's tang_heap_info() sums total_size over the active heaps under mm_lock_save() and reads kfree_size(0) and kmax_free_size(0) after releasing it. ports/bl616/td_desktop_bl616.c gives System Monitor kfree_size(0), the SDK's own kmin_free_size() and tang_heap_info()'s total; on the fork it kept a minimum of its own samples, while the SDK's low-water mark covers every allocation. `ble` (ports/bl616/tang_ble.c) prints free heap, the total and the largest block."
  sources:
    - "Bouffalo SDK 2.3.36, components/mm/mm.h: mm_heap_t, mem_manager_t g_mem_manager, kmalloc, krealloc, kfree_size, kmax_free_size, kmin_free_size, kmin_free_size_reset, mm_lock_save; components/mm/tlsf/tlsf_allocator.c"
    - "Bouffalo SDK 2.3.36, bsp/board/bl616dk/board.c lines 388-389: the OCRAM and WRAM heap registrations"
    - "This project's defconfig line 25 (CONFIG_MM_ENABLE_MIN_FREE_TRACKING=y), ports/bl616/tang_heap.c tang_heap_info(), ports/bl616/td_desktop_bl616.c lines 225-252, ports/bl616/tang_ble.c"
  verification: "Read from SDK 2.3.36 and this project on 2026-10-11. On the board, `ble` on 2.3.36 printed '79604 bytes free of 121720, largest block 29504' (core-log entry 27), so kfree_size(0), kmax_free_size(0) and the summed total return plausible values. System Monitor's readings, the low-water figure among them, have not been confirmed on screen. Revised 2026-10-11 (core-log entry 41): replaced the fork's mem.h and g_kmemheap API with 2.3.36's mm.h, its two heaps and its own low-water tracking."

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

- record_id: BL6-007
  kind: SOC
  topic_id: BL6
  title: "bflb_uart_putchar is CPU-bound per byte, so it cannot keep a fast UART busy"
  status: VERIFIED
  verified_date: 2026-10-05
  statement: "bflb_uart_putchar() in the Bouffalo SDK's lhal driver calls bflb_mtimer_get_time_ms() once before every byte to start its 100 ms full-FIFO timeout, then polls UART_FIFO_CONFIG_1's TX free count and writes UART_FIFO_WDATA. On this board that made each byte cost about 7.5 us of CPU, so a loop of putchar calls sent about 133 KB/s regardless of the configured rate -- below even 2 Mbaud's 200 KB/s line rate, and well under 5 Mbaud's 500 KB/s. Reading the free count (UART_TX_FIFO_CNT_MASK, shift 0) once and writing that many bytes, consulting the clock only while the FIFO is full, sends at the wire rate."
  consequence: "ports/bl616/tang_fpga_uart.c's uart_write fills the FIFO directly with the same 100 ms timeout, which every frame to the core goes through. It was found because a CD-quality stream to the Phosphor core (176 KB/s) underran for 2.2 s of a 4 s track; afterwards the same track streamed in real time with zero underruns, and nesload and the desk layer run at their full line rate too."
  sources:
    - "Bouffalo SDK, drivers/lhal/src/bflb_uart.c: bflb_uart_putchar()"
    - "Bouffalo SDK, drivers/lhal/include/hardware/uart_reg.h: UART_FIFO_CONFIG_1_OFFSET, UART_FIFO_WDATA_OFFSET, UART_TX_FIFO_CNT_MASK"
  verification: "Measured on this board on 2026-10-05 with timers around the send: 692 frames of 1036 bytes took 5364 ms through putchar at 5 Mbaud, and 1394 ms (about 2.0 ms a frame, the wire rate) through the FIFO fill, the stream then paced by the core's acknowledgements. Castlevania's ROM load and the desktop were re-tested on the new path and pass."

- record_id: BL6-008
  kind: TOOLCHAIN
  topic_id: BL6
  title: "The SDK's fatal paths end silently, so tang_crash.c takes them over"
  status: VERIFIED
  verified_date: 2026-10-11
  statement: "In Bouffalo SDK 2.3.36: exception_entry(uintptr_t *regs) (drivers/soc/bl616/std/startup/interrupt.c line 120) prints mcause, mepc and mtval with printf and, for anything but an environment call (causes 8 and 11), loops in while(1); it is not weak. vector.S calls it with a register frame, but the FreeRTOS trap handler (portASM.S line 454) calls it with a0 holding the masked mcause. vAssertCalled(), vApplicationStackOverflowHook() and vApplicationMallocFailedHook() in components/os/freertos/freertos_port.c are weak; they print and call abort(), where the fork's spun. bflb_device_get_by_name() asserts through LHAL_PARAM_ASSERT on a name it does not know, where the fork returned NULL. kmalloc() returns NULL on failure with no assert or spin: pvPortMalloc (heap_3) calls kmalloc(size, MM_FLAG_HEAP_OCRAM_0) and then the malloc-failed hook, and newlib's malloc (components/libc/newlib/port_memory.c line 75) calls kmalloc(size, MM_FLAG_ALIGN_CACHE). The Bluetooth controller library's platform_reset() is still defined in arch_main.c.obj of libbtblecontroller_bl616_ble1m10s1bredr0.a and called from ke_mem.c.obj on a controller-heap failure; on the fork it cleared MIE, stored its error code and, unless the code was 0xC3C3C3C3 or 0xA5A5A5A5, jumped to address 0. printf goes to a UART this board does not expose. On the fork, bflb_malloc and its siblings (components/mm/tlsf/bflb_tlsf.c) also spun with interrupts off on any failed allocation."
  consequence: "On this board each of these ends with no output; on the fork a crash, an assert, an out-of-memory and a controller allocation failure all looked like the same freeze, with HDMI stopped, Bluetooth dropped, USB still enumerated and the console silent. ports/bl616/tang_crash.c takes over exception_entry and platform_reset with the linker's --wrap (CMakeLists.txt line 142). It passes environment calls and the two benign platform_reset codes through, and records no callee register when the argument equals the masked mcause, so portASM.S's call is not read as a frame. It defines vAssertCalled, the stack-overflow hook, an empty malloc-failed hook and a tick hook that records watchdog starvation, so each records and resets (FLS-003). Its watchdog lookup uses BFLB_NAME_WDT ('watchdog0' in 2.3.36, 'watchdog' on the fork). A failed allocation is no longer fatal: it returns NULL and is counted (BL6-009)."
  sources:
    - "Bouffalo SDK 2.3.36: drivers/soc/bl616/std/startup/interrupt.c line 120 exception_entry(); drivers/soc/bl616/std/startup/vector.S line 271; components/os/freertos/portable/GCC/RISC-V/common/portASM.S line 454; components/os/freertos/freertos_port.c; components/libc/newlib/port_memory.c line 75; components/mm/mm.h kmalloc"
    - "Bouffalo SDK 2.3.36, libbtblecontroller_bl616_ble1m10s1bredr0.a symbols: platform_reset defined in arch_main.c.obj, referenced from ke_mem.c.obj"
    - "Bouffalo SDK 7f44f9e: components/mm/tlsf/bflb_tlsf.c TLSF_MALLOC_ASSERT; disassembly of platform_reset and btble_ke_malloc in that build's tinytang_bl616.elf, 2026-10-05"
    - "This project's ports/bl616/tang_crash.c, CMakeLists.txt lines 142-143, FreeRTOSConfig.h (configCHECK_FOR_STACK_OVERFLOW 2, configUSE_MALLOC_FAILED_HOOK 1)"
  verification: "On the fork, `crash test trap` was recorded with the right PC after the wrap, and three reproductions of the cartridge-script freeze with Bluetooth connected left no record with every other path instrumented, the TLSF signature (core-log entry 34 of the archived log). On 2.3.36 the recorder, with its masked-mcause guard, recorded the boot fault that the unknown watchdog name's assert caused (core-log entries 23, 26 and 27). The 2.3.36 hooks and allocator were read from source on 2026-10-11; platform_reset's behaviour there rests on the unchanged symbol and caller, not a fresh disassembly. Revised 2026-10-11 (core-log entry 41): moved the record from the fork to 2.3.36, where the hooks abort, allocation failure returns NULL, the trap handler passes mcause and an unknown device name asserts."

- record_id: BL6-009
  kind: TOOLCHAIN
  topic_id: BL6
  title: "Allocation failure returns NULL; the core loader leaked 4 KB per load"
  status: VERIFIED
  verified_date: 2026-10-11
  statement: "In SDK 2.3.36 kmalloc and krealloc return NULL on failure (BL6-008). The linker wraps them (CMakeLists.txt line 143) with the versions in ports/bl616/tang_heap.c, which call the SDK's and, when a non-zero request is refused, record the count, size, task and caller under bflb_irq_save; krealloc(NULL, n) is handled as kmalloc(n, MM_FLAG_HEAP_ANY) so its refusal is recorded once, and a krealloc its own heap refuses is retried in any heap and the block moved (BL6-010). The allocation, its locking and the heap accounting stay in the SDK. On the fork the same file replaced bflb_malloc, bflb_calloc, bflb_realloc and bflb_malloc_align, whose failure spun with interrupts off. vApplicationMallocFailedHook is empty. fpga_program() in tang_jtag_programmer.c allocated a 4096-byte fbuf_cached before choosing between its two paths, and only the disabled slow path used and freed it, so the compiled fast path leaked 4 KB on every tangload; it is now allocated only on the slow path. TinyDesk Shell asks for a 32 KB stack for each `tdsh run` script (TDSH_SCRIPT_TASK_STACK); CMakeLists.txt passes -DTDSH_SCRIPT_TASK_STACK=16384, and the port caps worker stacks at 16 KB (BL616_WORKER_STACK_MAX in tdsh_platform_bl616.c), the shell task's own size."
  consequence: "With the Bluetooth stack up, each core load cost 4 KB of a heap of about 125 KB until a load could not get even its 4 KB buffer, and a script's 32 KB stack failed sooner; under the fork's allocator either froze the board with interrupts off (BL6-008). Now the buffer is allocated only on the path that frees it, a refused allocation makes the command fail and is counted, and `ble` and `crash` report free heap, the largest free block and refusals. The cap leaves a cartridge script 5.6 KB of stack spare; an overflow would be recorded by the stack-overflow hook (BL6-008)."
  sources:
    - "Bouffalo SDK 2.3.36: components/mm/mm.h kmalloc, krealloc, MM_FLAG_HEAP_ANY"
    - "Bouffalo SDK 7f44f9e: components/mm/tlsf/bflb_tlsf.c and tlsf.h (tlsf_walk_pool, tlsf_get_pool)"
    - "This project's CMakeLists.txt lines 69 and 143, ports/bl616/tang_heap.c, ports/bl616/tang_jtag_programmer.c fpga_program() lines 1044-1086, ports/bl616/tdsh_platform_bl616.c lines 89 and 143"
    - "third_party/tinydesk-shell/include/tdsh.h TDSH_SCRIPT_TASK_STACK and src/core/tdsh_script.c"
  verification: "On 2026-10-05, on the fork, with both Bluetooth devices connected and the mouse moving, the build with the wraps and the cap loaded Castlevania; free heap then fell 4.1 KB per run and the third run's 4096-byte buffer was refused, the command failing without a freeze. With the leak fixed, seven runs, two of them after a desktop session, left free heap at 64,704 B and the largest block at 40,536 B every time, with the script using 10,728 of 16,384 bytes of stack (core-log entry 35 of the archived log). Revised 2026-10-11 (core-log entry 41): on 2.3.36 the wraps moved to kmalloc and krealloc and only record, and the stack size became a build definition; a refused allocation on 2.3.36 has not been observed on hardware, so that path rests on source. Revised 2026-10-12 (core-log entry 42): `crash` on 2.3.36 recorded refused allocations on hardware on 2026-10-11 (BL6-010), and the krealloc wrap now also moves a block to another heap."

- record_id: BL6-010
  kind: TOOLCHAIN
  topic_id: BL6
  title: "Two heaps: krealloc and task stacks could not use the WRAM heap"
  status: VERIFIED
  verified_date: 2026-10-12
  statement: "Bouffalo SDK 2.3.36's bl616dk board.c registers two TLSF heaps: OCRAM from __HeapBase to __HeapLimit (line 388) and WRAM at _heap_wifi_start, _heap_wifi_size 0x8000 (line 389), and MM_FLAG_HEAP_ANY tries OCRAM, PSRAM, then WRAM (any_alloc_order, line 41). krealloc (components/mm/mm.c line 631) finds the heap holding the pointer (mm_find_heap_by_addr, line 670) and reallocates only there, with no fallback to another heap. pvPortMalloc (components/os/freertos/portable/MemMang/heap_3.c line 66) allocates with MM_FLAG_HEAP_OCRAM_0, so every FreeRTOS task stack, a `tdsh run` script worker's 16 KB included, must come from OCRAM. newlib's realloc and TinyDesk Shell's tdsh_realloc both reach krealloc. A script's line table starts at 64 entries of 8 B and doubles (third_party/tinydesk-shell/src/core/tdsh_script.c line 189); for a script of more than 64 lines the 128-entry table plus tdsh's 32 B allocation header is a 1,056 B krealloc. ports/bl616/tang_heap.c now retries a refused krealloc with kmalloc(size, MM_FLAG_HEAP_ANY), copies min(kmalloc_size(ptr), size) bytes and frees the old block, recording the refusal only when that also fails. The HCI trace buffer in ports/bl616/tang_ble_trace.c was a static 16 KB array in .bss, which comes out of OCRAM; `bletrace clear` now allocates it, `bletrace free` returns it, and the monitor hook records nothing while no buffer exists, the pointer being published and withdrawn under bflb_irq_save. The linker also wraps pvPortMalloc (CMakeLists.txt line 146) with __wrap_pvPortMalloc in tang_heap.c, heap_3's version with MM_FLAG_HEAP_ANY: under vTaskSuspendAll it allocates from OCRAM first and WRAM after, records a refusal and calls the malloc-failed hook; vPortFree's kfree finds either heap by address. Its callers in this image are xTaskCreate, xQueueGenericCreate, xTimerCreate, usb_osal_malloc, usb_osal_timer_create and the Bluetooth controller's btblecontroller_malloc."
  consequence: "Free memory alone does not say an allocation will succeed: a refusal can leave the other heap untouched, and `crash` reports a largest free block across both heaps. scripts/phosphor.tdsh has 94 lines, so its line table grew to 128 entries and was refused while WRAM was free; the script then returned with no output and the Phosphor core was never loaded, which is how the visualizer appeared not to load. Shorter scripts such as boot.tdsh (72 lines) succeeded or failed with the heap's state. With the fallback the launcher runs. Removing the static trace buffer moved __HeapBase from 0x63010b18 to 0x6300cb18 and raised the reported heap from 121,720 B to 138,104 B. Launched from the desktop's menu with only the Terminal open, a script's 16,384 B worker stack was refused (pvPortMalloc, task tdsh) and the Terminal printed 'tdsh: failed to start script worker (-1)'; the largest free block was then 29,504 B, the idle WRAM heap, which the SDK's pvPortMalloc cannot use. The old SDK fork had one heap of 135,536 B, which is why the same launch worked on it (TDSH-008). With the pvPortMalloc wrap the launch starts. Any task or queue, the Bluetooth controller's allocations included, may now land in WRAM once OCRAM cannot serve it, where before it failed."
  sources:
    - "Bouffalo SDK 2.3.36: bsp/board/bl616dk/board.c lines 41, 388-389 and 461; components/mm/mm.c krealloc line 631 and kmalloc_size; components/os/freertos/portable/MemMang/heap_3.c line 66; components/libc/newlib/port_memory.c _realloc_r"
    - "third_party/tinydesk-shell/src/core/tdsh_script.c lines 184-189 and src/core/tdsh_core.c tdsh_realloc and tdsh_alloc_header_t"
    - "This project's ports/bl616/tang_heap.c __wrap_krealloc and __wrap_pvPortMalloc, CMakeLists.txt line 146, ports/bl616/tang_ble_trace.c, scripts/phosphor.tdsh; the BL616 ELF's __HeapBase, __HeapLimit, _heap_wifi_size and the callers of __wrap_pvPortMalloc in its disassembly"
  verification: "On 2026-10-11, on firmware 6715393-dirty.a597e26, `tdsh run /scripts/phosphor.tdsh` printed nothing, `phosphor peek 0x04` read the desktop core's ABI 0x00010001 and `crash` recorded one refused allocation, 1056 bytes for task tdsh_script. With the krealloc fallback (dfc13d1-dirty.07c32e8) the same command loaded the Phosphor core, peek 0x04 read ABI 0x0001000a and 0xac read 0xb, with no refusal recorded. On dfc13d1-dirty.ea4fc91, which adds the on-demand trace buffer, the launcher ran again over USB with no refusal, `bletrace clear` lowered free heap from 79,128 B to 62,736 B and `bletrace free` restored it, and on 2026-10-12 `phosphor play` played Fleetwood Mac - Landslide with 0 underruns while the user watched the oscilloscope visualizer on screen and reported it working. The desktop-menu refusal was read from `crash` (16,384 B, task tdsh, caller resolved to pvPortMalloc in heap_3.c) and the Terminal's free-heap line, 61,224 B free of 138,104 B, largest block 29,504 B. The 32 B header is the size of tdsh_alloc_header_t, a union with max_align_t, on this target. Revised 2026-10-12 (core-log entry 43): with the pvPortMalloc wrap, firmware b405c3a-dirty.547ec23 booted with no refusal recorded, and the user launched phosphor.tdsh from the desktop's menu and reported that it works; which heap the worker stack came from was not read."

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

- record_id: USB-007
  kind: USB
  topic_id: USB
  title: "A CDC console write waits on the host reading, so an attached but idle computer can stall the board"
  status: VERIFIED
  verified_date: 2026-10-05
  statement: "A bulk IN packet completes only when the host issues IN tokens for it, and a Linux host does that for a CDC ACM port only while a program has the tty open and is reading. Opening the port raises DTR, which CherryUSB reports through usbd_cdc_acm_set_dtr, and closing it lowers DTR. With the board enumerated and no program reading, console_flush() in ports/bl616/usb_cdc_bl616.c waited up to a million taskYIELD() calls per 512-byte packet, then cleared the busy flag itself and started the next packet over one the hardware still held."
  consequence: "Every writer to the console waited with it, and the desktop draws through the console, so with the cable attached TinyDesk on HDMI was laggy and unresponsive, and a large burst -- opening the Terminal window -- looked like a freeze. console_flush() now sends nothing unless DTR is up, waits at most 50 ms for a packet, and leaves an untaken packet pending, dropping further output until the host reads it, whose completion clears the way. Separately, while the desk layer shows the desktop its drawing goes only to the layer's mirror and not to USB, so a computer terminal sees the shell's transcript but not the window redraws; a consequence is that the desktop is no longer visible on USB, so a tool cannot detect it from its drawing (TOOL-011) and must go by the 'desktop: starting TinyDesk' and 'desktop: exited' lines, which still reach USB."
  sources:
    - "ports/bl616/usb_cdc_bl616.c: console_flush(), usbd_cdc_acm_bulk_in() and usbd_cdc_acm_set_dtr()"
    - "Linux cdc-acm: read URBs are submitted when the tty is opened and killed when it is closed"
  verification: "On this board on 2026-10-05 the user found TinyDesk on HDMI smooth with the cable attached and nothing reading the port, where before it had been unusable; a listen-only capture while the user dragged windows and ran ls /music in the Terminal held only the transcript -- the desktop's start line, the prompt, the command, its output and 'desktop: exited' -- with no cursor positioning at all, and the console answered after the desktop exited."

- record_id: USB-008
  kind: USB
  topic_id: USB
  title: "Starting the Bluetooth radio takes the BL616's USB device down; re-asserting its clock brings it back"
  status: VERIFIED
  verified_date: 2026-10-11
  statement: "With the radio's bring-up run from the shell, the board leaves USB entirely -- no CDC and no FT2232, so nothing the device stack could report is available -- while the processor keeps running and the system stays up.  A flash breadcrumb written at each step of ble_start_ex() lands, the last one included, and the crash recorder's watchdog task keeps being fed through the whole window (about 29 s of heartbeat in one run), so the radio start completes and the CPU is alive; only the USB device is gone.  Re-asserting the USB clock with GLB_Set_USB_CLK_From_WIFIPLL(1) and re-initialising the device makes the console come back and stay: it re-enumerates as a fresh ffff:5454 device with no power cycle."
  consequence: "The BL616 clocks its USB device from a divided output of the WiFi PLL, which the BSP's bsp/board/bl616dk/board.c sets up with GLB_Set_USB_CLK_From_WIFIPLL, and the radio's RF bring-up disturbs it.  ports/bl616/usb_cdc_bl616.c provides tdsh_bl616_console_restart(), which re-asserts the clock and re-initialises the device (re-registration is legal, USB-004), and ports/bl616/tang_ble.c calls it once the radio is up.  This is why the console's disappearance at radio start is neither a crash nor a reset, and leaves no record."
  sources:
    - "This project's ports/bl616/usb_cdc_bl616.c: tdsh_bl616_console_restart() and tdsh_bl616_console_init()"
    - "This project's ports/bl616/tang_ble.c: the call after the stack comes up, and the flash-crumb diagnostic that established it (removed in core-log entry 32)"
    - "core-log entries 27 to 30"
  verification: "Established on the board on 2026-10-11: the crumbs showed ble_start_ex completing every step while the heartbeat advanced, and the console re-enumerated as a new device after the restart, then survived a full blescan and two device pairings."

- record_id: FLS-002
  kind: FLASH
  topic_id: FLS
  title: "Application at 0x40000 up to 0xE0000 bytes, staged at 0x120000, and a soft reset lands in the vendor loader"
  status: VERIFIED
  verified_date: 2026-10-05
  statement: "The application occupies flash from 0x40000 with a maximum of 0xE0000 bytes (917,504), ending at 0x120000. A new image is staged in an erased region at 0x120000, which with the same maximum ends exactly at 0x200000, where the vendor's data record begins; it is verified against the file and only then copied into the application slot. The copy must run from .tcm_code with interrupts disabled, because once the first application sector is erased no instruction may be fetched from the application's XIP flash. The boot header carries magic at 0x00, 0x08 and 0x64, a CRC-32 of the first 252 bytes at 0xFC, and a body length at 0x84 counting bytes after the 4 KiB header region. A soft reset lands in the vendor loader. A read-back of the whole 4 MiB flash taken on 2026-09-26 holds data only at 0x0-0x1C000 (the vendor loader), 0x40000-0x76000 (the application then installed) and 0x200000-0x201000 (the vendor data record), and is erased everywhere else, so 0x76000-0x200000 held nothing to preserve. An earlier 0x80000 maximum, with staging at 0x100000, was Tang-Control's choice of layout (utils/firmware_image.h, whose own static assertions require only that the slot and the staging area fit below 0x200000), and the vendor loader does not enforce it: it booted images of 582,256 and 594,400 bytes, whose header body lengths exceed 0x80000."
  consequence: "tangflash validates the header and the file's length against it before erasing anything, and the user power-cycles afterwards; the vendor loader below 0x40000 is never touched. ports/bl616/tdsh_tang_flash.c sets FW_APP_MAX_SIZE 0xE0000 and FW_STAGING_BASE 0x120000, with static assertions that the slot stays below the staging area and the staging area below 0x200000, and tools/tinytang_flash.py's FW_APP_MAX matches. A board still running a build from before this change refuses an image over 0x80000 itself, so moving such a board across takes two installs: first an image under 0x80000 built with the new limits, then the large one. Bluetooth LE alone took the image past the old limit (BLE-002), and the new slot leaves about 323 KB of room above a 594,400-byte image."
  sources:
    - "Full flash read-back /home/vash/Desktop/tang-console138k-bl616-backup-20260926.bin (4,194,304 bytes), mapped in 4 KiB sectors on 2026-10-05"
    - "Tang-Control, utils/firmware_update.cpp: FW_APP_BASE, FW_STAGING_BASE, the sector helpers and the erase/write/verify loops"
    - "Tang-Control, utils/firmware_image.h: the flash-layout comment and the static assertions on the application and staging regions"
    - "This project's ports/bl616/tdsh_tang_flash.c: FW_APP_MAX_SIZE, FW_STAGING_BASE, FW_VENDOR_DATA, check_boot_header() and the static assertions"
  verification: "tangflash ran roughly a dozen times on 2026-10-03 under the old layout, each followed by a power cycle and a working console. On 2026-10-05 a non-Bluetooth image built with the new limits was installed by the old tangflash and booted, then the 582,256-byte Bluetooth image was installed by the new one and booted after a power cycle, and later 594,400-byte builds (the last cb9e7d4-dirty.aee808e) installed and booted the same way, each confirmed by platform. Revised 2026-10-11 (core-log entry 10): folded in the header layout, the staging procedure and the .tcm_code rule from the retired FLS-001, so this record stands alone."

- record_id: FLS-003
  kind: FLASH
  topic_id: FLS
  title: "A watchdog reset lands in the vendor loader; crash records live at 0x3FF000"
  status: VERIFIED
  verified_date: 2026-10-05
  statement: "The BL616's watchdog (bflb_wdg, 32 kHz clock divided by 32, reset mode) resets the chip into the vendor loader, which, as after a software reset (FLS-002), starts SIPEED's FT2232 debugger instead of the application, so the board re-enumerates as /dev/ttyUSB0 and /dev/ttyUSB1 and needs a power cycle to run TinyTang again; the power cycle leaves RAM unusable, so a .noinit record does not survive. The reset recorder register GLB_RESET_STS0 reads 0x7D after a power-up; its bits are active low (wdt_rst_n is bit 3). The last 4 KB sector of the 4 MiB flash, 0x3FF000, lies in the region the full read-back of FLS-002 found erased, above the application slot, the staging area and the vendor data record at 0x200000."
  consequence: "ports/bl616/tang_crash.c writes its record and the last 32 (task, PC) samples to 0x3FF000 with interrupts off before resetting, and the next boot prints them, appends them to /sd/crash.log and erases the sector. tangflash calls tang_crash_suspend() before its commit, which runs with interrupts off for longer than the watchdog's 4 s."
  sources:
    - "Bouffalo SDK 7f44f9e: drivers/lhal/include/bflb_wdg.h, examples/peripherals/wdg/wdg_reset; drivers/soc/bl616/std/src/bl616_glb.c GLB_Get_Reset_Reason()"
    - "FLS-002's full flash read-back of 2026-09-26"
  verification: "On 2026-10-05 crash test trap, crash test spin and crash test hang each reset the board to the FT2232; after a power cycle the trap and spin records were read back from flash with the right task and PC, and the hang, which runs no code before the reset, left none, as designed. A later tangflash run with the recorder installed committed and booted (core-log entry 34)."

- record_id: FLS-004
  kind: FLASH
  topic_id: FLS
  title: "The BL616's loader is a Sipeed build, and where the full backups are"
  status: VERIFIED
  verified_date: 2026-10-06
  statement: "TangCore 0.9's installation writes bl616_fpga_partner_console138k.bin (88,672 bytes, MD5 4dbe9bb117d2dd186553340ab6ade45c) to the BL616 at 0x0 and its firmware at 0x40000. The board's own loader at 0x0, from the full BL616 read-back of 2026-09-26 (FLS-002), is not that file: 60,741 of its first 88,672 bytes differ, from 0x84 onwards, and the file's body after 0x1000 is found nowhere in the backup. It is therefore another build of Sipeed's FPGA-partner loader, most likely the one fitted at the factory. Copies of both full flashes are kept outside every repository in /run/media/vash/GIT/Tang-Console-138K-backups: tang-console138k-bl616-backup-20260926.bin (4,194,304 bytes, SHA-256 d0bce6135adf15cf9493a7c3f5d4b2310b33746f08151da2876019730d85e217) and tang-console138k-fpga-flash-backup-20261006.bin (16,777,216 bytes, SHA-256 2e4688e65ae85766d71b44817854fd9bbaffc58f7fb8557f0d103db1f7b5baaa), with SHA256SUMS.txt."
  consequence: "The loader that picks one-wire or two-wire at power-up (BRD-006) and that every warm reset lands in (FLS-003) is Sipeed's, not part of TangCore's package as released. With both backups the board can be returned to its state before TinyTang: the BL616 through Dev Cube and the FPGA's flash through Gowin Programmer or openFPGALoader -f."
  sources:
    - "TangCore 0.9 release: firmware-bl616/bl616_fpga_partner_console138k.bin and flash_console138k.ini"
    - "/home/vash/Desktop/tang-console138k-bl616-backup-20260926.bin and its copy on the GIT drive"
  verification: "Compared and copied on 2026-10-06; both copies were verified by SHA-256 against their sources (core-log entry 39)."

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
  consequence: "ports/bl616/tang_osd.c clips text at the row end and its menu uses column 0 for the selection marker, which is the only highlight the page offers. The logo occupies rows 25 and 26 across columns 11 to 20, so anything scrolling on the page must stop at 25 rows; the console mirror that did so, tang_osd_term.c, was retired in 318b23c when the desktop layer replaced it, and the menu core has no page at all."
  sources:
    - "nestang, src/iosys/textdisp.v: the module comment '32x28 text display in 8x8 font', the LOGO_X/LOGO_Y constants, and the is_cursor and logo comparisons in the pixel logic"
    - "nestang, src/assets/font.vh: 'localparam [7:0] FONT[0:127][0:7]'"
    - "nestang, src/iosys/iosys_bl616.v: the comment on command 0x05, 'display string from cursor', and the cursor_x < 32 test"
  verification: "The user confirmed the page, the menu marker and the blinking cursor on hardware. The glyph table was decoded and checked: 'A', '0', '/', '_' and '.' render as expected. Revised 2026-10-11 (core-log entry 10): the consequence cited ports/bl616/tang_osd_term.c, removed on 2026-10-04."

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
  verification: "Exercised on this board. scripts/boot-cart.tdsh has been run on its happy path and on its guard path (TDSH-004), and the black-screen-with-audio symptom was seen before the overlay clearing of PROT-005."

- record_id: PROT-008
  kind: PROTOCOL
  topic_id: PROT
  title: "Between a core loading and the desktop painting, the screen is the core's uninitialised text page"
  status: VERIFIED
  verified_date: 2026-10-04
  statement: "nes2hdmi's mixer takes the desktop layer only while the wide layer is enabled *and* the overlay is asserted; with the overlay on and the layer still off it falls to the overlay colour, which iosys draws from the legacy 32x28 text page (its textdisp instance). A core comes out of reset with the overlay already on (iosys_bl616.v: `reg overlay_reg = 1`, which is PROT-005), and that page's store, src/iosys/gowin_dpb_menu.v, carries no `initial` and no `readmem` at all, so the compositor is showing its power-on contents. The font is initialised -- textdisp.v says so in as many words -- which is why those contents render as recognisable glyphs rather than as noise."
  consequence: "Every tangload therefore shows the loaded core's own uninitialised page until something takes the screen. boot.tdsh and boot-cart.tdsh now issue `osd clear` after the fpga probe and before osd desk on; tang_osd_clear() blanks all 28 rows of 32 columns with spaces and asserts the overlay itself, so the window is blank rather than garbled. The window is longest on a core that never writes that page -- the menu core, which has no ROM menu -- and short-lived on nestang, where the firmware writes the menu into it."
  sources:
    - "src/nes2hdmi.sv: the rgb mixer's wide/overlay/picture priority"
    - "src/iosys/iosys_bl616.v: `reg overlay_reg = 1` and the textdisp instance driving overlay_color"
    - "src/iosys/gowin_dpb_menu.v: no initial block or $readmem"
    - "ports/bl616/tang_osd.c: tang_osd_clear()"
  verification: "Seen on this board on 2026-10-04 as stray characters between tangload and the desktop on the menu core, reported by the user and traced to the code above; cleared by adding osd clear, and the user confirmed the window is now blank."

- record_id: PROT-009
  kind: PROTOCOL
  topic_id: PROT
  title: "A pipelined layer must address the raster ahead by its own depth, or the previous line's blanking reaches the left edge"
  status: VERIFIED
  verified_date: 2026-10-04
  statement: "hdmi.sv's counters are documented as 'the pixel to be generated by the user in THIS clock and sent out in the NEXT clock': rgb is registered once in nes2hdmi.sv and video_data_period is registered once in hdmi.sv from the same cx/cy, so a one-register colour path is aligned. The counters are free-running over the whole frame -- at VIC 4, frame_width 1650 and frame_height 750 against a 1280x720 screen -- so the coordinate a deeper path was addressed with is, at the start of each line, a coordinate in the previous line's horizontal blanking. textdisp_wide as first carried was three registers deeper than the stock path and addressed its store with the live cx; its off-screen reads fall back to cell 0, so visible pixels 0-2 of every line were cell (0,0)'s glyph columns 7, 0 and 0 in that cell's colours."
  consequence: "The symptom is a column of 2-pixel dashes down the whole left edge whose colour and pattern follow whatever character is in the top-left cell -- green under the prompt's 'r', white under 'boot:' -- and which disappears when that cell is a space. patches/0007 addresses textdisp_wide four pixels ahead (its depth with the address now registered), wrapping into the next line and frame with frame_width/frame_height taken from hdmi, so color is the pixel for the coordinate being presented. Any future layer or stage added to this path must be counted in its look-ahead. tools/tb_textdisp_wide.sv streams a free-running 1650x750 raster and checks every visible pixel; a testbench that holds cx still between checks cannot see latency at all, which is how the first version passed."
  sources:
    - "nestang c2450818, src/hdmi2/hdmi.sv: the cx/cy counter comment, video_data_period <= cx < screen_width && cy < screen_height, frame_width 1650 for VIDEO_ID_CODE 4"
    - "nestang c2450818, src/nes2hdmi.sv: the single rgb register in the 'calc rgb value to hdmi' block"
    - "third_party/patches/0002-desktop-text-layer.patch: the original three-stage textdisp_wide addressed by cx"
    - "third_party/patches/0007-wide-layer-alignment.patch: the look-ahead and registered address"
  verification: "Seen on this board on 2026-10-04: dashes at the left edge in the top-left cell's colour, green with 'r' at cell (0,0), absent once help scrolled a line starting with a space into row 0, and white after a reboot with 'b' of 'boot:' there, matching that glyph's column-0 rows 0 and 6 in a photograph. The streamed testbench failed 445264 of 922881 visible pixels on the old module and passes all of them on the fixed one, and fails again with the look-ahead off by one. With 0007 deployed the user reports the left edge clean with 'r' at cell (0,0) and again after a reboot."

- record_id: PROT-010
  kind: PROTOCOL
  topic_id: PROT
  title: "The TinyNES AE350 boot ROM's load protocol, and the file a program can ask for"
  status: VERIFIED
  verified_date: 2026-10-13
  statement: "A TinyNES ae350_soc core (TinyNES firmware/include/tinynes.h and firmware/boot/load.c) does not speak the 0xAA frames: its A25 boot ROM prints TinyNES LOAD about once a second on UART1 at 2,000,000 baud and hunts for the magic TNL1 (0x314c4e54). The header is that magic, the load address, the length, the entry, the image's CRC-32 and the header's CRC-32 over the first 20 bytes, all little-endian, CRC-32 being zlib's. The image follows in 8 KiB blocks (the last shorter), each followed by its CRC-32. The ROM answers the header and each block with ACK 0x06, or NAK 0x15 and a letter (T timeout, H header CRC, A address, C block CRC, O overrun, I image CRC); a NAKed block is sent again, a NAKed header or image ends the load. After the last block it reads the image back from the SDRAM (0x40000000 to 0x41ffffff), answers once more, prints TinyNES RUN <entry> and jumps. A program built on TinyNES's runtime ends with TinyNES EXIT <code> and the ROM waits again. A program that wants a file prints TinyNES WANT <address> <bytes> and takes one more image the same way at that address, entry equal to the address, holding TNR1 (0x31524e54), the file's length and the file. A program that prints TinyNES LINK has turned the core's UART to link mode (TinyNES fpga/ae350_soc/desk_link.sv): from then on the core speaks the 0xAA frames, answers the core ID 0x01 with 0x4e, takes the overlay 0x08 and the desk layer's 0x13, 0x14 and 0x15, sends the joypad report 0x03 from its first Wii Classic Controller (D-pad in bits 4 to 7, the Classic's B as 0x100, its Y as 0x001, LB 0x400 for 260 ms after a tap of HOME) and sends the program's text as frames of type 0x20, a line or at most 128 bytes each; the ROM returns the UART to raw bytes when it restarts."
  consequence: "ports/bl616/ae350_load.c implements the sender and an ELF loader (PT_LOAD segments by physical address, gaps zero-filled, as TinyNES's tools/load_ae350.py), tested on the host by tools/tests/test_ae350_load.sh against a model of the receiver; ae350run in ports/bl616/tang_fpga_uart.c holds the UART lock, keeps the desk layer's readers off the receive ring, waits for TinyNES LOAD, sends the ELF and, on a WANT line, the file, prints the program's lines and returns its exit code at TinyNES EXIT, giving up after 300 s of silence. On TinyNES LINK it calls ae350_follow_link: it releases the lock and the pipe flag, so the desk task draws and reads the joypad report again, clears the overlay so the game is shown, re-arms the layer with tang_osd_desk_core_reloaded, then follows FPGA_RESP_AE350_TEXT (0x20) frames with tang_fpga_wait until TinyNES EXIT or a BAD line, and restores the overlay as it was before LINK. The tangload core-ID request a TinyNES core receives is ignored by the ROM's magic hunt. Lead, not finding: tang_fpga_core_id drains the receive ring, so a core-ID probe during a linked game (the OLED link's, or the fpga command) can lose a text frame, and a lost TinyNES EXIT leaves ae350run waiting out its 300 s."
  sources:
    - "TinyNES 710d52e: firmware/include/tinynes.h (TN_LOAD_*), firmware/boot/load.c, tools/load_ae350.py"
    - "TinyNES core-log entry 21: fpga/ae350_soc/desk_link.sv, exts_uart.sv, firmware/apps/nes_bench/main.cpp (TinyNES LINK)"
    - "This project's ports/bl616/ae350_load.c, ae350_load.h, tang_fpga_uart.c (cmd_ae350run) and tools/tests/tb_ae350_load.c"
  verification: "On this board on 2026-10-13 (core-log entry 48): ae350run loaded TinyNES's 625,292-byte player from the card and the player asked for and received Castlevania (USA) (Rev 1) from /root/data/nes, CRC 12a6cb14, and ran to TinyNES EXIT 0; the user then double-clicked .nes files on the desktop, played and returned. The BL616's ELF loader built an image identical to load_ae350.py's from the same file (CRC 4ad53a69). Revised 2026-10-13 (core-log entry 49): link mode added; on this board the user double-clicked a .nes file, toggled TinyDesk over the running game with HOME and back, moved the pointer with the D-pad, and quit to the desktop."

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
  consequence: "This is the naming every core image on the card follows, and why /root/cores/console138k/ is the directory that matters here. It also explains the core IDs the UART reports: nestang answers 1."
  sources:
    - "Tang-Control, core/cores.cpp: init_core_list() and find_core_for_board()"
  verification: "Confirmed against this board's own card: ls /cores/console138k lists nestang.bin, snestang.bin, gbatang.bin, mdtang.bin, smstang.bin, pctang.bin and phosphortang.bin among others, matching the table's names. Revised 2026-10-13 (core-log entry 47): the card's folders moved under the shell's home, /root (cores, scripts, music, ae350, ble and demos, with ROMs in /root/data/nes), and the firmware's paths followed; the paths above are the new ones."

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
  consequence: "This is the same header layout this project validates in FLS-002, now with the procedure and the verification step that surround it. The power-cycle requirement matches this project's experience exactly. Note that tangflash here does not patch a header: it expects an image already carrying a valid one."
  sources:
    - "Tang-Control, README.md: the inline header-patching script and the two-case flashing section"
    - "Tang-Control, scripts/tangctl.py: FW_APP_MAX_SIZE, FW_BOOT_HEADER_SIZE, FW_HEADER_REGION, check_boot_image(), read_status(), CRC_VERIFY_MIN_BYTES_PER_SECOND"
  verification: "Read from the README and the client. The header layout it depends on is confirmed independently by FLS-002."

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

- record_id: TDSH-002
  kind: EXTERNAL
  topic_id: TDSH
  title: "The shell's terminal output is a small, closed set of sequences"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "The line editor and builtins emit: carriage return, line feed, ESC [ 2 K to erase a line, ESC [ 2 J and ESC [ H to clear and home, ESC [ <n> C and ESC [ <n> D to move the cursor, and SGR colour sequences. The editor redraws a line by emitting CR, erase-line, then the prompt and buffer, then a cursor-left by the number of characters past the cursor. Nothing else is emitted."
  consequence: "Anything that renders the console needs to understand only that set. A renderer on a narrow page must treat CR as the start of the logical line rather than the row, because on a 32-column page a row-based CR leaves the tail of the previous render behind on every keystroke of a wrapped line. The renderer built on this, tang_osd_term.c, was retired in 318b23c; the desktop's Terminal window now draws the shell."
  sources:
    - "third_party/tinydesk-shell/src/core/tdsh_terminal.c: redraw(), and the ESC [ C / ESC [ D writes in the arrow-key handling"
    - "third_party/tinydesk-shell/src/core/tdsh_builtin.c: the clear builtin's ESC [ H ESC [ 2 J"
  verification: "A console mirror was built against this set and the user confirmed the result on the core's screen. Revised 2026-10-11 (core-log entry 10): the consequence cited ports/bl616/tang_osd_term.c, removed on 2026-10-04."

- record_id: TDSH-004
  kind: EXTERNAL
  topic_id: TDSH
  title: "TinyDesk Shell v0.1.5: its script language, terminal interface and board numbers"
  status: VERIFIED
  verified_date: 2026-10-06
  statement: "The shell is TinyDesk Shell v0.1.5 at commit 8456dd1d8c2fa88706c065edba483be67fcb3553, from github.com/tinydesk-project/tinydesk-shell, consumed as the third_party/tinydesk-shell submodule; its portable core is C11 and MIT licensed, and it also ships POSIX and Windows host ports. The script language, uScript 1.1.1, provides variables, quoting, command substitution, arithmetic, if/elseif/else/endif, while/endwhile, for/endfor, functions with return statuses, pipes up to eight stages, and redirection; tdsh run passes no arguments to a script, and a script inherits the caller's variables. The line editor redraws command lines that wrap over several rows. tdsh_terminal_io_t has two optional members: columns(), the terminal's width read once per line, and read_byte_timeout(), with which the editor may ask the terminal for its width (ESC[6n); with neither, the editor assumes 80 columns. tdsh_board_int() in src/core/tdsh_board.c reads board configuration numbers as decimal or 0x hexadecimal, never octal (08 is 8 and 010 is 10). The ESP-IDF port makes network mode and network autowifi root-only. The release carries STANDARDS.md (TDSH-005)."
  consequence: "scripts/boot-cart.tdsh is written in this language and needs nothing beyond if, $?, and two variables; this port's commands are registered through tdsh_register_commands(). This port fills columns() from the width TinyDesk's Terminal window reports, through tdsh_bl616_terminal_set_columns() from the bridge's start() and resize(), and returns 0 at the console so the editor keeps 80. It offers no read_byte_timeout(), because at the console both the layer and a host terminal would answer the width query. Without columns() the editor wrapped the 76-column window at 80, putting four characters on the row after every full one. The port uses neither board configuration nor the network commands, so v0.1.5 needed no change here."
  sources:
    - "tinydesk-project/tinydesk-shell v0.1.5 (8456dd1): CHANGELOG.md, include/tdsh.h, include/tdsh_terminal.h (tdsh_terminal_io_t), src/core/tdsh_terminal.c (terminal_columns()), src/core/tdsh_board.c"
    - "third_party/tinydesk-shell/docs/SCRIPTING.md: the language reference, the run forms, and the limits table"
  verification: "Built into this firmware and running since v0.1.3 (2026-10-03); boot-cart.tdsh has been run on its happy and guard paths. Under v0.1.4 on 2026-10-05 the user confirmed that long lines wrap correctly in the console and the Terminal window once columns() was filled, and that Tab completion works. v0.1.5 was diffed against v0.1.4 and built into this firmware on 2026-10-06; upstream ctest 8/8 passed in a clean clone; the user confirmed the console, the desktop, the Terminal, Files, F12, Castlevania and Phosphor (core-log entry 37). Revised 2026-10-11 (core-log entry 10): folded in the language from the retired TDSH-001 and the terminal interface from the retired TDSH-003."

- record_id: TDSH-005
  kind: EXTERNAL
  topic_id: TDSH
  title: "Upstream's standards for ports"
  status: SOURCED
  verified_date: 2026-10-06
  statement: "STANDARDS.md, added in TinyDesk Shell v0.1.5 for both projects, sets rules that apply to community ports. A port is described as 'Name, a port of TinyDesk Shell to platform' (TinyTang is the given example) and must not start its name with TinyDesk. It picks one 2-8 letter prefix for every name it owns -- C symbols, macros, port-only files, build switches, board keys -- recorded in the Community ports table of the TinyDesk Shell README, which lists TinyTang with the prefix 'not chosen yet' while the standard's own examples use tang. Files implementing an upstream interface are named tdsh_platform_<platform>.c, tdsh_fs_<platform>.c, td_hal_<platform>.c and tdsh_bridge_<platform>.c; a port defines no new td_ or tdsh_ symbols except entry points named tdsh_<platform>_<verb>. Upstream code is tracked as submodules at a release tag and never edited in place: a needed change is a patch in third_party/patches/ applied by an idempotent script, listed in THIRD_PARTY.md and matched by an upstream issue or pull request, and both repositories' host tests must pass at the pinned commit. Commands are lowercase without separators, one per subject with verbs as subcommands; upstream's command names and gpio, spi, i2c, uart, adc, pwm, usb, sd, eth, wifi and ota are reserved; a generic port command should carry the prefix. Scripts end in .tdsh and start with #!/bin/tdsh. The platform string is <platform>/<threading>, then ', <port name> <version or build id>'. Commit subjects read 'Area: what changed' without a full stop, and a pull request states what changed for users, the platforms it was built and run on, and that host tests pass without warnings. A system-wide boot script, displays drawn outside the terminal, and audio and media playback commands are listed as not standardised, with ports asked to open issues describing what they did. Limits in tdsh.h are listed as a known deviation because they are not wrapped in #ifndef."
  consequence: "This port already matches the platform string, the script header, the submodule rules and the tang prefix in practice. It does not yet match the file naming (td_bridge_bl616.c should be tdsh_bridge_bl616.c; fpga_frames.c, usb_cdc_bl616.c, tdsh_tang_flash.c, td_desktop_bl616.c and td_phosphor_app.cpp lack the prefix) or the command naming (blescan, blekbd and blemouse would be ble subcommands; crash and the usb* commands are generic); the standard asks for these when the files are next worked on. Declaring the tang prefix upstream, and an issue or pull request making TDSH_SCRIPT_TASK_STACK overridable (the 32 KB request behind BL6-009), are the obvious first contributions."
  sources:
    - "tinydesk-project/tinydesk-shell v0.1.5 (8456dd1): STANDARDS.md, README.md Community ports, CONTRIBUTING.md"
  verification: "Read on 2026-10-06; this port's script headers and platform string were checked against it, and the deviations listed come from its file and command names. Not yet discussed with the author."

- record_id: TDSH-008
  kind: EXTERNAL
  topic_id: TDSH
  title: "Script session limits: why desktop scripts failed, and the 48-variable session that fits"
  status: VERIFIED
  verified_date: 2026-10-06
  statement: "tdsh run starts each script with a tdsh_script_job_t holding a full copy of the session, then a worker task with TDSH_SCRIPT_TASK_STACK of stack, then the script's runtime. Upstream v0.1.5 fixes TDSH_MAX_VARS 64, TDSH_VAR_NAME_MAX 32, TDSH_VAR_VALUE_MAX 256 and TDSH_SCRIPT_TASK_STACK 32768, giving a 19,096 B session on a 32-bit target (18,496 B of it the variable table) and a 19,352 B job, none overridable by a build. With those, on this board with Bluetooth and two devices connected and the desktop open, the heap had 48,208 B free with a 40,536 B largest block and about 7.7 KB outside it, and scripts opened from the Files app returned with no output: a 1,056 B tdsh_realloc of the script's line table (src/core/tdsh_script.c:189) was refused. TinyTang's carried patch, third_party/patches/tdsh/0001-configurable-script-limits.patch, wraps the four limits in #ifndef, keeping the upstream defaults and keeping each script's own session and variable copy; the header requires the same definitions in every component that includes tdsh.h, because the variable limits change tdsh_session_t's layout, and buffers include their terminating NUL. TinyTang sets TDSH_MAX_VARS=48, TDSH_VAR_NAME_MAX=32, TDSH_VAR_VALUE_MAX=128 and TDSH_SCRIPT_TASK_STACK=16384: the 7728 B variable table gives an 8328 B target session and an 8584 B script job, saving 10768 B per session against upstream defaults (8344 B and 8600 B on a 64-bit host). The comprehensive upstream fixture exceeds 32 variables and passes at 48; the usable name and value lengths are 31 and 127 bytes."
  consequence: "The larger table is a port choice, not a standard limit or a guarantee for all future scripts. CMake applies the patch idempotently before compilation and sets the same definitions for the shell, desktop, bridge and all C/C++ port sources. A 32-variable configuration was tried first and dropped because the upstream fixture needs more. A later desktop launch while the Phosphor window was open failed at xTaskCreate's 16384 B stack allocation before tangload could stop playback; playback was not an intentional core-load interlock. The app reserved 24576 B for 256 names regardless of file count; allocating 96 B per actual eligible file, capped at 256, reduces the tested 19-file catalog to 1824 B and lets those launches fit. The active-worker readouts support a bounded 16384 B future metadata/artwork budget for this configuration, not a reservation or a qualified decoder; docs/phosphor-memory-budget.md states the scope. The findings are upstream issue tinydesk-project/tinydesk-shell #2, whose author approved guards, consistent overrides and unchanged variable copies; remove the carried patch when a pinned release supplies the guards. A separate upstream request for an error on script-loading allocation failure is pending."
  sources:
    - "third_party/tinydesk-shell @ 8456dd1 with third_party/patches/tdsh/0001-configurable-script-limits.patch: include/tdsh.h, README.md, src/core/tdsh_script.c, src/core/tdsh_core.c, tests/test_full_uscript.c and tests/scripts/test_full_uscript_1_1.tdsh"
    - "CMakeLists.txt, scripts/apply-tdsh-patches.sh, tools/tests/test_script_limits.sh and tb_script_limits.c; BL616 ELF s_session symbol"
    - "Bouffalo SDK components/os/freertos/tasks.c: xTaskCreate stack allocation, caller 0xa00416d6 in the failed builds"
    - "ports/bl616/phosphor/td_phosphor_app.cpp: launch, load_folder and on_close; tools/tests/test_phosphor_catalog.sh"
    - "tools/make_script_heap_probe.py and guarded two-wire ble/crash readouts on 2026-10-06"
    - "https://github.com/tinydesk-project/tinydesk-shell/issues/2"
  verification: "The upstream-default failure was measured on this board on 2026-10-06 and the session size confirmed at 19,112 B on a 64-bit host build (core-log entry 39). Firmware f3097e1-dirty.a912290 was flashed, confirmed by platform and accepted by the user after repeated desktop core swaps while music played with the Phosphor window open. No allocations were refused after those tests and no crash record was present. With both Bluetooth devices ready, the generated Castlevania launcher measured 24936 B free of 135536 B, largest block 19372 B, at all three checkpoints while its copied session, worker stack and interpreter were live; the user confirmed completion. These are sampled values, not an all-time minimum. Script stack use was 10728/16384 B. The catalog regression fails against the original scan and passes sizing, filtering, sorting, bounds, directory-change and error cleanup checks against the fix. Upstream host suites pass Shell 8/8 and TinyDesk 10/10 at defaults and at 48 variables/128-byte values; native suites keep the default 32 KB stack because Linux pthread runs with 16 KB crashed in the script tests, so the 16 KB request was checked separately and on hardware. Reconstruction of the carried patch matched the submodule byte for byte. Revised 2026-10-11 (core-log entry 10): folded in the failure from the retired TDSH-006 and the patch from the retired TDSH-007."

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

- record_id: TDESK-011
  kind: EXTERNAL
  topic_id: TDESK
  title: "TinyDesk's delete confirmations focus their Delete button"
  status: SOURCED
  verified_date: 2026-10-04
  statement: "files.c binds the Delete key to do_delete(), which opens td_msgbox(\"Confirm\", \"Delete <name>?\", \"Delete|Cancel\"), and the desktop's icon menu reaches the same dialog through ask_delete(). td_msgbox adds its buttons in the order given and a window's first focusable widget takes the focus (widgets.c: `if (focusable && !win->focus) win->focus = w`), so Delete is focused; a focused button activates on Enter or Space (widget_key), and delete_answer() removes the file for button 0 with no further step."
  consequence: "Delete followed by Enter or Space removes the selected file. On this board the keyboard's Delete arrives as ESC [ 3 ~ (tang_key.c), which the desktop reads as TD_KEY_DELETE. /root/scripts/boot.tdsh sorts first in its folder, and without it the board boots on the FPGA's own core (DEV-006); restoring it is a tangput of scripts/boot.tdsh. This is TinyDesk's design as written, recorded so it is not mistaken for a fault; the project is not raising it upstream. When boot.tdsh went missing on 2026-10-04 this dialog was first suspected, but the user had deleted the file themselves."
  sources:
    - "third_party/tinydesk @ 791cad8, apps/files.c: do_delete(), delete_answer(), the TD_KEY_DELETE case in the list's key handler"
    - "third_party/tinydesk @ 791cad8, apps/desktop.c: item_menu_chosen() and ask_delete()"
    - "third_party/tinydesk @ 791cad8, src/widgets.c: td_msgbox() button order, the first-focusable rule, widget_key() for TD_WT_BUTTON"
    - "ports/bl616/tang_key.c: usage 0x4C (Delete) sent as ESC [ 3 ~"
  verification: "Read from the source; not exercised on hardware. Revised 2026-10-11 (core-log entry 10): folded in the boot.tdsh consequence from the retired TDESK-010. Revised 2026-10-13 (core-log entry 47): the card's folders moved under the shell's home, /root (cores, scripts, music, ae350, ble and demos, with ROMs in /root/data/nes), and the firmware's paths followed; the paths above are the new ones."

- record_id: TDESK-013
  kind: EXTERNAL
  topic_id: TDESK
  title: "TinyDesk v0.1.5: its port surface, Terminal backend and shell pin"
  status: VERIFIED
  verified_date: 2026-10-06
  statement: "TinyDesk v0.1.5 is tag v0.1.5 at commit feaf84130f03594845e5e9284817dca785b4d878, from github.com/tinydesk-project/tinydesk; its main branch carries two later documentation commits (docs/VALIDATION.md) that the release does not include. It is a terminal desktop: it draws overlapping text-mode windows with ANSI escape sequences, reads the keyboard and mouse back from the terminal, has no display hardware, and its core is portable C11. Its whole port surface is td_hal_t, four function pointers - read_byte returning the next byte or -1 without blocking, write returning the number of bytes accepted, millis, and sleep_ms - plus a context pointer. The Terminal app's td_term_backend_t has start(cols, rows), read, write, resize(cols, rows), user and set_user, and the upstream shell bridges pass the window's width from start() and resize() to the line editor through columns() (TDSH-004). It pins tinydesk-shell at 8456dd1 (TDSH-004), the same revision as this project's submodule. Against v0.1.4 the code this port builds changes only in include/tinydesk/td.h: TD_VERSION becomes 0.1.5 and TD_REPO_URL and TD_SHELL_REPO_URL point at tinydesk-project. v0.1.4 cut text at character boundaries (td_utf8_copy, td_utf8_pad, td_utf8_skip), kept the file name when the Editor saves, and reformatted every source with clang-format 16; its README lists TinyTang as a community port."
  consequence: "The port in ports/bl616/td_desktop_bl616.c is mostly a table of pointers over console calls this project already had, and the bridge in ports/bl616/td_bridge_bl616.c forwards the window width from start() and resize(). There is no second copy of the shell to keep in step. v0.1.5 needed no change to this port beyond the pins and the submodule URLs, which .gitmodules gives as tinydesk-project. Diffs across releases should be read after formatting both sides with the release's .clang-format, since otherwise the v0.1.4 reformatting hides the real changes."
  sources:
    - "tinydesk-project/tinydesk v0.1.5 (feaf841): include/tinydesk/td.h, include/tinydesk/td_hal.h (td_hal_t and 'This is the only thing a port has to provide.'), apps/td_apps.h (td_term_backend_t), RELEASE_NOTES.md, third_party/tdsh at 8456dd1"
    - "third_party/tinydesk commit 5ad7c45: 'the Terminal window gives the shell its width'"
  verification: "Built and running on this board since 2026-10-03: the desktop draws, opens windows, and its Terminal runs the shell; under v0.1.4 on 2026-10-05 the user confirmed the desktop, the Terminal, Files, the Editor's save, F12, and Castlevania from the Terminal. v0.1.5 was diffed against v0.1.4 and built into this firmware on 2026-10-06; upstream ctest 10/10 passed in a clean recursive clone; the user confirmed the console, the desktop, the Terminal, Files, F12, Castlevania and Phosphor (core-log entry 37). Revised 2026-10-11 (core-log entry 10): folded in the port surface from the retired TDESK-001 and the Terminal backend from the retired TDESK-012."

- record_id: TDESK-014
  kind: EXTERNAL
  topic_id: TDESK
  title: "Terminal's retained started state requires a port restart after desktop exit"
  status: VERIFIED
  verified_date: 2026-10-06
  statement: "In TinyDesk v0.1.5, terminal.c retains its static s_started across td_shutdown(). td_terminal_set_backend() calls ensure_vt(), which calls backend->start only when s_started is false. td_shutdown() restores the renderer's terminal state without resetting those app statics. This supports a persistent backend when its window closes, but reinstalling the backend after a port has stopped its task does not restart that task."
  consequence: "TinyTang ends its inner shell when the whole desktop exits to restore exclusive console ownership. It must explicitly start that bridge on every desktop entry rather than rely on td_terminal_set_backend(). tang_td_terminal_start does this idempotently and installs the backend; a failed task creation clears the redirect and makes the desktop command report failure and return to the console. The physical keyboard's later lack of console input was separately accompanied by a Bluetooth disconnect; USB echo worked and the redirect was cleared."
  sources:
    - "tinydesk-project/tinydesk v0.1.5 (feaf841): apps/terminal.c ensure_vt and td_terminal_set_backend; src/td.c td_shutdown"
    - "ports/bl616/td_bridge_bl616.c: backend_start, tang_td_terminal_start and td_bridge_bl616_stop"
    - "ports/bl616/td_desktop_bl616.c: desktop_run and cmd_desktop; tools/tests/test_terminal_lifecycle.sh"
  verification: "A host probe of the real upstream Terminal called start once across two backend installations. The lifecycle regression linked against the real upstream Terminal and BL616 bridge fails with the original startup and passes three reentries, keyboard/output rings, idempotent installation, graceful cleanup, failed allocation and retry with the fix. The user accepted the flashed restart fix and subsequent build; listen-only USB capture of a later desktop entry contains the fresh shell greeting and the successful diagnostic launcher, followed by desktop exit and a usable guarded console."

- record_id: TDESK-015
  kind: EXTERNAL
  topic_id: TDESK
  title: "Settings' Network button and the network tray launch an app by name"
  status: VERIFIED
  verified_date: 2026-10-07
  statement: "In TinyDesk v0.1.5, apps are found by the name they register with td_app_register(), and td_app_launch() does nothing when no app has that name. apps/settings.c's Network... button (x 1, y 15, beside Date & time... at x 16) calls td_app_launch(\"Network\"), as its Software update... button calls td_app_launch(\"Software Update\"). src/wm.c's taskbar also launches \"Network\" from its network tray, but draw_tray_net() draws that tray only when td_sysinfo()->net supplies a status callback. A button is as wide as its caption plus four columns."
  consequence: "This port leaves apps/network.c out (TDESK-008) and supplies no td_net_ops_t, so the tray never appears and the Settings button was the only way in, silently doing nothing. third_party/patches/tinydesk/0001-settings-bluetooth-button.patch, applied by scripts/apply-tinydesk-patches.sh at configure, makes that button Bluetooth... calling td_app_launch(\"Bluetooth\"), and moves Date & time... to x 18 because the longer caption is 16 columns wide; ports/bl616/td_bluetooth_app.c registers the app under that name. The Software Update window (core-log entry 50) used the same by-name launch without a patch, since its name already matched."
  sources:
    - "tinydesk-project/tinydesk v0.1.5 (feaf841): apps/settings.c on_network and launch; src/wm.c td_app_launch, draw_tray_net and taskbar_click; src/widgets.c td_button"
    - "third_party/patches/tinydesk/0001-settings-bluetooth-button.patch; tools/tests/test_tinydesk_patches.sh"
  verification: "On this board on 2026-10-07 with firmware f1b66ab-dirty.6090e46 the user confirmed the Bluetooth... and Date & time... buttons side by side and the Bluetooth window opening from Settings (core-log entry 53); TinyDesk's host suite passes 10 of 10 with the patch applied."

- record_id: TDESK-016
  kind: EXTERNAL
  topic_id: TDESK
  title: "The Editor's fixed buffers and the limits this port sets"
  status: VERIFIED
  verified_date: 2026-10-07
  statement: "In TinyDesk v0.1.5, apps/editor.c allocates its whole text buffer, malloc(TD_EDITOR_MAX + 1), and its undo history, TD_EDITOR_UNDO bytes of undone text plus 128 undo records, when the Editor opens, whatever the file's size, and frees both when it closes. The defaults are TD_EDITOR_MAX 16384 and TD_EDITOR_UNDO 6144, about 25 KB, and both may be set from the build. A file longer than TD_EDITOR_MAX opens read-only with 'File too large: read-only'; an edit that would take the text past it is refused with 'The file is full'; a single change larger than TD_EDITOR_UNDO stays in the text but cannot be undone, and the oldest undo steps are dropped first when the history is full. The upstream author confirmed this as expected in tinydesk issue #6 and suggested -DTD_EDITOR_MAX=8192 -DTD_EDITOR_UNDO=2048, leaving the issue open for an Editor that sizes its buffer to the file."
  consequence: "CMakeLists.txt sets TD_EDITOR_MAX=8192 and TD_EDITOR_UNDO=2048 for every translation unit, so the Editor takes about 12.8 KB instead of 25 KB; files over 8 KB open read-only, which leaves this project's scripts, playlists and configuration files editable. TinyDesk's test_history_limits pastes 10,001 bytes and so cannot pass at these limits; tools/tests/test_editor_limits.sh runs the rest of upstream's Editor tests at TinyTang's limits, read from CMakeLists.txt, with boundary checks in its place. When upstream sizes the buffer to the file, these limits become a ceiling rather than a fixed cost."
  sources:
    - "tinydesk-project/tinydesk v0.1.5 (feaf841): apps/editor.c TD_EDITOR_MAX, TD_EDITOR_UNDO, insert_text, the file load and the Editor's launch; tests/test_editor.c test_history_limits"
    - "github.com/tinydesk-project/tinydesk/issues/6, the author's reply of 2026-10-07T20:42Z"
    - "CMakeLists.txt; tools/tests/test_editor_limits.sh; tools/make_editor_test_files.py"
  verification: "On this board on 2026-10-07 with firmware 268021f-dirty.bab3b26 the user read System Monitor at 50 KB free with Files open and 38 KB with the Editor open on a 7,000-byte file, back to 50 KB on each close, against the 20 to 25 KB the Editor took before; the 7,000-byte file took an edit and kept it, and a 9,000-byte file opened read-only and stayed 9,000 bytes on the card (core-log entry 55)."

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

- record_id: PHOS-006
  kind: EXTERNAL
  topic_id: PHOS
  title: "The merged core's FPGA player is a raw-PCM sink, so every file goes through the AE350"
  status: VERIFIED
  verified_date: 2026-10-05
  statement: "In Tang-Phosphor's merged image (src/tang_phosphor_top.sv) the audio player is pcm_sink, which plays incoming bytes as signed 16-bit stereo samples and parses no container: the AE350 strips the WAV header itself before playing (platform_ae350.c, play_output). Its rate is play_rate, the AE350's last AE350_PLAY_RATE write, latched at stream start. Register 0xc0 (cpu_mode) routes the BL616's stream either to the AE350 (1) or straight to pcm_sink (0), and the core comes up with it at 0."
  consequence: "A WAV streamed with cpu_mode 0 sounds right only by accident -- it is raw PCM after a 44-byte header that plays as 11 samples (441011 for a 441000-sample file) -- and at the wrong pitch once the AE350 has set another rate: on 2026-10-05 a 44.1 kHz WAV played at 48 kHz straight after a 48 kHz Opus file. So TinyTang plays every file through the resident AE350 player and has no direct-stream command. The FPGA-native FLAC decoding and gapless handover of PHOS-004 belong to Tang-Phosphor's deployment core, not to this image; playlists on the merged core would also have to go through the AE350."
  sources:
    - "Tang-Phosphor a22ec9c, src/tang_phosphor_top.sv: the pcm_sink instance, play_rate(cpu_play_rate), and the cpu_mode multiplexers on register 0xc0"
    - "Tang-Phosphor a22ec9c, src/audio/pcm_sink.sv: rate <= play_rate at stream start"
    - "Tang-Phosphor a22ec9c, software/rbhost/host/platform_ae350.c: play_output() skipping the 0x2e-byte WAV header"
  verification: "Seen on this board on 2026-10-05: 441011 samples for the 1764044-byte test WAV streamed with cpu_mode 0, and the pitch change the user heard after Opus; through the AE350 the same file plays 441000 samples at 44.1 kHz after Opus at 48 kHz, which the user confirmed by ear."

- record_id: PHOS-007
  kind: EXTERNAL
  topic_id: PHOS
  title: "The resident AE350 player hangs before its first decode in some image layouts"
  status: VERIFIED
  verified_date: 2026-10-05
  statement: "Tang-Phosphor commit 1b313fe let a streamed player be built with no embedded input, which shifted everything after the input 16 bytes from the layout qualified in its entry 43. Built that way (863748 bytes, CRC 3d762d13) the player receives the whole file -- its log reads 'stream rx <size> first <magic>' -- and then never logs again: the loader stays in RUN, the RAM bridge counts ERROR responses with the first at address 0x00000000, and the bridge trace shows line fills at the top of the stack, then a fill at address 0, then fetches from the program entry at 0x40000000. With the code unchanged, 0 and 32 bytes of padding hang on every run and 16 and 48 bytes play on every run, so the trigger is the image layout and not the empty input entry. Instrumenting the player to find the faulting step moves its code and the hang disappears, even with the data layout matched to 32 bytes, and working runs also record ERROR responses from address 0."
  consequence: "Tang-Phosphor's software/rbhost/Makefile now reserves 16 bytes where a streamed player's input would go, restoring the layout qualified on hardware; that player (863764 bytes, CRC ef1502ed) passes the twelve-format sweep. It is a workaround, and any change to the player can move the layout and bring the hang back. The signature to recognise it by is the log stopping after 'stream rx' with the loader still in RUN (0x4020), a nonzero bridge error count (0x40b4) and a first error address of 0 (0x40c0). Finding the cause would start from a trap handler recording mcause, mepc and mtval in the program result words, since today a fault wedges the AE350 silently."
  sources:
    - "Tang-Phosphor 1b313fe, software/rbhost/Makefile: the stream-mode input change"
    - "Tang-Phosphor src/ae350/ae350_exts_regs.sv: the loader state, log, bridge counter and bridge trace registers read here"
  verification: "Reproduced on this board on 2026-10-05 over TinyTang's phosphor command: the unpadded player hung on three runs and a 32-byte-padded one on one; 16- and 48-byte-padded players played on every run, and the Makefile-built player passed the full sweep. The cause was not found."

- record_id: PHOS-008
  kind: EXTERNAL
  topic_id: PHOS
  title: "The qualified resident player image, and the stale one left in Tang-Phosphor's build directory"
  status: VERIFIED
  verified_date: 2026-10-05
  statement: "Tang-Phosphor's fixed Makefile (entry 69, 9e6f183) builds the resident AE350 player as 863764 bytes with CRC-32 ef1502ed, payload CRC-32 d8ea32ff, load and entry 0x40000000: make -C software/rbhost bench-universal BENCH_NAME=resident, with ~/.cache/tangcore-dev/toolchain/bin on PATH, since riscv64-unknown-elf-gcc (Xuantie-900 elf newlib V2.6.1, GCC 10.2.0) is not on the default PATH and the build fails with 'No such file or directory' without it. The image found in build/rbhost/bench/resident.tpi before that rebuild was dated 2026-10-02 and was 863748 bytes, CRC-32 3d762d13: the layout that hangs (PHOS-007)."
  consequence: "build/ is ignored by Tang-Phosphor's git, so a player sitting there says nothing about the tree it came from. Rebuild it and check the size and CRC before streaming it, over one-wire or onto the card. The rebuild reproduced entry 69's image byte for byte, which also confirms the build is deterministic."
  sources:
    - "Tang-Phosphor core-log entry 69 and software/rbhost/Makefile at 9e6f183"
    - "The rebuild on 2026-10-05: tools/ae350_run.py pack's report and zlib.crc32 of the output"
  verification: "Rebuilt on 2026-10-05 and played on this board over one-wire (TOOL-013), completing with zero underruns."

- record_id: PHOS-009
  kind: EXTERNAL
  topic_id: PHOS
  title: "Stopping a track on the merged core's pcm_sink, and what its counters report"
  status: VERIFIED
  verified_date: 2026-10-05
  statement: "pcm_sink.sv takes a stream's samples into a 2048-sample FIFO (FIFO_ADDRESS_WIDTH 11, not the 16384 docs/debug-registers.md gives for 0x64), fed by ae350_play_stream.sv's 512-entry queue, whose entries (data, start, end, cancel) leave strictly in order. Only the AE350 player can queue a cancel; the BL616 can restart the AE350 (0x43f0) and pause the sink (0x78 bit 0, which stops consumption and outputs silence), and neither clears the sink, which remains in its playing state with what was queued. Unpaused and empty in that state it counts an underrun every sample period; paused and full it accepts nothing, so the next track's START, queued behind the old samples, never arrives. The FIFO and the byte assembler clear only on a START or cancel. The underrun count at 0x6c is cleared only by the core's reset, although the register map describes it as per stream. 0x30 counts STARTs since the core loaded. 0x8c and 0x90, elapsed and duration, are tied to 0."
  consequence: "ports/bl616/phosphor/phosphor_player.cpp stops a track by pausing the sink and restarting the AE350, and at the next play unpauses until the stale tail has drained (0x64 at 0 and 0x68 still), pauses again, and lifts the pause when 0x30 moves; it reports underruns as the difference from the start of the track. The drained tail, at most about 58 ms of the old track, is heard at the start of the next play. Elapsed time is computed from 0x68 and 0x70, and a track's duration is not available from the core, which bounds the TinyDesk Phosphor app's progress display."
  sources:
    - "Tang-Phosphor src/audio/pcm_sink.sv, src/ae350/ae350_play_stream.sv, src/ae350/ae350_subsystem.sv, src/stream/stream_debug_sink.sv and docs/debug-registers.md"
  verification: "On this board on 2026-10-05: with the sink held paused across the next load, the track never started and the stuck queue read 0x64 = 2048 with 0x30 unmoved; unpausing drained 2562 samples. Without the hold, a track after a stop reported 168717 underruns. With drain and hold, tracks after a stop and after a replacement played with 0 underruns, and the format sweep passed (core-log entry 27)."

- record_id: PHOS-010
  kind: EXTERNAL
  topic_id: PHOS
  title: "A song loads silently while the whole file is sent"
  status: VERIFIED
  verified_date: 2026-10-06
  statement: "phosphor play and the Phosphor app send the whole file to the resident AE350 player before the first sample plays. A 4,387,971-byte MP3 was sent in 16,064 ms, about 3.7 s per MB, and then played at 44.1 kHz with no underruns; a 5,794,918-byte MP3 also plays. File names with spaces play, from the console with a quoted path and from the app."
  consequence: "With the short test files the load took under four to seven seconds and went unnoticed; with whole songs the app sits on 'loading' long enough to look broken, which is what was first taken for a file-name problem. Two follow-ups: report the load's progress in phosphor status and the app, since the player knows how many bytes it has sent, and find the file size at which the AE350's memory runs out, since it holds the whole file."
  sources:
    - "phosphor status on this board, 2026-10-06"
  verification: "Measured on this board on 2026-10-06; the user then played several more songs from the app and all started after the wait (core-log entry 39)."

- record_id: PHOS-011
  kind: EXTERNAL
  topic_id: PHOS
  title: "cpu_mode has its own register at 0xa8 from Tang-Phosphor register ABI 1.8"
  status: VERIFIED
  verified_date: 2026-10-06
  statement: "Tang-Phosphor's merged core routes the BL616 stream to the AE350 loader when bit 0 of 0xa8 is set and to the FPGA player when it is clear; 0xa8 reads back, powers up clear, and register 0x04 reads 0x00010008 (ABI 1.8) from that change on. Before ABI 1.8, the state PHOS-006 records at a22ec9c, cpu_mode was bit 0 of 0xc0, which is also the PMOD socket control word (PMOD-004), so writing 0xc0 = 1 to select the AE350 also held the renderer and released both sockets, and declaring the sockets with bit 0 clear deselected the AE350. A core older than ABI 1.8 ignores a write to 0xa8."
  consequence: "ports/bl616/phosphor/ae350_play.cpp reads 0x04 and refuses a core whose ABI is not 1.8 or later, with the error 'core register ABI older than 1.8; rebuild Tang-Phosphor', because an older core would play the file's bytes raw; it then writes 0xa8 = 1 on every play rather than only when it restarts the loader, so the route is reasserted even if something cleared it between tracks."
  sources:
    - "Tang-Phosphor b7ec367: src/debug/debug_regs.sv, src/tang_phosphor_top.sv, tests/debug_regs_tb.sv, docs/debug-registers.md; its core-log entry 71"
    - "ports/bl616/phosphor/ae350_play.cpp: REG_ABI, REG_ABI_CPU_MODE and REG_CPU_MODE"
  verification: "On this board on 2026-10-06 with firmware f8d6fdf-dirty.43c548f and the ABI 1.8 core, 0x04 read 0x00010008, 0xa8 read 1 during and after playback, and two plays of test.mp3 each completed at 441000 samples with zero underruns; the user heard the tone (core-log entry 45)."

- record_id: PHOS-012
  kind: EXTERNAL
  topic_id: PHOS
  title: "The resident AE350 player's track-handover contract and status words (unqualified)"
  status: INFERRED
  verified_date: 2026-10-07
  statement: "In Tang-Phosphor's resident gapless player experiment (step 3 of its entry 75 plan, built with BENCH_STREAM=2 by make -C software/rbhost bench-ondemand) the player never returns after a track. It extends the file-request mailbox at 0x4074, 0x4078 and 0x407c (USER(13..15)) so that a request of length 0 is a question selected by its offset and answered by the BL616 as one four-byte stream session, least significant byte first: 0 SIZE answers the current file's size, as the committed on-demand player already uses; 1 NEXT makes the BL616's queued track current and answers its size, 0 while nothing is queued, or 0xffffffff (STOP) when a stop is pending; 2 POLL answers 1 when a stop is pending and otherwise 0. A pending stop is answered once, to whichever of NEXT and POLL asks first. The player asks NEXT about every 50 ms while idle and POLL every 100 ms while decoding, keeps one request outstanding, reads each track in 64 KiB chunks at the file's own offsets, and longjmps out of the codec on a stop. It publishes its state in its result words: USER(0) at 0x4040 the marker 0x52455331 (RES1), USER(1) at 0x4044 tracks begun, USER(2) at 0x4048 tracks finished, USER(3) at 0x404c the latest finished track's samples, and USER(4) at 0x4050 that track's decode status, TRACK_ABORTED for a stopped one; a track counts as finished once all of it has been handed to the FPGA player. Same-rate tracks share one open play stream and so follow without a gap; a track at another rate waits for the stream to end and opens a new one."
  consequence: "While a resident player runs, the loader result at 0x402c is no longer a track's result, since the program does not return; a track's outcome is read from 0x4050 and its completion from 0x4048. The SIZE question is the only part of this contract in committed firmware (core-log entry 47); NEXT, POLL, STOP and the status words exist only in this record now, so a revival re-implements them on both sides from it and Tang-Phosphor remains the owner of the player's side."
  sources:
    - "Tang-Phosphor working tree on 2026-10-07 at 7cf9ede: software/rbhost/host/platform_ae350.c (uncommitted), software/ae350/include/ae350_request.h, and the untracked docs/experiment-resident-player.md"
    - "TinyTang working tree on 2026-10-07 at 515969c: ports/bl616/phosphor/ae350_file_server.h and .cpp (uncommitted, discarded in core-log entry 52)"
  verification: "Inferred by reading both uncommitted working trees on 2026-10-07. Neither side was built, swept or run on hardware, and neither tree was committed, so these sources cannot be re-read; treat every value here as a design to re-verify, not a measured contract."

- record_id: PHOS-013
  kind: EXTERNAL
  topic_id: PHOS
  title: "BL616-side lessons from the discarded resident gapless player experiment"
  status: INFERRED
  verified_date: 2026-10-07
  statement: "TinyTang's half of the PHOS-012 experiment settled these points in design. Residency is the loader in RUN (0x4020 bits 7:0 = 0x03) together with RES1 at 0x4040; neither alone suffices, because a trap and an ordinary program also leave the loader state and result words behind. The player is sent only when it is not resident, and after sending, the BL616 waits up to 2 s for RES1 before reading any counter, so it does not read a previous program's result words. A track is numbered as tracks begun plus one, read just before it is queued, and is over when tracks finished reaches that number, compared as a signed 32-bit difference for wraparound; this keeps a stopped track's late finish, published after the next track is queued, from passing for the new one. A stop pauses the sink at once (0x78 bit 0), clears the queued track, sets the pending stop, and serves the mailbox until the player takes it, so the player stays loaded; a player that has not taken the stop in 2 s is halted by the AE350 restart (0x43f0) as before. The file server keeps copies of the current and queued paths, since the caller's buffers change between tracks. One defect was found in review and not fixed: when a resident player is already running but the BL616 did not start it in this boot, for example after a BL616 reset with the FPGA left running, the mailbox baseline is never taken and stays 0, so a stale sequence in the mailbox would be served as a new request."
  consequence: "A revival of the resident player should carry these rules over and take the mailbox baseline whenever it finds a player it did not start. Tang-Phosphor's own notes set the order of work: the resident start failure of PHOS-007 is unexplained and a player that never returns exercises the RAM path far longer, so its RAM bridge experiment should be settled first; a rate change between tracks restarts the stream muted and must be reconciled with the I2S2 path's muted rate transition (Tang-Phosphor entry 78); and qualification needs both repositories together, the format sweep, a time to first sample against entry 76's 2.7 s, and a gapless check across same-rate tracks. Tang-Phosphor entry 76's on-demand player, served by TinyTang since core-log entry 47, remains the qualified one."
  sources:
    - "TinyTang working tree on 2026-10-07 at 515969c: ports/bl616/phosphor/phosphor_player.cpp (player_resident, stop_track, start_track, poll_track) and ae350_file_server.cpp (uncommitted, discarded in core-log entry 52)"
    - "Tang-Phosphor's untracked docs/experiment-resident-player.md, Open points, read on 2026-10-07"
  verification: "Inferred by code review of the uncommitted working tree on 2026-10-07; never built, deployed or tested. The baseline defect was found by reading start_track against ae350_file_server_begin, not by observation."

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

- record_id: PROV-005
  kind: TOOLCHAIN
  topic_id: PROV
  title: "MIT with thirteen Apache-2.0 files, the TangCore lineage, and what the notices must keep true"
  status: VERIFIED
  verified_date: 2026-10-06
  statement: "TinyTang's own code is MIT (LICENSE). Thirteen files are Apache-2.0, with the text at LICENSES/Apache-2.0.txt: ports/bl616/tang_jtag_programmer.c, nand2mario's Gowin JTAG programmer (TOOL-007; in Tang-Control's history 9 of its 10 commits are his), whose header gained a comment pointing at that text because its original sentence pointed at the root LICENSE of the tree it was written in; and ae350_play.cpp, ae350_play.h, flac_stream_prefix.h, fpga_debug.cpp, fpga_debug.h, fpga_ext_frame.h, fpga_file_stream.cpp, fpga_file_stream.h, fpga_stream.cpp and fpga_stream.h in ports/bl616/phosphor/ with tools/tests/fpga_ext_frame_test.cpp and flac_stream_prefix_test.cpp, whose every Tang-Control commit is this project's author's. Tang-Control is this project's author's fork of nand2mario's TangCore firmware-bl616 (66 of its commits are his), and TinyTang started from its board layer, so TinyTang descends from TangCore: its board knowledge was learned there, the BL616-to-core UART protocol is his design read from nestang (PROT-001), and the NES core is his nestang carried as patches. No other TangCore source file is in the tree. THIRD_PARTY.md names that lineage, the vendored programmer with openFPGALoader behind it, both TinyDesk submodules at their pinned MIT commits, nestang's iosys_bl616.v read as a specification but not copied (GPL-3.0), the Bouffalo SDK and the components linked from it (TOOL-016), and colibri (PROV-003). The project licence was Apache-2.0 when LICENSE and THIRD_PARTY.md were added on 2026-10-03 and changed to MIT the same day, at the user's preference for MIT unless Apache-2.0 was required."
  consequence: "Apache-2.0 was never required: it is permissive, so an Apache-2.0 file may sit inside an MIT project provided it keeps its notices and licence text. Choosing MIT gives up Apache-2.0's express patent grant, and means GPL-3.0 gateware from Tang-Phosphor cannot be copied into this tree. The sentence in THIRD_PARTY.md that must stay true is the one about the core UART protocol: it was read from a GPL-3.0 source as a specification and reimplemented, and that is what keeps a GPL-3.0 reading from becoming an unlicensed copy. Any file vendored from here on needs a line in THIRD_PARTY.md and a record here. The README once called TinyTang 'from-scratch' and said it did not 'inherit from TangCore', which was inaccurate; since 2026-10-06 README.md and THIRD_PARTY.md state the lineage and every Apache-2.0 file with its author. The Phosphor files are the project author's own and could be relicensed to MIT if ever wanted; until then they keep Apache-2.0."
  sources:
    - "This project's LICENSE, LICENSES/Apache-2.0.txt, README.md, THIRD_PARTY.md and the headers of the thirteen files"
    - "git log --follow in /run/media/vash/GIT/Tang-Control for fpga/programmer.cpp and utils/ (fpga_*, flac_stream_prefix.h, ae350_play.*), and its README: 'a fork of nand2mario's firmware-bl616'"
    - "nand2mario/tangcore README.md: the Apache-2.0 firmware licence"
    - "Tang-Phosphor, LICENSE and THIRD_PARTY.md: the model followed"
    - "Project statement recorded with the user, 2026-10-03: a preference for MIT unless Apache-2.0 turned out to be required"
  verification: "On 2026-10-06 every file under ports, tools, scripts and cmake was searched for Apache-2.0 and for nand2mario, TangCore and Tang-Control, and the authorship of each carried-over file was read from Tang-Control's history (core-log entry 38). Each THIRD_PARTY.md entry was written from a licence read directly. Revised 2026-10-11 (core-log entry 10): folded in THIRD_PARTY.md's coverage from the retired PROV-002 and the MIT trade-offs from the retired PROV-004."

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

- record_id: BLE-001
  kind: BOARD
  topic_id: BLE
  title: "The Bluetooth antenna is a U.FL jack with nothing fitted from the factory; an external antenna is now on it"
  status: VERIFIED
  verified_date: 2026-10-10
  statement: "The BL616 on the dock is U9, BL616C-50-Q2I-QFN40. Its antenna pin, net BL616_ANT, runs through L9 (0 ohm) to U35, an IPEX U.FL Gen1 jack; C186 and C187 on that net are not fitted, and the board carries no antenna of its own. The jack is on the dock's underside, marked ANT, to the left of the upper USB-A port beside the RETRO READY logo. With nothing on it the radio received a phone and a Logitech K950 at arm's length, the K950 at -76 to -94 dBm, and connected to the K950, but some connection attempts ended with HCI status 0x3E (connection failed to be established), each cleared by trying again; the Xbox Wireless Controller read about -70 dBm held against the board and -83 dBm further away, and its connections failed repeatedly with 0x3E. On 2026-10-10 a 2.4 GHz 6 dBi antenna was fitted to U35 through a U.FL/IPEX to RP-SMA pigtail, and the same controller then read -33 dBm, with the 0x3E failures and wedged connection attempts gone."
  consequence: "This board's Bluetooth reliability depends on that antenna: a board without one works only close up, and the bare board's failures (0x3E at connect, failed first encryption) look like protocol problems while being signal. If the antenna is ever removed or the board replaced, expect those failures back before suspecting the firmware. A client must still treat 0x3E as retryable rather than as the device refusing. Seating the U.FL connector on the dock's underside is a blind job."
  sources:
    - "Sipeed Tang Mega NEO dock schematics 31004 and 31005, Rev 1.4, page 8 (USB-JTAG & UART): U9, BL616_ANT, L9, U35, C186, C187"
    - "Photographs of this board's underside supplied by the user, 2026-10-05, showing the fitted jack"
    - "This project's ports/bl616/tang_ble.c: blescan's RSSI table and the connection-failure reports"
  verification: "Bare jack observed on this board on 2026-10-05: blescan listed the user's phone and the K950 by name, and blekbd failed with 0x3E on several attempts and connected on a later one each time. Antenna fitted by the user and measured with blescan on 2026-10-10; the first pairing attempt afterwards encrypted, bonded and ran the whole setup (core-log entry 6). Revised 2026-10-11 (core-log entry 10): the record said nothing was fitted and no antenna had been tried."

- record_id: BLE-002
  kind: TOOLCHAIN
  topic_id: BLE
  title: "The SDK's Bluetooth build: which controller library, how it starts, and what it costs"
  status: VERIFIED
  verified_date: 2026-10-05
  statement: "The SDK selects the BL616's Bluetooth controller by CONFIG_BTBLECONTROLLER_LIB. ble1m10s1bredr0 is the all-roles build: central, peripheral, broadcaster and observer, 10 connections, no BR/EDR, and the one examples/btble/central uses. ble1m0s1bredr0 has no central and no observer role and one connection, and ble1m0s1sbredr1 adds BR/EDR but still has no central role. The stack is brought up in the order examples/btble/central uses: rfparam_init, btble_controller_init, hci_driver_init, then bt_enable, whose ready callback reports the result; there is no call to shut the controller down again. CONFIG_BT_SETTINGS defaults to 0, in which case the host keeps nothing in flash, so the host alone keeps pairing keys only until reset. Built into this firmware, against the same tree without Bluetooth, ble1m10s1bredr0 with CONFIG_RF 1 grows the image from 346,160 to 582,256 bytes (+236,096: the controller library 113,954, the host stack libblestack 69,615, libbl616_phyrf 35,744, librfparam 6,639, about 10 KB else), shrinks the linker's ram_memory region from 447 KB to 415 KB, adds 17.4 KB of .bss and 0.8 KB of ITCM, and cuts the heap available at boot (__HeapLimit - __HeapBase) from 192,196 to 139,588 bytes."
  consequence: "proj.conf sets CONFIG_BLUETOOTH 1, CONFIG_BTBLECONTROLLER_LIB ble1m10s1bredr0, CONFIG_RF 1 and CONFIG_BLE_USE_MAC2 0, because scanning and connecting need the observer and central roles. ports/bl616/tang_ble.c starts the stack on the first Bluetooth command, or quietly after the boot script when pairings are saved on the card (BLE-012), so a board that never uses Bluetooth runs as before; once started it stays up until reset. The image no longer fitted the old 0x80000 slot, which is why the slot grew (FLS-002). Pairings survive a reset through the card, not CONFIG_BT_SETTINGS (BLE-012), and the heap the running stack takes is BLE-014."
  sources:
    - "Bouffalo SDK 7f44f9e (2025-04-27), components/wireless/bluetooth/ble_common.cmake: the role settings for each CONFIG_BTBLECONTROLLER_LIB value"
    - "Bouffalo SDK 7f44f9e, examples/btble/central/proj.conf and its main: the library choice and bring-up order"
    - "Bouffalo SDK 7f44f9e, components/wireless/bluetooth/btblecontroller/lib: libbtblecontroller_bl616_ble1m10s1bredr0.a, ble1m0s1bredr0.a and ble1m0s1sbredr1.a"
    - "This project's proj.conf and ports/bl616/tang_ble.c: ble_start()"
    - "Linker maps of this firmware at cb9e7d4 with and without the Bluetooth block, compared on 2026-10-05"
  verification: "Both builds were made from the same tree on 2026-10-05 and their maps and images compared; the Bluetooth build ran blescan and blekbd on this board. Revised 2026-10-11 (core-log entry 10): the consequence still described start-on-first-command only, keys in RAM only and the heap as unmeasured, all overtaken by BLE-012 and BLE-014."

- record_id: BLE-003
  kind: TOOLCHAIN
  topic_id: BLE
  title: "UUID-filtered GATT discovery found nothing against a real keyboard"
  status: VERIFIED
  verified_date: 2026-10-05
  statement: "Against the Logitech K950, bt_gatt_discover with BT_GATT_DISCOVER_PRIMARY and the HID service UUID 0x1812 as the filter returned no attribute, though the device has that service; the same call with no UUID returned all seven of its primary services, the HID service among them at handles 0x001F-0x004D. Descriptor discovery filtered on the CCC UUID 0x2902 likewise returned nothing, while unfiltered descriptor discovery over the HID range returned every CCC. The cause in the SDK's host was not established."
  consequence: "ports/bl616/tang_ble.c discovers every primary service and every descriptor in range and filters on the UUID in its own callback, assigning each CCC to the characteristic with the highest value handle below it. Expect the same of any other HID device until a filtered discovery is shown to work."
  sources:
    - "Bouffalo SDK 7f44f9e, components/wireless/bluetooth/blestack/src/include/bluetooth/gatt.h: struct bt_gatt_discover_params"
    - "This project's ports/bl616/tang_ble.c: the discovery callbacks"
  verification: "Observed on this board on 2026-10-05 in consecutive blekbd builds against the same K950: the filtered forms ended discovery with nothing found, and the unfiltered forms found the service and the CCCs."

- record_id: BLE-004
  kind: TOOLCHAIN
  topic_id: BLE
  title: "A subscription's notify callback is called with NULL data when its CCC write succeeds"
  status: VERIFIED
  verified_date: 2026-10-05
  statement: "The SDK's blestack is built with BFLB_BLE_PATCH_NOTIFY_WRITE_CCC_RSP, which adds a final branch to gatt_write_ccc_rsp: when a write enabling notifications succeeds, it calls the subscription's notify callback with data NULL and length 0. The unpatched branches remain: a failed write removes the subscription, and a successful write of 0 (an unsubscribe) also calls the callback with NULL. The SDK's own gatt.h documents the callback's data parameter as 'If NULL then subscription was removed', so the patch gives NULL a second meaning the header does not mention."
  consequence: "A NULL report in the notify callback cannot be read as an unsubscribe: tang_ble.c logs it and keeps the subscription, after its first version cleared the value handle on it and so stopped matching every report that followed. The real confirmation is reading the CCC back, which returned 01 00 for each of the K950's subscriptions."
  sources:
    - "Bouffalo SDK 7f44f9e, components/wireless/bluetooth/blestack/src/port/include/config.h: BFLB_BLE_PATCH_NOTIFY_WRITE_CCC_RSP"
    - "Bouffalo SDK 7f44f9e, components/wireless/bluetooth/blestack/src/host/gatt.c: gatt_write_ccc_rsp()"
    - "Bouffalo SDK 7f44f9e, components/wireless/bluetooth/blestack/src/include/bluetooth/gatt.h: bt_gatt_notify_func_t's data parameter"
  verification: "Observed on this board on 2026-10-05: each of the K950's seven CCC writes produced a NULL callback, and the reports arrived once the code stopped treating it as an unsubscribe."

- record_id: BLE-005
  kind: TOOLCHAIN
  topic_id: BLE
  title: "Too many ATT requests issued at once from host callbacks deadlock the host"
  status: VERIFIED
  verified_date: 2026-10-05
  statement: "Issuing about fifteen ATT operations (subscribes, writes and reads) back to back from inside a host callback left the host stopped for good: the requests beyond the stack's few ATT TX buffers wait for a buffer, and the buffers are released by the same host context that is waiting, so none is ever freed. Issuing one operation at a time, each from the completion callback of the one before, completed the whole set."
  consequence: "ports/bl616/tang_ble.c queues the setup of a device (the pre-subscription reads, every subscription, the read-backs, then Exit Suspend) in hid_setup_reports() and runs it through hid_setup_next(), one operation in flight at a time, each started from the previous one's completion. Any further GATT client work must go through the same sequencer."
  sources:
    - "This project's ports/bl616/tang_ble.c: hid_setup_reports() and hid_setup_next()"
  verification: "Observed on this board on 2026-10-05: the build that issued everything at once stalled in the discovering state with no further callbacks, and the sequenced build completed setup and reached ready on the next run. Revised 2026-10-11 (core-log entry 10): the consequence named kbd_setup_reports(), kbd_setup_next() and kbd_setup_step_done(), since renamed when the keyboard code became the generic HID slot."

- record_id: BLE-006
  kind: EXTERNAL
  topic_id: BLE
  title: "Logitech K950 over Bluetooth LE"
  status: VERIFIED
  verified_date: 2026-10-05
  statement: "The Logitech K950 keyboard (the keyboard of the MK955 Signature Slim set), in Bluetooth mode rather than on its Bolt receiver, is a HID over GATT device on Bluetooth LE advertising as 'Logi K950'. It uses a random address that increases by one each time it is put into pairing mode by holding an Easy-Switch key (seen as DB:88:A7:81:D9:CF to D9:D4). It accepts Just Works pairing at security level 2. Its primary services are 0x1800, 0x1801, 0x180A, 0x180F (battery), the HID service 0x1812 at 0x001F-0x004D, Logitech's 0xFD72 and a 128-bit vendor service from 0x005B. In the HID service, HID Information is at value handle 0x21, Boot Keyboard Input at 0x23 (notify, CCC 0x24), Boot Keyboard Output at 0x26, the Report Map at 0x28, Reports at 0x2A, 0x2E, 0x31, 0x34, 0x38, 0x3C, 0x40, 0x44 and 0x48 (notifying at 0x2A, 0x34, 0x38, 0x3C, 0x40 and 0x44), the Control Point at 0x4B and Protocol Mode at 0x4D. Writing 0 to Protocol Mode puts it in boot protocol, which reads back as 00, and it then sends the standard 8-byte boot report: modifiers, a reserved byte and six keycodes."
  consequence: "The boot report is byte-for-byte the format the wired keyboard link carries, so the existing translation in tang_key.c applies to it unchanged. Because the address changes with every pairing, a new pairing needs the device found in pairing mode; once paired, the bond on the card brings it back after a reset without that step (BLE-012). In boot protocol it still sent one 19-byte notification on 0x0044 beginning FF 04 00 01 01 01, presumably Logitech's own report, which a client should ignore."
  sources:
    - "Logitech MK955 Signature Slim product specifications: Bluetooth Low Energy connection beside the Logi Bolt receiver"
    - "GATT discovery and read-backs of this K950 by this project's ports/bl616/tang_ble.c (blekbd), 2026-10-05"
    - "Bluetooth SIG HID over GATT Profile 1.0: Protocol Mode, Boot Keyboard Input Report"
  verification: "On this board on 2026-10-05 blekbd paired with the K950, set boot protocol, wrote all seven CCCs and read them back as 01 00, and decoded 404 boot reports while the user typed: letters, Space, Enter, punctuation, right Shift, right Ctrl and right Alt, up to four keys held at once and every release. The user confirmed the result. Arrows, Esc and the function keys were not typed. Revised 2026-10-11 (core-log entry 10): the consequence said pairings are not kept across a reset, which BLE-012 changed."

- record_id: BLE-007
  kind: TOOLCHAIN
  topic_id: BLE
  title: "Only Bluetooth LE HID devices can be used; Classic HID has no host in the SDK"
  status: SOURCED
  verified_date: 2026-10-05
  statement: "The SDK's Bluetooth Classic support is a BR/EDR controller library, libbtblecontroller_bl616_ble1m0s1sbredr1, and the btprofile library, whose headers are a2dp, a2dp-codec, avdtp, avctp, avrcp, hfp_hf, rfcomm and sdp. There is no HID profile, host or device. The Rii K06 (Rii Mini Bluetooth Keyboard with IR learning) is listed as Bluetooth 3.0, which is Classic only."
  consequence: "The board can use keyboards, mice and controllers that implement HID over GATT on Bluetooth LE, and nothing that speaks only Classic Bluetooth HID; the K06 cannot be used, and the same applies to any other Classic-only device. A Classic HID host would have to be written on L2CAP over the BR/EDR library, and that library has no central role and is a separate build choice from the one this firmware needs (BLE-002)."
  sources:
    - "Bouffalo SDK 7f44f9e, components/wireless/bluetooth/btprofile/include/bluetooth: the profile headers listed"
    - "Bouffalo SDK 7f44f9e, components/wireless/bluetooth/ble_common.cmake: ble1m0s1sbredr1 with CONFIG_BT_BREDR 1 and CONFIG_BT_CENTRAL 0"
    - "Amazon product listing for the Rii K06 supplied by the user as a PDF, 2026-10-05"
  verification: "The SDK tree was read on 2026-10-05. No Classic device has been tried on this board."

- record_id: BLE-008
  kind: EXTERNAL
  topic_id: BLE
  title: "A BLE keyboard has no heartbeat and bypasses the core"
  status: VERIFIED
  verified_date: 2026-10-05
  statement: "The K950 in boot protocol sends a boot report when its key state changes and nothing while a key is held: across 571 reports in a 60-second watch there was no periodic report. The report arrives at the BL616 over the radio and never passes through the FPGA core."
  consequence: "tang_key.c stops auto-repeat 300 ms after the last report (TANG_KEY_REPORT_TIMEOUT_MS), a rule built on the wired link's 100 ms heartbeat from the core, so a BLE report must not be judged by it: tang_ble_keyboard() in ports/bl616/tang_ble.c reports the source live while the connection is READY, and the link's supervision timeout is the liveness signal, with the report zeroed on disconnect so a key held when the link drops is released rather than repeated. Because the core never sees the report, nestang patch 0006's left-alt pointer mode does not apply to it; tang_key_pointer() in ports/bl616/tang_key.c applies the same rule in firmware (arrows to the D-pad, Enter to A, Esc to B, all withheld from typing), and tang_osd_desk.c ORs the result into the pad word and keeps separate key state per keyboard."
  sources:
    - "blekbd watch of the K950 on this board, 2026-10-05"
    - "This project's ports/bl616/tang_key.h, TANG_KEY_REPORT_TIMEOUT_MS; third_party/patches/0006-pointer-mode.patch, link_key_to_pad"
  verification: "Host tests tools/tests/test_osd_desk.sh and test_key.sh cover repeat past 300 ms while live, release on disconnect, and pointer mode in console and desktop, and fail when the live flag or tang_key_pointer() is removed. The user confirmed on hardware on 2026-10-05 typing, repeat, release on power-off mid-hold, left-alt pointer and clicks and F12 (core-log entry 31)."

- record_id: BLE-009
  kind: EXTERNAL
  topic_id: BLE
  title: "Logitech M750 mouse over Bluetooth LE"
  status: VERIFIED
  verified_date: 2026-10-05
  statement: "The Logitech M750 mouse in Bluetooth mode is a HID over GATT device on Bluetooth LE advertising as 'Logi M750'. Like the K950 it uses a random address that increases by one each time it is put into pairing mode (seen as D6:86:9C:53:2B:57 to 2B:5A) and accepts Just Works pairing at security level 2. Its HID service is 0x001F-0x0035: HID Information at value handle 0x21, Boot Mouse Input at 0x23 (notify, CCC 0x24), the Report Map at 0x26, Reports at 0x28 (notify, CCC 0x29), 0x2C (notify, CCC 0x2D) and 0x30, the Control Point at 0x33 and Protocol Mode at 0x35. In report protocol, which it starts in, the report on 0x28 is 7 bytes: buttons in bytes 0-1 (bit 0 left, bit 1 right), X and Y as 12-bit signed values packed in bytes 2-4 (X = b2 | (b3 & 0x0F) << 8, Y = b3 >> 4 | b4 << 4), the wheel in byte 5 and, by position, horizontal pan in byte 6. Writing 0 to Protocol Mode puts it in boot protocol, which reads back as 00, and its boot report is then 4 bytes: buttons, X and Y as signed bytes, and the wheel as a signed byte. Like the K950 it sends one 19-byte notification beginning FF 04 00 01 01 01 on its vendor report, 0x2C."
  consequence: "Boot protocol gives the mouse everything the desktop uses, the wheel included, without parsing the report map; tang_mouse_boot_add() in ports/bl616/tang_pad.c reads it. The 7-byte report-protocol layout was decoded from motion and is not taken from the report map, so it must not be relied on for other mice. Out of range of the bare U35 jack (BLE-001) the link drops with HCI 0x08, a supervision timeout, and a reconnect using the RAM bond once failed encryption (security level 1, error 8) and then HCI 0x3E, after which the mouse no longer reconnected until it was put back in pairing mode."
  sources:
    - "blemouse and blekbd discovery, read-backs and watches of this M750 by this project's ports/bl616/tang_ble.c, 2026-10-05"
    - "Bluetooth SIG HID over GATT Profile 1.0: Protocol Mode, Boot Mouse Input Report; USB HID 1.11 Appendix B.2, the boot mouse report"
  verification: "On this board on 2026-10-05 a report-protocol watch while the user moved the mouse right, left, down and up gave the 7-byte layout above, and a boot-protocol watch gave 268 four-byte reports with motion and the wheel in both directions. The user then confirmed motion, clicks, drag, middle button, wheel and release on dropout in the desktop (core-log entry 32)."

- record_id: BLE-010
  kind: TOOLCHAIN
  topic_id: BLE
  title: "Bonded subscriptions outlive the connection unless volatile"
  status: VERIFIED
  verified_date: 2026-10-05
  statement: "In the SDK's Bluetooth host, remove_subscriptions() in blestack/src/host/gatt.c removes a subscription on disconnect only if the peer is not bonded or the subscription's flags carry BT_GATT_SUBSCRIBE_FLAG_VOLATILE; otherwise the bt_gatt_subscribe_params stays linked in the host's subscriptions list and is re-sent when the peer reconnects. bt_gatt_subscribe() returns -EALREADY (-120 here) when the params struct passed is already in that list."
  consequence: "A client that re-subscribes on every connection from its own params structs must mark them volatile, or a reconnect from a bonded peer, or a new pairing of the same device, finds them still linked: re-initialising one rewrites a list node the host is using, and subscribing it fails with -EALREADY. hid_add_sub() in ports/bl616/tang_ble.c sets the flag. Bonds now persist across resets (BLE-012), so this applies to every reconnect, not only within one boot."
  sources:
    - "Bouffalo SDK 7f44f9e, components/wireless/bluetooth/blestack/src/host/gatt.c: remove_subscriptions(), bt_gatt_subscribe()"
    - "Bouffalo SDK 7f44f9e, components/wireless/bluetooth/blestack/src/include/bluetooth/gatt.h: BT_GATT_SUBSCRIBE_FLAG_VOLATILE"
  verification: "On 2026-10-05, without the flag, the M750's re-pairing after a dropout logged 'subscribe 0x002C failed (-120)', and its failed reconnect showed the host completing a CCC write nobody had issued. With the flag the next pairing subscribed all three inputs (core-log entry 32). Revised 2026-10-11 (core-log entry 10): the consequence said bonds were in RAM only, which BLE-012 changed."

- record_id: BLE-011
  kind: TOOLCHAIN
  topic_id: BLE
  title: "Reconnection in this SDK is the whitelist initiator only"
  status: VERIFIED
  verified_date: 2026-10-05
  statement: "blestack/src/port/include/config.h defines CONFIG_BT_WHITELIST 1 unless it is already defined. In host/conn.c bt_le_set_auto_conn() is compiled only under !CONFIG_BT_WHITELIST, and in host/hci_core.c so is the disconnect path that puts an auto-connect object back to scanning; a build that calls bt_le_set_auto_conn() fails to link. What remains is bt_le_whitelist_add(), _rem() and _clear(), and bt_conn_create_auto_le(), which starts the controller's initiator with the whitelist filter policy. It returns -EINVAL during an explicit scan (BT_DEV_EXPLICIT_SCAN), while another connection is being made or with an empty whitelist, and -EALREADY while it is already running (BT_DEV_AUTO_CONN). When it connects a device, enh_conn_complete() clears BT_DEV_AUTO_CONN, so it stops after one connection; bt_conn_create_auto_stop() cancels it; and bt_conn_create_le() returns NULL while it runs."
  consequence: "ports/bl616/tang_ble.c keeps the whitelist equal to the paired devices that are waiting and restarts the initiator after every connection, disconnection and scan (ble_reconnect_update), under a lock because the shell and the background task both call it. It stops the initiator before blescan, pair and an explicit connect, and retries a failed start every second. A reconnection arrives on a connection object the host made itself, which hid_by_conn() matches to its slot by address."
  sources:
    - "Bouffalo SDK 7f44f9e, components/wireless/bluetooth/blestack/src/port/include/config.h: CONFIG_BT_WHITELIST"
    - "Bouffalo SDK 7f44f9e, components/wireless/bluetooth/blestack/src/host/conn.c: bt_conn_create_auto_le(), bt_le_set_auto_conn(), bt_conn_create_le(); host/hci_core.c: enh_conn_complete(), hci_disconn_complete()"
  verification: "The first build of entry 33 failed to link on bt_le_set_auto_conn. With the whitelist initiator the K950 and M750 reconnected after a power cycle and after going out of range, and three off/on cycles of the M750 each reconnected (core-log entry 33)."

- record_id: BLE-012
  kind: TOOLCHAIN
  topic_id: BLE
  title: "Pairings are kept on the SD card, and what a failed re-encryption means"
  status: VERIFIED
  verified_date: 2026-10-10
  statement: "With CONFIG_BT_SETTINGS 0 the host keeps no keys across a reset (BLE-002), and its settings backend (host/settings.c) stores through easyflash, which needs a key-value partition this board's flash layout does not have (FLS-002). The host's key table is reachable instead: bt_keys_find_addr() after pairing_complete gives the device's entry, and bt_keys_get_addr() at boot makes one that can be filled with the saved LTK (legacy or LE Secure Connections), IRK, flags and key size, after which a connection encrypts with it without pairing. Both the K950 and the M750 accepted that. On a reconnect of those two, on the bare jack (BLE-001), the first encryption attempt often failed with security level 1, error 8 (UNSPECIFIED) and then a disconnect with HCI 0x3E, and a following attempt succeeded: seen on the M750, where the second try encrypted, and on the K950 after a power cycle, where the third did. A different failure has the same surface: when a device has forgotten its key, for example because it was put back into pairing mode, the board's stored key can never encrypt again. Then the security callback may report error 2 (PIN_OR_KEY_MISSING), or may not fire at all, the connection showing only 'connected; requesting encryption' followed by HCI 0x3E with no 'encrypted' line; on the Xbox Wireless Controller that repeated on every attempt until the key was dropped by hand."
  consequence: "tang_ble.c captures the entry on pairing_complete into a record per slot, and the background task writes /sd/root/ble/bonds.bin (format in ports/bl616/tang_ble_bonds.h: magic, version, two 80-byte records, CRC-32; a damaged file means no pairings), reads it after the boot script, starts the stack quietly, restores the keys and puts the devices on the reconnect whitelist (BLE-011). The keys are stored unencrypted, so whoever holds the card can impersonate the board to those devices. An error-8 failure goes back to waiting and the next connection retries it, so it needs no handling, and it must not be used to drop a key: error 8 also arrives on signal alone, and dropping on it threw away good pairings. hid_security_changed() drops the stored key on PIN_OR_KEY_MISSING only, through hid_drop_key(), leaving the slot armed so the next connection pairs afresh. The silent case, a connection that reaches 'requesting encryption' and dies without ever logging 'encrypted', is not handled, and only blekbd forget (or blemouse forget) clears it; the narrow trigger, if one is added, is that sequence on a bonded slot, not the HCI code alone, because 0x3E is also a signal failure."
  sources:
    - "Bouffalo SDK 7f44f9e, components/wireless/bluetooth/blestack/src/host/keys.h: struct bt_keys, bt_keys_find_addr(), bt_keys_get_addr(); host/settings.c: easyflash backend"
    - "This project's ports/bl616/tang_ble.c: hid_capture_bond(), hid_restore_keys(), hid_security_changed() and hid_drop_key(); ports/bl616/tang_ble_bonds.c"
  verification: "tools/tests/test_bonds.sh checks the file format (15 checks). On 2026-10-05 the user paired both Logitech devices once, power-cycled the board without pairing mode and woke them, and both reconnected; the user also saw one go out of range and come back (core-log entry 33). On 2026-10-11 the M750 reconnected from the card after a power cycle and encrypted on its first attempt with the antenna fitted (core-log entry 9). The stale-key forms were observed with the Xbox controller on 2026-10-10: six error-8 attempts and then error 2 (core-log entry 3), and four silent 'requesting encryption' then 0x3E attempts ended by blekbd forget (core-log entry 5). Revised 2026-10-11 (core-log entry 10): the record said a failed first encryption needs no special handling, without the stale-key case. Revised 2026-10-13 (core-log entry 47): the card's folders moved under the shell's home, /root (cores, scripts, music, ae350, ble and demos, with ROMs in /root/data/nes), and the firmware's paths followed; the paths above are the new ones."

- record_id: BLE-014
  kind: TOOLCHAIN
  topic_id: BLE
  title: "Heap taken by the running Bluetooth stack"
  status: VERIFIED
  verified_date: 2026-10-05
  statement: "This SDK builds FreeRTOS with heap_3, so heap use is read from its own allocator, kfree_size() and g_kmemheap.heapsize in components/mm/mem.h; the allocator keeps no lowest-ever counter, so a transient peak is not measured. Starting the Bluetooth stack takes about 15.8 KB of heap (84,120 B free before and 68,292 B after in firmware 1cea7b9-dirty.a296b3b, 82,364 and 66,536 in a later one), and a connection about 700 B; reconnects return free heap to the same figure, so they do not leak. With both devices connected, 64,704 B of the 124,768 B heap stayed free, with a 40,536 B largest block, before and after seven cartridge loads and a desktop session. An earlier reading of 41,764 B free after a desktop session was not the desktop: every tangload, including the boot script's, leaked 4 KB (BL6-009) until that was fixed."
  consequence: "Bluetooth with two devices leaves about 65 KB of heap and a 40 KB largest block for everything else. A request near that size, such as the 32 KB script stack TinyDesk Shell asks for, can still fail once the heap fragments, which is why worker stacks are capped (BL6-009). `ble` reports free heap and the largest block."
  sources:
    - "Bouffalo SDK 7f44f9e, components/mm/mem.h and mem.c: kfree_size(); components/os/freertos/CMakeLists.txt: heap_3.c"
    - "This project's ports/bl616/tang_heap.c tang_heap_info() and ports/bl616/tang_ble.c ble_start_ex() and cmd_ble()"
  verification: "Read from the board with `ble` on 2026-10-05 (core-log entry 33), and with `ble` and `crash` after the leak fix (core-log entry 35). On 2026-10-11 `ble` read 64,984 B free of 122,744 B with a 52,664 B largest block, with the M750 paired (core-log entry 9). Revised 2026-10-11 (core-log entry 10): folded in the allocator details and the first measurement from the retired BLE-013."

- record_id: BLE-015
  kind: EXTERNAL
  topic_id: BLE
  title: "Xbox Wireless Controller over Bluetooth LE: pairing mode, pairing parameters and GATT map"
  status: VERIFIED
  verified_date: 2026-10-11
  statement: "The Xbox Wireless Controller is a HID over GATT device on Bluetooth LE with a public address (this one 78:86:2E:BB:45:79, Microsoft), advertising appearance Gamepad (0x03C4) and the HID service UUID 0x1812; it does not answer a Classic BR/EDR inquiry. BlueZ reads its PnP ID as USB 045E:0B13, version 0x0509. Its light blinks fast in pairing mode and slowly when idle and not pairing; pairing mode lasts a few minutes. Its LL features include LE Encryption, Data Packet Length Extension and LL Privacy, and its Peripheral Preferred Connection Parameters (handle 0x0007) are interval 6-12 (7.5-15 ms), latency 0, supervision timeout 300 (3 s); its ATT MTU is 23. Its Pairing Response is IO capability NoInputNoOutput, Bonding, no MITM, Secure Connections, key size 16, wanting EncKey and IdKey from the initiator and offering EncKey and IdKey as responder: against a bonding request it returns the intersection with what the initiator offered, and against BlueZ's request it negotiated initiator EncKey only (which LE Secure Connections ignores, so the host sends no keys) and responder EncKey and IdKey, after which it sends Identity Information and Identity Address Information. Its HID service is at 0x0016-0x0023: HID Information value 0x0018, HID Control Point 0x001A, Report Map 0x001C (283 bytes), an input Report at 0x001E (notify and read, CCC 0x001F) and an output Report at 0x0022 (write only); 0x0003 is the GAP Device Name, 'Xbox Wireless Controller'. A 128-bit vendor service 00000001-5F60-4C4F-9C83-A7953298D40D sits at 0x0024-0x002A, with characteristics 00000002-... at value 0x0026 (read), 00000003-... at 0x0028 (read) and 00000004-... at 0x002A (write). The controller sends input notifications on 0x001E on its own: about every connection event from the start of a link, before any CCC write, and even on an unencrypted link after pairing failed; on an encrypted link with BlueZ the first arrived 45 ms after encryption. It sends no ATT requests of its own to the host."
  consequence: "The controller works as a plain LE HID device on a mainstream host: with BlueZ 5.85 it paired with LE Secure Connections Just Works, bonded, and delivered 701 sixteen-byte reports while the user pressed buttons, with no vendor-service access and no host-side GATT server involvement. Bluepad32's uni_hid_parser_xboxone.c likewise performs no vendor write. Subscribing is not what starts its reports, so a host that sees no reports after encryption is not failing to subscribe. Commands fired at it from this board mean something only while it blinks fast. Its behaviour on this board's host is BLE-017, and its input report is BLE-016."
  sources:
    - "btmon captures of this controller pairing with the user's desktop (Intel adapter, BlueZ 5.85), 2026-10-11: one failed pairing (no agent) and one successful one, ~/xbox-desktop.snoop and ~/xbox-desktop2.snoop"
    - "GATT discovery, reads and blescan of this controller by this project's ports/bl616/tang_ble.c, 2026-10-10"
    - "Bluepad32's Xbox One parser, uni_hid_parser_xboxone.c"
  verification: "Observed on this board on 2026-10-10 across several pairings (core-log entries 3, 4, 5, 6 and 8), and on the user's desktop under btmon on 2026-10-11 (core-log entry 11). The blink meanings were confirmed by the user after several runs had read them backwards (core-log entry 8). Revised 2026-10-11 (core-log entry 11): added the pairing parameters, preferred connection parameters, PnP ID and unsolicited reports from the btmon captures."

- record_id: BLE-016
  kind: EXTERNAL
  topic_id: BLE
  title: "Xbox Wireless Controller: its 16-byte input report, from its Report Map"
  status: VERIFIED
  verified_date: 2026-10-11
  statement: "The controller's 283-byte Report Map describes a Game Pad (usage page 0x01, usage 0x05). Input Report ID 1 arrives over GATT as a 16-byte notification on 0x001E, without the report ID: X, Y, Z and Rz as four 16-bit little-endian axes with a logical range of 0 to 65535 (bytes 0-7, centred near 0x8000); Brake and Accelerator as 10-bit fields with a logical range of 0 to 1023, each padded to 16 bits (bytes 8-11); one 4-bit Hat switch, logical 1 to 8 with a null state, padded to a byte (byte 12); fifteen 1-bit buttons padded to 16 bits (bytes 13-14); and one Consumer-page bit padded to a byte (byte 15). Report ID 3 is an output report on the PID usage page: four 8-bit motor-scale fields on 0 to 100 and three 8-bit fields on 0 to 255."
  consequence: "A 16-byte report fits one notification at the default ATT MTU of 23, so the MTU is not an obstacle to reports and nothing needs to negotiate one. The Report Map arrives 22 bytes at a time at that MTU and has to be read with offsets, because the SDK does not continue a long read (gatt.h: 'the caller will need to read the remaining data separately using the handle and offset'); hid_read_done() re-issues the read from the next offset whenever a chunk fills the MTU. No report from this controller has yet reached this board."
  sources:
    - "The controller's Report Map read from value handle 0x001C by this project's ports/bl616/tang_ble.c, 2026-10-10, in 12-byte offset-tagged chunks"
    - "btmon capture of the controller paired with BlueZ, 2026-10-11: 701 notifications on 0x001E, every one Data[16]"
    - "Bouffalo SDK 7f44f9e, components/wireless/bluetooth/blestack/src/include/bluetooth/gatt.h: bt_gatt_read() long-value note"
    - "USB HID Usage Tables: Generic Desktop Game Pad, Simulation Controls Brake and Accelerator, Physical Interface Device page"
  verification: "Decoded by hand from the full map on 2026-10-10 (core-log entry 4); BlueZ's input device built from the same map gave axes swinging the full range and buttons 0-14 (core-log entry 6); the 16-byte length and the axis bytes were read from live notifications under btmon on 2026-10-11 (core-log entry 11). Revised 2026-10-11 (core-log entry 11): the record said 17 bytes, which counted the report ID that GATT does not send, and omitted the padding after the hat and the buttons."

- record_id: BLE-017
  kind: EXTERNAL
  topic_id: BLE
  title: "Xbox Wireless Controller on this host: no report has ever arrived, and two ways the link ends"
  status: VERIFIED
  verified_date: 2026-10-11
  statement: "On this board the controller has never delivered an input report, in two failure forms. In the first, seen on the bare jack (BLE-001) before the antenna was fitted, a fresh pairing at least once ran the whole setup -- discovery, the subscription with CCC 0x001F reading back 01 00, the full Report Map, Exit Suspend written to the Control Point and accepted, ready -- and the controller then sent no notification with buttons held and ended the link with HCI 0x13 (remote user terminated); at other times it ended the link earlier. In the second, seen consistently after the antenna was fitted, it bonds with LE Secure Connections on a link created at interval 40 (50 ms), latency 0 and supervision timeout 400 (4 s), never asks for other connection parameters, and answers no ATT request once the security procedure completes: a read of 0x0003 sent before security returned 22 bytes and then 2, 'Xbox Wireless Controller', while the same read after it never came back, primary discovery found 0 services, and a later bt_gatt_read() returned -ENOTCONN (-128) because the host held the link to be gone. With nothing issued after encryption -- no discovery, no subscription, no bond save -- the link was reported alive at 1, 2, 3, 5 and 8 s, none of five probe reads was answered, and it ended 8750 to 8801 ms after bonding with HCI 0x08, a supervision timeout, placing the controller's last packet near 4.75 s. On the next connection the controller reports error 2 (PIN_OR_KEY_MISSING), so it did not keep the bond this board saved. Once a bond exists it answers an unencrypted read with ATT 0x05 (Insufficient Authentication). An ATT MTU exchange sent while security was in progress drew ATT 0x0E (Unlikely Error) and HCI 0x3E on four attempts out of four. The second form is unchanged with host privacy off (CONFIG_DISABLE_BT_HOST_PRIVACY), which stops this board offering its identity key: paired, encrypted at level 2, 0 services, HCI 0x08, then error 2, twice, on firmware bb64cbb-dirty.34f7c44. With LE legacy pairing as well (smp_set_auth(BT_SMP_AUTH_BONDING), firmware cb9c96c-dirty.1055ce0) a third form appeared, repeated over six pairings: the controller encrypts, bonds and answers discovery after encryption, all five primary services including HID at 0x0016-0x0023; it then asks for its preferred parameters, which this board grants (interval 12, 15 ms; latency 0; timeout 300, 3 s), and the link ends with HCI 0x08 about one supervision timeout later, the next connection getting error 2. BlueZ moves the same controller to the same 15 ms and 3 s on its own initiative, with LE Connection Update, and receives 676 further reports. The update is not the cause: the controller asks this board for interval 6-12, latency 0, timeout 300 shortly after pairing, through the host's le_param_req, and with the request refused (firmware 9235969-dirty.ac83602) the link stayed at 50 ms and still ended the same way, discovery stalling after one or two answers, over three pairings in one 45-second watch. Since legacy pairing, most first connection attempts from this board end in HCI 0x3E (connection failed to be established) at -24 to -33 dBm, the controller never answering the board's connection; BlueZ connected to it first time on every attempt. An HCI trace of this board (bletrace, firmware 68ca825-dirty.c2a36fd) shows both: in a 0x3E attempt the board's LE Create Connection matches BlueZ's (scan 60/60 ms, interval 30-50 ms, public addresses) apart from a 4 s supervision timeout against BlueZ's 420 ms, the controller reports the connection complete at 50 ms, and about six connection events later it gives up with 0x3E, so the controller never heard from the peer at the link layer; and in every encrypted session the controller's last data packet arrives about 0.15 s after it sends its Identity Address Information, just after pairing completes on both sides, with the link timing out about 8.7 to 9.7 s after connection. At that same moment, on receiving the controller's IRK, this board's host issues LE Add Device To Resolving List, LE Set Privacy Mode and LE Set Address Resolution Enable (bt_id_add() in host/hci_core.c, used whenever the controller reports a resolving list), which BlueZ never issues during its session with the controller."
  consequence: "This is an open defect. On a working host (BLE-015) the controller sends reports within 45 ms of encryption with nothing asked of it, so on this board it is silent, not merely unsubscribed; and since it answers ATT before encryption and nothing after, the fault lies in the encrypted link or in how the controller judges the pairing, not in GATT. Ruled out: signal (antenna fitted), connection parameters requested by the controller (none), the ATT MTU (BLE-016), the order of the GATT setup steps (setup never starts), the vendor service (BLE-015), legacy pairing as the mode in use (the bond is LE Secure Connections), this firmware's post-pairing work (the hands-off run issued none), and this board's identity-key distribution and private address (privacy off changed nothing). LE Secure Connections is involved but is not the whole fault: with legacy pairing the controller answers after encryption, so the silence from encryption onwards is specific to Secure Connections on this board, while the link still fails a few seconds after pairing whether its parameter request is granted or refused, so the update is ruled out too. An HCI trace on this board (`bletrace`, read with tools/ble_trace_to_btsnoop.py and btmon) now shows the failure point: the controller stops sending data as pairing completes, which is also when this board's host turns on address resolution in its controller for the controller's IRK; that reconfiguration is not the cause: with public-address devices kept out of the resolving list (firmware 750bb0a-dirty.2c19591) no resolving-list, privacy-mode or address-resolution command reached the controller, and the controller still went silent at the same point in four encrypted sessions. An ATT MTU exchange sent once encryption is up, BlueZ's first operation (firmware 2a3e59b-dirty.e957079), is answered (MTU 23) and changes nothing either. In every traced encrypted session the controller's last two data packets are a GATT answer and an L2CAP Connection Parameter Update Request, sent about 0.1 s after pairing completes; it never sent that request to BlueZ, which moved it to 15 ms on its own about 2 s in, so the controller treats this board's link differently before any GATT difference arises. Creating the link at BlueZ's 45 ms and 420 ms and asking for a 251-octet data length once connected (firmware ba80bbf-dirty.8977791) changed nothing: the controller still asked for new parameters and fell silent at the same point. The controller accepted every LE Set Data Length command, but no LE Data Length Change event ever arrived, where BlueZ's adapter reported 251 octets each way, so whether this board's controller completes the exchange with the controller is not known. With BlueZ's own pairing flags as well (Bonding, MITM, Secure Connections, CT2; firmware b6a8490-dirty.f783aa8) this board's host traffic matches BlueZ's packet for packet through the moment of failure: encryption, an MTU Exchange Request, the controller's Identity Information and Identity Address Information, and no keys from the host. BlueZ's request is answered within 90 ms and the reports start; this board's is acknowledged at the link layer and never answered, and the controller's link layer stays up about 4.8 s more. The fault is therefore below the HCI, in the BL616's binary controller (libbtblecontroller_bl616_ble1m10s1bredr0.a), consistent with the 0x3E connects and the unreported data length change. The SDK in use is nand2mario's fork at 7f44f9e (2025-04-27); upstream bouffalolab/bouffalo_sdk has released components/wireless/bluetooth/btblecontroller about twenty times since, up to version 2.3.36 on 2026-10-03, with release notes that do not say what changed. That library (2.3.36, 8a3df34, 1,856,570 bytes, sha256 cf8b688b694bb96225d736c412350ea3c737f7a872bb755bb37ef267a2e64e23, version string lib_version_btblecontroller_1.6.210) keeps the controller API and the onchip HCI packet layout of this fork's, apart from bt_onchiphci_hanlde_rx_acl renamed bt_onchiphci_handle_rx_acl and upstream's host glue sending every command with dest_id 0, and it links against this fork's host with a shim for those two; but the board then hangs, with USB gone and no crash record, as soon as the radio is started. Upstream has also changed the BL616 PHY library (drivers/soc/bl616/phyrf, three releases since) and the BL616 linker scripts that size the controller's exchange memory (__EM_SIZE), so the controller cannot be taken alone: using it means taking upstream's radio layer, controller, PHY, RF parameters, memory layout and host glue, together. Still different from BlueZ and untested: who starts the parameter update (the controller asks this board; BlueZ updated before it asked); the pairing request's IO capability and MITM flag (BlueZ: KeyboardDisplay, MITM, CT2; both still end in Just Works); the responder key distribution this board asks for; the link layer, where BlueZ's adapter extended the data length to 251 octets at connection and used a 45 ms interval with a 420 ms supervision timeout; and the BL616 controller's handling of LE Secure Connections encryption, which no other device here has been shown to use. Do not issue an ATT MTU exchange while security is in progress."
  sources:
    - "This project's ports/bl616/tang_ble.c console logs on 2026-10-10 and 2026-10-11: setup steps, connection info, the probe reads of 0x0003, the hands-off hold and the disconnect reasons"
    - "btmon capture of the controller pairing with BlueZ, 2026-10-11 (~/xbox-desktop2.snoop)"
    - "HCI trace of this board's host recorded by ports/bl616/tang_ble_trace.c, decoded with btmon, 2026-10-11"
    - "github.com/bouffalolab/bouffalo_sdk commit history for components/wireless/bluetooth/btblecontroller since 2025-04-27, read 2026-10-11"
    - "bouffalolab/bouffalo_sdk 8a3df34: btblecontroller/lib/libbtblecontroller_bl616_ble1m10s1bredr0.a, btble_inc/hci_onchip.h and btble_lib_api.h, blestack/src/bl_hci_wrapper/bl_hci_wrapper.c, BLUETOOTH-LIBS.md; commit history of drivers/soc/bl616/phyrf and bsp/board/bl616dk/bl616_ram.ld"
    - "Bouffalo SDK toolchain sys/errno.h (ENOTCONN 128) and components/wireless/bluetooth/blestack/src/host/gatt.c; host/smp.c ID_DIST, SEND_KEYS and RECV_KEYS"
    - "Bluepad32 CHANGELOG.md, 5.0.0-beta0: 'Disabled LE Secure Connections by default to improve compatibility with BLE controllers that fail key exchange.'"
    - "This project's ports/bl616/tang_fpga_link.h: FPGA_CMD_INPUT 0x16 and FPGA_CMD_KEYBOARD 0x17"
  verification: "Observed on this board on 2026-10-10: the first form in core-log entries 3 to 5, the second over the instrumented builds of entries 7 and 8, with the pre-security answer and the post-security silence each seen on the same build. The privacy-off test ran on 2026-10-11 (core-log entry 11). Revised 2026-10-11 (core-log entry 11): added the privacy-off result, the comparison with the working BlueZ capture, and the untested differences that remain. Revised 2026-10-11 (core-log entry 12): added the legacy-pairing result and the connection update as the point of failure. Revised 2026-10-11 (core-log entry 13): the parameter update ruled out by refusing it; the 0x3E connect failures added. Revised 2026-10-11 (core-log entry 14): added the HCI trace findings. Revised 2026-10-11 (core-log entry 15): address resolution ruled out. Revised 2026-10-11 (core-log entry 16): the MTU exchange ruled out; the parameter request as the controller's last act. Revised 2026-10-11 (core-log entry 17): BlueZ's connection parameters and data length request ruled out. Revised 2026-10-11 (core-log entry 18): host traffic now matches BlueZ's; the fault placed in the controller library; newer upstream libraries noted. Revised 2026-10-11 (core-log entry 19): the newer controller library tried alone and found to hang the board. Revised 2026-10-11 (core-log entries 28 and 31): closed as unsupported on this board.  The move to Bouffalo SDK 2.3.36, made for this pad's controller library, changed nothing -- the pad pairs, encrypts, bonds and then goes silent exactly as the forms above record -- so the fault is confirmed to be the BL616's binary controller library and not the host, the SDK version or anything this project changes; rfparam_init() is called exactly as the SDK's own examples call it, so the radio path is not misconfigured either.  Two further facts close the question of what a working pad would have bought: the BL616's core link now carries the commands that send input into a core -- `FPGA_CMD_INPUT 0x16`, the buttons and the mouse's dx, dy and wheel, since core-log entry 79, and `FPGA_CMD_KEYBOARD 0x17`, the Bluetooth keyboard's HID boot report, since entry 80 -- where core-log entry 29 had recorded that it carried none, so what stands between a pad and a running core is this record's own finding, the binary controller library, and not the link; and no other BLE-HID gamepad was available to test, so whether this host path handles gamepads at all is untested, while a BLE keyboard and mouse both pair and report (core-log entry 31). Revised 2026-10-16 (core-log entry 82): the sentence that said the core link carried no command to send input into a core was rewritten, since entry 79's `0x16` and entry 80's `0x17` gave it two, and a source line for them was added."

- record_id: BLE-018
  kind: BLE
  topic_id: BLE
  title: "2.3.36's bt_conn_unref asserts on an over-release, where the fork was silent"
  status: VERIFIED
  verified_date: 2026-10-11
  statement: "In blestack 2.3.36, bt_conn_unref() ends with BT_ASSERT(old > 0): it disassembles to an atomic_dec, a branch and a call to bt_assert, so releasing a reference that is not held asserts.  The fork's bt_conn_unref (nand2mario/bouffalo_sdk 7f44f9e) is only atomic_dec and a debug print, so the same over-release was silent there.  A connection object from bt_conn_create_le() comes with a reference the caller owns only when it is newly created; with an object already present for the peer it returns that object without one (conn.c, the 'not ref if conn of this peer has existed' fix), and bt_conn_lookup_handle() and bt_conn_lookup_addr_le() each take one of their own."
  consequence: "A connection reference must be released only when it was taken.  This port released references the SDK's own disconnect teardown already drops -- in hid_disconnected() and in hid_connected()'s error path -- which on 2.3.36 asserted in recv_thread and reset the board on every Xbox controller attempt, hiding the pad's own defect behind a crash."
  sources:
    - "Bouffalo SDK 2.3.36 components/wireless/bluetooth/blestack/src/host/conn.c: bt_conn_unref(), bt_conn_create_le(), bt_conn_lookup_handle(), bt_conn_lookup_addr_le() and bt_conn_set_state()"
    - "nand2mario/bouffalo_sdk 7f44f9e, components/wireless/bluetooth/blestack/src/host/conn.c: bt_conn_unref() with no assert"
    - "core-log entries 28 and 30"
  verification: "Established on the board on 2026-10-11: the crash record's assert was resolved against the ELF to hci_disconn_complete's final bt_conn_unref, and a link-time wrap of bt_conn_ref and bt_conn_unref that read conn->ref (offset 72, from bt_conn_unref's own disassembly) showed two references and three releases; removing this port's two releases closed it, and two pairing attempts then ran the full churn with no reset."

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
  title: "Build versions in use: SDK 2.3.36, the three prebuilt toolchains and where they come from, and the open FPGA flow's pinned forks"
  status: VERIFIED
  verified_date: 2026-10-14
  statement: "The BL616 firmware is built against upstream Bouffalo SDK 2.3.36, a shallow clone at commit 8a3df34c in ~/.cache/tangcore-dev/sdk-2.3.36 identified by its CHANGELOG's v2.3.36, with the Xuantie-900 elf newlib toolchain V2.6.1 B-20220906 (riscv64-unknown-elf-gcc 10.2.0) in ~/.cache/tangcore-dev/toolchain/bin, driven by CMake and the SDK's project.build. That toolchain directory is a git checkout of bouffalolab/toolchain_gcc_t-head_linux at c4afe91 ('update to v2.6.1 20220906'); on 2026-10-12 Bouffalo replaced that repository's history with one commit, fa91b7e, whose README points to the release tag `default` of bouffalolab/toolchain, so c4afe91 is on no branch, though GitHub still serves it by SHA. That release's xuantie-gcc-riscv-v2.6.1-linux-x86_64.tar.xz (99,375,160 B, SHA-256 83817bd7cab693fc249bb98869de5e475c1ede577f4a188869fed96c8dbb8e1a) unpacks to a tree identical file for file to the c4afe91 checkout; the `default` tag is not immutable, since its macOS asset was replaced a day after publication. The configuration comes from defconfig through the SDK's Kconfig: cmake/bouffalo_sdk-config.cmake applies generated/autoconfig.cmake after the project's proj.conf. build/generated/sdk_version.h no longer names the SDK: its PROJECT_SDK_VERSION is CONFIG_PROJECT_SDK_VERSION, an application field whose Kconfig default is 1.1.1. The previous SDK, nand2mario's fork at 7f44f9e ('fixes for C++ build', reporting 2.0.0), stays in ~/.cache/tangcore-dev/sdk for rollback only. desktop.bin is built in this project by the open flow, tools/build_desktop_core_open.sh through tools/desktop_open/build.sh, for GW5AST-LV138PG484AC1/I0: Yosys 0.69+260 (git dcd59343b) with read_slang, from oss-cad-suite 20261008 in ~/oss-cad-suite, unpacked from YosysHQ's oss-cad-suite-linux-x64-20261008.tgz (SHA-256 8fa59d93d6cadbbee352077c059f2c5b72cda301e4508d9064a23d1ec11579c7) and differing from it only in Python bytecode caches; nextpnr-himbaechel from aquasock/nextpnr at 8be8657b, built for GW5AST-138C into ~/tools/nextpnr-mathieufro by ~/tools/rebuild-nextpnr.sh; and apicula from aquasock/apicula at 04bc3c5, whose generated GW5AST-138C database deps.conf pins by digest (aecb123e44cd, reproduced from a clean worktree). The AE350 firmware inside the TinyNES core is built with xPack riscv-none-elf-gcc 15.2.0-1 in ~/tools/xpack-riscv-none-elf-gcc-15.2.0-1, identical file for file to xPack's xpack-riscv-none-elf-gcc-15.2.0-1-linux-x64.tar.gz (SHA-256 aaaa8060c914851a3e5ee1ba82cc3d6f80972f90638a05c6e823a37557a33758, matching the publisher's .sha file). Gowin EDA 1.9.11.03 (~/tools/gowin-1.9.11.03) builds the NES and menu cores from nand2mario/nestang at c2450818, and through tools/build_desktop_core.sh a Gowin build of the desktop core kept for comparison only."
  consequence: "BL_SDK_BASE and TOOLCHAIN_BIN are overridable in the environment. Since core-log entry 41 the Makefile and tools/tests/test_fat_names.sh default BL_SDK_BASE to sdk-2.3.36; before that they defaulted to the fork, and every 2.3.36 build since entry 24 passed BL_SDK_BASE explicitly. Which SDK a build used shows in the BL_SDK_BASE paths of the build output, not in sdk_version.h. deps.conf pins the three prebuilt toolchains by their archives' SHA-256 and tools/fetch-deps.sh downloads, verifies and unpacks them, so the Xuantie compiler no longer depends on the orphaned c4afe91; if Bouffalo replaces or removes the archive, the digest still identifies the right one but nothing else hosts it. tools/build_desktop_core_open.sh defaults to seed 3, which with pll_nes at PLL_R[0] reproduces the card's desktop.bin (24a11e81a46c, desktop ABI 1.3) and passes both build gates with nothing overridden, the clock-topology profile having been re-recorded from that core (core-log entry 95); seed 26 of ABI 1.2 (16c6c5998c9a) is rebuilt from commit 97cf0db with nextpnr 7d1b131c, apicula cd3893f and the chipdb 446b89c0e96e pinned there; v0.1.0's seed-1 core b9af4f06221e is rebuilt from that tag. The fork sources that nextpnr is rebuilt from, /run/media/vash/GIT/nextpnr-mathieufro and apicula-mathieufro, are on removable storage."
  sources:
    - "~/.cache/tangcore-dev/sdk-2.3.36: git HEAD 8a3df34c; CHANGELOG v2.3.36; Kconfig 'config PROJECT_SDK_VERSION' default \"1.1.1\"; cmake/bouffalo_sdk-config.cmake including generated/autoconfig.cmake"
    - "~/.cache/tangcore-dev/sdk: git HEAD 7f44f9e 'fixes for C++ build'"
    - "riscv64-unknown-elf-gcc --version: Xuantie-900 elf newlib gcc Toolchain V2.6.1 B-20220906, 10.2.0"
    - "~/.cache/tangcore-dev/toolchain: git HEAD c4afe91cbd01bf7dce525e0d23b4219c8691e8f0, clean, origin bouffalolab/toolchain_gcc_t-head_linux; that repository's master is fa91b7e (2026-10-12, README 'We already move this. See https://github.com/bouffalolab/toolchain/releases/tag/default'), with no common ancestor with c4afe91 per the GitHub compare API; git fetch --depth 1 of c4afe91 succeeded on 2026-10-14"
    - "github.com/bouffalolab/toolchain release `default`: asset xuantie-gcc-riscv-v2.6.1-linux-x86_64.tar.xz, digest sha256:83817bd7..., updated 2026-10-12T09:49:56Z; xuantie-gcc-riscv-v2.10.2-macos-arm64.tar.xz updated 2026-10-13"
    - "github.com/YosysHQ/oss-cad-suite-build release 2026-10-08: oss-cad-suite-linux-x64-20261008.tgz, 747,604,392 B, digest sha256:8fa59d93...; ~/oss-cad-suite.tgz has the same digest"
    - "github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack release v15.2.0-1: xpack-riscv-none-elf-gcc-15.2.0-1-linux-x64.tar.gz, 433,494,794 B, digest sha256:aaaa8060..., and its .sha file"
    - "This project's Makefile (BL_SDK_BASE, TOOLCHAIN_BIN), deps.conf, tools/fetch-deps.sh, tools/build_desktop_core_open.sh (NEXTPNR_SEED default 26), tools/desktop_open/build.sh, tools/build_desktop_core.sh, BUILDING.md"
    - "~/oss-cad-suite/VERSION 20261008; yosys -V: Yosys 0.69+260 (git sha1 dcd59343b)"
    - "/run/media/vash/GIT/nextpnr-mathieufro: HEAD 8be8657b; ~/tools/rebuild-nextpnr.sh: -DARCH=himbaechel -DHIMBAECHEL_UARCH=gowin -DHIMBAECHEL_GOWIN_DEVICES=GW5AST-138C"
    - "/run/media/vash/GIT/apicula-mathieufro: HEAD 04bc3c5 'GW5AST-138C: spine select ports hold VSS, not VCC'"
    - "nand2mario/nestang c2450818e1f0c858e13c5dd16746ee5221a5c760 ('upgrade iosys', 2025-05-01), two commits behind master; third_party/patches/README.md and tangcore's gitlink name it"
  verification: "Read on 2026-10-11 from the SDK trees, the toolchain, oss-cad-suite, the build scripts and the two fork checkouts. Firmware 6715393-dirty.a597e26, built on SDK 2.3.36, runs on the board with an open-flow desktop.bin (core-log entry 40). Revised 2026-10-11 (core-log entry 41): replaced SDK 2.0.0 and 'nothing here builds FPGA logic' with SDK 2.3.36 and the fork kept for rollback, the toolchain's Xuantie identity, the open flow's pins and Gowin's remaining role; with the Makefile default changed, a build with no BL_SDK_BASE override compiled against sdk-2.3.36. Revised 2026-10-14 (core-log entry 59): added where each prebuilt toolchain comes from and its archive digest, each archive downloaded, checked against the publisher's digest, unpacked and compared with the installed tree by diff -r; recorded the Xuantie repository's rewrite; moved the fork pins from 7ed099ec and 93761f4 to the 7d1b131c and cd3893f the checkouts and deps.conf hold; named nestang's pin; and replaced 'seed 2 by default' as the build of record with the seed-1 discrepancy. Revised 2026-10-14 (core-log entry 60): the script defaults to seed 1, so the discrepancy is gone. Revised 2026-10-14 (core-log entry 67): the default is now seed 26, the board-tested ABI 1.2 core. Revised 2026-10-18 (core-log entry 95): the open flow's pins moved to nextpnr 8be8657b, apicula 04bc3c5 and chipdb aecb123e44cd, and the default seed to 3, the ABI 1.3 core of record."

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
  verified_date: 2026-10-11
  statement: "The configurations used to isolate TOOL-001 survive as three SDK config files - proj.min.conf, proj.nonewlib.conf and proj.rtos.conf - alongside the SDK's own device example vendored under ref/ as cdc_acm_template.c, ref_main.c and ref_rtos_main.c and built from inside this project. CMakeLists.txt picks them from the environment: TINYTANG_MIN, TINYTANG_RTOS and TINYTANG_NONEWLIB include their config file in place of proj.conf; TINYTANG_REF, or TINYTANG_RTOS, builds ref/cdc_acm_template.c with ref_main.c or ref_rtos_main.c in place of the firmware; TINYTANG_USB_ONLY adds -DTINYTANG_USB_ONLY=1, with which main.cpp brings up USB and then idles with no scheduler or shell. TINYTANG_NO_FS and TINYTANG_NO_SHELL were removed in core-log entry 32. The bisection was made on the SDK fork at 7f44f9e (TOOL-003). Under SDK 2.3.36 the configuration comes from defconfig through Kconfig, applied after proj.conf, so the three config files no longer change the build."
  consequence: "The negative results in TOOL-001 have a reproducible witness against the SDK they were found on: with BL_SDK_BASE set to the fork, setting a variable and rebuilding re-runs the bisection. On 2.3.36 only ref/ and TINYTANG_USB_ONLY still do what their names say; reproducing the bisection there would mean editing defconfig. Anyone removing these files or guards should know they are the evidence for why CONFIG_CHERRYUSB_HOST is set and CONFIG_NEWLIB is not."
  sources:
    - "This project's CMakeLists.txt lines 1-45: the TINYTANG_* guards and the config selection; main.cpp line 64, the TINYTANG_USB_ONLY idle loop"
    - "This project's proj.min.conf, proj.nonewlib.conf, proj.rtos.conf, defconfig and ref/"
    - "Bouffalo SDK 2.3.36: cmake/bouffalo_sdk-config.cmake including generated/autoconfig.cmake"
  verification: "The files and guards were read directly on 2026-10-11. ref/ was refreshed for 2.3.36 and enumerated on the board as ffff:ffff CherryUSB_CDC_DEMO (core-log entry 25). That the three config files produced TOOL-001's result is this project's own account and was not re-run. Revised 2026-10-11 (core-log entry 41): dropped TINYTANG_NO_FS and TINYTANG_NO_SHELL, and recorded that the config files reproduce the bisection only on the fork."

- record_id: TOOL-006
  kind: TOOLCHAIN
  topic_id: TOOL
  title: "This project's host-side tools, and what they need"
  status: VERIFIED
  verified_date: 2026-10-11
  statement: "The board-facing tools are Python scripts in tools/ that need pyserial: tinytang_flash.py writes a firmware image to the BL616 over the CDC with no BOOT button involved, tinytang_put.py puts a file onto the SD card, tinytang_run.py runs shell commands on the board and streams their output, and tinytang_console.py is the console guard they share (TOOL-011). ble_trace_to_btsnoop.py needs no hardware: it turns the text of the board's `bletrace dump` into a btsnoop file (monitor datalink 2001) that `btmon -r` decodes. The board records only while `bletrace clear` has allocated its 16 KB buffer, and `bletrace free` returns it to the heap (BL6-010). The other scripts in tools/ generate test files or check the tree. The retired Tang-Control helpers are recorded separately in TCTL-010. A build machine therefore needs Python with pyserial in addition to the SDK and the RISC-V toolchain of TOOL-003."
  consequence: "These are the only supported ways to reflash the board and to place files on the card. tinytang_flash.py needs the CDC port free and a power cycle afterwards (FLS-002), and a live screen session holds the port; run without a terminal it needs --yes, or its confirmation prompt blocks while holding the port. One command's console output is cut off after about 4.5 KB, pacing the output does not help, and shell redirection does not capture tdsh_printf, so `bletrace dump` is fetched in ranges (`bletrace dump <from> <count>`, 80 packets at a time) and the pieces joined. tinytang_run.py's --seconds is a window for streaming output after the command, not a delay, and Bluetooth log lines are handed out once, so its output is saved to a file whole before it is filtered."
  sources:
    - "This project's tools/: tinytang_flash.py, tinytang_put.py, tinytang_run.py, tinytang_console.py, ble_trace_to_btsnoop.py"
    - "This project's README.md: 'Python with pyserial for the tools in tools/'"
    - "This project's ports/bl616/usb_cdc_bl616.c console_flush() and ports/bl616/tang_ble_trace.c"
  verification: "All board-facing tools were used against this board. On 2026-10-11 a 227-packet trace dumped whole came back cut at 137 lines (about 4.5 KB) with or without a one-tick pause per line, and fetched in three ranges came back complete, identical where they overlapped and in time order; the converter's output was decoded by btmon 5.85 (core-log entry 14). Revised 2026-10-11 (core-log entry 14): the record listed only three tools. Revised 2026-10-12 (core-log entry 42): the trace buffer became an on-demand allocation."

- record_id: TOOL-007
  kind: TOOLCHAIN
  topic_id: TOOL
  title: "The vendored Gowin JTAG programmer is Apache-2.0, from nand2mario via Tang-Control"
  status: VERIFIED
  verified_date: 2026-10-03
  statement: "ports/bl616/tang_jtag_programmer.c is nand2mario's bit-banged GPIO JTAG programmer for Gowin GW5A and GW2A, (c) 2025.2, licensed Apache-2.0, itself based in part on openFPGALoader by Gwenhael Goavec-Merou. It was taken from Tang-Control's fpga/programmer.cpp and used unmodified apart from its include list. ports/bl616/tang_jtag_glue.h supplies what it expected from its own tree - the console, the card file, the GPIO device handle, and stubs for the FPGA UART bookkeeping - and documents that provenance."
  consequence: "This is working code inherited from the previous firmware rather than reimplemented, so a claim that only board knowledge was carried over is not quite right: the programmer is a translation unit. Its licence is Apache-2.0 and travels with the file, which is a different licence from the MIT TinyDesk shell submodules (TDSH-004, TDESK-013)."
  sources:
    - "This project's ports/bl616/tang_jtag_programmer.c: the Apache-2.0 header, the 2025.2 nand2mario copyright and the openFPGALoader attribution"
    - "This project's ports/bl616/tang_jtag_glue.h: the provenance comment"
  verification: "Read from the file header and the glue header in the tree. The programmer is exercised on every tangload, which reports ID=0001081b (DEV-001)."

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

- record_id: TOOL-011
  kind: TOOLCHAIN
  topic_id: TOOL
  title: "The console's input is exclusive, so the tools ask the board what holds it"
  status: VERIFIED
  verified_date: 2026-10-05
  statement: "The CDC console is one byte stream with one reader at a time: at the shell prompt a byte is part of a command line, while the desktop runs it is a keystroke into whichever window has focus, and while a command such as tangput runs it is that command's data. Nothing on the wire distinguishes them. The firmware therefore answers a status probe that never reaches any reader: the USB receive interrupt takes ESC [ ? 7 7 n out of the input, and the next task to read the console replies ESC [ ? 7 7 ; <state> n, with 1 at the shell prompt, 2 while the desktop runs and 3 for a command that reads the console. A command that reads nothing does not answer until it ends."
  consequence: "Before the probe the console had no such answer, and a raw upload sent while the desktop held it became keystrokes: on 2026-10-04 a 131088-byte ROM produced root-directory entries named new.txt and New folder with binary data for names and damaged the allocation table until /cores, /scripts and /roms were unreachable, with no error on either side; the recovery is fsck.vfat from a PC before any reformat. The first guard, in tools/tinytang_put.py, looked for the desktop's drawing on USB and sent a carriage return to wake a prompt. Once the desktop's drawing left USB (USB-007) it could not see the desktop, its carriage return was Enter in whatever window had focus, and with the Terminal window open the shell in it printed a real prompt to USB, so the guard could pass with the desktop up. USB-007 suggested following the desktop's start and exit lines instead; asking the board was chosen because it types nothing and does not depend on having seen the start of the session. tools/tinytang_console.py's require_shell() is used by tinytang_put.py, tinytang_flash.py, phosphor_format_sweep.py and tinytang_run.py, which had no guard before and now asks before each line, so a second command waits for the first to end. No answer is treated as not ready, which also covers a firmware that predates the probe; flashing the first firmware with the probe needed the old guard and the user's word that the console was at a prompt. The probe is matched within one USB packet, so a lone Esc typed at a terminal is not held, and tangput turns the matching off while it receives, because a file may contain the sequence."
  sources:
    - "This project's ports/bl616/usb_cdc_bl616.c: usbd_cdc_acm_bulk_out()'s probe matcher, tdsh_bl616_console_set_raw() and the reply in tdsh_bl616_console_read_byte()"
    - "This project's ports/bl616/tdsh_platform_bl616.c: tdsh_bl616_console_state(), set around bl616_readline and, through tdsh_bl616_console_set_desktop(), around desktop_run() in td_desktop_bl616.c"
    - "This project's tools/tinytang_console.py: PROBE, REPLY, console_state() and require_shell()"
  verification: "The FAT damage was read back from the board on 2026-10-04, and the same send succeeded once the desktop was exited. Exercised on this board with firmware e912c5d-dirty.3417d90, 2026-10-05: state 1 at the prompt; no answer within 1 s during 'sleep 5' and state 1 4.8 s later, when the prompt returned; tinytang_run.py held a second command until 'sleep 3' ended; a 6120-byte file holding twenty copies of the probe arrived whole through tangput; and with the desktop and its Terminal window up the answer was 2, tinytang_run.py and tinytang_put.py both refused, and the user saw nothing typed on screen. State 3 was not observed, since no command that reads the console was running during the probe; the busy case was seen only as no answer. Revised 2026-10-11 (core-log entry 10): folded in the upload incident from the retired TOOL-009."

- record_id: TOOL-012
  kind: TOOLCHAIN
  topic_id: TOOL
  title: "One-wire debugging reaches the FPGA directly, and nothing of TinyTang or TinyDesk"
  status: VERIFIED
  verified_date: 2026-10-05
  statement: "With the board in one-wire mode (BRD-006) the host drives the FPGA without the BL616's firmware in between. JTAG is FT2232 interface 0: openFPGALoader v0.13.1 with -c ft2232 --detect scanned the chain at 6 MHz and reported one Gowin GW5AST-138, IDCODE 0x0001081b (TOOL-002), and programmed nothing. Loading a core is an SRAM load and must use the .fs stream; a raw .bin is shifted in but does not start the FPGA, which leaves it unconfigured and the UART silent. The FPGA UART is interface 1, /dev/ttyUSB1, at 2,000,000 baud with the same 0xAA len_hi len_lo type framing the BL616 uses: the core-ID request AA 00 01 01 was answered AA 00 02 01 00, core 0. A core speaks only when asked, so four seconds of listening at 2 Mbaud and at 115200 with nothing sent returned no bytes; that silence is not evidence either way."
  consequence: "One-wire is for the FPGA alone: identifying it, loading a bitstream into SRAM, and the 0xAA and extended (0x10) protocols from the host, which is how Tang-Phosphor developed its core (tools/fpga_uart.py, scripts/flash-otg.sh, scripts/uart_probe.py). It cannot see TinyTang or TinyDesk at all: the CDC, the shell, the status probe, tinytang_put/run/flash.py, phosphor, tangload and nesload all need two-wire. It also cannot inspect a fault reached under TinyDesk, because getting into one-wire takes a cold power-up that puts the FPGA back on its own core, and a core loaded over one-wire is replaced at the next two-wire boot when boot.tdsh loads the menu core. The port is opened with DTR and RTS held low so opening it moves no control line; with the BL616 running the debugger, the host is the only other party on the UART."
  sources:
    - "Observed on this board, 2026-10-05: openFPGALoader --detect output; AA 00 01 01 sent once on /dev/ttyUSB1 at 2 Mbaud and the five reply bytes; udevadm interface numbers for ttyUSB0 and ttyUSB1"
    - "Tang-Phosphor, scripts/flash-otg.sh and tools/fpga_uart.py at 532e19d; its core-log entry 29 for the .fs-versus-.bin cause"
    - "This project's ports/bl616/tang_fpga_link.h: the frame layout and FPGA_CMD_CORE_ID 0x01"
  verification: "JTAG detect and the core-ID round trip were run on this board on 2026-10-05 with no change to the FPGA, which answered core 0 before and after. The .fs requirement is Tang-Phosphor's finding and was not repeated here, since no core was loaded over one-wire."

- record_id: TOOL-013
  kind: TOOLCHAIN
  topic_id: TOOL
  title: "The one-wire core loop: deploy over JTAG, then debug and exercise over the UART"
  status: VERIFIED
  verified_date: 2026-10-05
  statement: "With the board in one-wire mode (BRD-006) a core is deployed, debugged and exercised from the host with no BL616 firmware involved. Deploy: Tang-Phosphor's scripts/flash-otg.sh <image>.fs runs oss-cad-suite's openFPGALoader v1.1.1 with -b tangmega138k and loads SRAM; the merged place3 image (build/merged/place3/tang_phosphor_merged.fs, 39929717 bytes, whose .bin twin is the card's phosphortang.bin at 4987082 bytes) loaded in 17 s. The script has no execute bit, so it is run with bash. Identify: AA 00 01 01 on /dev/ttyUSB1 at 2 Mbaud answered AA 00 02 01 50, core 0x50. Debug: tools/fpga_uart.py peek and poke work over the extended 0x10 channel; the scratch register 0x0020 (docs/debug-registers.md) took 0xC0FFEE42 and 0x12345678 and read both back, and was restored to 0. Exercise: scripts/play_stream.py --tpi <player> --input <file> sets cpu_mode, restarts the AE350 loader, streams the player and then the file, and polls 0x4020 for the run to complete; the qualified player (PHOS-008) took 10.1 s and a 402304-byte test.mp3 4.0 s, and the core then reported 441000 samples played at 0x68, 0 underruns at 0x6c and 44100 Hz at 0x70, with one completed run in 0x4020."
  consequence: "This is the loop for working on a core by itself: rebuild, SRAM-load, peek and poke its registers, stream to it, all without touching the card or reflashing anything, and a power cycle with the power input returns the board to TinyTang with the core gone, since SRAM loads are volatile. At 2 Mbaud the link moves about 85 KB/s, so the player upload dominates; the extended channel's baud switch to 5 Mbaud (EXTCTL-001 opcode 0x03) is the next speed-up and has not been tried in one-wire. Two cautions carry over from Tang-Phosphor: a .bin does not start the FPGA (TOOL-012), and the player image must be the qualified one (PHOS-008). The MP3 result equals Tang-Phosphor entry 43's figure for the same file. Whether the tone was heard cleanly was not reported by the user."
  sources:
    - "Tang-Phosphor at 9e6f183: scripts/flash-otg.sh, scripts/play_stream.py, tools/fpga_uart.py, docs/debug-registers.md"
    - "This project's tools/make_codec_corpus.sh, which regenerated test.mp3 (402304 bytes) and test.flac (131601 bytes, the size Tang-Phosphor entry 69 recorded)"
    - "Observed on this board, 2026-10-05"
  verification: "Every step above was run on this board on 2026-10-05 in one-wire mode with the reported figures read back over the UART."

- record_id: TOOL-014
  kind: TOOLCHAIN
  topic_id: TOOL
  title: "A Pico 2 CMSIS-DAP probe on U1201: a JTAG path of its own, beside one-wire and two-wire"
  status: SOURCED
  verified_date: 2026-10-05
  statement: "A Raspberry Pi Pico 2 running CMSIS-DAP JTAG-probe firmware is wired to the FPGA module's 8-pin header U1201 (PSX-004) and plugs into the host as a separate USB device, 2e8a:000c, not through the BL616 or the FT2232. The user reports it set up and validated, and plugs it in on request. Tang-Phosphor's scripts/flash-pico.sh SRAM-loads a .fs with oss-cad-suite's openFPGALoader -c cmsisdap --vid 0x2e8a --pid 0x000c -b tangmega138k --freq 2000000 (PICO_VID, PICO_PID and PICO_FREQ override); like flash-otg.sh it refuses a .bin, which does not start the FPGA (TOOL-012). The probe bit-bangs JTAG, its clock must stay at or below 2 MHz (4 MHz and up read garbage IDCODEs), and its 64-byte bulk endpoint makes a full bitstream take about 10 to 20 minutes. Tang-Phosphor's README names its real value as GAO, Gowin's internal logic analyser, over the module's debug connector. Tang-PSX found the same probe (lonehog/JTAGprobe, CMSIS-DAP v2; TCK GP19, TMS GP14, TDI GP18, TDO GP21) under OpenOCD 0.12.0 with IDCODE 0x0001081B, where openFPGALoader 0.13.1 could not open it; Tang-Phosphor's later openFPGALoader from oss-cad-suite read the same IDCODE through it."
  consequence: "This is the third way to reach the FPGA, beside the BL616's tangload in two-wire and the FT2232 in one-wire (TOOL-012, TOOL-013). Because it is its own USB device on its own header, it is not bound to the power-input choice that makes one-wire and two-wire exclusive (BRD-006, BRD-007): Tang-Control lists Pico 2 JTAG programming as part of its two-wire debug arrangement (TCTL-005), although Tang-Phosphor's README files flash-pico.sh under one-wire. So it is the candidate for JTAG access while TinyTang runs: GAO captures, a --detect or status read that checks a tangload's result independently of the BL616's own report, or a core load when the card or tangload is in question. It is too slow for routine loads; use tangload in two-wire or flash-otg.sh in one-wire. Two rules come from PSX-004: the probe shares TCK, TMS and TDI with the BL616, so it must be idle (or unplugged) whenever TinyTang runs tangload, and only pins 2 to 5 and 8 are wired to it, never pin 1 (about 4.4 V) or pins 6 and 7 (the core UART). A core loaded through it under TinyTang replaces the running core without the firmware being told, so the desktop layer and the Phosphor playback task do not know the core changed."
  sources:
    - "Tang-Phosphor at 4936ed1: scripts/flash-pico.sh (added in 67979d8, core-log entry 30) and README.md, section 'Pico 2 JTAG probe'"
    - "Tang-PSX, .ai/core-reference.md record BRD-006 (copied here as PSX-004)"
    - "TCTL-005: Tang-Control's two-wire debug arrangement, which includes JTAG programming through a Pico 2"
    - "The user, 2026-10-05: the probe is set up, validated and available"
  verification: "Not yet run by this project. Tang-Phosphor entry 30 verified the probe against the GW5AST-138 (IDCODE 0x1081b) at 2 MHz; Tang-PSX confirmed it under OpenOCD on 2026-09-29 (its core-log entry 44). Whether it works alongside a running TinyTang in two-wire is Tang-Control's statement and has not been tried on this board."

- record_id: TOOL-015
  kind: TOOLCHAIN
  topic_id: TOOL
  title: "Reentrant FatFS takes an RTOS mutex in every call, so no file call may run inside a critical section"
  status: VERIFIED
  verified_date: 2026-10-05
  statement: "The SDK's FatFS is R0.15 (components/fs/fatfs). With FF_FS_REENTRANT 1 every public file function locks the volume on entry and unlocks it on exit; under OS_TYPE 3 ffsystem.c implements that as xSemaphoreTake(Mutex[vol], FF_FS_TIMEOUT) and xSemaphoreGive on a FreeRTOS mutex, one per volume plus one for the system, and a take that times out makes the call return FR_TIMEOUT. FreeRTOS documents that its API functions must not be called from inside a taskENTER_CRITICAL section, where the scheduler and the tick are held off, so a take there cannot block safely and a give that wakes a higher-priority waiter yields from inside the section."
  consequence: "Since this project turned FF_FS_REENTRANT on (fatfs_conf_user.h), no f_* call may run inside a critical section. nand2mario's JTAG programmer (TOOL-007) read the bitstream with f_read inside one; it worked at the bare console, where nothing else used the card, and failed tangload from TinyDesk's Terminal, where the desktop also uses it. The programmer now reads each block outside the section and shifts it to the FPGA inside, which still keeps the shift's whole-register GPIO writes free of interruption. FF_FS_TIMEOUT is in ticks, 1000 here, so a task that holds the volume for longer than a second makes the other user's call fail rather than wait."
  sources:
    - "Bouffalo SDK 2.0.0 at 7f44f9e (TOOL-003): components/fs/fatfs/ff.h (R0.15), ff.c lock_volume and unlock_volume, ffsystem.c ff_mutex_create, ff_mutex_take and ff_mutex_give under OS_TYPE == 3"
    - "FreeRTOS kernel reference, taskENTER_CRITICAL(): FreeRTOS API functions must not be called from within a critical section, https://www.freertos.org/Documentation/02-Kernel/04-API-references/04-RTOS-kernel-control/01-taskENTER_CRITICAL_taskEXIT_CRITICAL"
    - "ports/bl616/tang_jtag_programmer.c, fpga_program, the JTAG_FAST path"
  verification: "Observed on this board on 2026-10-05: with FF_FS_REENTRANT on, tangload of phosphortang.bin failed from TinyDesk's Terminal and succeeded at the bare console; after the reads were moved out of the critical section it succeeded at the console and, by the user's test, from the desktop. Which of the blocked take or the yielding give broke the load was not traced; either is outside what FreeRTOS allows."

- record_id: TOOL-016
  kind: TOOLCHAIN
  topic_id: TOOL
  title: "The SDK components this firmware links, and their licences, now including the Bluetooth stack"
  status: VERIFIED
  verified_date: 2026-10-05
  statement: "The Bouffalo SDK is Apache-2.0 for its own code but bundles third-party components under their own licences, and these are linked into this firmware: FreeRTOS Kernel V10.4.6, MIT, Copyright (C) 2021 Amazon.com, Inc. or its affiliates, selected by CONFIG_FREERTOS in proj.conf; CherryUSB, Apache-2.0, Copyright (C) 2006 Bertrik Sikken, (c) 2016 Intel Corporation and (c) 2022 sakumisu, which is the device CDC console and the FreeRTOS OSAL hosting it; FatFs R0.15 w/patch3, Copyright (C) 2022 ChaN, whose condition reaches only source redistribution. Since blescan and blekbd (BLE-002) the firmware also links the SDK's Bluetooth stack, in three parts. The host, libblestack.a built from the SDK's source, is derived from Zephyr's Bluetooth host: its linked files are Apache-2.0 by SPDX tag, copyright Intel Corporation 2015-2018, Nordic Semiconductor ASA 2016-2017, Wind River Systems 2011-2014 and 2016-2017, Oticon A/S 2019 and Vinayak Kariappa Chettimada 2016, with Bouffalo Lab port files (2018, 2019) that carry no licence header. Inside the host, pairing's cryptography is TinyCrypt, Copyright (C) 2017 Intel Corporation under a BSD-3-Clause text; micro-ecc, Copyright (c) 2014 Kenneth MacKay under a BSD-2-Clause text, in ecc.c, ecc_dh.c, ecc_dsa.c and ecc_platform_specific.c; and ctr_prng.c, Copyright (c) 2016 Chris Morrison under a BSD-2-Clause text. All three BSD texts require a distribution in binary form to reproduce the notice, conditions and disclaimer in its documentation. The controller, libbtblecontroller_bl616_ble1m10s1bredr0.a, and the radio's libbl616_phyrf.a and librfparam.a are prebuilt binaries with no licence of their own; the controller's port file btblecontroller_port_uart.c reads 'Copyright (C) RivieraWaves 2009-2015', so the controller is CEVA RivieraWaves IP, and the SDK's root Apache-2.0 is the only grant found covering its redistribution. Not linked: the SDK's LVGL, TJpgDec, mbedTLS, littlefs and multimedia codecs."
  consequence: "Naming the SDK is not naming what the SDK carries. The firmware image is a binary distribution of all of these, so FreeRTOS's MIT text and the BSD notices must travel with it: their texts are reproduced verbatim in LICENSES/BSD-3-Clause-tinycrypt.txt, LICENSES/BSD-2-Clause-micro-ecc.txt and LICENSES/BSD-2-Clause-tinycrypt-ctr_prng.txt, and THIRD_PARTY.md names every part of the stack and its holders. Anyone publishing a firmware image should ship THIRD_PARTY.md and LICENSES/ with it. Whether CEVA or Bouffalo place any condition on the controller blob beyond the SDK's Apache-2.0 is not stated anywhere in the SDK; if a release ever depends on that, ask Bouffalo rather than infer it. Enabling CONFIG_BT_SETTINGS, mbedTLS or another SDK component later adds that component's licence at that moment."
  sources:
    - "build/build_out/tinytang_bl616.map: the 38 libblestack.a objects linked, and the controller, phyrf and rfparam archives"
    - "Bouffalo SDK at 7f44f9e (TOOL-003), components/wireless/bluetooth/blestack/src: the copyright and SPDX lines of each linked object's source; src/common/tinycrypt/source/aes_encrypt.c, ecc.c and ctr_prng.c for the three BSD texts"
    - "Bouffalo SDK components/wireless/bluetooth/btblecontroller/btblecontroller_port/btblecontroller_port_uart.c: 'Copyright (C) RivieraWaves 2009-2015'"
    - "Bouffalo SDK root LICENSE: Apache-2.0; the SDK's only per-component licence files are for MQTT-C, TinyMaix, cmake and pikapython, none linked here"
    - "Bouffalo SDK components/os/freertos/tasks.c: 'FreeRTOS Kernel V10.4.6 / Copyright (C) 2021 Amazon.com, Inc. or its affiliates. / SPDX-License-Identifier: MIT'"
    - "Bouffalo SDK components/usb/cherryusb/core/usbd_core.c: the Apache-2.0 SPDX line and the three copyright holders"
    - "Bouffalo SDK components/fs/fatfs/ff.c: 'FatFs - Generic FAT Filesystem Module R0.15 w/patch3' and ChaN's condition"
    - "This project's proj.conf: set(CONFIG_FREERTOS 1)"
  verification: "The linked-object list was taken from this build's map, and each object's source header was read from the SDK; the LICENSES texts were extracted programmatically from the SDK files named above, comment markers removed and wording unchanged. strings over the controller and PHY archives found no licence or copyright text. The FreeRTOS, CherryUSB and FatFs lines were read from the SDK's own source headers on 2026-10-03, and the not-linked list was checked against this project's configuration and sources. Revised 2026-10-11 (core-log entry 10): folded in the retired TOOL-008, so this record stands alone."

- record_id: TOOL-017
  kind: TOOLCHAIN
  topic_id: TOOL
  title: "The open-flow desktop core drives HDMI and VGA with the apicula fork at 93761f4 or later and nextpnr rebuilt against its chipdb; the fault was the packer's IOLOGIC encoding and the cell-to-HCLK-block map, never the clock network"
  status: VERIFIED
  verified_date: 2026-10-11
  statement: "GW5AST-LV138PG484AC1/I0 revision C, open flow (yosys, the nextpnr-himbaechel fork, the apicula fork aquasock/apicula epic/gw5ast138c).  Before fork commit 1c2b6cb the packer wrote three things differently from Gowin EDA 1.9.11.03, each decoded with gowin_unpack and each fatal to HDMI on the board: (1) an OSER10's aux IOLOGICB was left empty, where the vendor writes CLKOMUX=ENABLE, LSRIMUX_0, OCLKCE=CE, WRFCLKSEL=UNK102 and the main half's FCLKSEL values -- at right-edge and bottom-edge cells, single-ended or differential, and in the fork's own io_ser oser10-default shape rebuilt today, which retracts fork commit 84dddea for OSER10; (2) an OSER10's main half lacked HWL=TRUE and carried LSROMUX_0, and a gearbox whose FCLK is on HCLK lane 0 lacked FCLKSEL0=HCLK0 and FCLKSEL3=HCLK0_ (lane 2 has no rows for them); (3) an ODDR's LSROMUX_0 was emitted as the int 0, which has no row, so the table's negative-key fuse was written instead ((21,66) on tile type 247, (11,134) on 245/232).  1c2b6cb fixes all three.  A fourth fault was in the fork's chipdb: gw5_hclk_arcs split each long side between its two HCLK blocks at the row-63 inter-HCLK bridge, so io2hclk served rows 55 to 61 -- among them (57,181), the red TMDS serialiser, K17/J17 -- from blocks 0 and 1, nextpnr routed that serialiser's FCLK through block 1 lane 2, and HDMI was blank unless FCLKSEL0=HCLK0/FCLKSEL3=HCLK0_ were hand-added at R58C182 and R59C182.  A Gowin sweep of one OSER10 along both long edges, decoded only, served rows 41 to 52 from blocks 0/1 and rows 55 to 64 from blocks 2/3, with no fuses in the bridge cell; rows 53 and 54 cannot host an OSER10.  The split is the block midpoint, row 54, and fork commit 93761f4 makes it so, moving rows 55, 57, 59 and 61 on both edges and nothing else.  With it nextpnr places all three TMDS serialisers on block 3 lane 2, which carries no lane-0 bits, and HDMI works; the vendor uses lane 0 of the same block.  The bottom side's split (column 90/91) is unmeasured."
  consequence: "tools/build_desktop_core_open.sh is the build of record; it needs the apicula fork at 93761f4 or later, its chipdb regenerated, and the nextpnr fork rebuilt against that chipdb (~/tools/rebuild-nextpnr.sh).  tools/desktop_open/build.sh packs with the plain gowin_pack and adds no fuses.  The design is TinyTang's own, fpga/desktop/ with top module desktop_top and no nestang patches; on that netlist before the HDMI PLLs moved, every seed from 1 to 32 with pll_nes at PLL_R[0] failed timing, so from 2026-10-11 to 2026-10-13 fpga/desktop/desktop.cst placed pll_nes at PLL_L[0] and the build defaulted to seed 2, producing desktop.89f3ae9350c4.bin (desktop.6639878beff9.fs), now kept on the card as desktop.bin.pre-thermal-fix (7b21952ec575, the seed-17 nestang-netlist core, is kept as desktop.bin.pre-cleanup).  In that core the main clk reaches its 1199 sinks over fabric because the stock router refuses the MPLLCLKOUT0 to TLPLL0CLK0 hop, and it must stay so: every core that carried clk over TLPLL0CLK0, wholly (global resources only, setup and hold met) or in part (SPINE8 alone), loaded with a dead clk domain, because the chipdb's TLPLL0CLK0 to spine to quadrant-bridge model does not match the silicon (the centre SPINE8 to SPINE10 and SPINE16/18/19 are the same nodes as CBRIDGEOUT_TOP0..2 and CBRIDGEOUT_BOTTOM0/2/3, and Gowin's own build adds GT10 <- CBRIDGEOUT_BOTTOM0 at R28C87 and SPINE16 <- BRMDCLK1_BOT, which the chipdb does not model).  Writing PLL attribute 211 = 2 (DISABLE), the one PLL-attribute difference from Gowin, onto every PLL kills the HDMI pixel PLL too and must not be done.  hclk5 on fabric and the HCLK fabric entry work as they are, so the dark-HDMI fault needed no clock-network change; a later, separate thermal fault did.  fpga/desktop/desktop.cst now moves pll_27 and pll_hdmi to PLL_R[1] and PLL_R[3], pins div5 to RIGHTSIDE[4], sends the TMDS clock lane through a fourth OSER10, and moves pll_nes to PLL_R[0] so that `clk` enters the clock plane through TRPLL0CLK0 instead of the dead TLPLL0CLK0; that build passes setup and hold at seeds 1-4, and the user held the desktop picture clean to 60.4 C where the previous image was distorted by 55 C (core-log entry 52).  Its seed-1 core, desktop.b9af4f06221e.bin (desktop.7453ad6c9400.fs), was the card's desktop.bin from 2026-10-13 (core-log entry 53) and is the core v0.1.0 shipped. Desktop ABI 1.2 (the OLED's three font sets, core-log entry 67) changed the netlist, and no seed of it from 1 to 37 kept the pixel clock's fabric fallbacks at that core's 1; 21 met setup and hold. The best of them by that count, seed 6 (4 fallbacks), loaded with HDMI synced at 720p and black and the UART silent, so its clk domain was dead. Its clk route differs from the two working cores' (seed 1 of the ABI 1.1 netlist and seed 26 of the ABI 1.2 one) only in going through SPINE17 <- CBRIDGEOUT_TOP2 at X94Y81 and six GT00 <- SPINE17 taps; both working cores take TRPLL0CLK0 through SPINE8 and SPINE10 to SPINE16 <- CBRIDGEOUT_TOP0 and SPINE25 <- CBRIDGEOUT_TOP2 and never SPINE17, and only seeds 10 and 26 of the 37 avoid SPINE17. That SPINE17 hop is a lead for one more mis-modelled bridge node, not a finding: it rests on two dead cores and two working ones, and the ABI 1.1 netlist's seed 1 also lacks it. Since core-log entry 68 tools/desktop_open/clock_route_hops.py, step 2d of tools/desktop_open/build.sh, reads clk's route from pnr.json; since core-log entry 70 it refuses the build when any pip touches SPINE17 unless ALLOW_SPINE17_CLOCK is set, because seed 16, chosen for that hop, was loaded on the board on 2026-10-14 and failed as seed 6 did -- HDMI synced at 720p and black with the UART silent, while seed 26 answered on the same path -- so two dead cores now share the hop against two working ones; it flags seeds 6 and 16 and passes 10 and 26. Seed 26, desktop.16c6c5998c9a.bin (desktop.be6f111c0b53.fs), has 21 pixel-clock fallbacks, 11 of them in the HDMI text layer; the user accepted HDMI, VGA and all three OLED font sets, and a boot at 67.3 C, and it is the card's desktop.bin and the build's default since core-log entry 67, with tools/desktop_open/clock_profile.json recorded from it. clk is on global resources only through TRPLL0CLK0 there too, where the same verdict through TLPLL0CLK0 is the dead route above, so the topology gate tells neither apart and pll_nes must stay at PLL_R[0]; a changed netlist needs a seed sweep and a board test, not only the gates.  The 2026-10-11 image cannot be rebuilt: apycula's chipdb was then an untracked build product, every copy on this machine regenerates to a database that differs from that day's, and rebuilding it with the fork at 93761f4 and its own era's nextpnr gives a different timing model and routes `clk` on global resources.  The current image is rebuilt bit for bit by the default build against the chipdb deps.conf pins (446b89c0e96e), as v0.1.0's seed-1 core is from that tag.  tools/build_desktop_core.sh (Gowin) is kept only as a comparison build, and fpga/hdmi_test/ with tools/build_hdmi_test.sh (HDMI_TEST_RED_BALL, HDMI_TEST_FREE_BALLS), tools/bitstream_tile_diff.py and tools/gowin_pack_transplant.py are the instruments that found this."
  sources:
    - "This project's tools/build_desktop_core_open.sh, tools/desktop_open/build.sh, tools/gowin_pack_transplant.py, tools/bitstream_tile_diff.py, tools/build_hdmi_test.sh, fpga/hdmi_test/ and fpga/desktop/desktop.cst"
    - "Gowin EDA 1.9.11.03 builds of a one-PLL probe with CLKOUT0 on PLL_L[0] driving fabric, decoded with the fork's gowin_unpack against the open-flow core's PLL tile R28C2 and clock pips"
    - "aquasock/apicula epic/gw5ast138c 1c2b6cb and 93761f4, apycula/gowin_pack.py: GW5AST_138C.get_out_iologic_attrs, fclk_lane_attrs, oser10_aux_attrs, get_IOLOGIC_DUMMY_fuses; apycula/chipdb.py: gw5_hclk_arcs; tests/test_io_iologic_138c.py; fuzz/gw5ast138c/shapes/io_ser.py and io_basic.py"
    - "Gowin EDA 1.9.11.03 builds of fpga/hdmi_test/ (differential, single-ended, ball AA9, pinned dividers, red's OSER10 swept over rows 41 to 64 of both long edges) and of io_ser oser10-default, decoded with the fork's gowin_unpack"
  verification: "On this board on 2026-10-11, each test preceded by a Gowin control the user confirmed (core-log entries 33 to 39): open-flow test cores with the hand-added fuses and then with the fixed packer alone -- byte-identical for OSER10 at 480p (c02147242cb5) and 720p (c32651b53b9e) -- showed bars on HDMI and VGA; the fixed packer's ODDR core (0863cccdfdcc) showed bars; the desktop core from the fixed packer alone (3a9decc7a3ba) showed VGA and blank HDMI; with the four R58/R59 fuses added (67583d390cdc) it showed the console mirrored on HDMI and VGA; and with the corrected map and no added fuses (7b21952ec575) it showed the desktop on HDMI and VGA.  Revised 2026-10-11 (core-log entry 38): the packer faults are fixed in the fork and the remaining fault is the io2hclk map; entry 37's record had the 33 fuses as the fix.  Revised 2026-10-11 (core-log entry 39): the io2hclk map is corrected and the interim fuses are gone.  Revised 2026-10-11 (core-log entry 40): with each test preceded by the user's confirmation of the control core, the TinyTang-netlist cores that routed clk over TLPLL0CLK0 (rel1 global-only, S on SPINE8) and the attribute-211 core (D) showed no desktop, D not even HDMI sync, while desktop.89f3ae9350c4.bin, pll_nes at PLL_L[0], seed 2, clk on fabric, showed the desktop on HDMI and VGA and, copied to desktop.bin and loaded, declared the tang.ini sockets (0xc0 = 0x0230). Revised 2026-10-13 (core-log entry 47): the card's folders moved under the shell's home, /root (cores, scripts, music, ae350, ble and demos, with ROMs in /root/data/nes), and the firmware's paths followed; the paths above are the new ones. Revised 2026-10-13 (core-log entry 50): the card's desktop core shows the same temperature-dependent HDMI glitch that TinyNES's DEV-018 records, seen by the user on a hot board and clear again on cooling, and its HDMI clock-network change is the candidate fix no reviewed seed would pack. Revised 2026-10-13 (core-log entry 51): the no-clock-network-change claim is scoped to the dark-HDMI fault, since the later thermal fault did require one. Revised 2026-10-13 (core-log entry 52): the thermal fix is validated with pll_nes at PLL_R[0] and `clk` on TRPLL0CLK0, and the record now says its own build-of-record recipe does not reproduce the card's image. Revised 2026-10-14 (core-log entry 60): the build defaults to seed 1 and, with nothing overridden, passed both gates and reproduced the card's b9af4f06221e and 7453ad6c9400 bit for bit; the clock profile is re-recorded from that core, and the seed-2 PLL_L[0] core is described as the past build it is. Revised 2026-10-14 (core-log entry 67): desktop ABI 1.2's sweep of seeds 1 to 37, the dead seed-6 core with its clk route compared against seed 26 and against the ABI 1.1 seed-1 core rebuilt bit for bit in a worktree, the SPINE17 lead, and seed 26 as the board-tested build of record, rebuilt with nothing overridden to 16c6c5998c9a and be6f111c0b53. Revised 2026-10-14 (core-log entry 68): the build now warns on a clk route through SPINE17. Revised 2026-10-14 (core-log entry 70): seed 16, timing clean with clk through SPINE17, was packed as desktop.ed93285f0e65.bin (fs 4b4eafb7a455) and loaded from the card twice; each load left HDMI synced at 720p and black with the UART silent while seed 26 answered on the same path, so the check now refuses a clk route through SPINE17 unless ALLOW_SPINE17_CLOCK is set. Revised 2026-10-18 (core-log entry 94): the dead SPINE17 and SPINE18 cores are explained by unprogrammed spine select ports and logic in the select tiles (TOOL-021), not by those spines; the refusal stays until a SPINE17 core is tested. Revised 2026-10-18 (core-log entry 95): the build of record is now seed 3 of desktop ABI 1.3, desktop.24a11e81a46c.bin (desktop.d993899c24a4.fs), with clk through SPINE18 and the pixel clock on global resources only, built with apicula 04bc3c5 and nextpnr 8be8657b (TOOL-021) and rebuilt bit for bit with nothing overridden; seed 26 of ABI 1.2 is rebuilt from commit 97cf0db."

- record_id: TOOL-018
  kind: TOOLCHAIN
  topic_id: TOOL
  title: "A registered command's help text must fit one `help` line, and a tool gates the width"
  status: VERIFIED
  verified_date: 2026-10-13
  statement: "The shell renders the registered command list one line per command, printing two columns of indent, the name in a ten-column field and one space before the help text: `printf(\"  \" C_CYAN \"%-10s\" C_RESET \" %s\\n\", commands[i].name, commands[i].help)` in third_party/tinydesk-shell/src/core/tdsh_builtin.c. A help text therefore has TERMINAL_COLS - 13 columns: 67 at the 80 columns the USB console and the desktop terminal use. A longer one wraps, and the wrapped line reads as though it belonged to the command under it."
  consequence: "The width is a gate rather than a style note: tools/check_help_width.py parses the registered tables in ports/, third_party/tinydesk and third_party/tinydesk-shell, names every entry over budget with its file and line, and exits nonzero, so it can gate a commit. It reads the tables rather than a compiled image, because a long string is the defect wherever it ends up. Seven of the 116 commands registered when the gate was written exceeded it and were shortened (core-log entry 57)."
  sources:
    - "This project's third_party/tinydesk-shell/src/core/tdsh_builtin.c: the command-list printf of the help listing, in its coloured and plain forms"
    - "This project's tools/check_help_width.py: the docstring's columns(help) <= TERMINAL_COLS - 2 - 10 - 1, the --cols and -v options, and its exit status of 0 or 1"
  verification: "Established in core-log entry 57 and confirmed on this board there: seven of the 116 registered commands were over budget and were shortened in place, and the user saw the help output unwrapped at 80 columns. Re-run here on 2026-10-16 with the same meaning: tools/check_help_width.py reports 137 entries, none over 80 columns."

- record_id: TOOL-019
  kind: TOOLCHAIN
  topic_id: TOOL
  title: "The keyboard link's usage table, and the key reserved for the screen swap"
  status: VERIFIED
  verified_date: 2026-10-13
  statement: "ports/bl616/tang_key.c translates a HID keyboard report into the ANSI byte sequences TinyDesk's input parser decodes. Its fkey_sequences table emits F1 to F11, usages 0x3a to 0x44, as ESC [ 11~ through ESC [ 23~, the forms third_party/tinydesk/src/input.c's tilde_key() already decodes to TD_KEY_F1 to TD_KEY_F11. F12, usage 0x45, is TANG_KEY_USAGE_TOGGLE (ports/bl616/tang_key.h) and is skipped before translation, so no layer ever sees it. The arrow and editing keys already used the same CSI forms."
  consequence: "The table is the one place a HID usage becomes a TinyDesk key, so a key added to it reaches both the shell and the desktop. F12's reservation is what makes it the swap between TinyDesk and a running core, and leaving it out of the table is what keeps a layer from ever receiving it."
  sources:
    - "This project's ports/bl616/tang_key.c: the fkey_sequences table, its 0x3a F1 to 0x44 F11 entries, and the skip of TANG_KEY_USAGE_TOGGLE"
    - "This project's ports/bl616/tang_key.h: TANG_KEY_USAGE_TOGGLE 0x45"
    - "This project's third_party/tinydesk/src/input.c: tilde_key() and its TD_KEY_F1 to TD_KEY_F12 returns"
  verification: "Established in core-log entry 57 and confirmed on this board there: the function keys worked with F12 still swapping the screen. Read again from this project's tree on 2026-10-16, where the table and the reserved usage are as recorded."

- record_id: TOOL-020
  kind: TOOLCHAIN
  topic_id: TOOL
  title: "Output printed before tangload has no raster, and `osd desk on` mirrors rather than replays"
  status: VERIFIED
  verified_date: 2026-10-13
  statement: "The board draws nothing until a core is programmed, so a line printed before tangload has no raster to appear on; and `osd desk on` mirrors the console from the point it is issued forward rather than replaying what came before, because nothing was mirrored while the layer was off (ports/bl616/tang_osd_desk.c)."
  consequence: "Boot narration the user has to read must come after the core is loaded; a line placed before tangload is reachable only by someone already attached to the USB console. scripts/boot.tdsh is ordered for this, and the core's name sits in the post-load line."
  sources:
    - "This project's ports/bl616/tang_osd_desk.c: the `osd desk` command and its comments on the layer mirroring the console only while it is on"
    - "This project's scripts/boot.tdsh: the boot order the consequence describes"
  verification: "Established in core-log entry 57, where the line naming the core was moved after tangload because it could never be seen before it. Read again from this project's tree on 2026-10-16; not re-observed on hardware in this cycle."

- record_id: TOOL-021
  kind: TOOLCHAIN
  topic_id: TOOL
  title: "GW5AST-138C select spines clock only with their select ports held at VSS and no logic in the select tiles"
  status: VERIFIED
  verified_date: 2026-10-18
  statement: "Gowin's GW5AST-138C.fse (IDE 1.9.11.03) has no VCC row for a logic cell's CLK ports. A Gowin build whose clk takes TRPLL0CLK0 -> centre SPINE11 -> CBRIDGEOUT_TOP3 -> SPINE19 (X94) and SPINE27 (X96) drives (27,94) and (81,94) CLK0 and CLK1 from VSS: N210 <= VSS at (28,94) and (82,94), arriving as N211 -> X02 -> CLK0/CLK1, with no other select-tile port programmed. apicula's fse_create_spine_select_wires (upstream 7c913a8, unchanged on upstream master on 2026-10-18) lists the ports for SPINE17 ((27|81,93) CLK1), SPINE18 ((·,93) CLK0, CLK2), SPINE19 ((·,94) CLK0, CLK1), SPINE20 ((·,93) A7, A6), SPINE21 ((·,94) A6, CLK2) and SPINE23 ((·,92) A7, A6), and nextpnr's enable_spines() ties them to the constant the table names, whose comment says VSS/GND while every entry said 1 (VCC)."
  consequence: "A VCC tie routes to a fuseless pip and leaves the port unprogrammed, and nextpnr did not keep logic out of those tiles, so every open-flow clk route through SPINE17, SPINE18 or SPINE19 loaded dead: desktop seeds 6 and 16 and entries 87 and 91's I2C cores, whose selects were unprogrammed or carried user nets. The SPINE17 refusal at step 2d and the measured-dead bridge pairs in TOOL-017 were symptoms of this, not proof that those spines or pairs are dead. apicula 04bc3c5 sets every select port to VSS (measured for SPINE19, a lead for the others that SPINE18 now supports) with tests/test_spine_select_138c.py, and nextpnr 8be8657b refuses LUT, DFF, ALU, SSRAM and wide-MUX bels in the select tiles. tools/desktop_open/clock_route_decode.py decodes a bitstream's clock pips and select ports from the vendor tables without the model's filters. SPINE17 stays dropped in the chipdb and refused at step 2d until a core using it is tested, and the core of record is still desktop.bin seed 26, built with the earlier pins."
  sources:
    - "Gowin IDE 1.9.11.03 GW5AST-138C.fse: wire tables 2 (GENERAL), 38, 90 and 91, read by tools/desktop_open/clock_route_decode.py"
    - "aquasock/apicula epic/gw5ast138c 04bc3c5: apycula/chipdb.py fse_create_spine_select_wires, tests/test_spine_select_138c.py; upstream YosysHQ/apicula master apycula/chipdb.py, same table with 1s"
    - "aquasock/nextpnr epic/gw5ast138c 8be8657b: himbaechel/uarch/gowin/globals.cc enable_spines, gowin.cc isValidBelForCellType and isBelLocationValid, gowin_utils.cc get_spine_select_tiles"
    - "Decoded bitstreams: desktop-i2c-pins-gowin (Gowin, fs in its build stage), desktop.be6f111c0b53.fs (seed 26), desktop.4b4eafb7a455.fs (seed 16), desktop.97b2295ecd08.fs (entry 91), and the entry-94 sweep build/desktop-selfix/s1..s8"
  verification: "On 2026-10-18 (core-log entry 94) the 8-seed sweep with apicula 04bc3c5 and nextpnr 8be8657b put clk on SPINE9 -> TOP1 -> SPINE18 and SPINE25 in every seed with another clock on SPINE19; each pnr.json routed every select port of a used spine to $PACKER_GND, each bitstream programmed those ports, and no logic sat in the select tiles. Seed 3, desktop.24a11e81a46c.bin, loaded after a Gowin control, answered as core 84, declared the sockets, read the DS3231 through pmodi2c, and the user accepted HDMI, VGA and the OLED. SPINE20, 21 and 23 have not carried a clock on the board."

```

---

## 6. Record template

```yaml
- record_id: <TOPIC>-<NNN>
  kind: DEVICE | BOARD | SOC | USB | FLASH | PROTOCOL | EXTERNAL | TOOLCHAIN
  topic_id: <TOPIC>
  title: "Short noun phrase"
  status: VERIFIED | SOURCED | INFERRED
  verified_date: YYYY-MM-DD
  statement: "What the source says, with exact values and units"
  consequence: "What it means for this project, or where it is implemented"
  sources:
    - "Document or repository, revision/commit/path/symbol, URL"
  verification: "How and when it was confirmed, or why it is not yet confirmed; then any 'Revised <date> (core-log entry N): ...' notes"
```

---

## 7. Maintenance boundary

- Keep external facts, their sources, and their verification status here.
- Board faces shared with another project are cited, not copied. The PMOD socket pins, the seating rule and the personality registry live in Tang-Phosphor's `.ai/core-reference.md` beside the constraint file and the RTL that implement them; this file records only the part that bounds this firmware, so the same board does not acquire two sources that can drift.
- The exception is a project being archived. Tang-PSX is backed up and kept, but stops being a live source, so the records that bear on this firmware were copied here as PSX-001 through PSX-006. Copying is for reach, not for preservation: nothing is lost when that project is archived, but its reference stops being the thing anyone consults day to day.
- Keep build results, failures, rationale, and chronology in `core-log.md`; cite a log entry number in `verification` instead of repeating it.
- Do not add speculative records for topics that have not been looked up. An unanswered question stays out of this file until a source answers it.
- Keep this file current rather than historical: when a record's source is replaced by a newer document or tool version, or its fact changes, revise the record in place and let git and `core-log.md` carry the history (section 1).
- A `core-syntax.md` audit is required whenever this file changes, per `.ai/core.md`.

```yaml
last_reviewed: 2026-10-11
```

---

## 8. Retired record ids

These ids are no longer records. Each one's facts that are still true were carried into the record named, so code, documents and log entries that cite a retired id should be read as citing that record. The summary is what the retired record said, kept so an old citation can be understood; it is not current fact. The full text is in git history.

| Retired id | Now in | What it said |
|---|---|---|
| DEV-003 | DEV-006 | Recorded the FPGA as unconfigured at power-up, which held only because tangload always replaced what was there before it was seen |
| PMOD-003 | PMOD-004 | Recorded the socket control register as 0x10, which is the bring-up core's map only |
| PMOD-005 | PMOD-006 | Tang-Phosphor's merged core adds personality 5, I2S2 stereo PCM playback; TinyTang parses /tang.ini (tang_ini.c) and sends it to a core with ID 0x50 at ABI 1.8 or later after every tangload, accepting none, oledrgb, encoder, i2s2, vga_j1 and vga_j2, refusing per the contract and warning about unknown keys; flip is applied as declared, with no flip-safety check yet; on the board only through host tests so far |
| FLS-001 | FLS-002 | Recorded the application slot as at most 0x80000 bytes with staging at 0x100000, a limit Tang-Control chose rather than one the flash or the vendor loader imposes |
| TDSH-001 | TDSH-004 | Recorded TinyDesk Shell v0.1.3 at 232a39f and its scripting language |
| TDSH-003 | TDSH-004 | TinyDesk Shell v0.1.4 at 3b7d7f8, the same language as v0.1.3; its terminal interface adds optional columns() and read_byte_timeout(), and without either the line editor assumes 80 columns |
| TDSH-006 | TDSH-008 | Each tdsh run clones a 19,096 B session (97% the 64-entry variable table) into a 19,352 B job and asks for a 32 KB stack; with the desktop open only ~7.7 KB lies outside the 40.5 KB largest block, so a script's own small allocations are refused and it returns silently (upstream issue #2) |
| TDSH-007 | TDSH-008 | TinyTang's first smaller configuration used 32 variables, 32-byte names, 128-byte values and a 16 KB stack, bringing its session to 5,752 B |
| TDESK-001 | TDESK-013 | Recorded TinyDesk's four-function port surface and its shell pin at 232a39f |
| TDESK-010 | TDESK-011 | Called the Files app's delete dialog a possible TinyDesk bug and suspected it of removing /scripts/boot.tdsh, which the user had deleted themselves |
| TDESK-012 | TDESK-013 | TinyDesk v0.1.4 at f4c1d29 pins the shell at 3b7d7f8, the same as this project; td_hal_t is unchanged, and the Terminal passes its window width to the backend's start() and resize() for the shell bridge to hand to the line editor |
| PROV-002 | PROV-005 | Recorded the licence and notice files when they landed, at which point the project licence was Apache-2.0 |
| PROV-004 | PROV-005 | TinyTang's own code is MIT, in LICENSE; the single exception is ports/bl616/tang_jtag_programmer.c, which stays Apache-2.0 as nand2mario's file with its text at LICENSES/Apache-2.0.txt and a note added to its header. Apache-2.0 was never required - it is permissive, and there is no copyleft in the tree |
| BLE-013 | BLE-014 | Starting the Bluetooth stack takes 15,828 B of heap (84,120 free before, 68,292 after, of 127,248) on top of BLE-002's static cost; a connection costs about 700 B, and free heap returns to the same figure after each reconnect |
| TOOL-008 | TOOL-016 | Linked out of the SDK, each under its own licence rather than the SDK's: FreeRTOS V10.4.6 (MIT, (C) 2021 Amazon.com) via CONFIG_FREERTOS, CherryUSB (Apache-2.0) for the CDC console and its FreeRTOS OSAL, and FatFs R0.15 w/patch3 (ChaN, source-redistribution condition only); LVGL, TJpgDec, mbedTLS, littlefs and the codecs are not linked |
| TOOL-009 | TOOL-011 | Recorded the console's input as exclusive and the guard as looking for the desktop's markers, which stopped working once the desktop's drawing left USB (USB-007): tangput feeds the same CDC byte stream the desktop reads its typed input from, so a transfer is safe only while the console is at a shell prompt, and the desktop holding it turns a file into keystrokes; tools/tinytang_put.py's require_shell() guard checks for the desktop's markers ([Start], Terminal - tdsh, or the alternate-screen sequence) before sending |
