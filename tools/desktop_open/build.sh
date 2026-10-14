#!/usr/bin/env bash
#
# Build TinyTang's desktop core end to end with the open flow.
#
#   tools/desktop_open/build.sh <reconstructed-design-tree> [out-dir]
#
#   NEXTPNR_BUILD   directory holding nextpnr-himbaechel
#                   (default /home/vash/tools/nextpnr-mathieufro)
#   APICULA_FORK    the fork's apycula (default /run/media/vash/GIT/apicula-mathieufro)
#
# This is the three steps the project has been running by hand: synthesis
# (`synth-desktop.sh`), place and route (`pnr-desktop.sh`), and packing with the
# fork's `gowin_pack`.  It exists because running them by hand cost real time:
#
#  * `pnr-desktop.sh` passes nextpnr NO `*_as_gpio` options, so the packer has to
#    be run without them too.  Passing the vendor's six (mspi, ready, done, i2c,
#    cpu, sspi) makes `gowin_pack` refuse with "i2c_as_gpio has conflicting
#    settings in nexpnr and gowin_pack", which is the packer working correctly.
#    Matching the vendor's pinout would mean passing the options to both tools,
#    and this flow does not do that today.
#  * The packer needs the fork's apycula, whose `get_pll_attrvals` handles the
#    string parameters the slang front end leaves in place; the suite's would
#    reject them.
#  * Nothing here defaults into /tmp.  A build that lives in a temporary
#    directory has cost this project a bitstream before.
#
# The tree is left as it was apart from the build artefacts the two scripts
# write into it (`.open-shim/`, `.open-pnr/`, `desktop-top-open.json`).
#
# Brought over from Tang-Build 737ddb4 (scripts/build-desktop-core.sh with its
# synth, pnr and gate helpers), which owned this flow until TinyTang's core
# build moved to it; tools/build_desktop_core_open.sh is the entry point.
#
# SPDX-License-Identifier: MIT
set -euo pipefail
export LC_ALL=C

here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

tree=${1:-}
if [[ -z $tree || ! -d $tree ]]; then
    echo "usage: $0 <reconstructed-design-tree> [out-dir]" >&2
    exit 2
fi
tree=$(cd "$tree" && pwd)
out=${2:-$tree/.open-fs}

A=${APICULA_FORK:-/run/media/vash/GIT/apicula-mathieufro}
NPNR=${NEXTPNR_BUILD:-/home/vash/tools/nextpnr-mathieufro}
# The placement seed.  Placement decides how much clock skew this design's main
# clock accumulates: `clk` rides general fabric, because the chipdb's model of
# the TLPLL0CLK0 spine and bridge path does not match the silicon (cores routed
# over it meet timing and load dead), so the skew depends on where its segments
# land.  With pll_nes pinned to PLL_R[0], seed 26 is the hardware-proven core
# (16c6c5998c9a, HDMI, VGA and the OLED's three font sets correct and clean
# when booted at 67.3 C; see fpga/desktop/desktop.cst) -- the gate below
# refuses to publish a placement that is not violation-free.  A seed is not a
# property of the design: any netlist change re-rolls the placement, and a
# timing-clean seed can still load with a dead `clk` (core-log entry 67).
SEED=${NEXTPNR_SEED:-26}
export NEXTPNR_SEED=$SEED
PY=${PY:-/home/vash/oss-cad-suite/py3bin/python3}
PYDEPS=${PYDEPS:-/home/vash/tools/pydeps}
DEVICE_PART="GW5AST-LV138PG484AC1/I0"
DEVICE_FAMILY="GW5AST-138C"

log() { printf '%s\n' "$*" >&2; }

if [[ -z ${OSS_CAD_SUITE_SOURCED:-} ]]; then
    for env in "$HOME/oss-cad-suite/environment" /opt/oss-cad-suite/environment; do
        # shellcheck disable=SC1090
        [[ -f $env ]] && { source "$env"; SUITE=$(cd "$(dirname "$env")" && pwd); break; }
    done
fi
[[ -n ${SUITE:-} ]] && export PYTHONHOME="$SUITE"
export PYTHONPATH="$A:$PYDEPS:${PYTHONPATH:-}"
[[ -d $A/apycula ]] || { log "no fork apycula at $A"; exit 2; }
mkdir -p "$out"

