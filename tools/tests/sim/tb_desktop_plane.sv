`timescale 1ns/1ps
//
// The pixel plane end to end: UART frames from the BL616 into desktop_link,
// through pixplane, out of desktop_hdmi's raster (desktop ABI 1.8).
//
// What can be wrong is mostly position, not colour: a cursor computed wrong,
// pixels doubled onto the wrong output, the look-ahead off by a pixel so a
// line's first pixel arrives at the end of the previous one (the fault
// textdisp_wide's header records from hardware), a run that does not carry
// onto the next row, or a write past the end that wraps onto the top of the
// picture.  Each has a pixel below that only it would get wrong.
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
    wire [17:0] plane_waddr;
    wire [15:0] plane_wdata;
    wire        uart_tx;

    desktop_link #(.FREQ(21_492_000), .CORE_ID(16'h0054)) link (
        .clk(clk), .hclk(clk_pixel), .resetn(resetn),
        .overlay(overlay),
        .wide_x(wide_x), .wide_y(wide_y), .wide_ch(wide_ch),
        .wide_fg(wide_fg), .wide_bg(wide_bg), .wide_we(wide_we), .wide_on(wide_on),
        .plane_we(plane_we), .plane_waddr(plane_waddr), .plane_wdata(plane_wdata),
        .plane_on(plane_on),
        .hid1(), .hid2(),
        .uart_rx(uart_rx), .uart_tx(uart_tx)
    );

    wire [10:0] cx;
    wire [9:0]  cy;
    wire [23:0] rgb;
    wire tmds_clk_n, tmds_clk_p;
    wire [2:0] tmds_d_n, tmds_d_p;

    desktop_hdmi video (
        .clk(clk), .resetn(resetn), .overlay(overlay),
        .wide_we(wide_we), .wide_x(wide_x), .wide_y(wide_y), .wide_ch(wide_ch),
        .wide_fg(wide_fg), .wide_bg(wide_bg), .wide_on(wide_on),
        .plane_we(plane_we), .plane_waddr(plane_waddr), .plane_wdata(plane_wdata),
        .plane_on(plane_on),
        .desktop_cx(cx), .desktop_cy(cy), .desktop_rgb(rgb),
        .clk_pixel(clk_pixel), .clk_5x_pixel(clk_5x_pixel),
        .tmds_clk_n(tmds_clk_n), .tmds_clk_p(tmds_clk_p),
        .tmds_d_n(tmds_d_n), .tmds_d_p(tmds_d_p)
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

    task cursor(input [15:0] x, input [15:0] y);
        begin
            frame(8'h18, 16'd5);
            send_byte(x[15:8]); send_byte(x[7:0]); send_byte(y[15:8]); send_byte(y[7:0]);
        end
    endtask

    task pixel(input [15:0] p);
        begin send_byte(p[15:8]); send_byte(p[7:0]); end
    endtask

    /* ---------------------------------------------------- what reaches HDMI */

    /* rgb is registered one pixel behind cx/cy, so the coordinate it shows is
     * the one the raster held a clock earlier.  Eight output rows are kept. */
    localparam integer NR = 8;
    reg [9:0]  rows [0:NR-1];
    reg [23:0] cap  [0:NR-1][0:1279];
    reg [10:0] pcx = 0;
    reg [9:0]  pcy = 0;
    reg        capturing = 0;
    integer r;

    initial begin
        rows[0] = 0;   rows[1] = 1;   rows[2] = 2;   rows[3] = 3;
        rows[4] = 200; rows[5] = 201; rows[6] = 718; rows[7] = 719;
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

    integer fails = 0, checks = 0;
    task expect_px(input [255:0] what, input integer x, input integer y, input [23:0] want);
        reg [23:0] got;
        begin
            got = at(x, y);
            checks = checks + 1;
            if (got !== want) begin
                fails = fails + 1;
                $display("FAIL: %0s at (%0d,%0d): got %06h, want %06h", what, x, y, got, want);
            end
        end
    endtask

    /* One whole HDMI frame, from its start.  The raster is still at (0,0) when
     * capture begins, so the end is found by first leaving row 0 and then
     * coming back to (0,0); waiting for (0,0) twice in a row returns at once.
     * The captured end stops on the frame's last visible line, before the next
     * frame's first pixels can overwrite row 0. */
    task frame_out;
        begin
            wait (cx == 0 && cy == 0);
            @(posedge clk_pixel);
            capturing = 1;
            wait (cy == 10'd720);
            capturing = 0;
        end
    endtask

    localparam [15:0] RED = 16'hF800, GREEN = 16'h07E0, BLUE = 16'h001F,
                      WHITE = 16'hFFFF, GREY = 16'h8410, YELLOW = 16'hFFE0,
                      CYAN = 16'h07FF, MAGENTA = 16'hF81F;

    initial begin
        repeat (20) @(posedge clk);
        resetn = 1;
        repeat (40) @(posedge clk);

        // Pixel (0,0): a line's first pixel, which a wrong look-ahead loses.
        cursor(0, 0);   frame(8'h19, 16'd3);  pixel(RED);
        // (639,0) then (0,1): a run carries onto the next row.
        cursor(639, 0); frame(8'h19, 16'd5);  pixel(GREEN); pixel(BLUE);
        // Two neighbours in the middle.
        cursor(10, 100); frame(8'h19, 16'd5); pixel(WHITE); pixel(GREY);
        // The last pixel, then one past it, which must be dropped, not wrapped.
        cursor(639, 359); frame(8'h19, 16'd5); pixel(YELLOW); pixel(CYAN);
        // A cursor off the plane parks: its pixels go nowhere.
        cursor(0, 600); frame(8'h19, 16'd3);  pixel(MAGENTA);
        // Show it.
        frame(8'h1A, 16'd2); send_byte(8'h01);

        frame_out();

        // RGB565 widens by repeating each field's top bits.
        expect_px("first pixel of the first line", 0, 0, 24'hFF0000);
        expect_px("doubled across",               1, 0, 24'hFF0000);
        expect_px("doubled down",                 0, 1, 24'hFF0000);
        expect_px("doubled both",                 1, 1, 24'hFF0000);
        expect_px("never written is black",       2, 0, 24'h000000);
        expect_px("last pixel of the first row",  1278, 0, 24'h00FF00);
        expect_px("last pixel, doubled",          1279, 1, 24'h00FF00);
        expect_px("run carried to the next row",  0, 2, 24'h0000FF);
        expect_px("carried, doubled",             1, 3, 24'h0000FF);
        expect_px("middle pixel",                 20, 200, 24'hFFFFFF);
        expect_px("middle pixel, doubled",        21, 201, 24'hFFFFFF);
        expect_px("its neighbour",                22, 200, 24'h848284);
        expect_px("its neighbour, doubled",       23, 201, 24'h848284);
        expect_px("left of the pair is black",    19, 200, 24'h000000);
        expect_px("the very last pixel",          1278, 718, 24'hFFFF00);
        expect_px("the very last, doubled",       1279, 719, 24'hFFFF00);

        // Off: the text layer's path, which is black with the layer disabled.
        frame(8'h1A, 16'd2); send_byte(8'h00);
        frame_out();
        expect_px("plane off shows the text path", 0, 0, 24'h000000);
        expect_px("plane off, middle",             20, 200, 24'h000000);

        // On again: the store kept what it held.
        frame(8'h1A, 16'd2); send_byte(8'h01);
        frame_out();
        expect_px("plane back on, contents kept", 20, 200, 24'hFFFFFF);
        expect_px("no wrap reached the top",       0, 0, 24'hFF0000);

        if (fails == 0) $display("desktop plane: PASS (%0d checks)", checks);
        else            $display("desktop plane: FAIL (%0d of %0d checks)", fails, checks);
        $finish;
    end

endmodule
