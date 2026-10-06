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
| Where are the board's PMOD sockets, and how are their pins numbered? | PMOD | PMOD-001 |
| How does the core learn what is seated in the PMOD sockets? | PMOD | PMOD-002 |
| Which register selects a PMOD personality, and what are the values? | PMOD | PMOD-003 |
| Where are the USB OTG registers, and which bit is which? | BL6 | BL6-001 |
| How much RAM does the BL616 have? | BL6 | BL6-002 |
| How do I read free heap? | BL6 | BL6-003 |
| Why do xPortGetFreeHeapSize() and friends fail to link? | BL6 | BL6-004 |
| Is there any real-time clock? | BL6 | BL6-005 |
| How do I read the CPU clock? | BL6 | BL6-006 |
| Why is the FPGA link slower than its baud rate? | BL6 | BL6-007 |
| Why does a crash or out-of-memory freeze the board silently instead of reporting? | BL6 | BL6-008 |
| What happens now when the heap runs out, and why did loading cores use it up? | BL6 | BL6-009 |
| What speed does the console negotiate? | USB | USB-001 |
| Can the OTG block tell me what is on the other end of the cable? | USB | USB-002 |
| Can the OTG connector power a keyboard? | USB | USB-003 |
| Is it safe to re-register the console's endpoints? | USB | USB-004 |
| Why is there no host deinit, and what must be done by hand? | USB | USB-005 |
| How do I tell whether a host is attached? | USB | USB-006 |
| Why is TinyDesk on HDMI laggy with the computer attached? | USB | USB-007 |
| Where does the application live in flash, and what does a soft reset do? | FLS | FLS-002 |
| What does a watchdog reset do, and where is a crash record kept across it? | FLS | FLS-003 |
| Which BL616 loader does the board have, and where are the full flash backups? | FLS | FLS-004 |
| Where is the Bluetooth antenna, and how well does the radio work without one? | BLE | BLE-001 |
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
| How much heap does the running Bluetooth stack take? | BLE | BLE-014 |
| What is the frame format a loaded core expects? | PROT | PROT-001 |
| Which commands does a loaded core understand? | PROT | PROT-002 |
| Which UART and rate reach a loaded core? | PROT | PROT-003 |
| What is the layout of the core's on-screen text page? | PROT | PROT-004 |
| Why does the screen go black when the OSD is on? | PROT | PROT-005 |
| How could a controller navigate a menu? | PROT | PROT-006 |
| Why can a ROM load end in a black screen with no error? | PROT | PROT-007 |
| What shows on screen between a core loading and the desktop painting? | PROT | PROT-008 |
| Why are there marks down the left edge of the desktop layer? | PROT | PROT-009 |
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
| Which shell revision is this, and what can a script do? | TDSH | TDSH-004 |
| What rules does upstream set for this port: names, commands, patches, pull requests? | TDSH | TDSH-005 |
| Why does a script started from the desktop return without running? | TDSH | TDSH-006 |
| Which sequences must a console mirror understand? | TDSH | TDSH-002 |
| What does TinyDesk need from a port? | TDESK | TDESK-013, TDESK-003 |
| How much RAM does the desktop need? | TDESK | TDESK-002 |
| Why does the Terminal window say "No shell backend in this build."? | TDESK | TDESK-004 |
| How is a shell hosted in the Terminal window? | TDESK | TDESK-005 |
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

