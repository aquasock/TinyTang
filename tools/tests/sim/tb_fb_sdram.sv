`timescale 1ns/1ps
//
// The SDRAM framebuffer (fpga/desktop/fb_sdram.sv) against a timing-checking
// model of the part (sdram_w9825_model.sv): pixels written through the link's
// clock, a 1280x720 raster scanned out at 1:1, and every visible pixel of a
// whole frame compared with what was written -- or black, where nothing was.
//
// Positions are where a framebuffer goes wrong: the corners, both sides of every
// SDRAM row boundary in a line (x = 511/512 and 1023/1024), the last line, a
// whole line of gradient, and a scatter of pseudo-random points.  The model
// counts every command issued early by the datasheet's own numbers, and the
// controller reports whether a line was ever shown before its fetch finished.
//
//   tools/tests/test_fb_sdram.sh
//
module tb_fb_sdram #(
    parameter integer READ_DELAY = 2
);

    reg clk_w = 0, clk = 0;
    always #23.26 clk_w = ~clk_w;          // ~21.49 MHz, the link's clock
    always #6.734 clk   = ~clk;            // ~74.25 MHz, the pixel clock
    wire sdram_clk = ~clk;                 // the part is clocked on the inverse

    reg resetn = 0;

    // The raster, with hdmi.sv's 720p totals.
    localparam integer FW = 1650, FH = 750;
    reg [10:0] cx = 0;
    reg [9:0]  cy = 0;
    always @(posedge clk) begin
        if (cx == FW - 1) begin
            cx <= 0;
            cy <= (cy == FH - 1) ? 10'd0 : cy + 10'd1;
        end else cx <= cx + 11'd1;
    end

    reg        we = 0;
    reg [10:0] wx = 0;
    reg [9:0]  wy = 0;
    reg [15:0] wdata = 0;
    wire [23:0] rgb;
    wire [12:0] a;
    wire [1:0]  ba, dqm;
    wire cs_n, ras_n, cas_n, we_n;
    wire [15:0] dq;
    wire ready, underrun, dropped;
    wire [10:0] fetch_max;

    fb_sdram #(.INIT_WAIT(300), .READ_DELAY(READ_DELAY)) dut (
        .clk_w(clk_w), .we(we), .wx(wx), .wy(wy), .wdata(wdata),
        .clk(clk), .resetn(resetn), .cx(cx), .cy(cy),
        .frame_width(11'(FW)), .frame_height(10'(FH)), .rgb(rgb),
        .sd_a(a), .sd_ba(ba), .sd_cs_n(cs_n), .sd_ras_n(ras_n), .sd_cas_n(cas_n),
        .sd_we_n(we_n), .sd_dqm(dqm), .sd_dq(dq),
        .ready(ready), .underrun(underrun), .dropped(dropped), .fetch_max(fetch_max)
    );

    sdram_w9825_model #(.SIZE_WORDS(2 * 1024 * 1024)) part (
        .clk(sdram_clk), .A(a), .BA(ba), .nCS(cs_n), .nRAS(ras_n), .nCAS(cas_n),
        .nWE(we_n), .DQM(dqm), .DQ(dq)
    );

    /* ------------------------------------------------------------ expected */
    reg [15:0] want [int];                 // key y * 2048 + x
    integer nwritten = 0;

    task put(input integer x, input integer y, input [15:0] v);
        begin
            @(posedge clk_w);
            we <= 1; wx <= x; wy <= y; wdata <= v;
            @(posedge clk_w);
            we <= 0;
            repeat (18) @(posedge clk_w);  // about 1 in 20: well above UART speed
            want[y * 2048 + x] = v;
            nwritten = nwritten + 1;
        end
    endtask

    function automatic [23:0] widen(input [15:0] p);
        widen = {p[15:11], p[15:13], p[10:5], p[10:9], p[4:0], p[4:2]};
    endfunction

    /* -------------------------------------------------------------- compare */
    /* fb_sdram's rgb is the pixel for the raster position it is presented with:
     * its look-ahead is the whole pipeline.  (desktop_hdmi adds one register
     * after it, which is why the plane bench compares one position late; this
     * bench has no such stage.)  Comparing one position late here hid an
     * off-by-one in the capture behind an equal and opposite one. */
    wire [10:0] pcx = cx;
    wire [9:0]  pcy = cy;
    reg         comparing = 0;
    integer     compared = 0, mismatches = 0;
    always @(posedge clk) begin
        if (comparing && pcx < 1280 && pcy < 720) begin : cmp
            reg [23:0] exp;
            int key;
            key = pcy * 2048 + pcx;
            exp = want.exists(key) ? widen(want[key]) : 24'h000000;
            compared = compared + 1;
            if (rgb !== exp) begin
                mismatches = mismatches + 1;
                if (mismatches <= 10)
                    $display("MISMATCH (%0d,%0d): got %06h want %06h", pcx, pcy, rgb, exp);
            end
        end
    end

    integer i, x, y, seed, ref0, cyc0;
    longint need;
    initial begin
        repeat (20) @(posedge clk);
        resetn = 1;
        wait (ready);

        // Corners and the last line.
        put(0, 0, 16'hF800);       put(1279, 0, 16'h07E0);
        put(0, 719, 16'h001F);     put(1279, 719, 16'hFFFF);
        put(640, 360, 16'hFFE0);
        // Both sides of each row boundary within a line.
        for (y = 1; y < 720; y = y + 359) begin
            put(511, y, 16'h1234);  put(512, y, 16'h5678);
            put(1023, y, 16'h9ABC); put(1024, y, 16'hDEF0);
        end
        // A whole line.
        for (x = 0; x < 1280; x = x + 1) put(x, 100, 16'(x * 51 + 7));
        // A scatter.
        seed = 1;
        for (i = 0; i < 300; i = i + 1) begin
            seed = (seed * 1103515245 + 12345) & 32'h7FFFFFFF;
            put((seed >> 8) % 1280, (seed >> 4) % 720, 16'(seed));
        end
        // Off the picture: dropped, not wrapped.
        @(posedge clk_w); we <= 1; wx <= 1280; wy <= 5; wdata <= 16'hAAAA;
        @(posedge clk_w); wx <= 3; wy <= 720;
        @(posedge clk_w); we <= 0;

        // Drain, then compare one whole frame from its start.
        repeat (20000) @(posedge clk);
        wait (cx == 0 && cy == 0);
        ref0 = part.refreshes;
        cyc0 = part.cycle;
        @(posedge clk);
        comparing = 1;
        wait (cy == 10'd720);
        comparing = 0;
        wait (cx == 0 && cy == 0);

        $display("fb_sdram: READ_DELAY %0d, %0d pixels written, %0d compared, %0d mismatches",
                 READ_DELAY, nwritten, compared, mismatches);
        $display("fb_sdram: %0d SDRAM timing violations, underrun %0d, writes dropped %0d, longest fetch %0d of %0d clocks",
                 part.violations, underrun, dropped, fetch_max, FW);
        // 8192 refreshes per 64 ms, and no gap longer than eight intervals.
        // 8192 refreshes per 64 ms is one per 64e6/8192 ns; a clock is 13.468 ns.
        need = longint'(part.cycle - cyc0) * 64'd13468 * 64'd8192 / 64'd64_000_000_000;
        $display("fb_sdram: %0d refreshes in a frame (need %0d), longest gap %0d clocks (limit %0d)",
                 part.refreshes - ref0, need, part.max_ref_gap, 8 * 579);
        if (mismatches == 0 && compared == 1280 * 720 && part.violations == 0 && !underrun && !dropped &&
            longint'(part.refreshes - ref0) >= need && part.max_ref_gap <= 8 * 579)
            $display("fb_sdram: PASS");
        else
            $display("fb_sdram: FAIL");
        $finish;
    end

endmodule
