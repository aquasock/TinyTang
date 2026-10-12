#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Take the synthesised desktop core into place-and-route with nextpnr-himbaechel.
#
#   tools/desktop_open/pnr.sh <reconstructed-design-tree> [netlist.json]
#
# This needs the FORK's nextpnr-himbaechel, not oss-cad-suite's.  The suite's
# binary carries the database published in apicula's PyPI package, which for
# GW5AST-138C has no PLL site and no clock pips, so it stops at
# "no BELs remaining to implement cell type 'PLL'".  The fork's is built
# against the database regenerated from the local Gowin install, which carries
# them.  See Tang-Build's TOOLCHAIN.md, "Building nextpnr against the database".
#
# Point at it with NEXTPNR_HIMBAECHEL, or leave that unset to use the default
# path below.  The script refuses the suite's copy outright rather than trying
# to place and reporting a wall that is really a wrong binary.
#
# Two pieces of design-to-toolchain translation are needed first, and both are
# done here rather than by editing the design:
#
#  * nextpnr's SDC reader does not accept `//` comments, and desktop.sdc opens
#    with one.  Converted to `#`.
#  * nextpnr requires every IO to have a location.  Any top-level port the
#    constraint file does not pin is dropped -- but only after checking it
#    really is unused, because silently deleting a port that something reads
#    would be worse than the error it avoids.
#
# Tang-Build also carries a third transform, forcing the main PLL clock onto the
# modeled clock network through a static DCS (its tools/desktop-clock-
# distribution.py).  It made the clock route in the model and produced a
# bitstream that did not boot on hardware (2026-10-09), so it is not carried
# here; this script builds the plain configuration that boots VGA.
set -euo pipefail
export LC_ALL=C

here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

tree=${1:-}
if [[ -z $tree || ! -d $tree ]]; then
    echo "usage: $0 <reconstructed-design-tree> [netlist.json]" >&2
    exit 2
fi
tree=$(cd "$tree" && pwd)
netlist=${2:-$tree/desktop-top-open.json}
[[ -f $netlist ]] || { echo "no netlist at $netlist (run synth-desktop.sh first)" >&2; exit 2; }

if [[ -z ${SYNTH_DESKTOP_NO_ENV:-} ]]; then
    for env in "$HOME/oss-cad-suite/environment" /opt/oss-cad-suite/environment; do
        [[ -f $env ]] && { source "$env"; break; }
    done
fi

# The suite's environment script UNSETS PYTHONHOME ("unset PYTHONHOME if set"),
# and nextpnr's embedded interpreter then resolves its prefix to the /yosyshq
# the suite was built with and dies with "failed to get the Python codec of the
# filesystem encoding".  It has to be set again, after that script runs.
if [[ -z ${SYNTH_DESKTOP_NO_ENV:-} ]]; then
    : "${PYTHONHOME:=${HOME}/oss-cad-suite}"
    export PYTHONHOME
fi

