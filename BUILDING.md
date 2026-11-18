# Building a release

TinyTang is the root of a release: it carries the BL616 firmware a user flashes
first, the desktop core, the host tools that load everything else, and the patch
appliers for the third-party trees.  TinyNES, the NES core and the AE350 firmware
inside it, is pinned by `deps.conf` as the application that runs on it.

`deps.conf` is the manifest; `tools/fetch-deps.sh` materialises and verifies it.
An entry that is `UNPINNED` there is a known gap, and this document says which
ones a builder has to close by hand.  Run `tools/fetch-deps.sh --check` first:
it prints exactly where a tree stands, and it does not change anything.

## Toolchains

The three prebuilt toolchains are pinned as their release archives, each by the
SHA-256 its publisher shows for it.  `fetch-deps.sh` downloads each one into
`.downloads/` under the release root, refuses it if the digest differs, and
unpacks it into a sibling directory named for the entry.  All three are Linux
x86-64 archives.

- **oss-cad-suite 20261008** -- yosys, iverilog, openFPGALoader, apycula.
  YosysHQ's prebuilt bundle, unpacked to `oss-cad-suite/`.
- **xPack GNU RISC-V Embedded GCC 15.2.0-1** -- builds the AE350 firmware that
  goes inside the TinyNES core.  Unpacked to `xpack-riscv-none-elf-gcc/`.
- **Xuantie-900 RISC-V toolchain V2.6.1** (GCC 10.2.0) -- builds the BL616
  firmware.  Bouffalo's own archive, unpacked to `xuantie-900-gcc/`.  Bouffalo
  used to publish this as the git repository `toolchain_gcc_t-head_linux`; on
  2026-10-12 it replaced that history with a pointer to the release archive.
  The archive is file-for-file the checkout every build here was made with.

## What you must supply yourself

"Open flow" in this document means the open **toolchain**, not a vendor-free
device model.  yosys, nextpnr, apicula and `gowin_pack` are open-source; the
device database they use is generated from Gowin EDA's own device files, which
cannot be redistributed.  The generated database's digest is pinned below, and
a machine without a Gowin install cannot regenerate it.

- **Gowin EDA 1.9.11.03** -- needed only to *generate* the device database, and
  only if you do not already have the pinned one.  It cannot be redistributed, so
  a release can only name the version it was checked against.  It is the one
  `UNPINNED` entry left in the manifest.

## Steps

```sh
# 1. the root and its submodules
git clone https://github.com/aquasock/TinyTang.git
cd TinyTang
git submodule update --init --recursive

# 2. everything else, at pinned commits, verified
tools/fetch-deps.sh              # --check first to see where you stand

# 3. point the builds at the sources and toolchains step 2 unpacked
export BL_SDK_BASE=$PWD/../bouffalo_sdk                   # or your own checkout
export PATH=$PWD/../xuantie-900-gcc/bin:$PWD/../xpack-riscv-none-elf-gcc/bin:$PATH
source ../oss-cad-suite/environment

# 4. the three artifacts
make CHIP=bl616 BOARD=bl616dk                            # the BL616 firmware
tools/build_desktop_core_open.sh                         # the desktop core
( cd TinyNES && tools/build_ae350_soc.sh )               # the NES core

# 5. check what you got against the table below
```

The device database is the one entry with a digest rather than a commit.  If the
apicula checkout has no `apycula/GW5AST-138C.msgpack.xz`, generate it:

```sh
GOWINHOME=<gowin-install> PYTHONPATH=<apicula>:<pydeps> \
    python3 -m apycula.chipdb_builder GW5AST-138C
```

`fetch-deps.sh` checks the result against the pinned SHA-256 and fails the run if
it differs.  That check is the whole point: a database generated later once gave
a different timing model and different clock routing for byte-identical source,
and that cost the desktop core its rebuildability on 2026-10-13.

## Expected artifacts

Recorded from builds of this tree; a release should reproduce these or say why
not.

- **desktop core**, `tools/build_desktop_core_open.sh` at its default seed 3,
  written to the build directory it is given (the section below records the
  ABI 1.11 core that is on the card, which is what a card should carry, and its
  seed, 3, the script's default; the ABI 1.10, 1.9, 1.8, 1.7, 1.6 and 1.3
  entries that follow are kept as the older records)

