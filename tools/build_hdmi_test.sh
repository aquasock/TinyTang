#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Build the minimal HDMI test core (fpga/hdmi_test/) with the open flow or,
# as a reference only, with Gowin EDA.
#
#   tools/build_hdmi_test.sh <open|gowin> <720p|480p> [out-dir]
#
# The two PLL wrappers are the desktop core's own, copied from the tree that
# tools/build_desktop_core.sh --prepare-only reconstructs (prepared here when
# build/desktop/source-path.txt is missing).  480p uses a pll_hdmi variant
# with MDIV_SEL 42 and ODIV0_SEL 9: 27 MHz x 42 = 1134 MHz VCO (inside the
# datasheet band of 650-1300 MHz that gowin_pack enforces), /9 = 126 MHz
# bit clock, /5 = 25.2 MHz pixel clock.
#
# Open flow: yosys synth_gowin, the nextpnr-himbaechel fork, the apicula fork's
# gowin_pack, then tools/desktop_open/fs_to_bin.py.  The same environment
# overrides as tools/desktop_open/build.sh apply (NEXTPNR_BUILD, APICULA_FORK,
# PY, PYDEPS, NEXTPNR_SEED), and HDMI_TEST_PLL27_SITE / HDMI_TEST_PLLHDMI_SITE
# choose the open build's PLL sites (default PLL_L[1] / PLL_L[3], the desktop
# core's; the Gowin build of this core chose PLL_B[2] / PLL_R[2]).  The output directory receives hdmi_test.bin and
# a copy stamped with its sha256 prefix, plus the logs.
set -euo pipefail
export LC_ALL=C
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
flow="${1:-}"
mode="${2:-}"
[[ "$flow" == open || "$flow" == gowin ]] && [[ "$mode" == 720p || "$mode" == 480p ]] || {
    echo "usage: $0 <open|gowin> <720p|480p> [out-dir]" >&2; exit 2; }
out="$(realpath -m "${3:-$root/build/hdmi_test/$flow-$mode}")"
build="${DESKTOP_BUILD_DIR:-$root/build/desktop}"

if [[ ! -f "$build/source-path.txt" || ! -d "$(cat "$build/source-path.txt")" ]]; then
    DESKTOP_BUILD_DIR="$build" "$root/tools/build_desktop_core.sh" --prepare-only
fi
tree="$(cat "$build/source-path.txt")"

rm -rf "$out"
mkdir -p "$out/src"
src="$out/src"
cp "$root/fpga/hdmi_test/hdmi_test.v" "$root/fpga/hdmi_test/hdmi_test.cst" \
   "$root/fpga/hdmi_test/build.tcl" "$src/"
cp "$tree/src/pll/gowin_pll_27.v" "$tree/src/pll/gowin_pll_hdmi.v" "$src/"

if [[ $mode == 480p ]]; then
    sed -i 's/^parameter MODE_480P = 0;/parameter MODE_480P = 1;/' "$src/hdmi_test.v"
    sed -i -e 's/^defparam PLL_inst.MDIV_SEL = 55;/defparam PLL_inst.MDIV_SEL = 42;/' \
           -e 's/^defparam PLL_inst.ODIV0_SEL = 4;/defparam PLL_inst.ODIV0_SEL = 9;/' \
           "$src/gowin_pll_hdmi.v"
    grep -q '^parameter MODE_480P = 1;' "$src/hdmi_test.v" &&
    grep -q '^defparam PLL_inst.MDIV_SEL = 42;' "$src/gowin_pll_hdmi.v" &&
    grep -q '^defparam PLL_inst.ODIV0_SEL = 9;' "$src/gowin_pll_hdmi.v" ||
        { echo "480p substitution did not apply" >&2; exit 1; }
    period5=7.937     # 126 MHz
    pixel=25.2
else
    period5=2.6936    # 371.25 MHz
    pixel=74.25
fi
period1=$(awk "BEGIN{printf \"%.3f\", $period5 * 5}")
printf 'create_clock -name sys_clk -period 20 [get_nets {sys_clk}]\ncreate_clock -name hclk5 -period %s [get_nets {hclk5}]\ncreate_clock -name hclk -period %s [get_nets {hclk}]\n' \
    "$period5" "$period1" > "$src/hdmi_test.sdc"

if [[ $flow == gowin ]]; then
    gowin_sh="${GOWIN_SH:-/home/vash/tools/gowin-1.9.11.03/IDE/bin/gw_sh}"
    [[ -x "$gowin_sh" ]] || { echo "Gowin gw_sh missing; set GOWIN_SH" >&2; exit 1; }
    export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}"
    if [[ -f /usr/lib/x86_64-linux-gnu/libfreetype.so.6 ]]; then
        export LD_PRELOAD="/usr/lib/x86_64-linux-gnu/libfreetype.so.6${LD_PRELOAD:+:$LD_PRELOAD}"
    fi
    ( cd "$src" && "$gowin_sh" build.tcl ) > "$out/build.log" 2>&1 ||
        { tail -30 "$out/build.log" >&2; exit 1; }
    cp "$src/impl/pnr/hdmi_test_tr_content.html" "$src/impl/pnr/hdmi_test.rpt.txt" "$out/"
    python3 "$root/tools/check_gowin_timing.py" "$out/hdmi_test_tr_content.html"
    install -m 644 "$src/impl/pnr/hdmi_test.bin" "$out/hdmi_test.bin"
