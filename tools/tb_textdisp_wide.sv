`timescale 1ns/1ps
//
// Verifies textdisp_wide without a board.
//
// A build says the layer elaborates and closes timing; it cannot tell a correct
// address decode from a plausible one, and this can.  The raster is driven the
// way hdmi.sv drives it -- free-running counters over the whole 1650x750 frame,
// blanking included -- and every visible pixel of a full frame is checked
// against the font, which exercises the cell addressing, the glyph row/column
// selection, the per-cell colours and the pipeline's alignment together.
//
// Alignment is the part a static probe cannot see.  The colour path is several
// clocks deep, so what the layer outputs at a given clock must be the pixel
// for the coordinate the raster is presenting *now*, not the one it presented
// three clocks ago.  An earlier version of this testbench held cx still for
// four clocks per check, which hides any latency at all; on hardware the
// layer was three pixels late, so pixels 0-2 of every line showed the tail of
// the previous line -- blanking, which reads cell 0 -- and cell (0,0)'s glyph
// was painted down the whole left edge.  Streaming across the line wrap is
// what catches that.
//
// The write-port guard is checked too: a column or row past the grid must be
// dropped rather than wrap into another cell.
//
//   tools/test_textdisp_wide.sh
//
module tb_textdisp_wide;

    localparam integer FW = 1650;    // hdmi.sv frame_width for VIC 4 (720p60)
    localparam integer FH = 750;
    localparam integer SW = 1280;
    localparam integer SH = 720;
    localparam integer COLS = 80;
    localparam integer ROWS = 45;

    reg clk = 0, hclk = 0;
    reg we = 0;
    reg [6:0]  wx = 0;
    reg [5:0]  wy = 0;
    reg [6:0]  wch = 0;
    reg [14:0] wfg = 0, wbg = 0;
    reg [10:0] cx = 0;
    reg [9:0]  cy = 0;
    reg        run = 0;
    wire [14:0] color;

    textdisp_wide dut (
        .clk(clk), .hclk(hclk), .we(we),
        .wx(wx), .wy(wy), .wch(wch), .wfg(wfg), .wbg(wbg),
        .cx(cx), .cy(cy),
        .frame_width(11'(FW)), .frame_height(10'(FH)),
        .color(color)
    );

    always #5.0 clk = ~clk;      // ~100 MHz write clock
    always #6.73 hclk = ~hclk;   // ~74.25 MHz pixel clock

    // The raster, exactly as hdmi.sv counts it.
    always @(posedge hclk) if (run) begin
        cx <= (cx == FW - 1) ? 11'd0 : cx + 11'd1;
        cy <= (cx == FW - 1) ? ((cy == FH - 1) ? 10'd0 : cy + 10'd1) : cy;
    end

    // What the grid should hold.
    reg [6:0]  ch_m [0:COLS*ROWS-1];
    reg [14:0] fg_m [0:COLS*ROWS-1];
    reg [14:0] bg_m [0:COLS*ROWS-1];

    integer fails  = 0;
    integer checks = 0;
    integer i, x, y, n;

    task write_cell(input [6:0] x, input [5:0] y, input [6:0] ch,
                    input [14:0] fg, input [14:0] bg);
        begin
            @(negedge clk);
            wx = x; wy = y; wch = ch; wfg = fg; wbg = bg; we = 1;
            @(negedge clk);
            we = 0;
        end
    endtask

    function [14:0] expect_px(input integer px, input integer py);
        integer c;
        reg [7:0] bits;
        begin
            c = (py >> 4) * COLS + (px >> 4);
            bits = FONT[ch_m[c]][(py >> 1) & 7];
            expect_px = bits[(px >> 1) & 7] ? fg_m[c] : bg_m[c];
        end
    endfunction

    initial begin
        /* Every cell distinct.  The foreground carries bit 14 and the
         * background never does, so the two can never be confused, and both
         * encode the cell's position, so a pixel from the wrong cell is wrong
         * in colour even where the glyph bits happen to agree.  Cell (0,0) is
         * 'b', the glyph that was on hardware's top-left when the leak was
         * photographed; '*' and '_' are the font's only glyphs with ink in
         * column 7, and they go at the line ends where an off-by-a-few at the
         * wrap would show. */
        for (y = 0; y < ROWS; y = y + 1) begin
            for (x = 0; x < COLS; x = x + 1) begin
                i = y * COLS + x;
                ch_m[i] = 7'(33 + ((x * 7 + y * 13) % 94));
                fg_m[i] = 15'h4000 | 15'(i);
                bg_m[i] = 15'((x * 37 + y * 11) & 15'h3FFF);
            end
            ch_m[y * COLS + 0]        = (y == 0) ? 7'd98 : 7'd42;   // 'b', then '*'
            ch_m[y * COLS + COLS - 1] = 7'd95;                       // '_'
        end
        for (i = 0; i < COLS * ROWS; i = i + 1)
            write_cell(7'(i % COLS), 6'(i / COLS), ch_m[i], fg_m[i], bg_m[i]);

        // Off-grid writes must be dropped, not wrapped into (0,1) or (0,0).
        write_cell(7'd80,  6'd0,  7'd35, 15'h7FFF, 15'h7FFF);
        write_cell(7'd127, 6'd44, 7'd35, 15'h7FFF, 15'h7FFF);
        write_cell(7'd0,   6'd45, 7'd35, 15'h7FFF, 15'h7FFF);

        /* Start a few pixels before the end of the last line of a frame, so
         * the pipeline is full of real raster -- blanking -- by the time the
         * first visible pixel is due, which is exactly the hardware's
         * situation at every line start. */
        @(negedge hclk);
        cx = 11'(FW - 8);
        cy = 10'(FH - 1);
        run = 1;

        /* After each pixel-clock edge the raster holds the coordinate the
         * compositor will register on the next edge, and `color` must be that
         * coordinate's pixel: the compositor's rgb register is the same one
         * clock deep as hdmi.sv's video_data_period, so any further lag is
         * lag on screen.  One full frame, then line 0 again. */
        for (n = 0; n < FW * FH + FW + 8; n = n + 1) begin
            @(posedge hclk);
            #1;
            if (n >= 3 && cx < SW && cy < SH) begin
                checks = checks + 1;
                if (color !== expect_px(cx, cy)) begin
                    fails = fails + 1;
                    if (fails <= 8)
                        $display("FAIL out(%0d,%0d) cell(%0d,%0d) got %04x want %04x",
                                 cx, cy, cx >> 4, cy >> 4, color, expect_px(cx, cy));
                end
            end
        end

        if (fails == 0)
            $display("textdisp_wide: PASS (%0d pixel checks)", checks);
        else
            $display("textdisp_wide: FAIL (%0d of %0d pixel checks wrong)", fails, checks);

        $finish;
    end

endmodule
