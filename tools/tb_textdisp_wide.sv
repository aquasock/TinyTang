`timescale 1ns/1ps
//
// Verifies textdisp_wide without a board.
//
// A build says the layer elaborates and closes timing; it cannot tell a correct
// address decode from a plausible one, and this can.  Every output pixel of a
// whole 16x16 cell block is checked against the font, which exercises the cell
// addressing, the glyph row/column selection and the per-cell colours together.
// The write-port guard is checked too: a column past the grid must be dropped
// rather than wrap into the next row.
//
//   tools/test_textdisp_wide.sh
//
module tb_textdisp_wide;

    reg clk = 0, hclk = 0;
    reg we = 0;
    reg [6:0]  wx = 0;
    reg [5:0]  wy = 0;
    reg [6:0]  wch = 0;
    reg [14:0] wfg = 0, wbg = 0;
    reg [10:0] cx = 0;
    reg [9:0]  cy = 0;
    wire [14:0] color;

    textdisp_wide dut (
        .clk(clk), .hclk(hclk), .we(we),
        .wx(wx), .wy(wy), .wch(wch), .wfg(wfg), .wbg(wbg),
        .cx(cx), .cy(cy), .color(color)
    );

    always #5.0 clk = ~clk;      // ~100 MHz write clock
    always #6.73 hclk = ~hclk;   // ~74.25 MHz pixel clock

    integer fails  = 0;
    integer checks = 0;
    integer osrow, oscol;
    reg [7:0]  osbits;
    reg [14:0] osexp;

    task write_cell(input [6:0] x, input [5:0] y, input [6:0] ch,
                    input [14:0] fg, input [14:0] bg);
        begin
            @(negedge clk);
            wx = x; wy = y; wch = ch; wfg = fg; wbg = bg; we = 1;
            @(negedge clk);
            we = 0;
        end
    endtask

    // Check one whole 16x16 output block against the font.
    task check_cell(input [6:0] cxi, input [5:0] cyi, input [6:0] ch,
                    input [14:0] fg, input [14:0] bg);
        integer ox, oy, row, col;
        reg [7:0] bits;
        reg [14:0] exp;
        begin
            for (oy = 0; oy < 16; oy = oy + 1) begin
                for (ox = 0; ox < 16; ox = ox + 1) begin
                    cx = cxi * 16 + ox;
                    cy = cyi * 16 + oy;
                    repeat (4) @(posedge hclk);
                    #1;
                    row  = (oy >> 1) & 7;
                    col  = (ox >> 1) & 7;
                    bits = FONT[ch][row];
                    exp  = bits[col] ? fg : bg;
                    checks = checks + 1;
                    if (color !== exp) begin
                        fails = fails + 1;
                        if (fails < 8)
                            $display("FAIL cell(%0d,%0d) out(%0d,%0d) got %04x want %04x",
                                     cxi, cyi, ox, oy, color, exp);
                    end
                end
            end
        end
    endtask

    initial begin
        repeat (10) @(posedge hclk);

        // A mid-grid cell, the last cell of the grid, and the first.
        write_cell(7'd5,  6'd3,  7'd65, 15'h1234, 15'h0ABC);  // 'A'
        write_cell(7'd79, 6'd44, 7'd90, 15'h7FFF, 15'h0001);  // 'Z' bottom-right
        write_cell(7'd0,  6'd0,  7'd48, 15'h0001, 15'h7FFF);  // '0' top-left

        // The guard: column 80 does not exist.  Without the guard this lands
        // on index 80, which is cell (0,1).
        write_cell(7'd0,  6'd1,  7'd66, 15'h2222, 15'h0333);  // 'B' at (0,1)
        write_cell(7'd80, 6'd0,  7'd81, 15'h3FFF, 15'h0000);  // 'Q' - must drop

        repeat (10) @(posedge hclk);

        check_cell(7'd5,  6'd3,  7'd65, 15'h1234, 15'h0ABC);
        check_cell(7'd79, 6'd44, 7'd90, 15'h7FFF, 15'h0001);
        check_cell(7'd0,  6'd0,  7'd48, 15'h0001, 15'h7FFF);
        check_cell(7'd0,  6'd1,  7'd66, 15'h2222, 15'h0333);  // survived the drop

        /* The raster's counters run past the visible area -- the frame is
         * 1650x750 and the screen is the top-left 1280x720 -- so an unclamped
         * cell address reaches 3783 of a 3600-cell store: a read past the end
         * of the array for the whole of blanking.  It must clamp, and cell 0
         * is what it clamps to.  Cell (0,0) holds '0' (48) with foreground
         * 0x0001 on background 0x7FFF. */
        begin
            cx = 11'd1600;
            cy = 10'd700;
            repeat (4) @(posedge hclk);
            #1;
            osrow  = (700 >> 1) & 7;
            oscol  = (1600 >> 1) & 7;
            osbits = FONT[7'd48][osrow];
            osexp  = osbits[oscol] ? 15'h0001 : 15'h7FFF;
            checks = checks + 1;
            if (color !== osexp) begin
                fails = fails + 1;
                $display("FAIL: an off-screen read did not fall back to cell 0 (got %04x want %04x)",
                         color, osexp);
            end
        end

        if (fails == 0)
            $display("textdisp_wide: PASS (%0d pixel checks)", checks);
        else
            $display("textdisp_wide: FAIL (%0d of %0d pixel checks wrong)", fails, checks);

        $finish;
    end

endmodule
