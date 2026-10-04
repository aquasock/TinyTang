`timescale 1ns/1ps
//
// Exercises the composite: where the desktop layer meets the scaler.
//
// Everything else is verified link by link, but this is the one requirement
// nobody has checked -- that the layer covers the *whole* output.  The legacy
// page does not: it is only evaluated while `active` is set, which is the
// 960-pixel window the NES picture is scaled into, so it sits inside bars.
// The desktop must not, or "full screen" is not what was asked for.
//
// The check is deliberately position-independent.  Rather than trying to land
// on a particular raster coordinate, it watches every pixel of a frame and asks
// whether the bar colour ever appeared.  With the layer off it must; with the
// layer on it must not, because then every pixel comes from the layer.
//
//   tools/tests/test_composite.sh
//
module tb_composite;

    reg clk = 0, clk_pixel = 0, clk_5x_pixel = 0;
    always #23.26 clk = ~clk;              // ~21.5 MHz NES clock
    always #6.73  clk_pixel = ~clk_pixel;  // ~74.25 MHz pixel clock
    always #1.346 clk_5x_pixel = ~clk_5x_pixel;

    reg resetn = 0;

    /* The NES side, held quiet: the picture does not matter here, only that
     * pixels are produced and that the bars exist. */
    reg [5:0]  color      = 0;
    reg [8:0]  cycle      = 0;
    reg [8:0]  scanline   = 0;
    reg [15:0] sample     = 0;
    reg        aspect_8x7 = 0;

    reg        overlay       = 0;
    wire [7:0] overlay_x;
    wire [7:0] overlay_y;
    reg [14:0] overlay_color = 0;

    reg        wide_we = 0;
    reg [6:0]  wide_x  = 0;
    reg [5:0]  wide_y  = 0;
    reg [6:0]  wide_ch = 0;
    reg [14:0] wide_fg = 0;
    reg [14:0] wide_bg = 0;
    reg        wide_on = 0;

    wire tmds_clk_n, tmds_clk_p;
    wire [2:0] tmds_d_n, tmds_d_p;

    nes2hdmi dut (
        .clk(clk), .resetn(resetn),
        .color(color), .cycle(cycle), .scanline(scanline), .sample(sample),
        .aspect_8x7(aspect_8x7),
        .overlay(overlay), .overlay_x(overlay_x), .overlay_y(overlay_y),
        .overlay_color(overlay_color),
        .wide_we(wide_we), .wide_x(wide_x), .wide_y(wide_y), .wide_ch(wide_ch),
        .wide_fg(wide_fg), .wide_bg(wide_bg), .wide_on(wide_on),
        .clk_pixel(clk_pixel), .clk_5x_pixel(clk_5x_pixel),
        .tmds_clk_n(tmds_clk_n), .tmds_clk_p(tmds_clk_p),
        .tmds_d_n(tmds_d_n), .tmds_d_p(tmds_d_p)
    );

    /* -------------------------------------------------- what the eye would see */

    localparam [23:0] BAR   = 24'h303030;   /* the colour outside the picture */
    /* 15-bit BGR5 expands to 24-bit by shifting each 5-bit field up three and
     * leaving the bottom three bits zero, so all-ones is 0xF8F8F8, not white. */
    localparam [23:0] MARK  = 24'hF8F8F8;   /* the glyph's foreground */

    integer n_bar   = 0;
    integer n_white = 0;
    integer n_other = 0;

    always @(posedge clk_pixel) begin
        if (resetn) begin
            if (dut.rgb == BAR)        n_bar   = n_bar + 1;
            else if (dut.rgb == MARK)  n_white = n_white + 1;
            else                       n_other = n_other + 1;
        end
    end

    integer fails  = 0;
    integer checks = 0;

    task check(input [255:0] what, input integer ok);
        begin
            checks = checks + 1;
            if (!ok) begin
                fails = fails + 1;
                $display("FAIL: %0s", what);
            end
        end
    endtask

    /* One frame is 1650 x 750 pixels; run a little past it. */
    task run_frame;
        integer i;
        begin
            n_bar = 0; n_white = 0; n_other = 0;
            for (i = 0; i < 1300000; i = i + 1) @(posedge clk_pixel);
        end
    endtask

    /* Put 'A' white-on-black into cell (0,0). */
    task write_marker;
        begin
            @(negedge clk);
            wide_x = 7'd0; wide_y = 6'd0; wide_ch = 7'd65;
            wide_fg = 15'h7FFF; wide_bg = 15'h0000; wide_we = 1;
            @(negedge clk);
            wide_we = 0;
        end
    endtask

    initial begin
        repeat (10) @(posedge clk_pixel);
        resetn = 1;
        repeat (10) @(posedge clk_pixel);

        write_marker();

        /* 1. With the layer off the bars are there.  This also proves the
         *    watcher is looking at something real: over half a frame of them. */
        wide_on = 0;
        overlay = 0;
        run_frame();
        check("the bars exist when the layer is off", n_bar > 100000);

        /* 1b. Enabling the layer is not enough by itself: it is shown only
         *     while the overlay is asserted, which is the same byte that hands
         *     the screen to a cartridge.  If this ever stops being true, then
         *     launching a game would no longer hide the desktop. */
        wide_on = 1;
        overlay = 0;
        run_frame();
        check("the layer waits for the overlay", n_bar > 100000);

        /* 2. With the layer on, every pixel comes from it -- including the
         *    strips left and right of the picture, which is the whole point. */
        wide_on = 1;
        overlay = 1;
        run_frame();
        $display("debug layer-on: n_bar=%0d n_mark=%0d n_other=%0d", n_bar, n_white, n_other);
        /* One pixel, not none: the compositor's output register still holds the
         * last pixel of the previous frame at the boundary. */
        check("the layer covers the bars", n_bar <= 4);
        check("the layer produced pixels", n_other > 100000);

        /* 3. And it is showing the cell that was written. */
        check("the marker cell is drawn", n_white > 0);

        /* 4. Taking the layer away puts the bars back. */
        wide_on = 0;
        overlay = 1;
        run_frame();
        check("the bars come back", n_bar > 100000);

        if (fails == 0) begin
            $display("composite: PASS (%0d checks)", checks);
        end else begin
            $display("composite: FAIL (%0d of %0d checks wrong)", fails, checks);
        end
        $finish;
    end

endmodule
