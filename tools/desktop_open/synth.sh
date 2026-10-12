#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Synthesise TinyTang's desktop core (`nestang_top`, the GW5AST-138C design)
# with the open toolchain, up to a nextpnr-ready JSON netlist.
#
#   tools/desktop_open/synth.sh <reconstructed-design-tree> [out.json]
#
# The tree is one produced by TinyTang's own:
#
#   tools/build_desktop_core.sh --prepare-only
#
# The SystemVerilog is taken as the vendor wrote it -- this script does not
# rewrite the design.  It does require the four portability edits from
# third_party/patches/desktop/open/0001-open-toolchain-portability.patch to be applied; it will tell you
# if they are not.  It writes the cell-library shim and the netlist *into* the
# tree (`.open-shim/`, `nestang-top-open.json`), so pass a scratch copy.
#
# Tang-Build's OPEN-FLOW-DESKTOP.md explains what this does and why.
set -euo pipefail

# comm(1) needs byte order, and sort(1) defaults to the locale's collation.
export LC_ALL=C

here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

tree=${1:-}
out=${2:-nestang-top-open.json}
if [[ -z $tree || ! -d $tree ]]; then
    echo "usage: $0 <reconstructed-design-tree> [out.json]" >&2
    echo "  (get one with: tools/build_desktop_core.sh --prepare-only)" >&2
    exit 2
fi
tree=$(cd "$tree" && pwd)

# oss-cad-suite puts everything on PATH via its environment file, and it must
# be preferred: a distribution yosys is generally too old for `read_slang` and
# may not ship the gowin cell library at all.  Set SYNTH_DESKTOP_NO_ENV=1 to
# use whatever yosys is already on PATH.
if [[ -z ${SYNTH_DESKTOP_NO_ENV:-} ]]; then
    for env in "$HOME/oss-cad-suite/environment" /opt/oss-cad-suite/environment; do
        [[ -f $env ]] && { source "$env"; break; }
    done
fi
command -v yosys >/dev/null 2>&1 || { echo "yosys not found on PATH" >&2; exit 2; }

# ------------------------------------------------------------- cell library --
# read_slang needs the Gowin cell libraries visible but must not treat them as
# design sources, and slang rejects the `specify` blocks in cells_sim.v.
datdir=$(yosys-config --datdir 2>/dev/null || true)
[[ -n ${datdir:-} && -d $datdir/gowin ]] || \
    datdir=$(cd "$(dirname "$(command -v yosys)")/../share/yosys" 2>/dev/null && pwd)
[[ -n ${datdir:-} && -d $datdir/gowin ]] || datdir=${YOSYS_DATDIR:-}
if [[ -z ${datdir:-} || ! -d $datdir/gowin ]]; then
    echo "cannot find yosys's gowin cell library" >&2
    echo "  yosys is $(command -v yosys) -- $(yosys --version 2>/dev/null)" >&2
    exit 2
fi
echo "yosys: $(command -v yosys)  ($(yosys --version 2>/dev/null | head -1))"
echo "gowin cell library: $datdir/gowin"

# ---------------------------------------------------------------- preflight --
# The four portability edits, each identified by a line that only exists when
# the patch is applied.
missing=0
check () {
    local file=$1 pattern=$2 what=$3
    if ! grep -qF -- "$pattern" "$tree/$file"; then
        echo "  missing: $what  ($file)" >&2
        missing=1
    fi
}
forbid () {
    local file=$1 pattern=$2 what=$3
    if grep -qE -- "$pattern" "$tree/$file"; then
        echo "  still there: $what  ($file)" >&2
        missing=1
    fi
}
check src/iosys/iosys_bl616.v    "output reg  [7:0] kbd_data"        "iosys_bl616.v: kbd_data is an output"
check src/hdmi2/packet_picker.sv "frame_counter <= (frame_counter"   "packet_picker.sv: frame_counter non-blocking"
check src/nes2hdmi.sv            "audio_divider <= audio_divider + 1" "nes2hdmi.sv: audio_divider non-blocking"
# hdmi.sv already contains a non-blocking control_data assignment elsewhere, so
# identify this edit by the blocking one being gone from the reset branch.
forbid src/hdmi2/hdmi.sv '^[[:space:]]*control_data = 6'\''d0;' \
       "hdmi.sv: control_data blocking assignment"
if (( missing )); then
    echo >&2
    echo "Apply the design-side portability edits first:" >&2
    echo "    (cd $tree && patch -p1 < $here/../../third_party/patches/desktop/open/0001-open-toolchain-portability.patch)" >&2
    exit 1
fi

