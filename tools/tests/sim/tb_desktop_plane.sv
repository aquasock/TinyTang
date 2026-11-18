`timescale 1ns/1ps
//
// The pixel plane end to end: UART frames from the BL616 into desktop_link,
// through fb_sdram and a timing-checking model of the dock's SDRAM, out of
// desktop_hdmi's raster (desktop ABI 1.9: 1280x720, shown 1:1).
//
// What can be wrong is mostly position, not colour: a cursor decoded wrong, the
// look-ahead off by a pixel so a line's first pixel arrives at the end of the
// previous one (the fault textdisp_wide's header records from hardware), a run
// that does not carry onto the next row, a row split across SDRAM rows wrong
// at x = 511/512 or 1023/1024, a 1280-pixel row that does not continue from one
// frame into the next, or a write past the end that wraps onto the top of the
// picture.  Each has a pixel below that only it would get wrong.  Then the
// whole frame: the plane's checksum (registers 0x14/0x18) must be the one the
// bench computes from what it wrote, and it must not be when frame 0x1B sets a
// read alignment the model does not answer at.
//
//   tools/tests/test_desktop_plane.sh
//
module tb_desktop_plane;

    reg clk = 0, clk_pixel = 0, clk_5x_pixel = 0;
    always #23.26 clk = ~clk;              // ~21.49 MHz, the link's clock
    always #6.73  clk_pixel = ~clk_pixel;  // ~74.25 MHz pixel clock
    always #1.346 clk_5x_pixel = ~clk_5x_pixel;

    reg resetn = 0;
    reg uart_rx = 1;

    wire        overlay;
    wire [6:0]  wide_x;
    wire [5:0]  wide_y;
    wire [6:0]  wide_ch;
    wire [14:0] wide_fg, wide_bg;
    wire        wide_we, wide_on;
    wire        plane_we, plane_on;
    wire [10:0] plane_wx;
    wire [9:0]  plane_wy;
    wire [15:0] plane_wdata;
    wire [3:0]  plane_rd_cfg;
    wire [31:0] plane_status, plane_sum1, plane_sum2;
    wire        uart_tx;

    desktop_link #(.FREQ(21_492_000), .CORE_ID(16'h0054)) link (
        .clk(clk), .hclk(clk_pixel), .resetn(resetn),
        .overlay(overlay),
        .wide_x(wide_x), .wide_y(wide_y), .wide_ch(wide_ch),
        .wide_fg(wide_fg), .wide_bg(wide_bg), .wide_we(wide_we), .wide_on(wide_on),
        .plane_we(plane_we), .plane_wx(plane_wx), .plane_wy(plane_wy),
        .plane_wdata(plane_wdata), .plane_on(plane_on),
        .plane_rd_cfg(plane_rd_cfg), .plane_status(plane_status),
        .plane_sum1(plane_sum1), .plane_sum2(plane_sum2),
        .hid1(), .hid2(),
        .uart_rx(uart_rx), .uart_tx(uart_tx)
    );

    wire [10:0] cx;
    wire [9:0]  cy;
    wire [23:0] rgb;
    wire tmds_clk_n, tmds_clk_p;
    wire [2:0] tmds_d_n, tmds_d_p;
    wire [12:0] sd_a;
    wire [1:0]  sd_ba, sd_dqm;
    wire        sd_cs_n, sd_ras_n, sd_cas_n, sd_we_n;
    wire [15:0] sd_dq;

    desktop_hdmi video (
        .clk(clk), .resetn(resetn), .overlay(overlay),
        .wide_we(wide_we), .wide_x(wide_x), .wide_y(wide_y), .wide_ch(wide_ch),
        .wide_fg(wide_fg), .wide_bg(wide_bg), .wide_on(wide_on),
        .plane_we(plane_we), .plane_wx(plane_wx), .plane_wy(plane_wy),
        .plane_wdata(plane_wdata), .plane_on(plane_on),
        .plane_rd_cfg(plane_rd_cfg), .plane_status(plane_status),
        .plane_sum1(plane_sum1), .plane_sum2(plane_sum2),
        .sd_a(sd_a), .sd_ba(sd_ba), .sd_cs_n(sd_cs_n), .sd_ras_n(sd_ras_n),
        .sd_cas_n(sd_cas_n), .sd_we_n(sd_we_n), .sd_dqm(sd_dqm), .sd_dq(sd_dq),
        .desktop_cx(cx), .desktop_cy(cy), .desktop_rgb(rgb),
        .clk_pixel(clk_pixel), .clk_5x_pixel(clk_5x_pixel),
        .tmds_clk_n(tmds_clk_n), .tmds_clk_p(tmds_clk_p),
        .tmds_d_n(tmds_d_n), .tmds_d_p(tmds_d_p)
    );

    // The part, on the inverse of the pixel clock as desktop_top's ODDR drives it.
    wire sdram_clk = ~clk_pixel;
    sdram_w9825_model #(.SIZE_WORDS(2 * 1024 * 1024)) part (
        .clk(sdram_clk), .A(sd_a), .BA(sd_ba), .nCS(sd_cs_n), .nRAS(sd_ras_n),
        .nCAS(sd_cas_n), .nWE(sd_we_n), .DQM(sd_dqm), .DQ(sd_dq)
    );

    /* ------------------------------------------------------ the wire, 2 Mbaud */

    task send_byte(input [7:0] b);
        integer i;
        begin
            uart_rx = 0; #500;
            for (i = 0; i < 8; i = i + 1) begin uart_rx = b[i]; #500; end
            uart_rx = 1; #500;
        end
    endtask

    /* 0xAA len_hi len_lo type; len counts the type byte. */
    task frame(input [7:0] type_, input [15:0] len);
        begin
            send_byte(8'hAA); send_byte(len[15:8]); send_byte(len[7:0]); send_byte(type_);
        end
    endtask

    /* The bench's own copy of the cursor and of the picture it should make. */
    reg [15:0] want [int];                 // key y * 2048 + x
    integer bx = 0, by = 0;

    task cursor(input [15:0] x, input [15:0] y);
        begin
            frame(8'h18, 16'd5);
            send_byte(x[15:8]); send_byte(x[7:0]); send_byte(y[15:8]); send_byte(y[7:0]);
            if (x >= 2048 || y >= 1024) begin bx = 0; by = 1023; end
            else begin bx = x; by = y; end
        end
    endtask

    task pixel(input [15:0] p);
        begin
            send_byte(p[15:8]); send_byte(p[7:0]);
            if (bx < 1280 && by < 720) want[by * 2048 + bx] = p;
            if (bx >= 1279) begin bx = 0; if (by != 1023) by = by + 1; end
            else bx = bx + 1;
        end
    endtask

    task read_cfg(input [7:0] v);
        begin frame(8'h1B, 16'd2); send_byte(v); end
    endtask

    /* ---------------------------------------------------- what reaches HDMI */

    /* rgb is registered one pixel behind cx/cy, so the coordinate it shows is
     * the one the raster held a clock earlier.  Some output rows are kept. */
    localparam integer NR = 10;
    reg [9:0]  rows [0:NR-1];
    reg [23:0] cap  [0:NR-1][0:1279];
    reg [10:0] pcx = 0;
    reg [9:0]  pcy = 0;
    reg        capturing = 0;
    integer r;

    initial begin
        rows[0] = 0;   rows[1] = 1;   rows[2] = 5;   rows[3] = 200;
        rows[4] = 300; rows[5] = 301; rows[6] = 359; rows[7] = 360;
        rows[8] = 718; rows[9] = 719;
    end

    always @(posedge clk_pixel) begin
        pcx <= cx;
        pcy <= cy;
        if (capturing && pcx < 11'd1280)
            for (r = 0; r < NR; r = r + 1)
                if (pcy == rows[r]) cap[r][pcx] <= rgb;
    end

    function automatic [23:0] at(input integer x, input integer y);
        integer k;
        begin
            at = 24'hDEADBE;
            for (k = 0; k < NR; k = k + 1)
                if (rows[k] == y) at = cap[k][x];
        end
    endfunction

    function automatic [23:0] widen(input [15:0] p);
        widen = {p[15:11], p[15:13], p[10:5], p[10:9], p[4:0], p[4:2]};
    endfunction

    integer fails = 0, checks = 0;
    task expect_px(input [255:0] what, input integer x, input integer y, input [23:0] want_);
        reg [23:0] got;
        begin
            got = at(x, y);
            checks = checks + 1;
            if (got !== want_) begin
                fails = fails + 1;
                $display("FAIL: %0s at (%0d,%0d): got %06h, want %06h", what, x, y, got, want_);
            end
        end
    endtask

    task expect_true(input [255:0] what, input ok);
        begin
            checks = checks + 1;
            if (!ok) begin
                fails = fails + 1;
                $display("FAIL: %0s", what);
            end
        end
    endtask

    /* One whole HDMI frame, from its start, ending on its last visible line. */
    task frame_out;
        begin
            wait (cx == 0 && cy == 0);
            @(posedge clk_pixel);
            capturing = 1;
            wait (cy == 10'd720);
            capturing = 0;
        end
    endtask

    /* The checksum the plane should report for the picture the bench wrote:
     * s2 += s1; s1 += p over the visible pixels in raster order. */
    reg [31:0] e1, e2;
    task expected_sums;
        integer x, y;
        reg [15:0] pv;
        begin
            e1 = 0; e2 = 0;
            for (y = 0; y < 720; y = y + 1)
                for (x = 0; x < 1280; x = x + 1) begin
                    pv = want.exists(y * 2048 + x) ? want[y * 2048 + x] : 16'h0000;
                    e2 = e2 + e1;
                    e1 = e1 + {16'd0, pv};
                end
        end
    endtask

    /* The registers' copy of the checksum is the one latched at the end of the
     * last frame and carried into the link's clock: wait past a frame end. */
    task settle_sums;
        begin
            wait (cy == 10'd721);
            repeat (20) @(posedge clk);
        end
    endtask

    localparam [15:0] RED = 16'hF800, GREEN = 16'h07E0, BLUE = 16'h001F,
                      WHITE = 16'hFFFF, GREY = 16'h8410, YELLOW = 16'hFFE0,
                      CYAN = 16'h07FF, MAGENTA = 16'hF81F;

    integer i;
    initial begin
        repeat (20) @(posedge clk);
        resetn = 1;
        repeat (40) @(posedge clk);

        // The model answers at two clocks on the rising edge (fb_sdram.sv);
        // the link comes up at the board's three.
        expect_true("the link's read alignment comes up as 3, rising edge",
                    plane_rd_cfg == 4'd3);
        read_cfg(8'h02);

        // Pixel (0,0): a line's first pixel, which a wrong look-ahead loses.
        cursor(0, 0);    frame(8'h19, 16'd3);  pixel(RED);
        // (1279,0) then (0,1): a run carries onto the next row.
        cursor(1279, 0); frame(8'h19, 16'd5);  pixel(GREEN); pixel(BLUE);
        // Both sides of each SDRAM row boundary within a line.
        cursor(511, 5);  frame(8'h19, 16'd5);  pixel(WHITE); pixel(GREY);
        cursor(1023, 5); frame(8'h19, 16'd5);  pixel(YELLOW); pixel(CYAN);
        // Two neighbours in the middle.
        cursor(20, 200); frame(8'h19, 16'd5);  pixel(WHITE); pixel(GREY);
        // A whole 1280-pixel row in two frames, the second continuing from
        // where the first left the cursor: 640 pixels each.
        cursor(0, 300);
        frame(8'h19, 16'd1281);
        for (i = 0; i < 640; i = i + 1) pixel(16'(i * 97 + 1));
        frame(8'h19, 16'd1281);
        for (i = 640; i < 1280; i = i + 1) pixel(16'(i * 97 + 1));
        // The last pixel, then one past it, which must be dropped, not wrapped.
        cursor(1279, 719); frame(8'h19, 16'd5); pixel(YELLOW); pixel(CYAN);
        // Cursors off the plane park: their pixels go nowhere.
        cursor(0, 800);    frame(8'h19, 16'd3); pixel(MAGENTA);
        cursor(3000, 0);   frame(8'h19, 16'd3); pixel(MAGENTA);
        cursor(1300, 359); frame(8'h19, 16'd3); pixel(MAGENTA);   // x off: dropped
        // Show it.
        frame(8'h1A, 16'd2); send_byte(8'h01);

        frame_out();

        // RGB565 widens by repeating each field's top bits.
        expect_px("first pixel of the first line",   0, 0, 24'hFF0000);
        expect_px("its right neighbour is black",    1, 0, 24'h000000);
        expect_px("last pixel of the first row",     1279, 0, 24'h00FF00);
        expect_px("run carried to the next row",     0, 1, 24'h0000FF);
        expect_px("shown 1:1, not doubled",          1, 1, 24'h000000);
        expect_px("left of the first row boundary",  511, 5, 24'hFFFFFF);
        expect_px("right of the first row boundary", 512, 5, 24'h848284);
        expect_px("left of the second row boundary", 1023, 5, 24'hFFFF00);
        expect_px("right of the second boundary",    1024, 5, 24'h00FFFF);
        expect_px("beside the boundary is black",    510, 5, 24'h000000);
        expect_px("middle pixel",                    20, 200, 24'hFFFFFF);
        expect_px("its neighbour",                   21, 200, 24'h848284);
        expect_px("left of the pair is black",       19, 200, 24'h000000);
        expect_px("whole row, first",                0, 300, widen(16'(1)));
        expect_px("whole row, end of frame one",     639, 300, widen(16'(639 * 97 + 1)));
        expect_px("whole row, start of frame two",   640, 300, widen(16'(640 * 97 + 1)));
        expect_px("whole row, last",                 1279, 300, widen(16'(1279 * 97 + 1)));
        expect_px("the row after it is untouched",   0, 301, 24'h000000);
        expect_px("an x off the plane is dropped",   0, 360, 24'h000000);
        expect_px("the very last pixel",             1279, 719, 24'hFFFF00);
        expect_px("left of the very last",           1278, 719, 24'h000000);

        // The whole frame, by the plane's own checksum.
        settle_sums();
        expected_sums();
        expect_true("the plane is ready", plane_status[31]);
        expect_true("no underrun", !plane_status[30]);
        expect_true("no write dropped", !plane_status[29]);
        expect_true("status reports the alignment set", plane_status[27:24] == 4'd2);
        expect_true("checksums seen", plane_status[23:16] != 8'd0);
        expect_true("the fetch fits a line",
                    plane_status[10:0] > 11'd1000 && plane_status[10:0] < 11'd1650);
        expect_true("the frame checksum is the picture written",
                    plane_sum1 == e1 && plane_sum2 == e2);
        $display("desktop plane: checksum %08h %08h, expected %08h %08h, longest fetch %0d",
                 plane_sum1, plane_sum2, e1, e2, plane_status[10:0]);

        // A read alignment the model does not answer at shows in the checksum.
        read_cfg(8'h03);
        settle_sums(); settle_sums();
        expect_true("a wrong read alignment changes the checksum",
                    !(plane_sum1 == e1 && plane_sum2 == e2));
        read_cfg(8'h02);
        settle_sums(); settle_sums();
        expect_true("and the right one restores it", plane_sum1 == e1 && plane_sum2 == e2);

        // Off: the text layer's path, which is black with the layer disabled.
        frame(8'h1A, 16'd2); send_byte(8'h00);
        frame_out();
        expect_px("plane off shows the text path", 0, 0, 24'h000000);
        expect_px("plane off, middle",             20, 200, 24'h000000);

        // On again: the store kept what it held, and the top was never wrapped onto.
        frame(8'h1A, 16'd2); send_byte(8'h01);
        frame_out();
        expect_px("plane back on, contents kept", 20, 200, 24'hFFFFFF);
        expect_px("no wrap reached the top",      0, 0, 24'hFF0000);

        expect_true("no SDRAM timing violation", part.violations == 0);

        if (fails == 0) $display("desktop plane: PASS (%0d checks)", checks);
        else            $display("desktop plane: FAIL (%0d of %0d checks)", fails, checks);
        $finish;
    end

endmodule