# --------------------------------------------------------------- 1. synthesis
log "=== 1. synthesis ==="
bash "$here/synth.sh" "$tree" "$tree/desktop-top-open.json"
[[ -f $tree/desktop-top-open.json ]] || { log "no netlist produced"; exit 1; }

# ---------------------------------------------------------- 2. place and route
log "=== 2. place and route ==="
bash "$here/pnr.sh" "$tree" "$tree/desktop-top-open.json"
[[ -f $tree/.open-pnr/pnr.json ]] || { log "no routed netlist produced"; exit 1; }

# A routed netlist is not a working bitstream.  `pnr-desktop.sh` passes nextpnr
# `--timing-allow-fail`, so a violated path is reported and the flow carries on
# and packs it -- and a build with a hold violation is a build that runs by
# luck, whatever the screen happens to do.  TinyTang's Gowin recipe sets the
# same bar for its own binary ("zero setup/hold violations"), so require it here
# and refuse to pack.  tools/pnr-timing.py explains why this reads the log:
# hold violations appear in no structured field of nextpnr's report.
log "=== 2b. timing gate: setup and hold (seed $SEED) ==="
if ! "$PY" "$here/pnr_timing.py" "$tree/.open-pnr/nextpnr.log" \
    --require-clock keyboard_link.clk=50 \
    --require-clock desktop_sockets.pixel_clk=74.25 \
    --require-clock clk=21.49; then
    log ""
    log "REFUSING TO PACK: the place-and-route has setup or hold violations."
    log "A bitstream from this netlist is not one to put on the board."
    log "The paths and their slack are above; the PLL site that decides the"
    log "main clock's skew is recorded in $tree/src/desktop/desktop.cst and"
    log "Tang-Build's evidence/hclk-route-gap.txt."
    exit 1
fi

# --------------------------------------------------------------------------
# 2c. clock topology gate.
#
# A clean violation count is NOT evidence that the clocks work.  The chipdb's
# HCLK interconnect is incomplete, so nextpnr can route a clock on general
# fabric -- or complete it on "global resources only" -- and report no problem
# at all.  On 2026-10-09 a build passed every timing check here and did not
# boot on the board, while the build that does boot also fails this gate.  A
# pass has to be measured, not assumed, so every clock net is inventoried and
# any one that does not ride the dedicated network stops the build.
#
# The override exists because experimental bitstreams still have to be built to
# be tested on hardware.  It bypasses ONLY this topology gate -- never the
# setup/hold gate above -- and the build is then labelled unverified on the way
# out.  Do not describe such a build as a good core.
# --------------------------------------------------------------------------
# 2c. clock topology check.
#
# NOT a demand that every clock be dedicated.  On this die a PLL's clock
# reaches the clock plane through fabric and a logic gate, and nextpnr says so
# itself: "no clock gate reaches both its source and its loads".  No build on
# record has every clock dedicated -- the hardware-proven baseline, the seed-26
# core with pll_nes at PLL_R[0], has `clk` on global resources only and the
# pixel clock and hclk5 partly on fabric -- so an all-dedicated gate refuses
# every build forever, which is a wall, not a check.  "Global resources only"
# is not by itself the dead TLPLL0CLK0 route TOOL-017 records: it is that route
# only with pll_nes at PLL_L[0], which the profile no longer guards against, so
# pll_nes stays at PLL_R[0] (fpga/desktop/desktop.cst).  Nor does a matching
# verdict prove a global route works: the one dead PLL_R[0] core on record
# (seed 6, entry 67) differed from the working ones only in taking `clk`
# through SPINE17 <- CBRIDGEOUT_TOP2, which this check does not look at.  What is checked instead is *regression*: the
# run is compared with the recorded profile of the hardware-proven build, and
# it stops only when a clock that was better has become worse.  The full
# topology is written beside the log, so a fabric fallback is visible and
# recorded rather than silent.
#
# A pass here still proves nothing about the clock: a run that matches the
# profile is exactly as unverified as that build is.  Only the board decides.
log "=== 2c. clock topology check (vs the recorded profile) ==="
topo=$tree/.open-pnr/clock-topology.txt
# Wrapped in `if` so `set -e` cannot abort before the exit code is read.  A
# bare failing command here killed the script at the inventory, which made the
# ALLOW_UNVERIFIED_CLOCK override below unreachable -- the override could never
# be used on the one run it exists for.
if "$PY" "$here/clock_route_inventory.py" "$tree/.open-pnr/nextpnr.log" \
    --compare "$here/clock_profile.json" > "$topo" 2>&1; then
    rc_topo=0
