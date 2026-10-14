# Building a release

TinyTang is the root of a release: it carries the BL616 firmware a user flashes
first, the desktop core, the host tools that load everything else, and the patch
appliers for the third-party trees.  TinyNES, the NES core and the AE350 firmware
inside it, is pinned by `deps.conf` as the application that runs on it.

`deps.conf` is the manifest; `tools/fetch-deps.sh` materialises and verifies it.
An entry that is `UNPINNED` there is a known gap, and this document says which
ones a builder has to close by hand.  Run `tools/fetch-deps.sh --check` first:
it prints exactly where a tree stands, and it does not change anything.

## What you must supply yourself

These are in the manifest but cannot be fetched or verified by it.  No checksum
is recorded for any of them, and one of them has no recorded source at all.

- **oss-cad-suite** -- yosys, iverilog, openFPGALoader.  YosysHQ's prebuilt
  bundle.  In use: release `20261008`.
- **xPack GNU RISC-V Embedded GCC 15.2.0-1** -- builds the AE350 firmware that
  goes inside the TinyNES core.
- **Xuantie-900 RISC-V toolchain V2.6.1** (GCC 10.2.0) -- builds the BL616
  firmware.  **Where this came from is not written down anywhere.**  That is the
  worst gap in the release and the first thing to fix.
- **Gowin EDA 1.9.11.03** -- needed only to *generate* the device database, and
  only if you do not already have the pinned one.  It cannot be redistributed, so
  a release can only name the version it was checked against.

## Steps

```sh
# 1. the root and its submodules
git clone https://github.com/aquasock/TinyTang.git
cd TinyTang
git submodule update --init --recursive

# 2. everything else, at pinned commits, verified
tools/fetch-deps.sh              # --check first to see where you stand

# 3. point the builds at the toolchains you supplied
export BL_SDK_BASE=<apicula-sibling>/bouffalo_sdk        # or your own checkout
export PATH=<xuantie>/bin:<xpack>/bin:$PATH
source <oss-cad-suite>/environment

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

Recorded from builds on 2026-10-13; a release should reproduce these or say why
not.

- **desktop core**, seed 1, `tools/build_desktop_core_open.sh`
  - `desktop.fs` sha256 `7453ad6c940062882f80416b334bf516b0382ee0092f60bb52b547d703103989`
  - `desktop.bin` sha256 `b9af4f06221e5c11750f3575f3b6d6e344a2671a4ca0d98f738d8d2ec0eb96de`
  - The build's own clock-topology gate reports a regression against
    `tools/desktop_open/clock_profile.json` and needs `ALLOW_UNVERIFIED_CLOCK=1`.
    That is expected and explained in core-log entries 52 and 53; the build is
    still the one the board runs.
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
    and that is the digest the release card carries.
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

- **The three toolchains.**  No URL and no digest for any of them, and for the
  Xuantie compiler not even a source.  A builder has to supply the same versions
  by hand.
- **The device database.**  Hash-pinned, which detects drift, but it still has to
  be *generated* from a Gowin install.  A machine with no vendor software cannot
  build this from scratch, so the classroom goal is not met yet.  Shipping the
  database is the next release's question, and it is a licensing decision before
  it is an engineering one.
- **The NES and menu cores.**  Not covered here at all: they come from nestang and
  need Gowin EDA to build.  Only the desktop core and the TinyNES core are
  open-flow.

## Licences

The three project repositories are MIT; TinyNES and TinyTang each carry a
`LICENSE`.  `THIRD_PARTY.md` in each records every component, its provenance and
its terms.  Two things in the stack are worth knowing before redistributing:
the Bluetooth controller and PHY in the firmware are binary-only, and Gowin EDA
may not be shipped at all.
