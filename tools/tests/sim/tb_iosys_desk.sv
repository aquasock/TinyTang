`timescale 1ns/1ps
//
// Exercises the core's command decode for the desktop layer.
//
// This is the link in the chain that was verified only by compiling.  The
// firmware side is tested on the host (fpga_frames, tang_osd_desk) and the
// layer module is tested in simulation (textdisp_wide), but between them sits
// `iosys_bl616` turning bytes off UART1 into writes on the layer's port --
// three frame types, a five-byte cell, a cursor that advances and wraps.  A
// mistake there writes the right cells in the wrong places, which on hardware
// looks like a desktop that is subtly scrambled and gives no clue why.
//
// Frames are driven on uart_rx at 2 Mbaud, the rate the link actually runs, so
// the UART and the decode are exercised together rather than separately.
//
//   tools/tests/test_iosys_desk.sh
//
module tb_iosys_desk;

    reg clk = 0, hclk = 0;
    always #23.26 clk = ~clk;        // ~21.49 MHz, the rate nestang_top uses
    always #6.73  hclk = ~hclk;      // ~74.25 MHz pixel clock

    reg resetn = 0;

    // iosys inputs this test does not drive.
    reg  [11:0] joy1 = 0, joy2 = 0;
    reg  [15:0] mgmt_readdata = 0;
    reg  [1:0]  fdd_request = 0;
    reg  [7:0]  kbd_data = 0;
    wire [7:0]  overlay_x = 0;
    wire [7:0]  overlay_y = 0;
    reg         uart_rx = 1;

    wire        overlay;
    wire [14:0] overlay_color;
    wire [6:0]  wide_x;
    wire [5:0]  wide_y;
    wire [6:0]  wide_ch;
    wire [14:0] wide_fg;
    wire [14:0] wide_bg;
    wire        wide_we;
    wire        wide_on;
    wire [7:0]  rom_loading;
    wire [7:0]  rom_do;
    wire        rom_do_valid;
    wire [15:0] mgmt_address;
    wire        mgmt_read;
    wire        mgmt_write;
    wire [15:0] mgmt_writedata;
    wire        kbd_data_valid;
    wire [31:0] core_config;
    wire        uart_tx;

    iosys_bl616 #(
        .FREQ(21_492_000),
        .CORE_ID(16'h0050)
    ) dut (
        .clk(clk), .hclk(hclk), .resetn(resetn),
        .overlay(overlay), .overlay_x(overlay_x), .overlay_y(overlay_y),
        .overlay_color(overlay_color),
        .wide_x(wide_x), .wide_y(wide_y), .wide_ch(wide_ch),
        .wide_fg(wide_fg), .wide_bg(wide_bg), .wide_we(wide_we), .wide_on(wide_on),
        .joy1(joy1), .joy2(joy2), .hid1(), .hid2(),
        .rom_loading(rom_loading), .rom_do(rom_do), .rom_do_valid(rom_do_valid),
        .mgmt_address(mgmt_address), .mgmt_read(mgmt_read),
        .mgmt_readdata(mgmt_readdata), .mgmt_write(mgmt_write),
        .mgmt_writedata(mgmt_writedata), .fdd_request(fdd_request),
        .kbd_data(kbd_data), .kbd_data_valid(kbd_data_valid),
        .core_config(core_config),
        .uart_rx(uart_rx), .uart_tx(uart_tx)
    );

    /* ------------------------------------------------- what was written */

    integer    nw = 0;
    reg [6:0]  lx  [0:31];
    reg [5:0]  ly  [0:31];
    reg [6:0]  lch [0:31];
    reg [14:0] lfg [0:31];
    reg [14:0] lbg [0:31];

    always @(posedge clk) begin
        if (resetn && wide_we && nw < 32) begin
            lx[nw]  = wide_x;
            ly[nw]  = wide_y;
            lch[nw] = wide_ch;
            lfg[nw] = wide_fg;
            lbg[nw] = wide_bg;
            nw      = nw + 1;
        end
    end

    integer fails  = 0;
    integer checks = 0;

    task check_int(input [255:0] what, input integer got, input integer want);
        begin
            checks = checks + 1;
            if (got !== want) begin
                fails = fails + 1;
                $display("FAIL: %0s (got %0d, want %0d)", what, got, want);
            end
        end
    endtask

    /* ---------------------------------------------------- the wire, at 2 Mbaud */

    task send_bit(input b);
        begin
            uart_rx = b;
            #500;
        end
    endtask

    task send_byte(input [7:0] b);
        integer i;
        begin
            send_bit(0);
            for (i = 0; i < 8; i = i + 1) begin
                send_bit(b[i]);
            end
            send_bit(1);
        end
    endtask

    /* 0xAA len_hi len_lo type; len counts the type byte. */
    task frame_header(input [7:0] type_, input [8:0] len);
        begin
            send_byte(8'hAA);
            send_byte(8'h00);
            send_byte(len);
            send_byte(type_);
        end
    endtask

    /* ------------------------------------------------------------ the test */

    initial begin
        repeat (20) @(posedge clk);
        resetn = 1;
        repeat (40) @(posedge clk);

        check_int("off until told", wide_on, 0);

        /* 0x15: enable the layer. */
        frame_header(8'h15, 9'd2);
        send_byte(8'h01);
        repeat (40) @(posedge clk);
        check_int("0x15 enables the layer", wide_on, 1);

        /* 0x13 + 0x14: a cursor, then one cell with its own colours.
         * 'A' (0x41), foreground 0x1234, background 0x0ABC. */
        nw = 0;
        frame_header(8'h13, 9'd3);
        send_byte(8'h05);
        send_byte(8'h03);
        frame_header(8'h14, 9'd6);
        send_byte(8'h41);
        send_byte(8'h12);               /* fg high */
        send_byte(8'h34);               /* fg low  */
        send_byte(8'h0A);               /* bg high */
        send_byte(8'hBC);               /* bg low  */
        repeat (60) @(posedge clk);

        check_int("one cell written", nw, 1);
        if (nw >= 1) begin
            check_int("cell x", lx[0], 5);
            check_int("cell y", ly[0], 3);
            check_int("glyph", lch[0], 8'h41);
            check_int("foreground", lfg[0], 15'h1234);
            check_int("background", lbg[0], 15'h0ABC);
        end

        /* Two cells in one run: the cursor advances. */
        nw = 0;
        frame_header(8'h13, 9'd3);
        send_byte(8'h00);
        send_byte(8'h00);
        frame_header(8'h14, 9'd11);      /* ten bytes: two cells */
        send_byte(8'h58);               /* 'X' */
        send_byte(8'h00); send_byte(8'h01);
        send_byte(8'h00); send_byte(8'h02);
        send_byte(8'h59);               /* 'Y' */
        send_byte(8'h00); send_byte(8'h03);
        send_byte(8'h00); send_byte(8'h04);
        repeat (80) @(posedge clk);

        check_int("two cells written", nw, 2);
        if (nw >= 2) begin
            check_int("first x", lx[0], 0);
            check_int("second x advanced", lx[1], 1);
            check_int("first glyph", lch[0], 8'h58);
            check_int("second glyph", lch[1], 8'h59);
        end

        /* A run that ends the row wraps to the next one, so a whole screen can
         * be painted as one run rather than a row at a time. */
        nw = 0;
        frame_header(8'h13, 9'd3);
        send_byte(8'h4F);               /* x = 79, the last column */
        send_byte(8'h0A);               /* y = 10 */
        frame_header(8'h14, 9'd11);
        send_byte(8'h2E); send_byte(8'h00); send_byte(8'h01);
        send_byte(8'h00); send_byte(8'h02);
        send_byte(8'h2E); send_byte(8'h00); send_byte(8'h03);
        send_byte(8'h00); send_byte(8'h04);
        repeat (80) @(posedge clk);

        check_int("the wrapped cell was written", nw, 2);
        if (nw >= 2) begin
            check_int("last column", lx[0], 79);
            check_int("row kept", ly[0], 10);
            check_int("wrapped to column 0", lx[1], 0);
            check_int("wrapped to the next row", ly[1], 11);
        end

        if (fails == 0) begin
            $display("iosys desk decode: PASS (%0d checks)", checks);
        end else begin
            $display("iosys desk decode: FAIL (%0d of %0d checks wrong)", fails, checks);
        end
        $finish;
    end

endmodule