- **desktop core, the card's one file: `tinytang.bin`, desktop ABI 1.11**,
  built 2026-11-17 at seed 3 into `build/desk111p-s3/out/` with
  `DESKTOP_BUILD_DIR=build/desk111p-s3 NEXTPNR_SEED=3`.  ABI 1.11 keeps twelve
  1280x720 pictures in the SDRAM (`fpga/desktop/fb_sdram.sv`: buffer b is bank
  b / 3 from row (b % 3) * 2160): frame 0x1C picks the buffer writes go to,
  0x1D the one shown from the next frame, register 0x24 reports twelve and
  both choices, and 0x10[15:12] the buffer on screen.  The HDMI library's
  `tmds_channel.sv` is pipelined by one clock, which is what lets every seed
  meet the pixel clock (THIRD_PARTY.md).  On the board all eleven buffers a
  book uses returned their spreads' checksums exactly, and a switch reached
  the screen within two frames.
  - `desktop.bin` sha256
    `a0f328f73820ca60e6db0192e9e692737f31e1c87da1ade4c5e85383d852000c`,
    4,558,518 bytes; the card's `tinytang.bin`
  - `desktop.fs` sha256
    `32277255a1f4c1bd929b243e929f8ba26e489b365b405eaa7ce7664c5bdb0d04`,
    36,493,624 bytes; `tinytang.fs.gz` is `gzip -9 -n` of it, 467,202 bytes
  - Gates passed with nothing overridden: routed pixel clock 92.99 MHz against
    74.25, `sys_clk` 180.83 against 50, `clk` 81.76 against 21.49, setup and
    hold met, no clock-topology regression, `clk` not through SPINE17.  All
    eight seeds passed, the pixel clock at 86.57 to 92.99 MHz; before the
    encoder was pipelined, sixteen seeds of this logic reached only 65.10 to
    72.27.  LUT4 9,726, DFF 4,309, ALU 1,822.
  - Provenance: `source_sha256`
    `399c23493312fb9a1095f3f1103ef788c7b4d590ab3ee102fbe9780793a97caf`.