# ------------------------------------------------------------------- shims --
# src/nes2hdmi.sv does $readmemb("background.txt", mem), and slang looks for
# that next to the file that reads it, not along -I.  The design keeps the data
# at src/assets/background.txt, so make it reachable from src/.  (slang also
# accepts it at the tree root; either one is enough -- this is the file-relative
# location, which is the one that matches the source.)
if [[ -f $tree/src/assets/background.txt && ! -e $tree/src/background.txt ]]; then
    ln -s assets/background.txt "$tree/src/background.txt"
    echo "linked src/background.txt -> assets/background.txt for \$readmemb"
fi

shim="$tree/.open-shim"
mkdir -p "$shim"
awk '/^[[:space:]]*specify[[:space:]]*$/ { skip=1 }
     !skip { print }
     /^[[:space:]]*endspecify[[:space:]]*$/ { skip=0 }' \
    "$datdir/gowin/cells_sim.v" > "$shim/cells_sim_nospecify.v"
cp "$datdir/gowin/cells_xtra_gw5a.v" "$shim/cells_xtra_gw5a.v"
echo "cell-library shim: $shim  ($(grep -c '^module' "$shim"/cells_*.v | paste -sd' ')) modules"

# ------------------------------------- which library modules to blackbox --
# The `-v` library is visible-but-not-instantiated, so every primitive the
# design instantiates has to be named again as a --blackboxed-module; without
# that the instance is dropped silently, leaving a netlist with no clocks in it.
# The list is therefore derived here rather than maintained by hand, and the
# derivation is deliberately a superset: every cell-library module whose name
# begins a line somewhere in the design.  That over-collects names which are
# only local signals -- the 6502 core's `DL` is one -- and blackboxing a module
# the design never instantiates is a no-op, so over-collecting is free while
# under-collecting is the bug.
libmods=$(grep -hoE '^[[:space:]]*module [A-Za-z_][A-Za-z0-9_]*' "$shim"/cells_*.v \
          | awk '{print $2}' | sort -u)
dnames=$(grep -rhoE '^[[:space:]]*[A-Za-z_][A-Za-z0-9_]*' "$tree/src" \
              --include=*.v --include=*.sv 2>/dev/null \
         | sed 's/^[[:space:]]*//' | sort -u)
bb=$(comm -12 <(echo "$libmods") <(echo "$dnames")) || true
if [[ -z $bb ]]; then
    echo "no cell-library module begins a line in the design -- is the tree complete?" >&2
    exit 1
fi
bbflags=$(echo "$bb" | sed 's/^/--blackboxed-module /' | paste -sd' ')
echo "blackboxing $(echo "$bb" | wc -l) of the library's $(echo "$libmods" | wc -l) modules"
echo "  $(echo "$bb" | paste -sd' ')"

# ------------------------------------------------------------------- yosys --
cd "$tree"
log="$shim/synth.log"
placeholder='--blackboxed-modules-INJECTED-BY-SYNTH-DESKTOP-SH'
sed -e "s|^write_json .*|write_json $out|" \
    -e "s|$placeholder|$bbflags|" \
    "$here/synth.ys" > "$shim/synth-desktop.ys"
grep -q -- "$placeholder" "$shim/synth-desktop.ys" && {
    echo "placeholder $placeholder not substituted" >&2; exit 1; }
echo "synthesising $tree/src/desktop/build.tcl's file list into $out"
yosys -s "$shim/synth-desktop.ys" 2>&1 | tee "$log"

# ----------------------------------------------------- did they survive? ----
# A blackbox instance drives the nets its output ports touch.  If the instance
# is dropped -- which is precisely what `-v` does when there is no matching
# --blackboxed-module -- the wires remain and nothing drives them, and yosys's
# check pass says so.  So the failure is measured directly: a driver-less wire
# whose name ends in an output pin of one of the blackboxed primitives.  The
# design's own dangling wires (`joypad_out`, `rv_dout`, `sys_inst.mgmt_readdata`
# -- an unconnected iosys input) are not primitive pins and are not errors.
if ! python3 - "$shim/cells_sim_nospecify.v:$shim/cells_xtra_gw5a.v" "$log" "$bb" <<'PY'
import re, sys
libs, log_path = sys.argv[1].split(":"), sys.argv[2]
names = set(sys.argv[3].split())

def output_pins(path):
    txt = open(path, errors="replace").read()
    pins = set()
    for m in re.finditer(r"(?m)^\s*module\s+(\w+)", txt):
        if m.group(1) not in names:
            continue
        e = re.search(r"(?m)^\s*endmodule", txt[m.end():])
        body = txt[m.start():m.end() + (e.start() if e else len(txt))]
        for d in re.finditer(r"\boutput\b([^;,)]*)", body):
            decl = re.sub(r"\[[^\]]*\]", " ", d.group(1))
            decl = re.sub(r"\b(wire|reg|logic|signed|supply0|supply1)\b", " ", decl)
            for tok in re.split(r"[\s,]+", decl):
                if re.fullmatch(r"[A-Za-z_]\w*", tok or ""):
                    pins.add(tok)
    return pins