else
    rc_topo=1
fi
sed 's/^/   /' "$topo"
if (( rc_topo != 0 )); then
    log ""
    if [[ -z ${ALLOW_UNVERIFIED_CLOCK:-} ]]; then
        log "REFUSING TO PACK: the clock topology regressed against the"
        log "recorded profile above.  Set ALLOW_UNVERIFIED_CLOCK=1 to pack"
        log "anyway; the result is an experimental bitstream, not a core."
        exit 1
    fi
    log "ALLOW_UNVERIFIED_CLOCK is set: packing a REGRESSED-clock build."
    log "Label it experimental.  It is not a good core until the board says so."
fi

# ----------------------------------------------------------------- 3. packing
# No *_as_gpio options: see the note at the top of this script.
#
# The plain packer, with no fuses added by hand.  Entry 38's four interim
# lane-0 fuses at R58C182/R59C182 went in core-log entry 39, once the fork's
# chipdb (>= 93761f4) served row 57 from HCLK block 3 as the
# vendor does; nextpnr now gives all three TMDS serialisers block 3 lane 2,
# which the board proved.
log "=== 3. pack (gowin_pack, the fork's apycula) ==="
"$PY" -W ignore -m apycula.gowin_pack -d "$DEVICE_FAMILY" \
    -o "$out/desktop.fs" "$tree/.open-pnr/pnr.json" 2>&1 \
    | grep -vE "warnings.warn|UserWarning|DeprecationWarning" | sed 's/^/   /' || true

log ""
if [[ -f $out/desktop.fs ]]; then
    ls -l "$out/desktop.fs" | sed 's/^/   /'
    sha256sum "$out/desktop.fs" | sed 's/^/   /'
    log "   packed ok"
else
    log "   NO BITSTREAM"
    exit 1
fi

# ------------------------------------------------------- 4. provenance stamp
# Name every build after its own contents, and emit the binary too.
#
# Why: the vertical-line hunt lost a cycle to Tang-Build's `evidence/desktop-open.bin`
# (`fadd00ba...`) -- an artefact that renders the display perfectly and whose
# source nobody recorded.  Recovering it meant guessing at a reconstruction,
# and guessing failed.  A stamped copy of every build makes that situation
# impossible.
#
# The stamp is the first 12 hex characters of the artefact's own sha256, which
# is this project's existing convention in prose (`7b95d942`, `84915457`,
# `fadd00ba`) written into the filename.  It is content-addressed, so the name
# IS the identity and two builds with the same name are the same bits; it says
# nothing about anyone's repository or working tree; and it is computed here,
# on the host.  Nothing hashes on the board -- the BL616 only ever receives a
# byte stream.
#
# The plain `desktop.fs` / `desktop.bin` names are still written, because the
# card's boot path and other tools expect them.
get_bin() {
    "$PY" "$here/fs_to_bin.py" "$out/desktop.fs" -o "$out/desktop.bin" \
        > /dev/null 2>&1 || { log "   bin conversion failed"; exit 1; }
}
short() { sha256sum "$1" | cut -c1-12; }
get_bin
BINHASH=$(short "$out/desktop.bin")
FSHASH=$(short "$out/desktop.fs")
log ""
log "=== 4. provenance stamp ==="
cp -f "$out/desktop.bin" "$out/desktop.$BINHASH.bin"
cp -f "$out/desktop.fs"  "$out/desktop.$FSHASH.fs"
ls -l "$out/desktop.$BINHASH.bin" "$out/desktop.$FSHASH.fs" | sed 's/^/   /'
log "   desktop.$BINHASH.bin  <- put this on the card for tangload"
log "   desktop.$FSHASH.fs  <- this one is for openFPGALoader"
log "   record both stamps, and the pair, in the log entry for this build."
