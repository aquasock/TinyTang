// TinyTang desktop HDMI output: 1280x720p60 with the desktop text layer
// composited full-screen, silent audio.
//
// Derived from nes2hdmi.sv in nestang by nand2mario (GPL-3.0); the NES frame
// buffer, scaler and palette have been removed.
// SPDX-License-Identifier: GPL-3.0-only

`timescale 1ns / 1ps

module desktop_hdmi (
	input clk,      // main logic clock
	input resetn,

    // overlay enable: the desktop layer shows only while this is set
    input overlay,

    // The desktop layer's write port (main logic clock) and enable.
    input        wide_we,
    input [6:0]  wide_x,
    input [5:0]  wide_y,
    input [6:0]  wide_ch,
    input [14:0] wide_fg,
    input [14:0] wide_bg,
    input        wide_on,

    output [10:0] desktop_cx,
    output [9:0] desktop_cy,
    output [23:0] desktop_rgb,

	// video clocks
	input clk_pixel,
	input clk_5x_pixel,

	// output signals
	output       tmds_clk_n,
	output       tmds_clk_p,
	output [2:0] tmds_d_n,
	output [2:0] tmds_d_p
);

localparam VIDEOID = 4;
localparam VIDEO_REFRESH = 60.0;

localparam CLKFRQ = 74250;

localparam AUDIO_BIT_WIDTH = 16;

// video stuff
reg [23:0] rgb;             // actual RGB output
wire [9:0] cy, frameHeight;
wire [10:0] cx, frameWidth;
assign desktop_cx = cx;
assign desktop_cy = cy;
assign desktop_rgb = rgb;

// audio: the link carries none, so the HDMI audio stream is silence
localparam AUDIO_RATE=48000;
localparam AUDIO_CLK_DELAY = CLKFRQ * 1000 / AUDIO_RATE / 2;
logic [$clog2(AUDIO_CLK_DELAY)-1:0] audio_divider;
logic clk_audio;

always_ff@(posedge clk_pixel) 
begin
    if (audio_divider != AUDIO_CLK_DELAY - 1)
        audio_divider <= audio_divider + 1;   // open-toolchain portability: was blocking
    else begin 
        clk_audio <= ~clk_audio; 
        audio_divider <= 0; 
    end
end

logic [15:0] audio_sample_word [1:0];
assign audio_sample_word[0] = 16'd0;
assign audio_sample_word[1] = 16'd0;

// The desktop layer, presented at 2x across the whole raster.
//
// When it is up it replaces the output entirely, which is what "full screen"
// means for a desktop; otherwise the output is black.
wire [14:0] wide_color;

textdisp_wide u_wide (
    .clk(clk), .hclk(clk_pixel),
    .we(wide_we), .wx(wide_x), .wy(wide_y),
    .wch(wide_ch), .wfg(wide_fg), .wbg(wide_bg),
    .cx(cx), .cy(cy),
    .frame_width(frameWidth), .frame_height(frameHeight),
    .color(wide_color)
);

// The enable is a register in the main logic clock; bring it across before it
// decides anything in this one.
reg wide_on_r, wide_on_rr;
always @(posedge clk_pixel) begin
    wide_on_r  <= wide_on;
    wide_on_rr <= wide_on_r;
end

// calc rgb value to hdmi
//
// The layer is shown when the overlay is asserted as well as enabled, so the
// BL616's overlay command (0x08) can still hide it.
always @(posedge clk_pixel) begin
    if (wide_on_rr && overlay)
        rgb <= {wide_color[4:0],3'b0,wide_color[9:5],3'b0,wide_color[14:10],3'b0};                 // BGR5 to RGB8
    else rgb <= 24'h000000;
end

// HDMI output.
logic[2:0] tmds;

hdmi #( .VIDEO_ID_CODE(VIDEOID), 
        .DVI_OUTPUT(0), 
        .VIDEO_REFRESH_RATE(VIDEO_REFRESH),
        .IT_CONTENT(1),
        .AUDIO_RATE(AUDIO_RATE), 
        .AUDIO_BIT_WIDTH(AUDIO_BIT_WIDTH),
        .START_X(0),
        .START_Y(0) )

hdmi( .clk_pixel_x5(clk_5x_pixel), 
        .clk_pixel(clk_pixel), 
        .clk_audio(clk_audio),
        .rgb(rgb), 
        .reset( 0 ),
        .audio_sample_word(audio_sample_word),
        .tmds(tmds), 
        .tmds_clock(tmdsClk), 
        .cx(cx), 
        .cy(cy),
        .frame_width( frameWidth ),
        .frame_height( frameHeight ) );

// Gowin LVDS output buffer
ELVDS_OBUF tmds_bufds [3:0] (
    .I({clk_pixel, tmds}),
    .O({tmds_clk_p, tmds_d_p}),
    .OB({tmds_clk_n, tmds_d_n})
);


endmodule