```yaml
DEV-001: "The FPGA is a GW5AST-138; its JTAG IDCODE is 0x0001081b, reported as ID=0001081b on every tangload"
DEV-002: "cores/console138k/nestang.bin is 4,593,044 bytes and byte-identical to nand2mario's generated console138k artifact (GW5AST-138B)"
DEV-003: "Superseded by DEV-006. Recorded the FPGA as unconfigured at power-up, which held only because tangload always replaced what was there before it was seen"
DEV-004: "A core bitstream can carry AE350 RISC-V software: the program is compiled to a hex file and read into a boot ROM inside the design with $readmemh, so it is synthesised into the bitstream and arrives with tangload; the firmware loads whole images and never AE350 code as an artifact of its own"
DEV-005: "The menu core is the console138k design with nestang's NES machine not instantiated: MENU_CORE in src/boards/console138k_menu.v, selected by build.tcl's third `menu` argument; it answers CORE_ID 1, the same value the patched NES core answers, and is about four fifths smaller in logic while its bitstream is only ~2.5 percent smaller"
DEV-006: "The FPGA is not empty at power-up: with no tangload it is already running a core that answers core ID 0 on the UART and shows a TangCore splash on HDMI, so it configures itself from storage of its own; boot.tdsh replaces it within seconds, which is why it went unseen"
DEV-007: "The FPGA's 16 MB XTX configuration flash (JEDEC 0b 40 18) holds one image at offset 0, 1,084,246 B, a compressed GW5AST-138 bitstream matching no TangCore 0.9 core; TangCore's installer and firmware never write that flash, so it is most likely the factory splash core"
BRD-001: "BL616 to FPGA JTAG: TMS GPIO0, TCK GPIO1, TDO GPIO2, TDI GPIO3"
BRD-002: "SD is gated behind GPIO 16 held high; without it f_mount returns FR_NOT_READY (3)"
BRD-003: "The two USB-A controller ports are FPGA pins: usb1_dp/dn H13/G13, usb2_dp/dn M15/M16, all IO_TYPE=LVCMOS33"
BRD-004: "The onboard debug bridge is a SIPEED FT2232 (0403:6010, product 'USB Debugger'), a separate USB path from the BL616's CDC"
BRD-005: "The board has two power inputs and runs on either; a power cycle is unplugging both and restoring power first"
BRD-006: "Which firmware the BL616 runs is chosen at a cold power-up: with the power input it runs TinyTang and the CDC (two-wire); on the OTG cable alone it comes up as the vendor's FT2232 USB Debugger (one-wire) and TinyTang does not run; removing the power cable from a running board changes nothing, since the OTG cable keeps it powered"
BRD-007: "One-wire and two-wire cannot be combined: by the user's account the board's modules are powered from one input or the other, never both, and making the two work together was tried at length in the earlier projects without success; no written record of that work was found"
BRD-008: "The dock's two 2x20 2.54 mm headers are the SDRAM connectors J9 (SDRAM0) and J10 (SDRAM1), MiSTer-style: FPGA I/O on pins 1-10, 13-28 and 31-40, +5 V on pin 11, ground on pin 12; they are not Raspberry Pi headers, and a Pi HAT fitted directly would put the board's 5 V on a HAT signal and short eight FPGA pins through the HAT's ground pins"
PMOD-001: "Two PMOD sockets: PMOD1 beside HDMI on W19 W20 F19 F20 E22 D22 E21 D21 and PMOD0 on V18 V19 G21 G22 F18 E18 C22 B22, all LVCMOS33; Sipeed interleaves the rows, so IO0/2/4/6 are Digilent pins 1-4 and IO1/3/5/7 are pins 7-10, and flipping a module swaps pins 1-4 with 7-10"
PMOD-002: "/tang.ini at the SD root is the socket contract (pmod0/pmod1 plus _flip, flat under [tang]); a missing file or absent entry releases the socket and unknown modules are refused; modules carry no ID pins, so presence can never be detected, and the parser is firmware work - formerly Tang-Control's, now this project's"
PMOD-003: "Socket control register 0x10: bit 0 renderer, bits 4-7 PMOD0 personality and 8-11 PMOD1, bits 12/13 upside-down; personalities 0 none, 1 oledrgb, 2 vga J1, 3 vga J2; 0x14 is scratch"
BL6-001: "USB_BASE 0x20072000; OTG_CSR +0x80 (ID 21, CROLE 20, SPD 23:22, VBUS_VLD 19, A_SESS 18, B_SESS 17, A_BUS_DROP 5, A_BUS_REQ 4); PDS usb_ctl 0x2000E500 (IDDIG 5, DRVBUS_POL 4)"
BL6-002: "OCRAM is 320 KB at 0x20FC0000; the PSRAM window is declared but this board has no external RAM"
BL6-003: "The allocator is TLSF: mem.h exposes g_kmemheap, kfree_size(), and heapsize; PMEM_HEAP is the same heap unless the chip is a BL618"
BL6-004: "xPortGetFreeHeapSize() and xPortGetMinimumEverFreeHeapSize() do not exist in this build; using them fails at link time"
BL6-005: "There is an HBN always-on RTC counter (HBN_Enable_RTC_Counter, HBN_Get_RTC_Timer_Val) with a selectable 32 kHz source: a counter, not a calendar, and no battery"
BL6-006: "bflb_clk_get_system_clock(BFLB_SYSTEM_CPU_CLK) returns the CPU clock"
BL6-007: "bflb_uart_putchar() reads the millisecond timer before every byte for its 100 ms timeout, costing about 7.5 us of CPU per byte here, which capped UART1 near 133 KB/s at 5 Mbaud; filling the TX FIFO from its free count reaches the wire rate"
BL6-008: "The SDK fails silently with interrupts off on every fatal path: exception_entry prints to an unconnected UART and spins, vAssertCalled spins, bflb_malloc/calloc/realloc/malloc_align spin on any failed allocation instead of returning NULL, and the Bluetooth controller's btble_ke_malloc calls platform_reset, which jumps to address 0"
BL6-009: "bflb_malloc, _calloc, _realloc and _malloc_align are replaced with --wrap by versions that return NULL and count the refusal (tang_heap.c); every tangload leaked 4 KB (the JTAG programmer's unused fbuf_cached) until that was fixed; script workers are capped at 16 KB of stack, and a cartridge script uses 10,728 B"
USB-001: "The CDC console negotiates USB 2.0 High Speed, 480 Mbps (lsusb -t); CONFIG_USB_HS sets CDC_MAX_MPS 512, which is legal only at HS"
USB-002: "No role signal follows the cable: OTG_CSR's ID bit tracks the forced configuration (PDS IDDIG), not the connector"
USB-003: "The OTG connector does not source VBUS: A_BUS_REQ is set and nothing comes out, in both DRVBUS_POL polarities, and a device that reacts to power stayed dark for 60 s"
USB-004: "usbd_add_endpoint() assigns by endpoint index, not by appending, and usbd_deinitialize() resets intf_offset and calls usb_dc_deinit(), so tdsh_bl616_console_init() is safe to call again"
USB-005: "There is no host deinit in the SDK: usbh_deinitialize() is software-only, and usb_hc_low_level_init() has no counterpart, so the port must be returned to device mode by hand"
USB-006: "CherryUSB fires USBD_EVENT_CONFIGURED on enumeration and USBD_EVENT_DISCONNECTED when the host goes away: the usable host-present signal"
USB-007: "An IN packet leaves only when the host reads, which it does only while a program has the port open; enumerated but unread, the console's old flush spun a million yields per packet and stalled every writer, including the desktop on HDMI. Writes now require DTR and wait at most 50 ms"
FLS-001: "Superseded by FLS-002. Recorded the application slot as at most 0x80000 bytes with staging at 0x100000, a limit Tang-Control chose rather than one the flash or the vendor loader imposes"
FLS-002: "Application at 0x40000 up to 0xE0000 bytes, staging at 0x120000 up to the vendor data record at 0x200000 (the backup shows 0x76000-0x200000 erased), commit runs from .tcm_code with interrupts off; the vendor loader boots images over 0x80000; a soft reset lands in the vendor loader, so a reflash needs a power cycle"
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
TDSH-001: "Superseded by TDSH-003. Recorded TinyDesk Shell v0.1.3 at 232a39f and its scripting language"
TDSH-002: "The shell emits a closed set: CR, LF, ESC[2K, ESC[2J, ESC[H, ESC[<n>C, ESC[<n>D and SGR colour, and nothing else"
TDSH-003: "Superseded by TDSH-004. TinyDesk Shell v0.1.4 at 3b7d7f8, the same language as v0.1.3; its terminal interface adds optional columns() and read_byte_timeout(), and without either the line editor assumes 80 columns"
TDSH-004: "TinyDesk Shell v0.1.5 at 8456dd1 (tinydesk-project/tinydesk-shell): the same language and terminal interface as v0.1.4; board.conf numbers are decimal or 0x hex, never octal, and network mode/autowifi are root only, neither used by this port"
TDSH-005: "Upstream STANDARDS.md (v0.1.5) binds ports: one prefix for every owned name (this port's is tang, still recorded upstream as not chosen), interface files named tdsh_platform_<platform>.c and the like, no edits inside submodules (carried patches only), pins at release tags, both repositories' host tests passing at the pin, prefixed or subcommand-style command names, and PRs that state the user-visible change, the platforms run on and passing tests"
TDSH-006: "Each tdsh run clones a 19,096 B session (97% the 64-entry variable table) into a 19,352 B job and asks for a 32 KB stack; with the desktop open only ~7.7 KB lies outside the 40.5 KB largest block, so a script's own small allocations are refused and it returns silently (upstream issue #2)"
TDESK-001: "Superseded by TDESK-012. Recorded TinyDesk's four-function port surface and its shell pin at 232a39f"
TDESK-002: "Screen memory is TD_MAX_COLS x TD_MAX_ROWS x 8 bytes, twice; the ESP32-C6 uses 80x25 or 256x96, and 100x30 costs 48 KB for the pair"
TDESK-003: "TinyDesk's filesystem is ports/common/td_fs_stdio.c, written against stdio, dirent.h and sys/stat.h, so it lands on this port's FatFS syscall layer unmodified"
TDESK-004: "The Terminal app draws 'No shell backend in this build.' exactly when s_backend is NULL; td_terminal_set_backend() supplies it"
TDESK-005: "A Terminal backend is td_term_backend_t: start, read, write, user, set_user; the ESP32-C6 reference runs the shell in a task over two FreeRTOS stream buffers"
TDESK-006: "The Editor saves with Ctrl+S; the host HAL clears IXON so it arrives, and a board port has no termios, so the user's terminal eats it as XOFF and freezes the display"
TDESK-007: "td_run() returns after td_quit(); td_shutdown() restores the terminal"
TDESK-008: "Network, MQTT, Modbus and Software Update are built on proto/td_sock.c and proto/td_tls.c: POSIX sockets, esp_timer.h, and mbedTLS with esp_crt_bundle.h"
TDESK-009: "Nine start-menu apps plus a clock window: About, Counter, Editor, Files, Log Viewer, Settings, System Monitor, Task Manager, Terminal, and Date & time"
TDESK-010: "Superseded by TDESK-011. Called the Files app's delete dialog a possible TinyDesk bug and suspected it of removing /scripts/boot.tdsh, which the user had deleted themselves"
TDESK-011: "TinyDesk's delete confirmations, in the Files app and on desktop icons, focus their Delete button, so Delete then Enter (or Space) removes the selected file with no further step"
TDESK-012: "Superseded by TDESK-013. TinyDesk v0.1.4 at f4c1d29 pins the shell at 3b7d7f8, the same as this project; td_hal_t is unchanged, and the Terminal passes its window width to the backend's start() and resize() for the shell bridge to hand to the line editor"
TDESK-013: "TinyDesk v0.1.5 at feaf841 (tinydesk-project/tinydesk) pins the shell at 8456dd1, the same as this project; only td.h's version and repository URLs change in code this port builds"
TOOL-001: "CONFIG_CHERRYUSB_HOST is required for the CDC to enumerate with FreeRTOS enabled; CONFIG_NEWLIB stops enumeration"
TOOL-002: "The Gowin programmer's accepted IDCODEs: GW5A-25 0x0001281b, GW5AT-60 0x0001481b, GWAST-138 0x0001081b, GW5AT-138 0x0001181b, GW2A-18 0x0000081b"
TOOL-003: "Bouffalo SDK 2.0.0 at ~/.cache/tangcore-dev/sdk with the T-Head RISC-V GCC 10.2.0 toolchain"
TOOL-004: "A Gowin bitstream names its device: nestang's console138k project is GW5AST-138B, and build.tcl generates the project from the device name rather than checking one in"
TOOL-005: "The USB-enumeration bisection is kept as proj.min.conf, proj.nonewlib.conf and proj.rtos.conf plus ref/, selected by TINYTANG_MIN, TINYTANG_RTOS, TINYTANG_NONEWLIB, TINYTANG_REF, TINYTANG_USB_ONLY, TINYTANG_NO_FS and TINYTANG_NO_SHELL"
TOOL-006: "tools/ holds tinytang_flash.py (reflash over CDC), tinytang_put.py (file onto the card) and tinytang_run.py (run a shell command), all needing Python with pyserial"
TOOL-007: "ports/bl616/tang_jtag_programmer.c is nand2mario's Apache-2.0 Gowin GPIO JTAG programmer from Tang-Control's fpga/programmer.cpp, based on openFPGALoader, vendored unmodified apart from its include list"
TOOL-008: "Superseded by TOOL-016. Linked out of the SDK, each under its own licence rather than the SDK's: FreeRTOS V10.4.6 (MIT, (C) 2021 Amazon.com) via CONFIG_FREERTOS, CherryUSB (Apache-2.0) for the CDC console and its FreeRTOS OSAL, and FatFs R0.15 w/patch3 (ChaN, source-redistribution condition only); LVGL, TJpgDec, mbedTLS, littlefs and the codecs are not linked"
TOOL-009: "Superseded by TOOL-011. Recorded the console's input as exclusive and the guard as looking for the desktop's markers, which stopped working once the desktop's drawing left USB (USB-007): tangput feeds the same CDC byte stream the desktop reads its typed input from, so a transfer is safe only while the console is at a shell prompt, and the desktop holding it turns a file into keystrokes; tools/tinytang_put.py's require_shell() guard checks for the desktop's markers ([Start], Terminal - tdsh, or the alternate-screen sequence) before sending"
TOOL-010: "A carried patch stops being recognised once a later cycle edits the lines it added: the reverse check wants those lines present verbatim and the forward check wants them absent, so scripts/apply-nestang-patches.sh presumes the series applied in a tree with local changes, and the guarantee is the fresh-clone reconstruction test rather than the applier's check"
TOOL-011: "The console's input is exclusive, so the host tools ask the board before sending: the probe ESC [ ? 7 7 n is taken out of the USB input by the firmware, unseen by the shell or the desktop, and answered ESC [ ? 7 7 ; 1 n at the shell prompt, 2 with the desktop running, and not at all while a command runs; tools/tinytang_console.py sends nothing unless the answer is 1, and tangput passes the sequence through as data while it receives"
TOOL-012: "One-wire debugging reaches the FPGA only: JTAG on FT2232 interface 0 (openFPGALoader -c ft2232; --detect reads 0x0001081b and changes nothing; an SRAM load must be .fs, not .bin) and the FPGA UART on interface 1 (/dev/ttyUSB1, 2 Mbaud, the same 0xAA frames as the BL616 link); no shell, desktop, boot script or TinyTang tool runs, and reaching it takes a cold power-up that also restarts the FPGA, so it cannot inspect a state reached under TinyDesk"
TOOL-013: "A core can be developed entirely over one-wire: SRAM-load its .fs over JTAG with Tang-Phosphor's scripts/flash-otg.sh (17 s for the merged Phosphor image), then identify, peek, poke and stream to it on /dev/ttyUSB1 at 2 Mbaud with tools/fpga_uart.py and scripts/play_stream.py; an MP3 played through the AE350 this way gave 441000 samples, 0 underruns, 44100 Hz"
TOOL-014: "A third debug path: a Raspberry Pi Pico 2 CMSIS-DAP probe (2e8a:000c) on the module's U1201 header is its own USB device on the host, set up and validated by the user; Tang-Phosphor's scripts/flash-pico.sh drives it with openFPGALoader -c cmsisdap at 2 MHz or less (a full .fs takes 10-20 min), its real use is GAO and independent JTAG checks, and it must be idle during a tangload"
TOOL-015: "With FF_FS_REENTRANT 1 the SDK's FatFS R0.15 takes a FreeRTOS mutex (xSemaphoreTake with FF_FS_TIMEOUT ticks) on entry to every file call and gives it on exit, and FreeRTOS forbids API calls inside taskENTER_CRITICAL, so no f_* call may run inside a critical section; the JTAG programmer's did and failed tangload under TinyDesk"
TOOL-016: "Linked out of the SDK under their own licences: FreeRTOS V10.4.6 (MIT, (C) 2021 Amazon.com), CherryUSB (Apache-2.0), FatFs R0.15 w/patch3 (ChaN), and since blescan/blekbd the Bluetooth stack: the Zephyr-derived host (Apache-2.0; Intel, Nordic, Wind River, Oticon, Chettimada), TinyCrypt (BSD-3-Clause, Intel 2017), micro-ecc (BSD-2-Clause, Kenneth MacKay 2014) and ctr_prng (BSD-2-Clause, Chris Morrison 2016), whose binary-form notices are reproduced in LICENSES/, plus the binary-only RivieraWaves-based controller and PHY libraries under the SDK's Apache-2.0 alone"
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
PROV-001: "Lineage as the user states it: nand2mario's TangCore is the origin for Tang-Phosphor and Tang-PSX; Tang-Control is a fork of the same repo for peek/poke and the 1-wire and 2-wire debug arrangements; the family also uses a DDR3 IP block, TinyDesk, and CERN's colibri as a reference; the remembered memory module turned out to be nand2mario's JTAG bit-bang programmer, and Tang-PSX is being archived rather than deleted"
PROV-002: "Superseded by PROV-004. Recorded the licence and notice files when they landed, at which point the project licence was Apache-2.0"
PROV-004: "Superseded by PROV-005. TinyTang's own code is MIT, in LICENSE; the single exception is ports/bl616/tang_jtag_programmer.c, which stays Apache-2.0 as nand2mario's file with its text at LICENSES/Apache-2.0.txt and a note added to its header. Apache-2.0 was never required - it is permissive, and there is no copyleft in the tree"
PROV-005: "TinyTang is MIT with thirteen Apache-2.0 files: nand2mario's JTAG programmer, and ten Phosphor transport files and two host tests written by this project's author in Tang-Control; it descends from nand2mario's TangCore firmware-bl616 through Tang-Control, a fork of it, and copies no other TangCore source"
PROV-003: "colibri is CERN's vendor-independent, fully verified, open-source VHDL common library at gitlab.cern.ch/colibri/colibri, with an unofficial SystemVerilog port at github.com/kavierim/colibri-sv pinned to upstream 3fa78412; read the port when working in SystemVerilog since it ships an AGENTS.md and targets Verilator and free tooling, but treat the original as the authority; licensed CERN-OHL-W-2.0 (weakly reciprocal), not MIT, with no code copied here"
PSX-001: "This board is GW5AST-138 revision C (package mark 2518CA0N), while the console138k core image it loads is built for revision B and works anyway"
PSX-002: "GW5AST-138: 138,240 LUTs, 139,095 registers, 340 BSRAM blocks of 18 Kbit, about 765 KB"
PSX-003: "50 MHz oscillator on FPGA pin V22; the BL616 control UART reaches the FPGA on V14 (FPGA RX) and U15 (FPGA TX), LVCMOS33 - the other end of the link PROT-003 describes from the BL616 side"
PSX-004: "Module connector U1201, 8-pin JST SH: 1 5V0 via diode (~4.4 V), 2 TMS T13, 3 TDO U13, 4 TCK V12, 5 TDI R13, 6 RX V14, 7 TX U15, 8 GND; the JTAG nets are shared with the BL616, so an external adapter must be released during a tangload, pins 6 and 7 stay unconnected, and pin 1 must not reach a 3.3 V adapter"
PSX-005: "Dock SDRAM is Winbond W9825G6KH-6, 32 MB x16 at 166 MHz, on connectors J9 and J10, separate from the SOM's 1 GiB x32 DDR3; neither is the BL616's memory, which is 320 KB of on-chip OCRAM (BL6-002)"
PSX-006: "A loaded core is indicated by active_core, the low byte of its CORE_ID (0x01 nestang, 0x50 Phosphor, 0x51 Gate 1), not by core_running, which reads no while a core answers; uploads are refused while a core runs; the link is 2 Mbaud with a 5 Mbaud fast mode and needs an iosys clock at least 8x the baud"
BLE-001: "The BL616's antenna pin runs through L9 (0 ohm) to U35, a U.FL jack marked ANT on the dock's underside, and nothing is fitted to it; bare, the radio still scans and connects at arm's length at -76 to -94 dBm, with occasional HCI 0x3E connection failures that a retry clears"
BLE-002: "Scanning and connecting need the SDK's ble1m10s1bredr0 controller library (central, observer, 10 links); ble1m0s1bredr0 has neither role; Bluetooth costs 236,096 B of image, 32 KB of RAM the linker reserves and 52.6 KB of boot heap, the controller cannot be shut down once started, and CONFIG_BT_SETTINGS 0 keeps pairing keys in RAM only"
BLE-003: "Against the K950, bt_gatt_discover by service UUID (0x1812) and descriptor discovery by UUID (0x2902) found nothing, while unfiltered discovery found the same attributes; discover everything in range and filter in the callback"
BLE-004: "With BFLB_BLE_PATCH_NOTIFY_WRITE_CCC_RSP on, the SDK calls a subscription's notify callback with data NULL when the CCC write enabling it succeeds, the same signal gatt.h documents as the subscription being removed; a NULL report there is not an unsubscribe"
BLE-005: "Issuing many ATT requests at once from host callbacks blocks the host on ATT TX buffer allocation for ever; issue one request at a time, each from the previous one's completion"
BLE-006: "Logitech K950 in Bluetooth mode is HID over GATT on BLE: a random address that changes each time it enters pairing mode, Just Works pairing, boot protocol supported (Boot Keyboard Input value handle 0x23, CCC 0x24, Protocol Mode 0x4D) and the standard 8-byte boot report"
BLE-007: "Only Bluetooth LE HID over GATT devices can be used: the SDK's Classic profiles are A2DP, AVRCP, HFP hands-free, RFCOMM and SDP, with no HID, so a Classic-only device such as the Rii K06 (Bluetooth 3.0) cannot connect"
BLE-008: "A BLE keyboard notifies only when its key state changes, with no heartbeat, so it counts as live while connected and is zeroed on disconnect; its reports bypass the core, so left-alt pointer mode is applied in firmware by tang_key_pointer()"
BLE-009: "Logitech M750 over BLE: random address that increases by one per pairing, Just Works, Boot Mouse Input at value handle 0x23 (CCC 0x24), Protocol Mode 0x35; its boot report is 4 bytes with the wheel in byte 3, and its 7-byte report-protocol report is 16-bit buttons, 12-bit X and Y packed in 3 bytes, wheel and pan"
BLE-010: "The SDK host keeps a bonded peer's GATT subscriptions on disconnect unless BT_GATT_SUBSCRIBE_FLAG_VOLATILE is set, and re-sends them on reconnect; reusing those params structs for a new subscription rewrites a node still in its list and returns -EALREADY"
BLE-011: "This SDK's config.h defines CONFIG_BT_WHITELIST, which compiles out bt_le_set_auto_conn and the host's reconnect-after-disconnect; reconnection is one whitelist initiator (bt_conn_create_auto_le) that connects the first whitelisted device to advertise and stops, cannot run beside an explicit scan or connect, and needs the whitelist unchanged while it runs"
BLE-012: "Pairings are kept on the SD card in /sd/ble/bonds.bin (tang_ble_bonds.c format, keys unencrypted) and put back into the host's key table at boot; the K950 and M750 then reconnect when woken, but the first encryption attempt after a disconnect often fails (level 1, error 8, then HCI 0x3E) and a retry succeeds"
BLE-013: "Superseded by BLE-014. Starting the Bluetooth stack takes 15,828 B of heap (84,120 free before, 68,292 after, of 127,248) on top of BLE-002's static cost; a connection costs about 700 B, and free heap returns to the same figure after each reconnect"
BLE-014: "Starting the Bluetooth stack takes about 15.8 KB of heap (84,120 free before, 68,292 after, of 127,248) on top of BLE-002's static cost, and a connection about 700 B; with both devices connected about 64.7 KB stays free (largest block 40.5 KB) across cartridge loads and desktop sessions, and reconnects do not leak"
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
  status: SUPERSEDED
  verified_date: 2026-10-03
  statement: "Nothing on this board reconfigures the GW5AST-138 on power-up: the fabric comes up empty and the BL616 has to program a core image over JTAG. On this bench a core was never present after a power cycle until tangload ran, and the load was what brought the HDMI output and the UART link to life."
  consequence: "Any procedure that spans a power cycle must reload the core, which is why a cartridge takes three commands in order - tangload to put the core in, fpga to confirm it answers, nesload to stream the ROM - rather than assuming a core is there. It also means there is no persisted fabric state to reason about at boot."
  sources:
    - "This project's session behaviour: a core appeared only after tangload on every boot"
    - "BRD-001: the JTAG pins the load drives"
  verification: "Exercised on this board across many power cycles; each began with an unconfigured FPGA until tangload ran. Not tested against a hypothetical on-board reconfiguration path, because none was found."
  superseded_by: "DEV-006"

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
  statement: "With /scripts/boot.tdsh absent, so that nothing ran tangload after power-up, the board's FPGA was already running a core 80 seconds into uptime: fpga reported 'core 0 answering on UART1 at 2000000 baud', where every core this project builds answers 1, the desk layer was off, and the HDMI output showed a TangCore splash on more than one monitor and capture device. The same core-0 answer was seen once before, on 2026-10-04 in core-log cycle 12, with boot.tdsh renamed away. The FPGA therefore configures itself at power-up from configuration storage of its own, with a stock TangCore image, before the BL616 programs anything over JTAG."
  consequence: "DEV-003 said the fabric comes up empty and was wrong; it held only because boot.tdsh's tangload replaces the stock core within seconds of power-up, before most displays have locked. Procedures still reload a core on every boot -- tangload is what puts this project's cores in, since they live on the card and not in that storage -- but 'no core answers' is not the state at boot: a core answering 0 is the sign that the boot chain did not run, which is how a missing boot.tdsh shows up (TDESK-010). tangload programs SRAM and leaves that storage alone, which is why the stock core returns on every power cycle. Core ID 0 is not in TangCore's core table (TCTL-006), so which image it is beyond the splash it draws is not established."
  sources:
    - "This board's console on 2026-10-04: fpga reporting core 0 with no tangload since power-up, and osd desk status reporting the layer off"
    - "The user's report of a TangCore splash at boot on several displays while boot.tdsh was missing"
    - "core-log cycle 12: the first core-0 observation"
    - "ports/bl616/tdsh_tang_flash.c: tangload calls fpga_program(), which loads SRAM ('Load SRAM' in its output)"
  verification: "Observed twice on this board, both times with boot.tdsh absent; restoring boot.tdsh returns the board to core 1 at boot. The mechanism -- which storage and by what configuration mode the FPGA loads at power-up -- is inferred from that behaviour and not traced to a schematic or a Gowin document, which is why this is INFERRED rather than VERIFIED."


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
  statement: "The two modes TCTL-005 names are two different programs on the BL616, and only one runs at a time. A cold power-up with the dedicated power input connected, with or without the OTG cable, runs this project's firmware, and the host sees the TinyTang CDC (ffff:5454). A cold power-up with the OTG cable alone comes up instead as SIPEED's 'USB Debugger' FT2232 (0403:6010, serial 2025041420), with interface 0 as /dev/ttyUSB0 and interface 1 as /dev/ttyUSB1, and the CDC absent. Removing the power cable from a board already running TinyTang changes nothing: the OTG cable keeps it powered (BRD-005), the CDC stays enumerated under the same device number, and the console keeps answering. The same FT2232 is what a soft reset reaches after tangflash (FLS-001)."
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
    - "docs/Tang_Mega_138K_Console_32001C__Schematics.pdf, sheet FPGA_EXT_CONN: J9 and J10, SMD_PIN_2x20_2.54mm"
    - "images/circuit_boards/dock_front.jpg: the two edge headers"
    - "Adafruit, 2.2-inch PiTFT HAT guide: ILI9340 over the Pi's SPI with GPIO 25 and four buttons"
  verification: "Read from the schematic text and the board photograph on 2026-10-06; nothing was fitted or measured (core-log entry 41). The HAT's own pin assignments beyond those Adafruit's guide states were not checked against its schematic."
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
  title: "Every fatal path in the SDK ends in a silent spin with interrupts off"
  status: VERIFIED
  verified_date: 2026-10-05
  statement: "In Bouffalo SDK 7f44f9e: exception_entry() (drivers/soc/bl616/std/startup/interrupt.c, called from vector.S and the FreeRTOS portASM.S trap handler) prints mcause, mepc and mtval with printf and then loops in while(1) for anything but an environment call; it is not weak. vAssertCalled() (components/os/freertos/freertos_port.c, the end of this project's configASSERT) is weak and loops in while(1); vApplicationStackOverflowHook and vApplicationMallocFailedHook are weak too. bflb_malloc(), bflb_realloc(), bflb_calloc() and bflb_malloc_align() (components/mm/tlsf/bflb_tlsf.c) wrap every allocation in TLSF_MALLOC_ASSERT, which on a NULL result prints, calls bflb_irq_save() and loops in while(1): an allocation never fails, it hangs with interrupts off. FreeRTOS is built with heap_3, so pvPortMalloc reaches the same allocator. The Bluetooth controller library's platform_reset() (arch_main.c.o in libbtblecontroller_bl616_ble1m10s1bredr0.a), whose only caller in this firmware is btble_ke_malloc on the controller's own heap, clears MIE, stores its error code and, unless the code is 0xC3C3C3C3 or 0xA5A5A5A5, jumps to address 0. printf goes to a UART this board does not expose."
  consequence: "On this board a crash, a FreeRTOS assert, an out-of-memory and a controller allocation failure all looked like the same freeze: HDMI stopped, Bluetooth dropped, USB stayed enumerated and the console stopped answering. ports/bl616/tang_crash.c takes over exception_entry and platform_reset with the linker's --wrap (CMakeLists.txt) and defines vAssertCalled and the stack and malloc hooks, so each of those records and resets (FLS-003). The TLSF spin is not covered by the recorder, because it disables interrupts before any hook could run; only the watchdog ends it, with no record."
  sources:
    - "Bouffalo SDK 7f44f9e: drivers/soc/bl616/std/startup/interrupt.c exception_entry(); components/os/freertos/freertos_port.c; components/mm/tlsf/bflb_tlsf.c TLSF_MALLOC_ASSERT; components/mm/mem.h KMEM_HEAP and PMEM_HEAP"
    - "Disassembly of platform_reset and btble_ke_malloc in build/build_out/tinytang_bl616.elf, 2026-10-05"
  verification: "Source read on 2026-10-05. crash test trap was recorded with the right PC after the wrap; three reproductions of the cartridge-script freeze with Bluetooth devices connected left no record with every other path instrumented, which is the TLSF signature (core-log entry 34)."

- record_id: BL6-009
  kind: TOOLCHAIN
  topic_id: BL6
  title: "Allocation failure returns NULL; the core loader leaked 4 KB per load"
  status: VERIFIED
  verified_date: 2026-10-05
  statement: "The linker renames bflb_malloc, bflb_calloc, bflb_realloc and bflb_malloc_align (CMakeLists.txt) to the versions in ports/bl616/tang_heap.c: the same tlsf_memalign and tlsf_realloc calls under the same bflb_irq_save lock, returning NULL on failure and recording the count, size, task and caller; bflb_realloc's free-byte count also uses the new block rather than the old pointer. vApplicationMallocFailedHook is now empty. fpga_program() in tang_jtag_programmer.c allocated a 4096-byte fbuf_cached before choosing between its two paths, and only the disabled slow path used and freed it, so the compiled fast path leaked 4 KB on every tangload. TinyDesk Shell asks for a 32 KB stack for each `tdsh run` script (TDSH_SCRIPT_TASK_STACK); the port now caps worker stacks at 16 KB, the shell task's own size."
  consequence: "With the Bluetooth stack up, each core load cost 4 KB of a heap of about 125 KB until a load could not get even its 4 KB buffer, and a script's 32 KB stack failed sooner; under the SDK's allocator either froze the board with interrupts off (BL6-008). Now the buffer is allocated only on the path that frees it, a refused allocation makes the command fail and is counted, and `ble` and `crash` report free heap, the largest free block and refusals. The cap leaves a cartridge script 5.6 KB of stack spare; an overflow would be recorded by the stack-overflow hook (BL6-008)."
  sources:
    - "Bouffalo SDK 7f44f9e: components/mm/tlsf/bflb_tlsf.c and tlsf.h (tlsf_walk_pool, tlsf_get_pool)"
    - "This project's ports/bl616/tang_jtag_programmer.c fpga_program(), ports/bl616/tang_heap.c, ports/bl616/tdsh_platform_bl616.c bl616_worker_run()"
    - "third_party/tinydesk-shell/include/tdsh.h TDSH_SCRIPT_TASK_STACK and src/core/tdsh_script.c"
  verification: "On 2026-10-05 with both Bluetooth devices connected and the mouse moving, the build with the wraps and the cap loaded Castlevania, then free heap fell 4.1 KB per run and the third run's 4096-byte buffer was refused, the command failing without a freeze; with the leak fixed, seven runs, two of them after a desktop session, left free heap at 64,704 B and the largest block at 40,536 B every time, with the script using 10,728 of 16,384 bytes of stack (core-log entry 35)."

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
  consequence: "Every writer to the console waited with it, and the desktop draws through the console, so with the cable attached TinyDesk on HDMI was laggy and unresponsive, and a large burst -- opening the Terminal window -- looked like a freeze. console_flush() now sends nothing unless DTR is up, waits at most 50 ms for a packet, and leaves an untaken packet pending, dropping further output until the host reads it, whose completion clears the way. Separately, while the desk layer shows the desktop its drawing goes only to the layer's mirror and not to USB, so a computer terminal sees the shell's transcript but not the window redraws; a consequence is that the desktop is no longer visible on USB, so a tool cannot detect it from its drawing (TOOL-009) and must go by the 'desktop: starting TinyDesk' and 'desktop: exited' lines, which still reach USB."
  sources:
    - "ports/bl616/usb_cdc_bl616.c: console_flush(), usbd_cdc_acm_bulk_in() and usbd_cdc_acm_set_dtr()"
    - "Linux cdc-acm: read URBs are submitted when the tty is opened and killed when it is closed"
  verification: "On this board on 2026-10-05 the user found TinyDesk on HDMI smooth with the cable attached and nothing reading the port, where before it had been unusable; a listen-only capture while the user dragged windows and ran ls /music in the Terminal held only the transcript -- the desktop's start line, the prompt, the command, its output and 'desktop: exited' -- with no cursor positioning at all, and the console answered after the desktop exited."

- record_id: FLS-001
  kind: FLASH
  topic_id: FLS
  title: "Application at 0x40000, staging at 0x100000, and a soft reset lands in the vendor loader"
  status: SUPERSEDED
  verified_date: 2026-10-03
  statement: "The application occupies flash from 0x40000 with a maximum of 0x80000 bytes. A new image is staged in an erased region at 0x100000, verified against the file, and only then copied into the application slot. The copy must run from .tcm_code with interrupts disabled, because once the first application sector is erased no instruction may be fetched from the application's XIP flash. The boot header carries magic at 0x00, 0x08 and 0x64, a CRC-32 of the first 252 bytes at 0xFC, and a body length at 0x84 counting bytes after the 4 KiB header region. A soft reset lands in the vendor loader."
  consequence: "tangflash validates the header and the file's length against it before erasing anything, and the user power-cycles afterwards. The vendor loader below 0x40000 is never touched."
  sources:
    - "Tang-Control, utils/firmware_update.cpp: FW_APP_BASE, FW_STAGING_BASE, the sector helpers and the erase/write/verify loops"
    - "This project's ports/bl616/tdsh_tang_flash.c: the constants and check_boot_header()"
  verification: "tangflash has been run roughly a dozen times this session, each time followed by a power cycle and a working console."
  superseded_by: "FLS-002"

- record_id: FLS-002
  kind: FLASH
  topic_id: FLS
  title: "The application slot is 0xE0000 bytes, staged at 0x120000 below the vendor data record"
  status: VERIFIED
  verified_date: 2026-10-05
  statement: "The application occupies flash from 0x40000 with a maximum of 0xE0000 bytes (917,504), ending at 0x120000. A new image is staged at 0x120000, which with the same maximum ends exactly at 0x200000, where the vendor's data record begins. A read-back of the whole 4 MiB flash taken on 2026-09-26 holds data only at 0x0-0x1C000 (the vendor loader), 0x40000-0x76000 (the application then installed) and 0x200000-0x201000 (the vendor data record), and is erased everywhere else, so 0x76000-0x200000 held nothing to preserve. The 0x80000 maximum FLS-001 recorded was Tang-Control's choice of layout (utils/firmware_image.h, whose own static assertions require only that the slot and the staging area fit below 0x200000), and the vendor loader does not enforce it: it booted images of 582,256 and 594,400 bytes, whose header body lengths exceed 0x80000. Everything else FLS-001 recorded holds unchanged: the header layout and CRC, the copy run from .tcm_code with interrupts off, and the soft reset landing in the vendor loader."
  consequence: "ports/bl616/tdsh_tang_flash.c sets FW_APP_MAX_SIZE 0xE0000 and FW_STAGING_BASE 0x120000, with static assertions that the slot stays below the staging area and the staging area below 0x200000, and tools/tinytang_flash.py's FW_APP_MAX matches. A board still running a build from before this change refuses an image over 0x80000 itself, so moving such a board across takes two installs: first an image under 0x80000 built with the new limits, then the large one. Bluetooth LE alone took the image past the old limit (BLE-002), and the new slot leaves about 323 KB of room above a 594,400-byte image."
  sources:
    - "Full flash read-back /home/vash/Desktop/tang-console138k-bl616-backup-20260926.bin (4,194,304 bytes), mapped in 4 KiB sectors on 2026-10-05"
    - "Tang-Control, utils/firmware_image.h: the flash-layout comment and the static assertions on the application and staging regions"
    - "This project's ports/bl616/tdsh_tang_flash.c: FW_APP_MAX_SIZE, FW_STAGING_BASE, FW_VENDOR_DATA and the static assertions"
    - "FLS-001, which this record supersedes"
  verification: "On 2026-10-05 a non-Bluetooth image built with the new limits was installed by the old tangflash and booted, then the 582,256-byte Bluetooth image was installed by the new one and booted after a power cycle, and later 594,400-byte builds (the last cb9e7d4-dirty.aee808e) installed and booted the same way, each confirmed by platform."

- record_id: FLS-003
  kind: FLASH
  topic_id: FLS
  title: "A watchdog reset lands in the vendor loader; crash records live at 0x3FF000"
  status: VERIFIED
  verified_date: 2026-10-05
  statement: "The BL616's watchdog (bflb_wdg, 32 kHz clock divided by 32, reset mode) resets the chip into the vendor loader, which, as after a software reset (FLS-001, FLS-002), starts SIPEED's FT2232 debugger instead of the application, so the board re-enumerates as /dev/ttyUSB0 and /dev/ttyUSB1 and needs a power cycle to run TinyTang again; the power cycle leaves RAM unusable, so a .noinit record does not survive. The reset recorder register GLB_RESET_STS0 reads 0x7D after a power-up; its bits are active low (wdt_rst_n is bit 3). The last 4 KB sector of the 4 MiB flash, 0x3FF000, lies in the region the full read-back of FLS-002 found erased, above the application slot, the staging area and the vendor data record at 0x200000."
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
  status: SUPERSEDED
  verified_date: 2026-10-03
  statement: "The shell is TinyDesk Shell v0.1.3 at commit 232a39fa3375f2c8eb2560cdc69440f7095f8a25, consumed here as the third_party/tinydesk-shell submodule. Its portable core is C11 and MIT licensed. The script language, uScript 1.1.1, provides variables, quoting, command substitution, arithmetic, if/elseif/else/endif, while/endwhile, for/endfor, functions with return statuses, pipes up to eight stages, and redirection. tdsh run passes no arguments to a script; a script inherits the caller's variables. It also ships POSIX and Windows host ports."
  consequence: "scripts/boot-cart.tdsh is written in this language and needed nothing beyond if, $?, and two variables. Its four shell commands are registered through tdsh_register_commands()."
  sources:
    - "third_party/tinydesk-shell/docs/SCRIPTING.md: the language reference, the run forms, and the limits table"
    - "third_party/tinydesk-shell/VERSION and .git (v0.1.3, 232a39f)"
  verification: "The shell runs on this board; boot-cart.tdsh has been run repeatedly, on its happy path and on its guard path."
  superseded_by: "TDSH-003"

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

- record_id: TDSH-003
  kind: EXTERNAL
  topic_id: TDSH
  title: "TinyDesk Shell v0.1.4: the same language, and a terminal interface that can carry its width"
  status: SUPERSEDED
  verified_date: 2026-10-05
  statement: "The shell is TinyDesk Shell v0.1.4 at commit 3b7d7f8aca22af99a57606c1f8297df11ab81889, consumed as the third_party/tinydesk-shell submodule; the core is C11 and MIT licensed, and the script language is unchanged from v0.1.3 (TDSH-001). The line editor now redraws command lines that wrap over several rows. tdsh_terminal_io_t gains two optional members: columns(), the terminal's width read once per line, and read_byte_timeout(), with which the editor may ask the terminal for its width (ESC[6n). With neither, the editor assumes 80 columns."
  consequence: "This port fills columns() from the width TinyDesk's Terminal window reports, through tdsh_bl616_terminal_set_columns() from the bridge's start() and resize(), and returns 0 at the console so the editor keeps 80, the console layer's width. It offers no read_byte_timeout(): at the console the layer and a host terminal would both answer the width query. Without columns() the editor wrapped the 76-column window at 80, which put four characters on the row after every full one."
  sources:
    - "third_party/tinydesk-shell @ 3b7d7f8, include/tdsh_terminal.h: tdsh_terminal_io_t and its comments"
    - "third_party/tinydesk-shell @ 3b7d7f8, src/core/tdsh_terminal.c: terminal_columns()"
  verification: "Built into this firmware on 2026-10-05; the user confirmed long lines wrap correctly in the console and in the Terminal window once columns() was filled, and that Tab completion works."
  superseded_by: "TDSH-004"

- record_id: TDSH-004
  kind: EXTERNAL
  topic_id: TDSH
  title: "TinyDesk Shell v0.1.5: board numbers without octal, root-only network switches"
  status: VERIFIED
  verified_date: 2026-10-06
  statement: "The shell is TinyDesk Shell v0.1.5 at commit 8456dd1d8c2fa88706c065edba483be67fcb3553, from its new home github.com/tinydesk-project/tinydesk-shell. Against v0.1.4 (TDSH-003) the core and headers change in two files: tdsh.h's TDSH_VERSION, and tdsh_board_int() in src/core/tdsh_board.c, which reads board configuration numbers as decimal or 0x hexadecimal, never octal (08 is 8 and 010 is 10). The ESP-IDF port makes network mode and network autowifi root-only. The release adds STANDARDS.md (TDSH-005). The script language and tdsh_terminal_io_t are unchanged from TDSH-003, whose columns() behaviour still holds."
  consequence: "The update needed no change to this port: it does not use board configuration or the network commands. Built in a clean clone, its own host tests pass 8 of 8 with no warnings."
  sources:
    - "tinydesk-project/tinydesk-shell v0.1.5 (8456dd1): CHANGELOG.md, include/tdsh.h, src/core/tdsh_board.c"
  verification: "Diffed against v0.1.4 and built into this firmware on 2026-10-06; upstream ctest 8/8 passed in a clean clone; the user confirmed the console, the desktop, the Terminal, Files, F12, Castlevania and Phosphor (core-log entry 37)."

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


- record_id: TDSH-006
  kind: EXTERNAL
  topic_id: TDSH
  title: "A script from the desktop runs out of heap in its first allocations"
  status: VERIFIED
  verified_date: 2026-10-06
  statement: "tdsh run starts each script with a tdsh_script_job_t holding a full copy of the session, then a worker task with TDSH_SCRIPT_TASK_STACK (32,768 B) of stack, then the script's runtime. On a 32-bit target tdsh_session_t is 19,096 B, of which 18,496 B is vars[TDSH_MAX_VARS], 64 entries of 1 + 32 + 256 bytes, and the job is 19,352 B; none of these limits can be overridden by a build in v0.1.5. This port caps worker stacks at 16 KB (BL6-009). With the Bluetooth stack and two devices connected, the heap at the console is 64,704 B free with a 40,536 B largest block and about 24 KB outside it; with the desktop open it is 48,208 B free with the same largest block and about 7.7 KB outside it. Opened from the desktop's Files app, castlevania.tdsh and phosphor.tdsh returned with no output: a 1,056 B tdsh_realloc of the script's line table (src/core/tdsh_script.c:189) was refused in task tdsh_script, and the task had used 3,704 B of stack. The same scripts run at the console."
  consequence: "Scripts are to be started from the console, or with the desktop closed, until the limits can be sized for this heap. The findings were filed upstream as tinydesk-project/tinydesk-shell issue #2, proposing #ifndef around TDSH_SCRIPT_TASK_STACK, TDSH_MAX_VARS, TDSH_VAR_NAME_MAX and TDSH_VAR_VALUE_MAX (STANDARDS.md sections 3 and 10), with an offer of the pull request; 32 variables of 128-byte values would bring the session to about 5.8 KB."
  sources:
    - "third_party/tinydesk-shell @ 8456dd1: include/tdsh.h, src/core/tdsh_script.c, src/core/tdsh_core.c"
    - "A listen-only capture of the desktop Terminal's output on USB, 2026-10-06, with crash and ble run in the Terminal"
    - "https://github.com/tinydesk-project/tinydesk-shell/issues/2"
  verification: "Measured on this board on 2026-10-06; the session size was computed for a 32-bit target and confirmed at 19,112 B on a 64-bit host build (core-log entry 39)."
- record_id: TDESK-001
  kind: EXTERNAL
  topic_id: TDESK
  title: "TinyDesk's port surface is four functions, and it pins the same shell revision"
  status: SUPERSEDED
  verified_date: 2026-10-03
  statement: "TinyDesk is a terminal desktop: it draws overlapping text-mode windows with ANSI escape sequences, reads the keyboard and mouse back from the terminal, and has no display hardware. Its whole port surface is td_hal_t, four function pointers - read_byte returning the next byte or -1 without blocking, write returning the number of bytes accepted, millis, and sleep_ms - plus a context pointer. It consumes tinydesk-shell as a submodule at 232a39fa, the same revision this project uses, and its core is portable C11."
  consequence: "The port in ports/bl616/td_desktop_bl616.c is mostly a table of pointers over console calls this project already had. There is no second copy of the shell to keep in step."
  sources:
    - "third_party/tinydesk/include/tinydesk/td_hal.h: the td_hal_t definition and the comment 'This is the only thing a port has to provide.'"
    - "third_party/tinydesk/README.md: the terminal-desktop description and the four-functions claim"
    - "third_party/tinydesk/.gitmodules: submodule third_party/tdsh at 232a39fa3375f2c8eb2560cdc69440f7095f8a25"
  verification: "Built into this firmware and running: the desktop draws, opens windows, and its Terminal runs the shell."
  superseded_by: "TDESK-012"

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

- record_id: TDESK-010
  kind: EXTERNAL
  topic_id: TDESK
  title: "Possible TinyDesk bug: the Files app's delete confirmation defaults to Delete, so two keystrokes remove a file"
  status: SUPERSEDED
  verified_date: 2026-10-04
  statement: "files.c binds the Delete key to do_delete() (`case TD_KEY_DELETE: do_delete()`), which opens td_msgbox(\"Confirm\", \"Delete <name>?\", \"Delete|Cancel\"). td_msgbox adds its buttons in the order given, and a window's first focusable widget takes the focus (widgets.c: `if (focusable && !win->focus) win->focus = w`), so the focused button is Delete; a focused button activates on Enter or Space (widget_key), and delete_answer() removes the file for button 0 with no further step. The desktop's own icon menu reaches the same kind of confirmation through ask_delete()."
  consequence: "On this board the keyboard sends Delete as ESC [ 3 ~ (tang_key.c), which the desktop's parser reads as TD_KEY_DELETE, and Enter is both a key and, under right-alt, the pointer's left click -- so a file in the selected row can be removed by Delete followed by Enter or Space, which is easy to do by accident while testing input. /scripts/boot.tdsh sorts first in that folder. When boot.tdsh is missing nothing runs tangload, the FPGA stays on the core it configured itself with at power-up, and the screen shows that core's TangCore splash rather than TinyDesk. Until TinyDesk changes the default (Cancel first, or no default on a destructive dialog), the card's boot chain is one confirmed dialog away from being removed, and restoring it is a tangput of scripts/boot.tdsh. This is a candidate for an upstream issue against TinyDesk rather than a local patch."
  sources:
    - "third_party/tinydesk @ 791cad8, apps/files.c: do_delete(), delete_answer(), the TD_KEY_DELETE case in the list's key handler"
    - "third_party/tinydesk @ 791cad8, src/widgets.c: td_msgbox() button order, the first-focusable rule, widget_key() for TD_WT_BUTTON"
    - "third_party/tinydesk @ 791cad8, apps/desktop.c: item_menu_chosen() and ask_delete()"
    - "ports/bl616/tang_key.c: usage 0x4C (Delete) sent as ESC [ 3 ~"
  verification: "Inferred from reading the source, not reproduced. On 2026-10-04 /scripts/boot.tdsh vanished from the card during the user's display testing, after the board had booted with it; no other file was missing or moved and no tool run from the host removes files, which is what made the Files app the suspect. The board then came up on the FPGA's own core (fpga reported core 0, the user saw TangCore), and restoring the file with tangput returned the boot chain."
  superseded_by: "TDESK-011"

- record_id: TDESK-011
  kind: EXTERNAL
  topic_id: TDESK
  title: "TinyDesk's delete confirmations focus their Delete button"
  status: SOURCED
  verified_date: 2026-10-04
  statement: "files.c binds the Delete key to do_delete(), which opens td_msgbox(\"Confirm\", \"Delete <name>?\", \"Delete|Cancel\"), and the desktop's icon menu reaches the same dialog through ask_delete(). td_msgbox adds its buttons in the order given and a window's first focusable widget takes the focus (widgets.c: `if (focusable && !win->focus) win->focus = w`), so Delete is focused; a focused button activates on Enter or Space (widget_key), and delete_answer() removes the file for button 0 with no further step."
  consequence: "Delete followed by Enter or Space removes the selected file. On this board the keyboard's Delete arrives as ESC [ 3 ~ (tang_key.c), which the desktop reads as TD_KEY_DELETE. This is TinyDesk's design as written, recorded so it is not mistaken for a fault; the project is not raising it upstream. It supersedes TDESK-010, which suspected the dialog of removing /scripts/boot.tdsh on 2026-10-04 -- the user had deleted that file themselves."
  sources:
    - "third_party/tinydesk @ 791cad8, apps/files.c: do_delete(), delete_answer(), the TD_KEY_DELETE case in the list's key handler"
    - "third_party/tinydesk @ 791cad8, apps/desktop.c: item_menu_chosen() and ask_delete()"
    - "third_party/tinydesk @ 791cad8, src/widgets.c: td_msgbox() button order, the first-focusable rule, widget_key() for TD_WT_BUTTON"
    - "ports/bl616/tang_key.c: usage 0x4C (Delete) sent as ESC [ 3 ~"
  verification: "Read from the source; not exercised on hardware."

- record_id: TDESK-012
  kind: EXTERNAL
  topic_id: TDESK
  title: "TinyDesk v0.1.4: the same port surface, and a Terminal that reports its width"
  status: SUPERSEDED
  verified_date: 2026-10-05
  statement: "TinyDesk v0.1.4 at commit f4c1d29f6327df1b3dd40e00fe301cf002021dd6 pins tinydesk-shell at 3b7d7f8, the same revision as this project's submodule, so there is still no second copy to keep in step. td_hal_t is unchanged: read_byte, write, millis and sleep_ms plus a context pointer (TDESK-001). The Terminal app's td_term_backend_t has start(cols, rows), read, write, resize(cols, rows), user and set_user, and the upstream shell bridges pass the window's width from start() and resize() to the line editor through columns() (TDSH-003). Release 0.1.4 also cuts text at character boundaries (td_utf8_copy, td_utf8_pad, td_utf8_skip), keeps the file name when the Editor saves, adds widget and Editor tests, and reformats every source with clang-format 16; its README lists TinyTang as a community port."
  consequence: "The update needed no change to this port's td_hal_t or to the build's source lists; the bridge in ports/bl616/td_bridge_bl616.c gained a resize() and forwards the width. Diffs across the release should be read after formatting both sides with the release's .clang-format, since otherwise the reformatting hides the real changes."
  sources:
    - "third_party/tinydesk @ f4c1d29, apps/td_apps.h: td_term_backend_t"
    - "third_party/tinydesk @ f4c1d29, include/tinydesk/td_hal.h: td_hal_t"
    - "third_party/tinydesk commit 5ad7c45: 'the Terminal window gives the shell its width'"
    - "third_party/tinydesk @ f4c1d29, .gitmodules and third_party/tdsh: the shell at 3b7d7f8"
  verification: "Built and run on this board on 2026-10-05; the user confirmed the desktop, the Terminal, Files, the Editor's save and F12, and Castlevania from the Terminal."
  superseded_by: "TDESK-013"

- record_id: TDESK-013
  kind: EXTERNAL
  topic_id: TDESK
  title: "TinyDesk v0.1.5: the projects move to tinydesk-project"
  status: VERIFIED
  verified_date: 2026-10-06
  statement: "TinyDesk v0.1.5 is tag v0.1.5 at commit feaf84130f03594845e5e9284817dca785b4d878, from github.com/tinydesk-project/tinydesk; its main branch carries two later documentation commits (docs/VALIDATION.md) that the release does not include. It pins tinydesk-shell at 8456dd1 (TDSH-004), the same revision as this project's submodule, so there is still no second copy. In the code this port builds only include/tinydesk/td.h changes: TD_VERSION becomes 0.1.5 and TD_REPO_URL and TD_SHELL_REPO_URL point at tinydesk-project. td_hal_t and the Terminal backend are unchanged from TDESK-012. The rest of the release is ESP32 over-the-air update work and documentation."
  consequence: "The update needed no change to this port beyond the pins and the submodule URLs, which .gitmodules now gives as tinydesk-project. Built in a clean recursive clone, its own host tests pass 10 of 10 with no warnings."
  sources:
    - "tinydesk-project/tinydesk v0.1.5 (feaf841): include/tinydesk/td.h, RELEASE_NOTES.md, third_party/tdsh at 8456dd1"
  verification: "Diffed against v0.1.4 and built into this firmware on 2026-10-06; upstream ctest 10/10 passed in a clean clone; the user confirmed the console, the desktop, the Terminal, Files, F12, Castlevania and Phosphor (core-log entry 37)."

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
  status: SUPERSEDED
  verified_date: 2026-10-03
  statement: "The Bouffalo SDK is Apache-2.0 for its own code but bundles third-party components under their own licences, and three of those are linked into this firmware. FreeRTOS Kernel V10.4.6, MIT, Copyright (C) 2021 Amazon.com, Inc. or its affiliates, selected by CONFIG_FREERTOS in proj.conf. CherryUSB, Apache-2.0, Copyright (C) 2006 Bertrik Sikken, (c) 2016 Intel Corporation and (c) 2022 sakumisu, which is the device CDC console and the FreeRTOS OSAL hosting it. And FatFs R0.15 w/patch3, Copyright (C) 2022 ChaN, whose condition obliges only a redistribution of source to retain its notice. Not linked into this firmware: the SDK's LVGL, TJpgDec, mbedTLS, littlefs and multimedia codecs."
  consequence: "This is the part PROV-002's list does not reach: naming the SDK is not naming what the SDK carries, and FreeRTOS's MIT text has to travel with copies and substantial portions, which is what this firmware's binary is. THIRD_PARTY.md now names all three. Anyone enabling a further SDK component - mbedTLS for the network apps, LVGL for graphics - inherits that component's licence at the moment they enable it."
  sources:
    - "Bouffalo SDK components/os/freertos/tasks.c: 'FreeRTOS Kernel V10.4.6 / Copyright (C) 2021 Amazon.com, Inc. or its affiliates. / SPDX-License-Identifier: MIT'"
    - "Bouffalo SDK components/usb/cherryusb/core/usbd_core.c: the Apache-2.0 SPDX line and the three copyright holders"
    - "Bouffalo SDK components/fs/fatfs/ff.c: 'FatFs - Generic FAT Filesystem Module R0.15 w/patch3' and ChaN's condition"
    - "This project's proj.conf: set(CONFIG_FREERTOS 1)"
  verification: "Read from the SDK's own source headers at ~/.cache/tangcore-dev/sdk, and the FreeRTOS selection confirmed in proj.conf. The not-linked list was checked against this project's configuration and sources rather than the SDK's inventory."
  superseded_by: "TOOL-016"

- record_id: TOOL-009
  kind: TOOLCHAIN
  topic_id: TOOL
  title: "The console's input is exclusive, and a raw upload's bytes are keystrokes"
  status: SUPERSEDED
  verified_date: 2026-10-04
  statement: "tangput takes raw bytes from the same CDC stream the desktop reads its typed input from, and on that wire a file and a burst of typing are the same thing: nothing distinguishes them, because the console is one byte stream with no framing of its own. The desktop, when it is running, is the reader holding it. So a transfer is safe only while the console is at a shell prompt, and tangput itself cannot tell the difference - it will accept whatever arrives and write it."
  consequence: "A file sent while the desktop is up is delivered to whichever window has focus instead of to tangput. Nothing reports an error and tangput never sees a short write, so the failure is silent until the card is looked at. Observed on 2026-10-04: a 131088-byte ROM sent this way produced a root directory of new.txt and New folder entries with binary data where filenames belong, and damaged the allocation table badly enough that /cores, /scripts and /roms became unreachable. The board then booted a stock core (fpga reports core 0; the patched image reports 1) and boot.tdsh could not find its core. The recovery is fsck.vfat from a PC, before any reformat. tools/tinytang_put.py now refuses to send unless it sees a shell prompt, checking for the desktop's own markers ([Start], Terminal - tdsh, or the alternate-screen sequence) first; the check belongs in the tool because the alternative is remembering, and this was done twice."
  sources:
    - "This project's tools/tinytang_put.py: require_shell() and DESKTOP_MARKERS"
    - "This project's ports/bl616/tang_osd_desk.c: the desktop layer reads the console's input and forwards it to the desktop"
    - "Observed on this board, 2026-10-04"
  verification: "Observed directly: the damaged directory listing was read back from the board, the card stopped accepting writes with 'cannot create', and the same send succeeded once the desktop was exited. The guard's refusing path has not itself been exercised."
  superseded_by: "TOOL-011"

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
  consequence: "TOOL-009's guard looked for the desktop's drawing on USB and sent a carriage return to wake a prompt. Once the desktop's drawing left USB (USB-007) it could not see the desktop, its carriage return was Enter in whatever window had focus, and with the Terminal window open the shell in it printed a real prompt to USB, so the guard could pass with the desktop up. USB-007 suggested following the desktop's start and exit lines instead; asking the board was chosen because it types nothing and does not depend on having seen the start of the session. tools/tinytang_console.py's require_shell() is used by tinytang_put.py, tinytang_flash.py, phosphor_format_sweep.py and tinytang_run.py, which had no guard before and now asks before each line, so a second command waits for the first to end. No answer is treated as not ready, which also covers a firmware that predates the probe; flashing the first firmware with the probe needed the old guard and the user's word that the console was at a prompt. The probe is matched within one USB packet, so a lone Esc typed at a terminal is not held, and tangput turns the matching off while it receives, because a file may contain the sequence."
  sources:
    - "This project's ports/bl616/usb_cdc_bl616.c: usbd_cdc_acm_bulk_out()'s probe matcher, tdsh_bl616_console_set_raw() and the reply in tdsh_bl616_console_read_byte()"
    - "This project's ports/bl616/tdsh_platform_bl616.c: tdsh_bl616_console_state(), set around bl616_readline and, through tdsh_bl616_console_set_desktop(), around desktop_run() in td_desktop_bl616.c"
    - "This project's tools/tinytang_console.py: PROBE, REPLY, console_state() and require_shell()"
  verification: "Exercised on this board with firmware e912c5d-dirty.3417d90, 2026-10-05: state 1 at the prompt; no answer within 1 s during 'sleep 5' and state 1 4.8 s later, when the prompt returned; tinytang_run.py held a second command until 'sleep 3' ended; a 6120-byte file holding twenty copies of the probe arrived whole through tangput; and with the desktop and its Terminal window up the answer was 2, tinytang_run.py and tinytang_put.py both refused, and the user saw nothing typed on screen. State 3 was not observed, since no command that reads the console was running during the probe; the busy case was seen only as no answer."

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
  statement: "Everything TOOL-008 recorded still holds: FreeRTOS Kernel V10.4.6, MIT, Copyright (C) 2021 Amazon.com, Inc. or its affiliates; CherryUSB, Apache-2.0, Copyright (C) 2006 Bertrik Sikken, (c) 2016 Intel Corporation and (c) 2022 sakumisu; FatFs R0.15 w/patch3, Copyright (C) 2022 ChaN, whose condition reaches only source redistribution. Since blescan and blekbd (BLE-002) the firmware also links the SDK's Bluetooth stack, in three parts. The host, libblestack.a built from the SDK's source, is derived from Zephyr's Bluetooth host: its linked files are Apache-2.0 by SPDX tag, copyright Intel Corporation 2015-2018, Nordic Semiconductor ASA 2016-2017, Wind River Systems 2011-2014 and 2016-2017, Oticon A/S 2019 and Vinayak Kariappa Chettimada 2016, with Bouffalo Lab port files (2018, 2019) that carry no licence header. Inside the host, pairing's cryptography is TinyCrypt, Copyright (C) 2017 Intel Corporation under a BSD-3-Clause text; micro-ecc, Copyright (c) 2014 Kenneth MacKay under a BSD-2-Clause text, in ecc.c, ecc_dh.c, ecc_dsa.c and ecc_platform_specific.c; and ctr_prng.c, Copyright (c) 2016 Chris Morrison under a BSD-2-Clause text. All three BSD texts require a distribution in binary form to reproduce the notice, conditions and disclaimer in its documentation. The controller, libbtblecontroller_bl616_ble1m10s1bredr0.a, and the radio's libbl616_phyrf.a and librfparam.a are prebuilt binaries with no licence of their own; the controller's port file btblecontroller_port_uart.c reads 'Copyright (C) RivieraWaves 2009-2015', so the controller is CEVA RivieraWaves IP, and the SDK's root Apache-2.0 is the only grant found covering its redistribution. Not linked: the SDK's LVGL, TJpgDec, mbedTLS, littlefs and multimedia codecs."
  consequence: "The firmware image is a binary distribution of all of these, so the BSD notices must travel with it: their texts are reproduced verbatim in LICENSES/BSD-3-Clause-tinycrypt.txt, LICENSES/BSD-2-Clause-micro-ecc.txt and LICENSES/BSD-2-Clause-tinycrypt-ctr_prng.txt, and THIRD_PARTY.md names every part of the stack and its holders. Anyone publishing a firmware image should ship THIRD_PARTY.md and LICENSES/ with it. Whether CEVA or Bouffalo place any condition on the controller blob beyond the SDK's Apache-2.0 is not stated anywhere in the SDK; if a release ever depends on that, ask Bouffalo rather than infer it. Enabling CONFIG_BT_SETTINGS, mbedTLS or another SDK component later adds that component's licence at that moment."
  sources:
    - "build/build_out/tinytang_bl616.map: the 38 libblestack.a objects linked, and the controller, phyrf and rfparam archives"
    - "Bouffalo SDK at 7f44f9e (TOOL-003), components/wireless/bluetooth/blestack/src: the copyright and SPDX lines of each linked object's source; src/common/tinycrypt/source/aes_encrypt.c, ecc.c and ctr_prng.c for the three BSD texts"
    - "Bouffalo SDK components/wireless/bluetooth/btblecontroller/btblecontroller_port/btblecontroller_port_uart.c: 'Copyright (C) RivieraWaves 2009-2015'"
    - "Bouffalo SDK root LICENSE: Apache-2.0; the SDK's only per-component licence files are for MQTT-C, TinyMaix, cmake and pikapython, none linked here"
    - "TOOL-008 for FreeRTOS, CherryUSB and FatFs"
  verification: "The linked-object list was taken from this build's map, and each object's source header was read from the SDK; the LICENSES texts were extracted programmatically from the SDK files named above, comment markers removed and wording unchanged. strings over the controller and PHY archives found no licence or copyright text."

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
  status: SUPERSEDED
  verified_date: 2026-10-03
  statement: "TinyTang's own code is MIT, in LICENSE at the repository root, which supersedes the Apache-2.0 choice PROV-002 recorded. The one exception is ports/bl616/tang_jtag_programmer.c, which stays Apache-2.0 as nand2mario's file (TOOL-007): the Apache-2.0 text is reproduced at LICENSES/Apache-2.0.txt, and that file's header gained a comment saying so, because its original sentence pointed at the root LICENSE of the tree it was written in."
  consequence: "Apache-2.0 was never required, which is the point worth keeping: it is permissive, not copyleft, so an Apache-2.0 file may sit inside an MIT project provided that file keeps its notices and its licence text. The cost of MIT is a mixed-licence repository rather than a uniform one, which is ordinary here - Tang-Phosphor is GPL-3.0 with MIT, BSD-2-Clause and Apache-2.0 files inside it. Two consequences follow and should be known rather than discovered: the express patent grant Apache-2.0 carries is given up, and GPL-3.0 gateware from Tang-Phosphor can no longer be copied into this tree, which a GPL project could do and a permissive one cannot."
  sources:
    - "This project's LICENSE, LICENSES/Apache-2.0.txt, THIRD_PARTY.md and ports/bl616/tang_jtag_programmer.c, all changed 2026-10-03"
    - "Project statement recorded with the user, 2026-10-03: a preference for MIT unless Apache-2.0 turned out to be required"
    - "PROV-002, which this record supersedes"
  verification: "Checked that nothing forces a copyleft licence: outside third_party and build, the only file in the tree carrying a licence of its own is the vendored programmer, so the project's inbound licensing is one file wide. Both licence texts are present and were read back after being written."
  superseded_by: "PROV-005"

- record_id: PROV-005
  kind: TOOLCHAIN
  topic_id: PROV
  title: "MIT with thirteen Apache-2.0 files, and the TangCore lineage stated"
  status: VERIFIED
  verified_date: 2026-10-06
  statement: "TinyTang's own code is MIT (LICENSE). Thirteen files are Apache-2.0, with the text at LICENSES/Apache-2.0.txt: ports/bl616/tang_jtag_programmer.c, nand2mario's Gowin JTAG programmer (TOOL-007; in Tang-Control's history 9 of its 10 commits are his); and ae350_play.cpp, ae350_play.h, flac_stream_prefix.h, fpga_debug.cpp, fpga_debug.h, fpga_ext_frame.h, fpga_file_stream.cpp, fpga_file_stream.h, fpga_stream.cpp and fpga_stream.h in ports/bl616/phosphor/ with tools/tests/fpga_ext_frame_test.cpp and flac_stream_prefix_test.cpp, which were added by entries 19 and 20 after PROV-004 was written and whose every Tang-Control commit is this project's author's. Tang-Control is this project's author's fork of nand2mario's TangCore firmware-bl616 (66 of its commits are his), and TinyTang started from its board layer, so TinyTang descends from TangCore: its board knowledge was learned there, the BL616-to-core UART protocol is his design read from nestang (PROT-001), and the NES core is his nestang carried as patches. No other TangCore source file is in the tree."
  consequence: "PROV-004's 'one exception' went stale when the Phosphor port landed, and the README's earlier claim that TinyTang was 'from-scratch' and did not 'inherit from TangCore' was inaccurate and read as discounting nand2mario's work. As of 2026-10-06 README.md and THIRD_PARTY.md state the lineage and every Apache-2.0 file with its author. The Phosphor files are the project author's own and could be relicensed to MIT if ever wanted; until then they keep Apache-2.0."
  sources:
    - "This project's LICENSE, LICENSES/Apache-2.0.txt, README.md, THIRD_PARTY.md and the headers of the thirteen files"
    - "git log --follow in /run/media/vash/GIT/Tang-Control for fpga/programmer.cpp and utils/ (fpga_*, flac_stream_prefix.h, ae350_play.*), and its README: 'a fork of nand2mario's firmware-bl616'"
  verification: "On 2026-10-06 every file under ports, tools, scripts and cmake was searched for Apache-2.0 and for nand2mario, TangCore and Tang-Control, and the authorship of each carried-over file was read from Tang-Control's history (core-log entry 38)."

- record_id: BLE-001
  kind: BOARD
  topic_id: BLE
  title: "The Bluetooth antenna is a bare U.FL jack, and the radio works close up without one"
  status: VERIFIED
  verified_date: 2026-10-05
  statement: "The BL616 on the dock is U9, BL616C-50-Q2I-QFN40. Its antenna pin, net BL616_ANT, runs through L9 (0 ohm) to U35, an IPEX U.FL Gen1 jack; C186 and C187 on that net are not fitted, and the board carries no antenna of its own. The jack is fitted, on the dock's underside, marked ANT, to the left of the upper USB-A port beside the RETRO READY logo. With nothing on it the radio received a phone and a Logitech K950 at arm's length, the K950 at -76 to -94 dBm, and connected to the K950, but some connection attempts ended with HCI status 0x3E (connection failed to be established), each cleared by trying again."
  consequence: "Bluetooth works on this board as delivered for a device near it, which is enough for a keyboard or controller in front of the console. A connection attempt can fail on signal alone, so a client must expect 0x3E and try again rather than treat it as the device refusing. An antenna on U35 (a U.FL 2.4 GHz whip, or about 31 mm of wire on the centre pin) is the remedy if range or reliability matters; none has been tried."
  sources:
    - "Sipeed Tang Mega NEO dock schematics 31004 and 31005, Rev 1.4, page 8 (USB-JTAG & UART): U9, BL616_ANT, L9, U35, C186, C187"
    - "Photographs of this board's underside supplied by the user, 2026-10-05, showing the fitted jack"
    - "This project's ports/bl616/tang_ble.c: blescan's RSSI table and blekbd's connection-failure report"
  verification: "Observed on this board on 2026-10-05 with nothing on U35: blescan listed the user's phone and the K950 by name, and blekbd failed with 0x3E on several attempts and connected on a later one each time."

- record_id: BLE-002
  kind: TOOLCHAIN
  topic_id: BLE
  title: "The SDK's Bluetooth build: which controller library, how it starts, and what it costs"
  status: VERIFIED
  verified_date: 2026-10-05
  statement: "The SDK selects the BL616's Bluetooth controller by CONFIG_BTBLECONTROLLER_LIB. ble1m10s1bredr0 is the all-roles build: central, peripheral, broadcaster and observer, 10 connections, no BR/EDR, and the one examples/btble/central uses. ble1m0s1bredr0 has no central and no observer role and one connection, and ble1m0s1sbredr1 adds BR/EDR but still has no central role. The stack is brought up in the order examples/btble/central uses: rfparam_init, btble_controller_init, hci_driver_init, then bt_enable, whose ready callback reports the result; there is no call to shut the controller down again. CONFIG_BT_SETTINGS defaults to 0, in which case the host keeps nothing in flash, so pairing keys last only until reset. Built into this firmware, against the same tree without Bluetooth, ble1m10s1bredr0 with CONFIG_RF 1 grows the image from 346,160 to 582,256 bytes (+236,096: the controller library 113,954, the host stack libblestack 69,615, libbl616_phyrf 35,744, librfparam 6,639, about 10 KB else), shrinks the linker's ram_memory region from 447 KB to 415 KB, adds 17.4 KB of .bss and 0.8 KB of ITCM, and cuts the heap available at boot (__HeapLimit - __HeapBase) from 192,196 to 139,588 bytes."
  consequence: "proj.conf sets CONFIG_BLUETOOTH 1, CONFIG_BTBLECONTROLLER_LIB ble1m10s1bredr0, CONFIG_RF 1 and CONFIG_BLE_USE_MAC2 0, because scanning and connecting to a keyboard need the observer and central roles. ports/bl616/tang_ble.c starts the stack on the first blescan or blekbd rather than at boot, so a board that never uses Bluetooth runs as before, and once started it stays up until reset. The image no longer fitted the 0x80000 slot, which is why the slot grew (FLS-002). The heap the running stack takes after bring-up has not been measured. Keeping a pairing across resets needs CONFIG_BT_SETTINGS and somewhere to store it."
  sources:
    - "Bouffalo SDK 7f44f9e (2025-04-27), components/wireless/bluetooth/ble_common.cmake: the role settings for each CONFIG_BTBLECONTROLLER_LIB value"
    - "Bouffalo SDK 7f44f9e, examples/btble/central/proj.conf and its main: the library choice and bring-up order"
    - "Bouffalo SDK 7f44f9e, components/wireless/bluetooth/btblecontroller/lib: libbtblecontroller_bl616_ble1m10s1bredr0.a, ble1m0s1bredr0.a and ble1m0s1sbredr1.a"
    - "This project's proj.conf and ports/bl616/tang_ble.c: ble_start()"
    - "Linker maps of this firmware at cb9e7d4 with and without the Bluetooth block, compared on 2026-10-05"
  verification: "Both builds were made from the same tree on 2026-10-05 and their maps and images compared; the Bluetooth build ran blescan and blekbd on this board."

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
  consequence: "ports/bl616/tang_ble.c queues the setup of a device (protocol mode, every subscription, the read-backs) and runs it through kbd_setup_next and kbd_setup_step_done, one operation in flight at a time. Any further GATT client work, such as a mouse or a controller, must go through the same kind of sequencer."
  sources:
    - "This project's ports/bl616/tang_ble.c: kbd_setup_reports(), kbd_setup_next() and kbd_setup_step_done()"
  verification: "Observed on this board on 2026-10-05: the build that issued everything at once stalled in the discovering state with no further callbacks, and the sequenced build completed setup and reached ready on the next run."

- record_id: BLE-006
  kind: EXTERNAL
  topic_id: BLE
  title: "Logitech K950 over Bluetooth LE"
  status: VERIFIED
  verified_date: 2026-10-05
  statement: "The Logitech K950 keyboard (the keyboard of the MK955 Signature Slim set), in Bluetooth mode rather than on its Bolt receiver, is a HID over GATT device on Bluetooth LE advertising as 'Logi K950'. It uses a random address that increases by one each time it is put into pairing mode by holding an Easy-Switch key (seen as DB:88:A7:81:D9:CF to D9:D4). It accepts Just Works pairing at security level 2. Its primary services are 0x1800, 0x1801, 0x180A, 0x180F (battery), the HID service 0x1812 at 0x001F-0x004D, Logitech's 0xFD72 and a 128-bit vendor service from 0x005B. In the HID service, HID Information is at value handle 0x21, Boot Keyboard Input at 0x23 (notify, CCC 0x24), Boot Keyboard Output at 0x26, the Report Map at 0x28, Reports at 0x2A, 0x2E, 0x31, 0x34, 0x38, 0x3C, 0x40, 0x44 and 0x48 (notifying at 0x2A, 0x34, 0x38, 0x3C, 0x40 and 0x44), the Control Point at 0x4B and Protocol Mode at 0x4D. Writing 0 to Protocol Mode puts it in boot protocol, which reads back as 00, and it then sends the standard 8-byte boot report: modifiers, a reserved byte and six keycodes."
  consequence: "The boot report is byte-for-byte the format the wired keyboard link carries, so the existing translation in tang_key.c applies to it unchanged. Because the address changes with every pairing and pairings are not kept across a reset (BLE-002), the device must be found by blescan before each blekbd; connecting by name or by the HID appearance would remove that step. In boot protocol it still sent one 19-byte notification on 0x0044 beginning FF 04 00 01 01 01, presumably Logitech's own report, which a client should ignore."
  sources:
    - "Logitech MK955 Signature Slim product specifications: Bluetooth Low Energy connection beside the Logi Bolt receiver"
    - "GATT discovery and read-backs of this K950 by this project's ports/bl616/tang_ble.c (blekbd), 2026-10-05"
    - "Bluetooth SIG HID over GATT Profile 1.0: Protocol Mode, Boot Keyboard Input Report"
  verification: "On this board on 2026-10-05 blekbd paired with the K950, set boot protocol, wrote all seven CCCs and read them back as 01 00, and decoded 404 boot reports while the user typed: letters, Space, Enter, punctuation, right Shift, right Ctrl and right Alt, up to four keys held at once and every release. The user confirmed the result. Arrows, Esc and the function keys were not typed."

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
  consequence: "A client that re-subscribes on every connection from its own params structs must mark them volatile, or a reconnect from a bonded peer (or a new pairing of the same device within one boot) finds them still linked: re-initialising one rewrites a list node the host is using, and subscribing it fails with -EALREADY. hid_add_sub() in ports/bl616/tang_ble.c sets the flag. Bonds are in RAM only here (BLE-002), so this arises within one boot."
  sources:
    - "Bouffalo SDK 7f44f9e, components/wireless/bluetooth/blestack/src/host/gatt.c: remove_subscriptions(), bt_gatt_subscribe()"
    - "Bouffalo SDK 7f44f9e, components/wireless/bluetooth/blestack/src/include/bluetooth/gatt.h: BT_GATT_SUBSCRIBE_FLAG_VOLATILE"
  verification: "On 2026-10-05, without the flag, the M750's re-pairing after a dropout logged 'subscribe 0x002C failed (-120)', and its failed reconnect showed the host completing a CCC write nobody had issued. With the flag the next pairing subscribed all three inputs (core-log entry 32)."

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
  title: "Pairings are kept on the SD card"
  status: VERIFIED
  verified_date: 2026-10-05
  statement: "With CONFIG_BT_SETTINGS 0 the host keeps no keys across a reset (BLE-002), and its settings backend (host/settings.c) stores through easyflash, which needs a key-value partition this board's flash layout does not have (FLS-002). The host's key table is reachable instead: bt_keys_find_addr() after pairing_complete gives the device's entry, and bt_keys_get_addr() at boot makes one that can be filled with the saved LTK (legacy or LE Secure Connections), IRK, flags and key size, after which a connection encrypts with it without pairing. Both the K950 and the M750 accepted that. On a reconnect, with the device near the board, the first encryption attempt often fails with security level 1, error 8 and then a disconnect with HCI 0x3E, and a following attempt succeeds: seen on the M750 at the gate check, where the second try encrypted, and on the K950 after the power cycle, where the third did."
  consequence: "tang_ble.c captures the entry on pairing_complete into a record per slot, and the background task writes /sd/ble/bonds.bin (format in ports/bl616/tang_ble_bonds.h: magic, version, two 80-byte records, CRC-32; a damaged file means no pairings), reads it after the boot script, starts the stack quietly, restores the keys and puts the devices on the reconnect whitelist (BLE-011). The keys are stored unencrypted, so whoever holds the card can impersonate the board to those devices. Because a failed first attempt goes back to waiting, the retry needs no special handling."
  sources:
    - "Bouffalo SDK 7f44f9e, components/wireless/bluetooth/blestack/src/host/keys.h: struct bt_keys, bt_keys_find_addr(), bt_keys_get_addr(); host/settings.c: easyflash backend"
    - "This project's ports/bl616/tang_ble.c, hid_capture_bond() and hid_restore_keys(); ports/bl616/tang_ble_bonds.c"
  verification: "tools/tests/test_bonds.sh checks the file format (15 checks). On 2026-10-05 the user paired both devices once with blemouse pair and blekbd pair, power-cycled the board without pairing mode and woke them, and both reconnected; the user also saw one go out of range and come back (core-log entry 33)."

- record_id: BLE-013
  kind: TOOLCHAIN
  topic_id: BLE
  title: "Heap taken by the running Bluetooth stack"
  status: SUPERSEDED
  verified_date: 2026-10-05
  statement: "This SDK builds FreeRTOS with heap_3, so heap use is read from its own allocator, kfree_size() and g_kmemheap.heapsize in components/mm/mem.h. In firmware 1cea7b9-dirty.a296b3b the heap is 127,248 B; starting the Bluetooth stack took it from 84,120 B free to 68,292 B, 15,828 B; with the M750 connected 67,592 B were free. After a desktop session with both devices connected 41,764 B were free, 42,464 B with the mouse disconnected, and three off/on cycles of the mouse returned to 41,764 B each time."
  consequence: "Bluetooth costs about 16 KB of heap at run time on top of the static cost BLE-002 recorded, and about 700 B per connection. Reconnecting does not leak. The `ble` command reports these figures. There is no lowest-ever counter in this allocator, so a transient peak is not measured."
  sources:
    - "Bouffalo SDK 7f44f9e, components/mm/mem.h and mem.c: kfree_size(); components/os/freertos/CMakeLists.txt: heap_3.c"
    - "This project's ports/bl616/tang_ble.c, ble_start_ex() and cmd_ble()"
  verification: "Read from the board with `ble` on 2026-10-05 (core-log entry 33)."
  superseded_by: "BLE-014"

- record_id: BLE-014
  kind: TOOLCHAIN
  topic_id: BLE
  title: "Heap taken by the running Bluetooth stack, without the loader's leak"
  status: VERIFIED
  verified_date: 2026-10-05
  statement: "Everything BLE-013 measured about the stack holds: about 15.8 KB of heap to start it (84,120 B free before and 68,292 B after in that build, 82,364 and 66,536 in a later one), about 700 B per connection, and reconnects returning free heap to the same figure. BLE-013's 41,764 B 'after a desktop session' was not the desktop: every tangload, including the boot script's, leaked 4 KB (BL6-009). With that fixed and both devices connected, 64,704 B of the 124,768 B heap stayed free, with a 40,536 B largest block, before and after seven cartridge loads and a desktop session."
  consequence: "Bluetooth with two devices leaves about 65 KB of heap and a 40 KB largest block for everything else. A request near that size, such as the 32 KB script stack TinyDesk Shell asks for, can still fail once the heap fragments, which is why worker stacks are capped (BL6-009). `ble` reports free heap and the largest block."
  sources:
    - "This project's ports/bl616/tang_heap.c tang_heap_info() and ports/bl616/tang_ble.c cmd_ble()"
  verification: "Read from the board with `ble` and `crash` on 2026-10-05 (core-log entry 35)."
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
