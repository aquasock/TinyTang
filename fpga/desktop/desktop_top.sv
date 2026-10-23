// TinyTang desktop core: top level for the Tang Console 138K.
//
// The BL616 runs TinyDesk and drives this core over UART: a full-screen text
// layer on HDMI (desktop_hdmi), and the PMOD socket personalities and the
// I2C bus on the USB-A port (desktop_pmod).
//
// Derived from nestang_top.sv in nestang by nand2mario (GPL-3.0); the NES
// machine, SDRAM, controllers and loader have been removed.
// SPDX-License-Identifier: GPL-3.0-only

module desktop_top (
    input sys_clk,

    // Button S1.  Pinned (with its pull-up) but not used.
    input s1,

    // UART
    input UART_RXD,
    output UART_TXD,

    inout [7:0] pmod0_io, pmod1_io,

    // The populated USB-A port's data pair, an I2C bus: SDA on D+, SCL on D-
    inout usb_sda,
    inout usb_scl,

    // HDMI TX
    output       tmds_clk_n,
    output       tmds_clk_p,
    output [2:0] tmds_d_n,
    output [2:0] tmds_d_p
);

wire [15:0] hid1, hid2;             // From BL616, unused

///////////////////////////
// Clocks
///////////////////////////

wire clk;       // 21.492 MHz main logic clock
wire hclk;      // 720p pixel clock: 74.25 Mhz
wire hclk5;     // 5x pixel clock: 371.25 Mhz
wire clk27;     // 27Mhz to generate hclk/hclk5

reg sys_resetn = 0;
reg [7:0] reset_cnt = 255;      // reset for 255 cycles before start everything
always @(posedge clk) begin
    reset_cnt <= reset_cnt == 0 ? 0 : reset_cnt - 1;
    if (reset_cnt == 0)
        sys_resetn <= 1'b1;
end

gowin_pll_27 pll_27 (.clkin(sys_clk), .clkout0(clk27));      // 27 MHz from the 50 MHz crystal
gowin_pll_nes pll_nes (.clkin(sys_clk), .clkout0(clk), .clkout1(), .clkout2());

gowin_pll_hdmi pll_hdmi (
    .clkin(clk27),
    .clkout(hclk5)
);

CLKDIV #(.DIV_MODE(5)) div5 (
    .CLKOUT(hclk),
    .HCLKIN(hclk5),
    .RESETN(sys_resetn),
    .CALIB(1'b0)
);

///////////////////////////
// Peripherals
///////////////////////////

wire overlay;                   // the link's overlay enable

// The desktop layer's write port and enable, from the link to the compositor
// inside desktop_hdmi.
wire [6:0]  wide_x;
wire [5:0]  wide_y;
wire [6:0]  wide_ch;
wire [14:0] wide_fg;
wire [14:0] wide_bg;
wire        wide_we;
wire        wide_on;

wire [10:0] desktop_cx;
wire [9:0] desktop_cy;
wire [23:0] desktop_rgb;
wire [15:0] desktop_socket_word;
wire desktop_socket_request, desktop_socket_ack;
wire desktop_oled_cell_we;
wire [8:0] desktop_oled_cell_index;
wire [15:0] desktop_oled_cell_word;
wire [16:0] desktop_oled_cursor;
wire [1:0] desktop_oled_font;
wire [31:0] desktop_oled_frames, desktop_oled_signature;
wire [63:0] desktop_i2c_rdata, desktop_i2c_wdata;
wire [31:0]  desktop_i2c_status, desktop_i2c_cmd, desktop_i2c_auto;
wire         desktop_i2c_cmd_toggle;
wire         desktop_i2c_res_toggle;
desktop_pmod desktop_sockets (
    .control_clk(clk), .pixel_clk(hclk), .sys_clk(sys_clk), .resetn(sys_resetn),
    .cx(desktop_cx), .cy(desktop_cy), .rgb(desktop_rgb),
    .socket_word(desktop_socket_word), .socket_request(desktop_socket_request),
    .socket_ack(desktop_socket_ack), .pmod0_io(pmod0_io), .pmod1_io(pmod1_io),
    .usb_sda(usb_sda), .usb_scl(usb_scl),
    .oled_cell_we(desktop_oled_cell_we), .oled_cell_index(desktop_oled_cell_index),
    .oled_cell_word(desktop_oled_cell_word), .oled_cursor(desktop_oled_cursor),
    .oled_font(desktop_oled_font),
    .oled_frames(desktop_oled_frames), .oled_signature(desktop_oled_signature),
    .i2c_cmd(desktop_i2c_cmd), .i2c_cmd_toggle(desktop_i2c_cmd_toggle),
    .i2c_wdata(desktop_i2c_wdata), .i2c_auto(desktop_i2c_auto),
    .i2c_rdata(desktop_i2c_rdata), .i2c_res_toggle(desktop_i2c_res_toggle),
    .i2c_status(desktop_i2c_status)
);
localparam [15:0] THIS_CORE_ID = 16'h0054;

// HDMI output
desktop_hdmi u_hdmi (
    .clk(clk), .resetn(sys_resetn),
    .overlay(overlay),
    .wide_we(wide_we), .wide_x(wide_x), .wide_y(wide_y), .wide_ch(wide_ch),
    .wide_fg(wide_fg), .wide_bg(wide_bg), .wide_on(wide_on),
    .desktop_cx(desktop_cx), .desktop_cy(desktop_cy), .desktop_rgb(desktop_rgb),
    .clk_pixel(hclk), .clk_5x_pixel(hclk5),
    .tmds_clk_n(tmds_clk_n), .tmds_clk_p(tmds_clk_p),
    .tmds_d_n(tmds_d_n), .tmds_d_p(tmds_d_p)
);

// The BL616 companion MCU
desktop_link #(.FREQ(21_492_000), .CORE_ID(THIS_CORE_ID))
    sys_inst (
    .clk(clk), .hclk(hclk), .resetn(sys_resetn),

    .overlay(overlay),
    .wide_x(wide_x), .wide_y(wide_y), .wide_ch(wide_ch),
    .wide_fg(wide_fg), .wide_bg(wide_bg), .wide_we(wide_we), .wide_on(wide_on),
    .hid1(hid1), .hid2(hid2),
    .uart_tx(UART_TXD), .uart_rx(UART_RXD),
    // The clock monitor's crystal reference.
    .sys_clk(sys_clk),
    .desktop_socket_word(desktop_socket_word),
    .desktop_socket_request(desktop_socket_request),
    .desktop_socket_ack(desktop_socket_ack),
    .desktop_oled_cell_we(desktop_oled_cell_we), .desktop_oled_cell_index(desktop_oled_cell_index),
    .desktop_oled_cell_word(desktop_oled_cell_word), .desktop_oled_cursor(desktop_oled_cursor),
    .desktop_oled_font(desktop_oled_font),
    .desktop_oled_frames(desktop_oled_frames), .desktop_oled_signature(desktop_oled_signature),

    .desktop_i2c_rdata(desktop_i2c_rdata),
    .desktop_i2c_res_toggle(desktop_i2c_res_toggle),
    .desktop_i2c_status(desktop_i2c_status),
    .desktop_i2c_cmd(desktop_i2c_cmd),
    .desktop_i2c_cmd_toggle(desktop_i2c_cmd_toggle),
    .desktop_i2c_wdata(desktop_i2c_wdata),
    .desktop_i2c_auto(desktop_i2c_auto),

    .rom_loading(), .rom_do(), .rom_do_valid()
);

endmodule
