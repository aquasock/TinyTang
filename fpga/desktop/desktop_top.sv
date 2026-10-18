// TinyTang desktop core: top level for the Tang Console 138K.
//
// The BL616 runs TinyDesk and drives this core over UART: a full-screen text
// layer on HDMI (desktop_hdmi), the PMOD socket personalities (desktop_pmod),
// and two keyboard-link receivers on the USB-A ports (keylink_rx).
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

    // USB1 and USB2: keyboard-link receivers on the D+ lines
    inout usb1_dp,
    inout usb1_dn,
    inout usb2_dp,
    inout usb2_dn,

    // HDMI TX
    output       tmds_clk_n,
    output       tmds_clk_p,
    output [2:0] tmds_d_n,
    output [2:0] tmds_d_p
);

// Pads, SNES layout (R L X A RT LT DN UP START SELECT Y B)
wire [11:0] joy_usb1, joy_usb2;
wire [11:0] hid1, hid2;             // From BL616

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

// The keyboard link's report, on its way up to the BL616.  The six usage codes
// are packed into one bus, key 0 in the low byte, which is the order the
// receiver reports them in.
wire [7:0]  link_kbd_mods;
wire [47:0] link_kbd_keys;

// Pointer mode.
//
// Left-alt because it is a modifier no typing here needs -- the BL616's key
// translator ignores both alts -- and because the report already carries it:
// the keyboard end sends its modifier byte and the receiver hands it up as
// link_mods, so no new plumbing is involved.  HID puts left-alt at bit 2.
//
// Why a modifier at all: the arrows are wanted twice -- as a pointer on the
// desktop and as arrow keys in a window -- and never at the same time, since
// scrolling across the desktop while typing in the Terminal is unusable.
// Held, this keyboard is a pointer; released, it is only a keyboard.
wire link_pointer_mode = link_mods[2];

// Which keys the pointer is currently using, so they can be withheld from the
// typing path while it uses them.
//
// Gating only the pad word was not enough, and the reason is worth keeping:
// those six keycodes reach the desktop twice, once as pad bits by this core
// and once inside the report above.  Suppressing one left the other, so in
// pointer mode an arrow moved the pointer *and* typed an arrow -- the same
// double duty, just from the other side.  The core is where the mode is
// decided, so the core is where the decision has to be applied to both.
//
// Zero means "no key" in a boot report, so dropping one here is exactly what
// the keyboard would have sent had the key never been pressed.
function [7:0] link_withhold_if_pointer;
    input [7:0] k;
    begin
        if (link_pointer_mode &&
            (k == 8'h52 || k == 8'h51 || k == 8'h50 || k == 8'h4F ||
             k == 8'h28 || k == 8'h29)) begin
            link_withhold_if_pointer = 8'd0;
        end else begin
            link_withhold_if_pointer = k;
        end
    end
endfunction

assign link_kbd_mods = link_mods;
assign link_kbd_keys = {link_withhold_if_pointer(link_keys[5]),
                        link_withhold_if_pointer(link_keys[4]),
                        link_withhold_if_pointer(link_keys[3]),
                        link_withhold_if_pointer(link_keys[2]),
                        link_withhold_if_pointer(link_keys[1]),
                        link_withhold_if_pointer(link_keys[0])};

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
    .joy1(joy_usb1), .joy2(joy_usb2),
    .link_mods(link_kbd_mods), .link_keys(link_kbd_keys),
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

// Port 1 carries the keyboard link instead of a low-speed USB host.  The
// keyboard runs Tang firmware that drives its own USB D+ line as a GPIO UART,
// and this reads it.  sys_clk is 50 MHz here, so 281250 baud divides to 177
// clocks, 0.44% from exact, while the keyboard's end is exact -- a UART
// tolerates two to three percent.
wire [7:0]  link_mods;
wire [7:0]  link_keys [6];
wire        link_valid;
wire [31:0] link_frames;
wire [31:0] link_bad;
wire [31:0] link_trunc;

keylink_rx #(.CLK_HZ(50_000_000), .BAUD(281_250)) keyboard_link (
    .clk(sys_clk), .resetn(sys_resetn), .rx(usb1_dp),
    .o_modifiers(link_mods), .o_keys(link_keys), .o_valid(link_valid),
    .o_frames(link_frames), .o_bad_checksum(link_bad), .o_truncated(link_trunc)
);

// Both arrow directions and the two buttons are mapped.  The arrow keycodes
// go to the SNES-layout directions, which is what the pointer reads for
// movement.
//
// Enter and Esc go to A and B rather than to Start and Select, and that is not
// a detail: the desktop's pointer is built out of xterm mouse reports, and
// tang_pad_step() only ever looks at TANG_PAD_A and TANG_PAD_B, the two bits
// the core's own menu uses as "choose".  Start and Select are gamepad bits
// that nothing on the pointer path inspects, so mapping the keys there made
// them do nothing at all -- no error, no effect, just silence.  As A and B they
// become the left and right mouse buttons, which is what a pointer-only
// desktop can actually act on.
function [11:0] link_key_to_pad;
    input [7:0] k;
    begin
        case (k)
            8'h52:   link_key_to_pad = 12'b0000_0001_0000;  // Up    -> bit 4
            8'h51:   link_key_to_pad = 12'b0000_0010_0000;  // Down  -> bit 5
            8'h50:   link_key_to_pad = 12'b0000_0100_0000;  // Left  -> bit 6
            8'h4F:   link_key_to_pad = 12'b0000_1000_0000;  // Right -> bit 7
            8'h28:   link_key_to_pad = 12'b0001_0000_0000;  // Enter -> A, left click
            8'h29:   link_key_to_pad = 12'b0000_0000_0001;  // Esc   -> B, right click
            default: link_key_to_pad = 12'b0000_0000_0000;
        endcase
    end
endfunction

// joy_usb1 is what the core reports upward to the BL616, so the desktop sees
// these keys without any firmware change.
//
// The pad half of pointer mode: with the modifier down the arrows and the two
// buttons become the pointer, and with it up this word is zero and the
// keyboard only types.  See link_pointer_mode above for why the mode exists,
// and link_withhold_if_pointer() for the other half -- both are needed, since
// these keycodes reach the desktop twice.
assign joy_usb1 = link_pointer_mode
                ? (link_key_to_pad(link_keys[0]) | link_key_to_pad(link_keys[1])
                 | link_key_to_pad(link_keys[2]) | link_key_to_pad(link_keys[3])
                 | link_key_to_pad(link_keys[4]) | link_key_to_pad(link_keys[5]))
                : 12'b0;

// Port 2 carries a second keyboard-link receiver.  Wiring both ports removes
// the guess about which physical socket is usb1: whichever one the keyboard is
// in, its arrows reach the core.
wire [7:0]  link2_mods;
wire [7:0]  link2_keys [6];
wire        link2_valid;
wire [31:0] link2_bad;
wire [31:0] link2_trunc;

keylink_rx #(.CLK_HZ(50_000_000), .BAUD(281_250)) keyboard_link2 (
    .clk(sys_clk), .resetn(sys_resetn), .rx(usb2_dp),
    .o_modifiers(link2_mods), .o_keys(link2_keys), .o_valid(link2_valid),
    .o_frames(), .o_bad_checksum(link2_bad), .o_truncated(link2_trunc)
);

assign joy_usb2 = link_key_to_pad(link2_keys[0]) | link_key_to_pad(link2_keys[1])
                | link_key_to_pad(link2_keys[2]) | link_key_to_pad(link2_keys[3])
                | link_key_to_pad(link2_keys[4]) | link_key_to_pad(link2_keys[5]);

endmodule