else
    A=${APICULA_FORK:-/run/media/vash/GIT/apicula-mathieufro}
    NPNR=${NEXTPNR_BUILD:-/home/vash/tools/nextpnr-mathieufro}
    PY=${PY:-/home/vash/oss-cad-suite/py3bin/python3}
    PYDEPS=${PYDEPS:-/home/vash/tools/pydeps}
    suite="$HOME/oss-cad-suite"
    # shellcheck disable=SC1091
    source "$suite/environment"
    # The suite's environment unsets PYTHONHOME; nextpnr's embedded Python and
    # gowin_pack both need it (tools/desktop_open/pnr.sh).
    export PYTHONHOME="$suite"
    export PYTHONPATH="$A:$PYDEPS:${PYTHONPATH:-}"
    pll27_site="${HDMI_TEST_PLL27_SITE:-PLL_L[1]}"
    pllhdmi_site="${HDMI_TEST_PLLHDMI_SITE:-PLL_L[3]}"
    sed -e "s/@PLL27_SITE@/$pll27_site/" -e "s/@PLLHDMI_SITE@/$pllhdmi_site/" \
        "$root/fpga/hdmi_test/hdmi_test_open.cst.in" >> "$src/hdmi_test.cst"
    echo "PLL sites: pll_27 $pll27_site, pll_hdmi $pllhdmi_site"
    # Synthesis renames the pixel net (it surfaces as an encoder's clk), so the
    # open flow constrains the divider's output pin, as clock_constraints.py
    # does for the desktop core, instead of the net by name.
    grep -v -e '{hclk}' -e '{sys_clk}' "$src/hdmi_test.sdc" > "$src/hdmi_test_open.sdc"
    printf 'create_clock -name board_clock -period 20 [get_pins {pll_27.PLL_inst/CLKIN}]\ncreate_clock -name video_pixel -period %s [get_pins {div5/CLKOUT}]\n' \
        "$period1" >> "$src/hdmi_test_open.sdc"
    ( cd "$src" && yosys -q -l "$out/synth.log" -p \
        "read_verilog hdmi_test.v gowin_pll_27.v gowin_pll_hdmi.v; synth_gowin -top hdmi_test -family gw5a -json $out/synth.json" )
    "$NPNR/nextpnr-himbaechel" --json "$out/synth.json" --write "$out/pnr.json" \
        --device GW5AST-LV138PG484AC1/I0 --vopt cst="$src/hdmi_test.cst" \
        --sdc "$src/hdmi_test_open.sdc" --seed "${NEXTPNR_SEED:-1}" \
        > "$out/nextpnr.log" 2>&1 || { tail -30 "$out/nextpnr.log" >&2; exit 1; }
    "$PY" "$root/tools/desktop_open/pnr_timing.py" "$out/nextpnr.log" || {
        echo "REFUSING TO PACK: setup or hold violations" >&2; exit 1; }
    # The pixel clock must have been timed at its real rate, not the default.
    python3 - "$out/nextpnr.log" "$pixel" <<'PY' || { echo "REFUSING TO PACK: pixel clock not constrained" >&2; exit 1; }
import re, sys
want = float(sys.argv[2])
got = [float(m) for m in re.findall(r"Max frequency for clock .*?(?:PASS|FAIL) at ([0-9.]+) MHz", open(sys.argv[1]).read())]
sys.exit(0 if any(abs(g - want) < 0.01 for g in got) else 1)
PY
    "$PY" "$root/tools/desktop_open/clock_route_inventory.py" "$out/nextpnr.log" \
        > "$out/clock-topology.txt" 2>&1 || true
    sed 's/^/   /' "$out/clock-topology.txt"
    "$PY" -m apycula.gowin_pack -d GW5AST-138C -o "$out/hdmi_test.fs" "$out/pnr.json" \
        > "$out/pack.log" 2>&1 || { tail -30 "$out/pack.log" >&2; exit 1; }
    "$PY" "$root/tools/desktop_open/fs_to_bin.py" "$out/hdmi_test.fs" -o "$out/hdmi_test.bin" > /dev/null
fi

stamp="$(sha256sum "$out/hdmi_test.bin" | cut -c1-12)"
cp -f "$out/hdmi_test.bin" "$out/hdmi_test.$stamp.bin"
echo "$flow $mode (pixel $pixel MHz): $out/hdmi_test.$stamp.bin"
sha256sum "$out/hdmi_test.bin"
