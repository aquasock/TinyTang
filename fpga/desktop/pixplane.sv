// SPDX-License-Identifier: GPL-3.0-only
//
// TinyTang: the desktop core's pixel plane (desktop ABI 1.8).
//
// A 640x360 RGB565 framebuffer, presented at 2x to fill the 1280x720 raster
// exactly as the text layer is, so one plane pixel covers 2x2 output pixels:
//
//     output pixel (cx, cy) shows plane pixel (cx >> 1, cy >> 1)
//
// It is what an app that owns the whole screen draws into -- the BL616's
// picture reader first -- the way a panel's framebuffer is on TinyDesk's LCD
// ports.  The cells of the text layer are not involved: while the plane is on,
// desktop_hdmi shows it instead of them.
//
// Storage.  640 x 360 x 16 bits is 3,686,400 bits, about 205 of the device's
// 340 block RAMs; the rest of the desktop core used 14 before this.  It is a
// plain array, written on the link's clock and read on the pixel clock, so
// synthesis maps it to dual-clock block RAM and simulation sees the same thing.
// Its contents start at zero, which is black.
//
// Writes.  The link presents a linear address (y * 640 + x) and one pixel per
// strobe; addresses past the last pixel are dropped rather than allowed to
// wrap into the top of the picture.
//
// Latency.  Four registers from address to colour, the same depth as
// textdisp_wide's, so the store is addressed four pixels ahead of the raster
// with the same look-ahead and the two layers are interchangeable in
// desktop_hdmi's one-register output stage without a shift.  Addressing with
// the raster's own cx would put the end of the previous line at the start of
// every line -- the fault textdisp_wide's header records from hardware.  The
// extra stage after the store is the read register the block RAMs' output
// multiplexer needs at this depth: 3.6 Mbit is many blocks, and their outputs
// are selected by the address's top bits.
//
// Colour.  RGB565 widens to RGB888 by repeating each field's top bits, so full
// scale is 0xFF rather than 0xF8.

module pixplane #(
    parameter integer W = 640,
    parameter integer H = 360
) (
    input  wire        clk,        // write port clock: the link's clock
    input  wire        hclk,       // scanout clock: the pixel clock

    input  wire        we,
    input  wire [17:0] waddr,      // y * W + x
    input  wire [15:0] wdata,      // RGB565

    input  wire [10:0] cx,         // the raster hdmi.sv generates, blanking included
    input  wire [9:0]  cy,
    input  wire [10:0] frame_width,
    input  wire [9:0]  frame_height,
    output reg  [23:0] rgb
);

    localparam integer PIXELS = W * H;

    reg [15:0] mem [0:PIXELS-1];

    always @(posedge clk) begin
        if (we && waddr < 18'(PIXELS)) mem[waddr] <= wdata;
    end

    /* The coordinate whose pixel `rgb` must hold LAT clocks from now, carried
     * into the next line and frame as hdmi.sv's counters carry. */
    localparam integer LAT = 4;
    wire        wrap = cx >= frame_width - 11'(LAT);
    wire [10:0] ax   = wrap ? cx + 11'(LAT) - frame_width : cx + 11'(LAT);
    wire [9:0]  ay   = !wrap                   ? cy
                     : (cy == frame_height - 1) ? 10'd0
                     :                            cy + 10'd1;

    /* Off-screen reads address pixel 0; they are only ever presented during
     * blanking, where nothing is sent. y * 640 is y * 512 + y * 128. */
    wire        in_screen = (ax < 11'(W * 2)) && (ay < 10'(H * 2));
    wire [8:0]  py = ay[9:1];
    wire [9:0]  px = ax[10:1];
    wire [17:0] raddr = in_screen ? ({py, 9'd0} + {2'd0, py, 7'd0} + {8'd0, px}) : 18'd0;

    reg [17:0] raddr_r;     // stage 0: the address
    reg [15:0] pix_r;       // stage 1: the store's read
    reg [15:0] pix_rr;      // stage 2: the read register after the block outputs
    // stage 3: rgb

    always @(posedge hclk) begin
        raddr_r <= raddr;
        pix_r   <= mem[raddr_r];
        pix_rr  <= pix_r;
        rgb     <= {pix_rr[15:11], pix_rr[15:13],
                    pix_rr[10:5],  pix_rr[10:9],
                    pix_rr[4:0],   pix_rr[4:2]};
    end

endmodule
