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

    // The pixel plane's write port (main logic clock) and enable (fb_sdram.sv),
    // its read alignment, and its status and checksum back in the main clock.
    input        plane_we,
    input [10:0] plane_wx,
    input [9:0]  plane_wy,
    input [15:0] plane_wdata,
    input        plane_on,
    input  [3:0] plane_rd_cfg,
    // The plane's buffers (desktop ABI 1.11): where writes go, and which is shown.
    input  [3:0] plane_wbuf,
    input  [3:0] plane_show,
    output reg [31:0] plane_status,
    output reg [31:0] plane_sum1,
    output reg [31:0] plane_sum2,

    // The SDRAM the plane lives in.  Its clock is driven by the caller, as the
    // inverse of clk_pixel.
    output [12:0] sd_a,
    output [1:0]  sd_ba,
    output        sd_cs_n,
    output        sd_ras_n,
    output        sd_cas_n,
    output        sd_we_n,
    output [1:0]  sd_dqm,
    inout  [15:0] sd_dq,

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

// The pixel plane: 1280x720 RGB565 in the SDRAM, shown 1:1, with the same
// look-ahead depth as the text layer, so either can be selected below without
// a shift.  Its controller runs on the pixel clock and is reset with the core.
wire [23:0] plane_rgb;
reg  [1:0]  plane_rst;          // resetn, brought into the pixel clock
reg  [3:0]  rd_cfg_r, rd_cfg_rr;
// The buffer to show crosses as a four-bit word: it is taken only when two
// samples in a row agree, so a change caught mid-flight is never used.
reg  [3:0]  show_r, show_rr, show_rrr, show_s;
always @(posedge clk_pixel) begin
    plane_rst <= {plane_rst[0], resetn};
    rd_cfg_r  <= plane_rd_cfg;
    rd_cfg_rr <= rd_cfg_r;
    show_r    <= plane_show;
    show_rr   <= show_r;
    show_rrr  <= show_rr;
    if (!plane_rst[1])           show_s <= 4'd0;
    else if (show_rr == show_rrr) show_s <= show_rr;
end
wire        fb_ready, fb_underrun, fb_dropped, fb_sum_toggle;
wire [10:0] fb_fetch_max;
wire [31:0] fb_sum1, fb_sum2;
wire [3:0]  fb_shown;

fb_sdram u_plane (
    .clk_w(clk), .we(plane_we), .wx(plane_wx), .wy(plane_wy), .wdata(plane_wdata),
    .wbuf(plane_wbuf), .show_buf(show_s), .shown(fb_shown),
    .clk(clk_pixel), .resetn(plane_rst[1]),
    .cx(cx), .cy(cy),
    .frame_width(frameWidth), .frame_height(frameHeight),
    .rgb(plane_rgb),
    .sd_a(sd_a), .sd_ba(sd_ba), .sd_cs_n(sd_cs_n), .sd_ras_n(sd_ras_n),
    .sd_cas_n(sd_cas_n), .sd_we_n(sd_we_n), .sd_dqm(sd_dqm), .sd_dq(sd_dq),
    .ready(fb_ready), .underrun(fb_underrun), .dropped(fb_dropped),
    .fetch_max(fb_fetch_max),
    .rd_cfg(rd_cfg_rr), .sum1(fb_sum1), .sum2(fb_sum2), .sum_toggle(fb_sum_toggle)
);

// Back into the main clock.  The checksum pair is latched once a frame and
// then held for a whole frame, so it is taken when its toggle has crossed;
// the status bits are slow and are simply resynchronised.
reg [2:0]  sum_sync;
reg [7:0]  sums_seen;
reg [17:0] status_r;
always @(posedge clk) begin
    sum_sync <= {sum_sync[1:0], fb_sum_toggle};
    if (sum_sync[2] != sum_sync[1]) begin
        plane_sum1 <= fb_sum1;
        plane_sum2 <= fb_sum2;
        sums_seen  <= sums_seen + 8'd1;
    end
    status_r     <= {fb_shown, fb_ready, fb_underrun, fb_dropped, fb_fetch_max};
    plane_status <= {status_r[13:11], 1'b0, rd_cfg_rr, sums_seen, status_r[17:14], 1'b0,
                     status_r[10:0]};
end

reg plane_on_r, plane_on_rr;
always @(posedge clk_pixel) begin
    plane_on_r  <= plane_on;
    plane_on_rr <= plane_on_r;
end

// calc rgb value to hdmi
//
// The layer is shown when the overlay is asserted as well as enabled, so the
// BL616's overlay command (0x08) can still hide it.
//
// The plane, when an app has claimed the screen with it, replaces both: an app
// that owns the screen owns all of it, as on TinyDesk's panel ports.
always @(posedge clk_pixel) begin
    if (plane_on_rr)
        rgb <= plane_rgb;
    else if (wide_on_rr && overlay)
        rgb <= {wide_color[4:0],3'b0,wide_color[9:5],3'b0,wide_color[14:10],3'b0};                 // BGR5 to RGB8
    else rgb <= 24'h000000;
end

// HDMI output.
logic[2:0] tmds;
wire tmdsClk;

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
    .I({tmdsClk, tmds}),
    .O({tmds_clk_p, tmds_d_p}),
    .OB({tmds_clk_n, tmds_d_n})
);


endmodule