- **desktop core, desktop ABI 1.10** (the card's file until the ABI 1.11 core),
  built 2026-11-17 at seed 2 into `build/desk110-s2/out/` with
  `DESKTOP_BUILD_DIR=build/desk110-s2 NEXTPNR_SEED=2`.  ABI 1.10 replaces the
  BL616 link's UART with `fpga/desktop/link_uart.sv`, timed on the 50 MHz
  crystal: it comes up at 2 Mbaud and moves to 5 or 10 Mbaud when register
  opcode 0x03 asks (capability bit 3, caps 0x1b), answering at the old rate
  first; a line held low for 3 us at a faster rate drops it back to 2 Mbaud.
  Register 0x1c reads the rate in bits per second and 0x20 the fallbacks and
  framing errors.  `uart_fixed.v` is gone.  On the board the firmware
  negotiated 10 Mbaud at load, a 1280x720 screen went in 1.86 s (992 KB/s),
  and 3,600 frames then matched their checksum.
  - `desktop.bin` sha256
    `537265ba89eeec74da349230760f272e80160fd7fc18dcf46721624ba5f41da6`,
    4,524,012 bytes; the card's `tinytang.bin`
  - `desktop.fs` sha256
    `d1bf50bfb8c2f8735bb7a1bbb2f00c86d5d7f2127faba23f652fd3d56700c94f`,
    36,217,061 bytes; `tinytang.fs.gz` is `gzip -9 -n` of it, 441,100 bytes
  - Gates passed with nothing overridden: routed pixel clock 75.90 MHz against
    74.25, `sys_clk` 149.03 against 50, `clk` 83.39 against 21.49, setup and
    hold met, no clock-topology regression, `clk` not through SPINE17.  Seed 2
    is the only one of eight whose routed pixel clock met 74.25; the others
    reached 65.39 to 74.05 MHz, failing on the HDMI encoder's TMDS adder chain
    as before.  LUT4 9,273 and DFF 4,150, against 8,958 and 4,062 for ABI 1.9.
  - Provenance: `source_sha256`
    `7f9a97942294bf4cba9c5ec66e22e1cbd4867705c896840c881f3d2d12009eb9`.

- **desktop core, desktop ABI 1.9** (the card's file until the ABI 1.10 core),
  built 2026-11-17 at seed 4 into `build/desk19-s4/out/` with
  `DESKTOP_BUILD_DIR=build/desk19-s4 NEXTPNR_SEED=4`.  ABI 1.9 moves the pixel
  plane into the dock's SDRAM at 1280x720, shown 1:1 (`fpga/desktop/fb_sdram.sv`,
  frames 0x18-0x1B, geometry 1280x720 at register 0x0c, status and frame
  checksum at 0x10-0x18), so block RAM falls from 242 to 17; the SDRAM pins
  are TinyGBA's, and its clock leaves through an ODDR as the inverse of the
  pixel clock.  The read alignment is 3 clocks on the rising edge, the only
  one of fourteen at which `tools/plane_sweep.py` got a 1280x720 frame back
  intact; 7,200 frames in a row then matched.
  - `desktop.bin` sha256
    `9c63348843e25e49d8aa3557bdc0a468bed2c8e5a9b35e66692381f072d260c3`,
    4,508,140 bytes; the card's `tinytang.bin`
  - `desktop.fs` sha256
    `420957e63dccbdfa82855278c7e9cb5542772f26bd83972bbaa7f5858414cc34`,
    36,089,829 bytes; `tinytang.fs.gz` is `gzip -9 -n` of it, 432,007 bytes
  - Gates passed with nothing overridden, with the corrected timing gate that
    reads the routed result (`tools/desktop_open/pnr_timing.py`): routed pixel
    clock 75.91 MHz against 74.25, setup and hold met, no clock-topology
    regression, `clk` not through SPINE17.  Seed 4 is the only one of eight
    whose routed pixel clock met 74.25; the others reached 70.51 to 73.58 MHz,
    and the failing path is the HDMI encoder's TMDS adder chain, as before.
  - Provenance: `source_sha256`
    `371673509c3ed6573c8c4c475264b93052e751ac56045cb7d7bcc5264f847460`.

- **desktop core, desktop ABI 1.8** (the card's file until the ABI 1.9 core),
  built 2026-11-17 at seed 3 into `build/desk18-s3/out/`.  ABI 1.8 adds the
  640x360 RGB565 pixel plane (`fpga/desktop/pixplane.sv`, removed in ABI 1.9; frames 0x18-0x1A,
  geometry at register 0x0c), which takes 228 block RAMs (242 in all).
  - `desktop.bin` sha256
    `13989970c925b5f960ec68fbedc57a882eb2a558c1137cf2746722ac1e033300`,
    5,174,764 bytes; the card's `tinytang.bin`
  - `desktop.fs` sha256
    `57b392049972d08e719be449baa59e9fff7950deb838ac9158218bfdb6f9b1c1`,
    41,433,573 bytes; `tinytang.fs.gz` is `gzip -9 -n` of it, 721,631 bytes
  - Gates passed with nothing overridden: setup and hold met (`clk` 67.40,
    pixel 81.26, sys 155.04 MHz against 21.49, 74.25 and 50), no clock-topology
    regression.  **Only seed 3 of eight passed**: the other seven missed the
    pixel clock at 58.7 to 69.2 MHz and regressed the clock topology.
    **Corrected 2026-11-17:** the timing gate then read nextpnr's placement
    estimate whenever the routed clock failed; re-read from the same logs, the
    routed pixel clock is 66.68 to 71.87 MHz at all eight seeds, seed 3 at
    67.12, so this core never met 74.25 MHz, though it ran on the board.  The
    failing path is the unchanged HDMI encoder's TMDS adder chain, displaced
    by the plane's block RAMs, so a later change to this core may need a wider
    seed sweep, or an 8-bit plane at half the block RAM.
  - Provenance: `source_sha256`
    `e9e2a104901e36202806a2497eb766b4ea0fd1223f855f7802476c76f785c17c`.


- **desktop core, ABI 1.7** (the card's file until the ABI 1.8 core),
  built 2026-11-17 from the tree `e1df16a` plus the uncommitted font and ABI
  change that its commit carries, at seed 3 into `build/desk17-s3/out/`, one of
  eight seeds built in parallel that all passed every gate.  ABI 1.7 puts 23
  block glyphs (halves, shades, quadrants, pointers, a square) in the font's
  slots 0x01-0x17, which the firmware uses only when this core answers 1.7.
  - `desktop.bin` sha256
    `ee07082bf4e51b624d6adf6ca2facea74d5b9b4b348da1e563d93b84261ee114`,
    4,492,268 bytes; the card's `tinytang.bin`
  - `desktop.fs` sha256
    `d31c1784cf3e5ebe90888c724b9577418c465446b6002dfa0560b6834ed21516`,
    35,962,597 bytes; the card's `tinytang.fs.gz` is `gzip -9 -n` of it,
    398,809 bytes
  - Gates passed with nothing overridden: setup and hold met (`clk` 77.42,
    pixel 83.00, sys 144.84 MHz against 21.49, 74.25 and 50), no clock-topology
    regression, no `clk` pip through SPINE17.  Proven on the board the same
    day: `fpgar 4` read `0x00010007`, and TinyDesk 0.5.2 drew its pictures and
    window furniture in the new glyphs.
  - Provenance: `source_sha256`
    `b6e459e4eaef888b044a5c65f644dfd8c5db816e78f3045ad9a360775f48f384`,
    apicula `495661a`, nextpnr `16237b0f`, in `provenance.txt` beside it.


- **desktop core, ABI 1.6** (the card's file until 2026-11-17),
  built 2026-11-03 from `207f9af`'s `fpga/desktop` at the script's default
  seed 3 into `build/desktop-rev/out/`
  - `desktop.bin` sha256
    `00d8b48cac8e54b27cdbdacf2adfe8e67331bd3ff95c05e6fd671192f46582cf`,
    4,526,774 bytes; the card's `tinytang.bin`
  - `desktop.fs` sha256
    `60dd20032095f6d1b7e14dab9e77ca57973e5d494051d0acf2955dd929e45fda`,
    36,239,160 bytes; the card's `tinytang.fs.gz` is `gzip -9 -n` of it,
    401,300 bytes
  - Gates passed with nothing overridden: setup and hold met, no
    clock-topology regression against `clock_profile.json`, and no `clk`
    pip through SPINE17. Proven on the board the same day: `tangini` read
    `core 0x54 (ABI 1.6) has 0xc0 = 0x0080, as declared` and `node status`
    returned TinyNode's block in full.
  - **The tuple that makes it**, which `deps.conf` now pins and the build
    writes beside every artifact as `provenance.txt`:
    - source `fpga/desktop` at `207f9af`, `source_sha256`
      `b313d00f7824b3d425f222dd8b24355a0e2a4186286491a14af69b539022d96d`
    - apicula `495661a`, nextpnr `16237b0f`
      (`nextpnr_binary_sha256` `2eca603a2a90424305fd72d53a50c3d1b8daeb21e8f0451690cfc4797b85c6c1`)
    - device database `e74b77e429e9be2e176477fa4d3c9984a1bdee8a17dad5dd3ab80620aefe175b`,
      regenerated from Gowin EDA 1.9.11.03
    - yosys 0.69+260, Python 3.11.6
  - **It reproduces.** `tools/build_desktop_core_open.sh --verify
    build/desktop-rev/out/provenance.txt` rebuilds from this tree and compares
    every input field and both output digests: fifteen fields match. Point that
    same command at a different nextpnr and it refuses, naming the six fields
    that moved -- which is the check that was missing when a core of record
    could not be rebuilt from its own recorded tuple.
  - The ABI 1.3 entry below is kept as history. Its expected bitstream
    `24a11e81a46c` does not come back from its own recorded tuple: built from
    commit `cfb29aa` with the apicula `04bc3c5` and nextpnr `8be8657b`
    `deps.conf` pinned there, and that chipdb regenerated from the same Gowin
    install, the flow produces `4558404a7404` instead. The flow is
    deterministic -- two builds from one tree and one toolchain are
    byte-identical -- so that gap is a record of the environment that was never
    kept, not a clock or a seed. Treat that digest as a statement about an
    October machine, not a guarantee.

- **desktop core**, `tools/build_desktop_core_open.sh` at its default seed 3,
  written to `build/desktop/open-s3/` (desktop ABI 1.3 with the PMOD I2C bus,
  apicula `04bc3c5`, nextpnr `8be8657b`, recorded 2026-10-18)
  - `desktop.fs` sha256 `d993899c24a44dcfa503554e27ca873d012b0592162a7335f6b4c46378ed865b`
  - `desktop.bin` sha256 `24a11e81a46cd6a9937c9dca8e2e0f5d8494fc6f4f1e029d3917cef846519fb0`
    -- **recorded, not reproducible** (2026-11-03): rebuilding commit `cfb29aa`
    from this tuple gives `4558404a7404`. Kept as history; see the note above.
  - The previous core of record, seed 26 of desktop ABI 1.2 (`desktop.bin`
    `16c6c5998c9a`), is rebuilt from commit `97cf0db` with the chipdb
    digest `446b89c0e96e` that `deps.conf` pinned there.
  - Both of the build's gates pass with nothing overridden: setup and hold
    are met, and the clock topology matches
    `tools/desktop_open/clock_profile.json`, which is recorded from this core.
    A build that needs `ALLOW_UNVERIFIED_CLOCK=1` is not this one.  The
    suspect-hop check (step 2d) reports no `clk` pip through `SPINE17`, and
    now refuses a placement that routes one there, because both timing-clean
    cores on record with such a route loaded with `clk` dead (core-log entry
    70); `ALLOW_SPINE17_CLOCK=1` overrides that refusal.
  - The seed is not a property of the design.  Any change to the core's
    netlist re-rolls the placement, and a timing-clean placement can still
    load with a dead main clock, so a changed core needs a seed sweep and a
    board test, not only these gates (core-log entry 67).
  - `v0.1.0` shipped the ABI 1.1 core built at seed 1 from that tag:
    `desktop.bin` `b9af4f06221e5c11750f3575f3b6d6e344a2671a4ca0d98f738d8d2ec0eb96de`,
    `desktop.fs` `7453ad6c940062882f80416b334bf516b0382ee0092f60bb52b547d703103989`.
- **TinyNES core**, `tools/build_ae350_soc.sh`
  - `ae350_soc.bin` sha256 `bafc2cf2ea79fc9e36a948b948afda62df07b9c5698da738d46eaaeb516ddfcf`
  - `ae350_soc.fs` sha256 `6b0c433a5c7e0783d6a06e557f9878ffb9cfac4d432072f2acf8b5dbb62640a8`
- **BL616 firmware**, `make CHIP=bl616 BOARD=bl616dk`
  - `build/build_out/tinytang_bl616.bin`, build identity from
    `build/tinytang/tinytang_build_id.h`, which the board prints as `platform`.
  - No release hash was recorded for the first build: it was made from a tree
    with uncommitted changes and reported `a1e6275-dirty.240903a`.  The build
    from entry 57's commit reported `dba104c-dirty.17f81b2`, 611520 bytes,
    sha256 `238cee62ce93fbf70d5332e03ea10e61776f1330afa059d99c0f60df1dc90c59`,
    and that is the digest the release card carries.  The firmware has
    changed since `v0.1.0` (the OLED font sets, core-log entry 67), so a build
    of this tree is a development build with its own identity, not that
    digest.
  - The identity keeps its `-dirty` suffix by design, and a bare one is not
    reachable without moving a pin.  `third_party/tinydesk` is pinned at
    upstream v0.5.2 and the build applies two carried patches to it on every
    configure (`scripts/apply-tinydesk-patches.sh`, run from `CMakeLists.txt`),
    so that submodule's working tree always differs from its pin.  Identify the
    patches by their own digests, not by the identity, because `git diff`
    renders a dirty submodule only as `<sha>-dirty` and the identity's hash is
    the same whatever patches are applied:
    `third_party/patches/tinydesk/0001-settings-bluetooth-button.patch`
    `070b58c505e5c600187b96456f3a7904ac74f2cfe29746d2759512de347f77a3`,
    `third_party/patches/tinydesk/0002-cartridge-is-not-text.patch`
    `c985f1d9bac1b0332cf8b897e83d0532516cd07b23c464037fad0f0e2e4aeec6`,
    `third_party/patches/tinydesk/0003-slider-widget.patch`
    `2a6146f0b0a9d8a204a807d73b7f882baef6692b5d14fda60d4a122034e72dbf` and
    `third_party/patches/tinydesk/0004-start-menu-exit.patch`
    `36af95dcaa4049cd785edbdef8cd3d9164320164ebc19c1bf4040065d5f2d3d7`.
    `third_party/tinydesk-shell` shows how it ends: its patch went upstream, the
    pin now names an `origin/main` commit, and `scripts/apply-tdsh-patches.sh`
    recognises that and applies nothing.

## What is not reproducible yet

Stated plainly, because a release that hides these is worse than one that names
them.

- **The toolchain archives' hosting.**  Each is pinned by digest, so a changed
  archive fails loudly, but none is hosted by this project.  The Xuantie archive
  sits under Bouffalo's release tag `default`, which is not immutable -- an
  asset under it was replaced a day after it was published -- so it may vanish
  or change, and the git commit it replaced (`c4afe91` in
  `bouffalolab/toolchain_gcc_t-head_linux`) is on no branch any more.  If either
  happens, the digest still says what the right archive is, but not where to
  get it.
- **The device database.**  Hash-pinned, which detects drift, but it still has to
  be *generated* from a Gowin install.  A machine with no vendor software cannot
  build this from scratch, so the classroom goal is not met yet.  Shipping the
  database is the next release's question, and it is a licensing decision before
  it is an engineering one.
- **Gowin's device files.**  The desktop core and the TinyNES core use the open
  toolchain, but both still need them to generate the database.  (The
  nestang-derived NES and menu cores, which needed Gowin EDA itself, were
  retired in core-log entry 25.)

## Licences

The three project repositories are MIT; TinyNES and TinyTang each carry a
`LICENSE`.  `THIRD_PARTY.md` in each records every component, its provenance and
its terms.  Two things in the stack are worth knowing before redistributing:
the Bluetooth controller and PHY in the firmware are binary-only, and Gowin EDA
may not be shipped at all.