# The fork's nextpnr, not the suite's -- see the header.
default_nextpnr=/home/vash/tools/nextpnr-mathieufro/nextpnr-himbaechel
nextpnr=${NEXTPNR_HIMBAECHEL:-$default_nextpnr}
case $nextpnr in
    */oss-cad-suite/*)
        cat >&2 <<'EOF'
NEXTPNR_HIMBAECHEL points at oss-cad-suite's nextpnr-himbaechel.  That build
carries the published GW5AST-138C database, which has no PLL site and no clock
pips for this device, so it cannot place this core.  Build the fork's instead
and point NEXTPNR_HIMBAECHEL at it -- Tang-Build's TOOLCHAIN.md, "Building nextpnr against
the database".
EOF
        exit 2
        ;;
esac
[[ -x $nextpnr ]] || {
    cat >&2 <<EOF
no nextpnr-himbaechel at $nextpnr

Set NEXTPNR_HIMBAECHEL to the fork's build, or build it where this script
expects it.  Tang-Build's TOOLCHAIN.md has the command and the revision to check it with.
EOF
    exit 2
}
echo "nextpnr: $nextpnr"

# The constraints, overridable so a sweep can try a variant without editing the
# tree.  `PNR_CST` lets a caller supply a copy with, say, the CLKDIV pinned to a
# different bel.
cst=${PNR_CST:-$tree/src/desktop/desktop.cst}
sdc=$tree/src/desktop/desktop.sdc
# Everything writable lives under here, so a caller can give each run its own
# directory and run several at once.  Without this, parallel seeds would
# overwrite each other's routed netlist and log.
work=${PNR_WORK:-$tree/.open-pnr}
mkdir -p "$work"
[[ -f $cst ]] || { echo "no constraints at $cst" >&2; exit 2; }

# ------------------------------------------------------------- constraints --
# `//` -> `#`; nextpnr's SDC parser rejects the C++-style comment.
sed 's|^\([[:space:]]*\)//|\1#|' "$sdc" > "$work/desktop.sdc"
# The synthesized aliases differ from the RTL names: hclk becomes
# desktop_sockets.pixel_clk, and the clock after the input buffer becomes
# keyboard_link.clk. Constrain their primitive pins before placement so neither
# domain silently inherits nextpnr's 12 MHz default.
python3 "$here/clock_constraints.py" "$netlist" "$work/desktop.sdc"
echo "timing constraints:"
sed 's/^/    /' "$work/desktop.sdc"

# ------------------------------------------------ unconstrained, unused IOs --
# Decide what has to be dropped, and refuse if anything has to be dropped that
# is actually in use.
python3 - "$netlist" "$cst" "$work" <<'PY'
import json, re, sys
netlist, cst_path, work = sys.argv[1], sys.argv[2], sys.argv[3]

doc = json.load(open(netlist))
top = doc["modules"]["desktop_top"]

# ports the constraint file gives a location to ("tmds_d_p[0]" -> "tmds_d_p")
constrained = set()
for line in open(cst_path):
    m = re.match(r'\s*IO_LOC\s+"([^"]+)"', line)
    if m:
        constrained.add(re.sub(r"\[.*\]$", "", m.group(1)))

unconstrained = [p for p in top["ports"] if p not in constrained]
if not unconstrained:
    print("every top-level port has a pin constraint")
    open(work + "/drop.ys", "w").write(
        "read_json %s\nwrite_json %s/pnr-input.json\n" % (netlist, work))
    sys.exit(0)

def consumers(bit):
    out = []
    for name, cell in top["cells"].items():
        for pin, bits in cell.get("connections", {}).items():
            if bit in bits:
                out.append((name, cell["type"], pin))
    return out

drop_ports, drop_cells, used = [], [], []
for port in unconstrained:
    bits = set(top["ports"][port]["bits"])
    bufs = [(n, c) for n, c in top["cells"].items()
            if c["type"] in ("IBUF", "IOBUF", "OBUF", "TBUF")
            and any(b in bits for b in
                    (c.get("connections", {}).get("I") or [])
                    + (c.get("connections", {}).get("O") or []))]
    if not bufs:
        drop_ports.append(port)
        continue
    in_use = False
    for bname, bcell in bufs:
        outs = set(bcell.get("connections", {}).get("O", []))
        for other, otype, opin in consumers(next(iter(outs), None)):
            if other != bname and not otype.startswith("$"):
                in_use = True
    if in_use:
        used.append(port)
    else:
        drop_ports.append(port)
        drop_cells.extend(n for n, _ in bufs)

if used:
    sys.stderr.write(
        "\nERROR: these top-level ports have no pin in %s and ARE used:\n" % cst_path)
    for p in used:
        sys.stderr.write("    %s\n" % p)
    sys.stderr.write("Add an IO_LOC for them (from the board's pinout) rather than\n"
                     "letting them be dropped.\n")
    sys.exit(1)

print("ports with no pin constraint: %s" % (", ".join(unconstrained) or "none"))
if drop_ports:
    print("unused, so dropped: %s%s"
          % (", ".join(drop_ports),
             "  (+ buffers: %s)" % ", ".join(drop_cells) if drop_cells else ""))

lines = ["read_json %s" % netlist, "select -module desktop_top"]
for n in drop_ports + drop_cells:
    lines.append("delete %s" % n)
lines += ["select -clear", "opt_clean -purge",
          "write_json %s/pnr-input.json" % work]
open(work + "/drop.ys", "w").write("\n".join(lines) + "\n")
PY

yosys -q -s "$work/drop.ys" > "$work/drop.log" 2>&1 || {
    echo "failed to prepare the netlist for place and route:" >&2
    tail -20 "$work/drop.log" >&2
    exit 1
}

pnr_json=$work/pnr-input.json

# ------------------------------------------------------------------ nextpnr --
echo
echo "placing and routing"

# Placement is the thing that decides whether the bit clock's route can be
# dedicated, and a design this size gives different outcomes on different runs.
# Both knobs are passthroughs so a sweep can be run without editing this script:
#
#   NEXTPNR_SEED=7        fixed seed
#   NEXTPNR_PLACER=heap   sa or heap
#   NEXTPNR_POST_ROUTE=/absolute/path/hook.py   optional routing inspection
#
# The synthesised netlist does not depend on either, so a sweep over seeds only
# has to re-place and re-route.
pnr_extra=()
[[ -n ${NEXTPNR_SEED:-} ]]   && pnr_extra+=(--seed "$NEXTPNR_SEED")
[[ -n ${NEXTPNR_PLACER:-} ]] && pnr_extra+=(--placer "$NEXTPNR_PLACER")
[[ -n ${NEXTPNR_POST_ROUTE:-} ]] && pnr_extra+=(--post-route "$NEXTPNR_POST_ROUTE")

set +e
"$nextpnr" \
    --json "$pnr_json" \
    --write "$work/pnr.json" \
    --device GW5AST-LV138PG484AC1/I0 \
    --vopt cst="$cst" \
    --sdc "$work/desktop.sdc" \
    --timing-allow-fail \
    --report "$work/report.json" \
    "${pnr_extra[@]}" 2>&1 | tee "$work/nextpnr.log"
rc=${PIPESTATUS[0]}
set -e

echo
if (( rc == 0 )); then
    echo "nextpnr finished; packed output in $work/pnr.json"
else
    echo "nextpnr stopped (exit $rc).  Log: $work/nextpnr.log"
    if grep -q "no BELs remaining to implement cell type 'PLL'" "$work/nextpnr.log"; then
        cat <<'EOF'

The design is fine; the binary is wrong.

  `no BELs remaining to implement cell type 'PLL'` means the architecture this
  nextpnr was built with carries no PLL site -- which is the PUBLISHED
  GW5AST-138C database, not the regenerated one.  Check that the binary in use
  is the fork's and not oss-cad-suite's, and that it was built with the fork's
  apycula on PYTHONPATH for the whole build.  The usual cause is a misspelled
  -DHIMBAECHEL_GOWIN_DEVICES: CMake ignores it with only a warning and builds
  the architecture from whatever apycula it found.

See Tang-Build's TOOLCHAIN.md, "Building nextpnr against the database".
EOF
    fi
fi
exit $rc