pins = set()
for lib in libs:
    pins |= output_pins(lib)

log = open(log_path, errors="replace").read()
pat = re.compile(r"Warning: Wire (\S+?)(?:\s*\[[^\]]*\])?\s+is used but has no driver")
bad = []
for wire in sorted(set(pat.findall(log))):
    lean = wire
    for pre in ("nestang_top.", "nestang_top\\"):
        if lean.startswith(pre):
            lean = lean[len(pre):]
            break
    lean = lean.lstrip("\\")           # `\pll_nes.PLL_inst.CLKOUT0`
    if "." in lean and lean.split(".")[-1] in pins:
        bad.append(lean)

if bad:
    sys.stderr.write("\nERROR: %d output(s) of instantiated cell-library "
                     "primitives are driver-less --\nthe instance was "
                     "dropped:\n" % len(bad))
    for w in bad:
        sys.stderr.write("    %s\n" % w)
    sys.stderr.write("The -v library is visible-but-not-instantiated; every "
                     "primitive the\ndesign instantiates needs a "
                     "--blackboxed-module entry.\n")
    sys.exit(1)
print("no primitive instance was dropped (%d dangling wires checked against "
      "%d output pins)" % (len(pat.findall(log)), len(pins)))
PY
then
    echo >&2
    echo "removing the incomplete netlist" >&2
    rm -f "$out"          # relative to $tree, or absolute if given as such
    exit 1
fi

# ------------------------------------- string parameters back to strings --
# `read_slang` keeps a Verilog *string* parameter as a bit vector, where
# `read_verilog` keeps it a string, so a design through this front end reaches
# `gowin_pack` carrying the ASCII of "50" and "TRUE" instead of those strings.
# MEASURED: clock-smoke, synthesised by plain `synth_gowin`, has
# `FCLKIN = 50` and `CLKOUT0_EN = TRUE`; this flow's netlist has
# `0011010100110000` and `01010100010100100101010101000101` for the same
# `defparam`s.  The packer wants the strings and rejects the bit patterns --
# `float()` on the former gives 1.1e13, and the latter raises a KeyError in
# `pll_attrvals` -- which is what stopped this core packing at all.
#
# The cell library states which of the PLL's parameters are strings and which
# are numbers (`cells_xtra_gw5a.v`: 43 of the former, `FCLKIN`, `CLKFB_SEL`,
# `CLKOUT0..6_EN`, the `DYN_*` and `DE*_EN` flags; 51 of the latter, the
# dividers and delay steps).  Converting is therefore restricted to PLL-family
# cells, where it is safe by construction: a PLL's numeric parameters are
# 32-bit, so a realistic divider leaves its high bytes 0x00 and cannot decode
# as text.  A LUT is not safe -- `INIT` is 16 bits and some decode to printable
# ASCII by chance, which is how a whole-netlist rule would silently corrupt
# look-up tables -- so PLL-family cells are the scope, not the whole design.
python3 - "$out" <<'PY'
import json, sys

PLL_TYPES = {"PLL", "PLLA", "rPLL", "PLLVR"}
path = sys.argv[1]
d = json.load(open(path))
converted = []
for m in d.get("modules", {}).values():
    for cn, c in m.get("cells", {}).items():
        if c.get("type") not in PLL_TYPES:
            continue
        parms = c.get("parameters")
        if not parms:
            continue
        for k, v in list(parms.items()):
            if not isinstance(v, str) or len(v) < 8 or len(v) % 8 or set(v) - set("01"):
                continue
            text = "".join(chr(int(v[i:i + 8], 2)) for i in range(0, len(v), 8))
            if all(0x20 <= ord(ch) <= 0x7e for ch in text):
                parms[k] = text
                converted.append("%s.%s = %r" % (cn, k, text))
if converted:
    json.dump(d, open(path, "w"))
    print("PLL string parameters restored (%d): %s"
          % (len(converted), ", ".join(converted)))
else:
    print("PLL string parameters restored: none")
PY

# ...and a readable record of the primitives this design is known to need.
python3 - "$out" nestang_top <<'PY'
import json, sys
from collections import Counter
top = json.load(open(sys.argv[1]))["modules"][sys.argv[2]]
counts = Counter(c["type"] for c in top["cells"].values())
print("%s: %d cells" % (sys.argv[2], len(top["cells"])))
for name in ("PLL", "rPLL", "PLLA", "CLKDIV", "OSER10", "ELVDS_OBUF",
             "DPB", "ALU"):
    if counts.get(name):
        print("  %-12s %d" % (name, counts[name]))
PY
