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

- **desktop core**, `tools/build_desktop_core_open.sh` at its default seed 26,
  written to `build/desktop/open-s26/` (desktop ABI 1.2, recorded 2026-10-14)
  - `desktop.fs` sha256 `be6f111c0b53b644bd55b18ca1cc128f75e26ca562f62fcf72f886d92f9783bb`
  - `desktop.bin` sha256 `16c6c5998c9a9c940ace86e0548f16e7e719f48982008ecc18e88d66599e0ba7`
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
    upstream v0.1.5 and the build applies two carried patches to it on every
    configure (`scripts/apply-tinydesk-patches.sh`, run from `CMakeLists.txt`),
    so that submodule's working tree always differs from its pin.  Identify the
    patches by their own digests, not by the identity, because `git diff`
    renders a dirty submodule only as `<sha>-dirty` and the identity's hash is
    the same whatever patches are applied:
    `third_party/patches/tinydesk/0001-settings-bluetooth-button.patch`
    `070b58c505e5c600187b96456f3a7904ac74f2cfe29746d2759512de347f77a3` and
    `third_party/patches/tinydesk/0002-open-with-handler-script.patch`
    `601ef5c77c26fabe2a1e3466920692798bdf8a2129593bf7f58c856d8977cc6a`.
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
- **The NES and menu cores.**  Not covered here: they come from nestang, which
  the manifest pins at `c2450818`, and need Gowin EDA to build.  Only the desktop
  core and the TinyNES core use the open toolchain; both still need Gowin's
  device files to generate the database.

## Licences

The three project repositories are MIT; TinyNES and TinyTang each carry a
`LICENSE`.  `THIRD_PARTY.md` in each records every component, its provenance and
its terms.  Two things in the stack are worth knowing before redistributing:
the Bluetooth controller and PHY in the firmware are binary-only, and Gowin EDA
may not be shipped at all.
