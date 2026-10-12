// SPDX-License-Identifier: GPL-3.0-only
//
// TinyTang: the desktop text layer.
//
// 80 columns by 45 rows of 8x8 cells, each cell carrying its own foreground and
// background colour.  It is what TinyDesk draws into.
//
// Geometry.  The layer is 640x360 in its own pixels and is presented at 2x to
// fill a 1280x720 raster exactly, so one cell covers 16x16 output pixels:
//
//     cell   = (cx[10:4], cy[9:4])        0..79 by 0..44
//     glyph  = row cy[3:1], column cx[3:1]
//
// i.e. output pixel (cx, cy) shows source pixel (cx>>1, cy>>1), which is the
// cell that contains it at the glyph bit that includes it.  Both is the same
// statement; the bit ranges are written out because that is what the hardware
// does and the mapping is the whole contract.
//
// Clocks.  The cell store has a write port on `clk` (the main logic clock,
// where the transport writes cells) and a read port on `hclk` (the pixel
// clock, where the raster is generated).  It is a plain
// array rather than a generated IP: the array is what both simulation and
// synthesis can see, so the design can be checked before it reaches hardware.
//
// Latency.  The colour path is four pixels deep, so the store is addressed
// four pixels ahead of the raster -- (cx, cy) advanced by LAT, wrapping into
// the next line and the next frame exactly as hdmi.sv's counters do -- and
// `color` is the pixel for the coordinate being presented now, aligned with
// desktop_hdmi's one-register `rgb` path.  Addressing with the raster's own cx
// instead is not a harmless shift: the first version of this layer did, three
// deep, and pixels 0-2 of every line were computed at the end of the previous
// line, in blanking, where the address falls back
// to cell 0, so cell (0,0)'s glyph is painted down the whole left edge in that
// cell's colours.  That is what hardware showed.  The address is registered
// before the store rather than decoded combinationally into it: the look-ahead
// adds a wrap test and a carry in front of the row multiply, and unregistered
// that path closed at +0.6 ns of a 13.5 ns pixel clock.
//
// Colour.  fg and bg are 15-bit BGR5, the same encoding as the existing
// `overlay_color`, so the compositor widens them the same way.

`include "font.vh"

module textdisp_wide #(
    parameter integer COLS = 80,
    parameter integer ROWS = 45
) (
    input  wire        clk,        // write port clock: the main logic clock
    input  wire        hclk,       // scanout clock: the pixel clock

    // Write port.  wx/wy outside the grid are dropped rather than allowed to
    // wrap into another row.
    input  wire        we,
    input  wire [6:0]  wx,
    input  wire [5:0]  wy,
    input  wire [6:0]  wch,
    input  wire [14:0] wfg,
    input  wire [14:0] wbg,

    // Scanout, in output pixels: the raster hdmi.sv generates, blanking
    // included, and its frame size, which the look-ahead wraps at.
    input  wire [10:0] cx,         // 0..frame_width-1, visible 0..1279
    input  wire [9:0]  cy,         // 0..frame_height-1, visible 0..719
    input  wire [10:0] frame_width,
    input  wire [9:0]  frame_height,
    output reg  [14:0] color
);

    localparam integer CELLS = COLS * ROWS;

    // {bg[14:0], fg[14:0], ch[6:0]}
    reg [36:0] mem [0:CELLS-1];

    wire write_ok = we && (wx < COLS) && (wy < ROWS);
    wire [11:0] waddr = wy * COLS + wx;

    /* The coordinate whose pixel `color` must hold LAT clocks from now:
     * the raster advanced by the pipeline's depth, carried into the next line
     * (and the next frame) the way hdmi.sv's counters carry. */
    localparam integer LAT = 4;
    wire        wrap = cx >= frame_width - 11'(LAT);
    wire [10:0] ax   = wrap ? cx + 11'(LAT) - frame_width : cx + 11'(LAT);
    wire [9:0]  ay   = !wrap                   ? cy
                     : (cy == frame_height - 1) ? 10'd0
                     :                            cy + 10'd1;

    /* The raster is not the visible area: its counters run to frame_width-1 and
     * frame_height-1, which on this core are 1650 and 750, so most of every
     * frame is off-screen.  Unclamped that reaches cell 3783 of a 3600-cell
     * store -- a read past the end of the array for the whole of blanking --
     * so off-screen reads address the first cell.  What they produce is only
     * ever presented during blanking, where hdmi.sv sends no video data, and
     * that holds only because of the look-ahead above. */
    wire in_screen = (ax < 11'd1280) && (ay < 10'd720);
    wire [11:0] raddr = in_screen ? (ay[9:4] * COLS + ax[10:4]) : 12'd0;

    always @(posedge clk) begin
        if (write_ok) mem[waddr] <= {wbg, wfg, wch};
    end

    // Stage 0: the address, and the coordinate it was decoded from.
    reg [11:0] raddr_r;
    reg [10:0] cx0;
    reg [9:0]  cy0;
    // Stage 1: the cell, and the coordinate that addresses it.
    reg [36:0] cell_r;
    reg [10:0] cx1;
    reg [9:0]  cy1;
    // Stage 2: the glyph row for that cell, and its colours.
    reg [7:0]  font_r;
    reg [14:0] fg_r;
    reg [14:0] bg_r;
    reg [10:0] cx2;
    // Stage 3: the pixel.

    always @(posedge hclk) begin
        raddr_r <= raddr;
        cx0     <= ax;
        cy0     <= ay;

        cell_r <= mem[raddr_r];
        cx1    <= cx0;
        cy1    <= cy0;

        font_r <= FONT[cell_r[6:0]][cy1[3:1]];
        fg_r   <= cell_r[21:7];
        bg_r   <= cell_r[36:22];
        cx2    <= cx1;

        color  <= font_r[cx2[3:1]] ? fg_r : bg_r;
    end

endmodule
