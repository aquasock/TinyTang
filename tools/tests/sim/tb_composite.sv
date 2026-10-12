`timescale 1ns/1ps
//
// Exercises the desktop compositor's gating: the layer fills the whole visible
// raster when it is enabled *and* the overlay is up, and the output is black
// otherwise.
//
// The overlay byte (command 0x08) is what hides the desktop when the BL616
// hands the screen to another core, so the layer must wait for it.
//
// The check is position-independent: every cell is painted a background that
// is never black, and the watcher counts black pixels inside the visible
// 1280x720 raster.  With the layer shown there must be none; hidden, they must
// all be black.
//
//   tools/tests/test_composite.sh
//
module tb_composite;

    reg clk = 0, clk_pixel = 0, clk_5x_pixel = 0;
    always #23.26 clk = ~clk;              // ~21.49 MHz main logic clock
    always #6.73  clk_pixel = ~clk_pixel;  // ~74.25 MHz pixel clock
    always #1.346 clk_5x_pixel = ~clk_5x_pixel;

    reg resetn = 0;

    reg        overlay = 0;
    reg        wide_we = 0;
    reg [6:0]  wide_x  = 0;
    reg [5:0]  wide_y  = 0;
    reg [6:0]  wide_ch = 0;
    reg [14:0] wide_fg = 0;
    reg [14:0] wide_bg = 0;
    reg        wide_on = 0;

    wire [10:0] cx;
    wire [9:0]  cy;
    wire [23:0] rgb;
    wire tmds_clk_n, tmds_clk_p;
    wire [2:0] tmds_d_n, tmds_d_p;

    desktop_hdmi dut (
        .clk(clk), .resetn(resetn), .overlay(overlay),
        .wide_we(wide_we), .wide_x(wide_x), .wide_y(wide_y), .wide_ch(wide_ch),
        .wide_fg(wide_fg), .wide_bg(wide_bg), .wide_on(wide_on),
        .desktop_cx(cx), .desktop_cy(cy), .desktop_rgb(rgb),
        .clk_pixel(clk_pixel), .clk_5x_pixel(clk_5x_pixel),
        .tmds_clk_n(tmds_clk_n), .tmds_clk_p(tmds_clk_p),
        .tmds_d_n(tmds_d_n), .tmds_d_p(tmds_d_p)
    );

    /* 15-bit BGR5 expands to 24-bit by shifting each 5-bit field up three and
     * leaving the bottom three bits zero, so all-ones is 0xF8F8F8. */
    localparam [23:0] MARK = 24'hF8F8F8;   /* the marker glyph's foreground */

    /* rgb is registered one pixel behind cx/cy. */
    reg visible_r = 0;
    always @(posedge clk_pixel) visible_r <= cx < 11'd1280 && cy < 10'd720;

    integer n_black = 0, n_mark = 0, n_other = 0;
    always @(posedge clk_pixel) begin
        if (resetn && visible_r) begin
            if (rgb == 24'h000000)  n_black = n_black + 1;
            else if (rgb == MARK)   n_mark  = n_mark + 1;
            else                    n_other = n_other + 1;
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
            n_black = 0; n_mark = 0; n_other = 0;
            for (i = 0; i < 1300000; i = i + 1) @(posedge clk_pixel);
        end
    endtask

    /* Paint every cell a space on dark blue, then 'A' white at (0,0). */
    task paint;
        integer i;
        begin
            for (i = 0; i < 80 * 45; i = i + 1) begin
                @(negedge clk);
                wide_x = 7'(i % 80); wide_y = 6'(i / 80); wide_ch = 7'd32;
                wide_fg = 15'h7FFF; wide_bg = 15'h4000; wide_we = 1;
                @(negedge clk);
                wide_we = 0;
            end
            @(negedge clk);
            wide_x = 7'd0; wide_y = 6'd0; wide_ch = 7'd65;
            wide_fg = 15'h7FFF; wide_bg = 15'h4000; wide_we = 1;
            @(negedge clk);
            wide_we = 0;
        end
    endtask

    localparam integer VISIBLE = 1280 * 720;

    initial begin
        repeat (10) @(posedge clk_pixel);
        resetn = 1;
        repeat (10) @(posedge clk_pixel);
        paint();

        /* 1. Layer off: black. */
        wide_on = 0; overlay = 0;
        run_frame();
        check("black when the layer is off", n_black >= VISIBLE);

        /* 2. Enabling the layer is not enough: it waits for the overlay. */
        wide_on = 1; overlay = 0;
        run_frame();
        check("the layer waits for the overlay", n_black >= VISIBLE);

        /* 3. Both: every visible pixel comes from the layer. */
        wide_on = 1; overlay = 1;
        run_frame();
        /* A pixel or so at the switch-over frame boundary is allowed. */
        check("the layer covers the raster", n_black <= 4);
        check("the layer produced pixels", n_other > 100000);
        check("the marker cell is drawn", n_mark > 0);

        /* 4. Overlay down hides it again. */
        wide_on = 1; overlay = 0;
        run_frame();
        check("the overlay hides it again", n_black >= VISIBLE);

        if (fails == 0) begin
            $display("composite: PASS (%0d checks)", checks);
        end else begin
            $display("composite: FAIL (%0d of %0d checks wrong)", fails, checks);
        end
        $finish;
    end

endmodule
