// SPDX-License-Identifier: GPL-3.0-only
// From aquasock/Tang-Phosphor 7cf9ede; TinyTang latches each complete pixel
// before its high byte so live terminal updates cannot split a RGB565 pixel.
//
// SSD1331 panel engine for the Pmod OLEDrgb.
//
// Extracted from the bring-up core so that the panel protocol exists exactly
// once while the source of pixels becomes a port.  The engine owns the
// module's power sequencing, the documented 44-byte initialisation list, the
// per-frame address window and the 6144-pixel stream, and it reports each
// frame boundary so a double-buffered store can swap banks only there.
//
// Pixel source contract: the engine presents px_x/px_y and samples px_data on
// the cycle it launches each pixel.  Between finishing one pixel and launching
// the next it waits two clocks (S_PIXWAIT), because the coordinate it advances
// has to travel through the scan mapper's registered mapping and the frame
// store's registered read before px_data describes the new pixel.  Sampling
// immediately would send the previous pixel's colour.
//
// The interface facts, the power-on sequence and every command byte come from
// the Pmod OLEDrgb Reference Manual (Rev B) and the SSD1331 datasheet.

module oled_panel #(
    parameter integer CLK_MHZ = 50,
    // SCK is two clocks per bit times this divider.  DIV=4 gives 160 ns from
    // 50 MHz and DIV=6 gives 162 ns from 74.25 MHz, both above the SSD1331's
    // 150 ns minimum clock cycle.
    parameter integer SPI_DIV = 4,
    parameter integer W       = 96,
    parameter integer H       = 64
) (
    input  logic        clk,
    input  logic        rst,

    // Pixel source (see the contract above).
    output logic [6:0]  px_x,
    output logic [5:0]  px_y,
    input  logic [15:0] px_data,

    // SSD1331 signals.
    output logic        cs_n,
    output logic        mosi,
    output logic        sck,
    output logic        dc,
    output logic        res_n,
    output logic        vccen,
    output logic        pmoden,

    // One-cycle pulse at each frame boundary, after the last pixel of a frame
    // and before the next frame's address window.
    output logic        frame_start,

    // One pulse per emitted pixel, at the launch of its high byte, so a
    // checksum can be told what this panel is actually being shown.  It lands
    // on the cycle after the launch, when the coordinate is unchanged and
    // px_data therefore still describes the same pixel.
    output logic        px_strobe,
    output logic [15:0] sampled_pixel
);
    localparam integer INIT_LEN  = 44;      // documented init bytes
    localparam integer FRAME_LEN = 6;       // window commands per frame

    // Delays, scaled from the 50 MHz reference the bring-up core was built on.
    localparam [23:0] D_20MS  = 24'd20_000  * CLK_MHZ[23:0];
    localparam [23:0] D_25MS  = 24'd25_000  * CLK_MHZ[23:0];
    localparam [23:0] D_100MS = 24'd100_000 * CLK_MHZ[23:0];
    localparam [23:0] D_10US  = 24'd10      * CLK_MHZ[23:0];

    // ------------------------------------------------------------------
    // SPI master.
    // ------------------------------------------------------------------
    logic       spi_start = 1'b0;
    logic       spi_busy, spi_done, spi_mosi, spi_sck, spi_dco;
    logic [7:0] spi_data = 8'h00;
    logic       spi_dc   = 1'b0;

    oled_spi #(.DIV(SPI_DIV)) spi (
        .clk(clk), .rst(rst), .start(spi_start), .data(spi_data), .dc(spi_dc),
        .busy(spi_busy), .done(spi_done), .sck(spi_sck), .mosi(spi_mosi), .dc_o(spi_dco)
    );

    assign mosi = spi_mosi;
    assign sck  = spi_sck;
    assign dc   = spi_dco;          // per-byte D/C from the SPI master

    // ------------------------------------------------------------------
    // The documented initialisation sequence, one byte per index.
    // ------------------------------------------------------------------
    function automatic logic [7:0] init_byte(input logic [5:0] i);
        case (i)
            6'd0:  init_byte = 8'hFD;   // unlock command register
            6'd1:  init_byte = 8'h12;   //   with key
            6'd2:  init_byte = 8'hAE;   // display off
            6'd3:  init_byte = 8'hA0;   // remap / colour depth
            6'd4:  init_byte = 8'h72;   //   65k colour
            6'd5:  init_byte = 8'hA1;   // display start line
            6'd6:  init_byte = 8'h00;
            6'd7:  init_byte = 8'hA2;   // display offset
            6'd8:  init_byte = 8'h00;
            6'd9:  init_byte = 8'hA4;   // normal display
            6'd10: init_byte = 8'hA8;   // multiplex ratio: 1 + 0x3F = 64 rows
            6'd11: init_byte = 8'h3F;
            6'd12: init_byte = 8'hAD;   // master configuration
            6'd13: init_byte = 8'h8E;
            6'd14: init_byte = 8'hB0;   // power saving mode
            6'd15: init_byte = 8'h0B;
            6'd16: init_byte = 8'hB1;   // phase length
            6'd17: init_byte = 8'h31;
            6'd18: init_byte = 8'hB3;   // clock ratio / oscillator frequency
            6'd19: init_byte = 8'hF0;
            6'd20: init_byte = 8'h8A;   // precharge speed, colour A
            6'd21: init_byte = 8'h64;
            6'd22: init_byte = 8'h8B;   // precharge speed, colour B
            6'd23: init_byte = 8'h78;
            6'd24: init_byte = 8'h8C;   // precharge speed, colour C
            6'd25: init_byte = 8'h64;
            6'd26: init_byte = 8'hBB;   // precharge voltage
            6'd27: init_byte = 8'h3A;
            6'd28: init_byte = 8'hBE;   // VCOMH deselect level
            6'd29: init_byte = 8'h3E;
            6'd30: init_byte = 8'h87;   // master current attenuation
            6'd31: init_byte = 8'h06;
            6'd32: init_byte = 8'h81;   // contrast, colour A
            6'd33: init_byte = 8'h91;
            6'd34: init_byte = 8'h82;   // contrast, colour B
            6'd35: init_byte = 8'h50;
            6'd36: init_byte = 8'h83;   // contrast, colour C
            6'd37: init_byte = 8'h7D;
            6'd38: init_byte = 8'h2E;   // disable scrolling
            6'd39: init_byte = 8'h25;   // clear window
            6'd40: init_byte = 8'h00;   //   column 0..
            6'd41: init_byte = 8'h00;   //   row 0..
            6'd42: init_byte = 8'h5F;   //   ..95
            6'd43: init_byte = 8'h3F;   //   ..63
            default: init_byte = 8'h00;
        endcase
    endfunction

    // Per-frame addressing: column window 0..95, then row window 0..63.
    function automatic logic [7:0] frame_byte(input logic [2:0] i);
        case (i)
            3'd0: frame_byte = 8'h15;   // set column address
            3'd1: frame_byte = 8'h00;
            3'd2: frame_byte = 8'h5F;   // 95
            3'd3: frame_byte = 8'h75;   // set row address
            3'd4: frame_byte = 8'h00;
            3'd5: frame_byte = 8'h3F;   // 63
            default: frame_byte = 8'h00;
        endcase
    endfunction

    // ------------------------------------------------------------------
    // Sequencer.
    // ------------------------------------------------------------------
    typedef enum logic [3:0] {
        S_POWER, S_RESLOW, S_RESHIGH, S_INIT, S_VCCEN,
        S_DISPON, S_FRAME, S_PIX, S_PIXWAIT
    } state_t;

    state_t      state = S_POWER;
    logic [23:0] delay = 24'd1_000_000;
    logic [5:0]  idx   = 6'd0;
    logic [6:0]  x     = 7'd0;
    logic [5:0]  y     = 6'd0;
    logic        hi    = 1'b1;
    logic        sent  = 1'b0;      // a byte has been handed to the SPI master

    logic cs_n_r   = 1'b1;
    logic res_n_r  = 1'b1;
    logic vccen_r  = 1'b0;
    logic pmoden_r = 1'b0;

    assign cs_n    = cs_n_r;
    assign res_n   = res_n_r;
    assign vccen   = vccen_r;
    assign pmoden  = pmoden_r;

    // The pixel source address is the current raster position.
    assign px_x = x;
    assign px_y = y;

    always_ff @(posedge clk) begin
        if (rst) begin
            state       <= S_POWER;
            delay       <= D_20MS;
            idx         <= 6'd0;
            x           <= 7'd0;
            y           <= 6'd0;
            hi          <= 1'b1;
            sent        <= 1'b0;
            cs_n_r      <= 1'b1;
            spi_start   <= 1'b0;
            spi_dc      <= 1'b0;
            frame_start <= 1'b0;
            px_strobe   <= 1'b0;
            sampled_pixel <= 16'h0000;
        end else begin
            spi_start   <= 1'b0;
            frame_start <= 1'b0;
            px_strobe   <= 1'b0;

            if (delay != 24'd0)
                delay <= delay - 24'd1;

            case (state)
                // Manual steps 1-4: D/C low, RES high, VCCEN low, PMODEN high,
                // then 20 ms for the 3.3 V rail to settle.
                S_POWER: begin
                    res_n_r  <= 1'b1;
                    vccen_r  <= 1'b0;
                    pmoden_r <= 1'b1;
                    if (delay == 24'd0) begin
                        res_n_r <= 1'b0;
                        delay   <= D_10US;
                        state   <= S_RESLOW;
                    end
                end

                // Manual step 5: RES low for at least 3 us.
                S_RESLOW: begin
                    if (delay == 24'd0) begin
                        res_n_r <= 1'b1;
                        delay   <= D_10US;
                        state   <= S_RESHIGH;
                    end
                end

                // Manual step 6: let the controller's reset complete.
                S_RESHIGH: begin
                    if (delay == 24'd0) begin
                        cs_n_r <= 1'b0;     // hold CS low across the sequence
                        idx    <= 6'd0;
                        sent   <= 1'b0;
                        state  <= S_INIT;
                    end
                end

                // Manual steps 7-28: the documented command list, DC low.
                S_INIT: begin
                    if (!spi_busy && !sent) begin
                        spi_data  <= init_byte(idx);
                        spi_dc    <= 1'b0;
                        spi_start <= 1'b1;
                        sent      <= 1'b1;
                    end else if (spi_done) begin
                        sent <= 1'b0;
                        if (idx == INIT_LEN - 1) begin
                            delay <= D_25MS;
                            state <= S_VCCEN;
                        end else begin
                            idx <= idx + 6'd1;
                        end
                    end
                end

                // Manual step 29: VCCEN high, wait 25 ms.
                S_VCCEN: begin
                    vccen_r  <= 1'b1;
                    pmoden_r <= 1'b1;
                    if (delay == 24'd0) begin
                        idx   <= 6'd0;
                        sent  <= 1'b0;
                        state <= S_DISPON;
                    end
                end

                // Manual step 30: display on, then leave 100 ms before drawing.
                S_DISPON: begin
                    if (!spi_busy && !sent) begin
                        spi_data  <= 8'hAF;
                        spi_dc    <= 1'b0;
                        spi_start <= 1'b1;
                        sent      <= 1'b1;
                    end else if (spi_done) begin
                        sent  <= 1'b0;
                        delay <= D_100MS;
                        idx   <= 6'd0;
                        state <= S_FRAME;
                    end
                end

                // Address window for this frame, then W x H pixels.
                S_FRAME: begin
                    if (delay == 24'd0) begin
                        if (!spi_busy && !sent) begin
                            spi_data  <= frame_byte(idx[2:0]);
                            spi_dc    <= 1'b0;
                            spi_start <= 1'b1;
                            sent      <= 1'b1;
                        end else if (spi_done) begin
                            sent <= 1'b0;
                            if (idx == FRAME_LEN - 1) begin
                                idx   <= 6'd0;
                                x     <= 7'd0;
                                y     <= 6'd0;
                                hi    <= 1'b1;
                                // Present (0,0) and then wait the same two clocks
                                // the inter-pixel path waits.  Going straight to
                                // S_PIX launched the frame's first pixel with the
                                // mapper and the store still holding the previous
                                // raster position, so it carried the last pixel
                                // of the frame that had just ended: on hardware
                                // that showed as the ramp's bottom-right colour
                                // appearing at the top-left corner, and it folded
                                // a one-pixel-wrong frame, which is why patterns
                                // 5 and 6 failed the mirror check while the flat
                                // fills and the orientation card could not see it.
                                delay <= 24'd2;
                                state <= S_PIXWAIT;
                            end else begin
                                idx <= idx + 6'd1;
                            end
                        end
                    end
                end

                // W*H pixels, high byte then low byte, DC high.
                S_PIX: begin
                    if (!spi_busy && !sent) begin
                        if (hi) sampled_pixel <= px_data;
                        spi_data  <= hi ? px_data[15:8] : sampled_pixel[7:0];
                        spi_dc    <= 1'b1;
                        spi_start <= 1'b1;
                        sent      <= 1'b1;
                        px_strobe <= hi;
                    end else if (spi_done) begin
                        sent <= 1'b0;
                        if (hi) begin
                            hi <= 1'b0;
                        end else begin
                            hi <= 1'b1;
                            if (x == W[6:0] - 7'd1 && y == H[5:0] - 6'd1) begin
                                frame_start <= 1'b1;
                                state       <= S_FRAME;
                            end else if (x == W[6:0] - 7'd1) begin
                                x     <= 7'd0;
                                y     <= y + 6'd1;
                                delay <= 24'd2;
                                state <= S_PIXWAIT;
                            end else begin
                                x     <= x + 7'd1;
                                delay <= 24'd2;
                                state <= S_PIXWAIT;
                            end
                        end
                    end
                end

                // Let the new coordinate reach the frame store before the next
                // pixel's bytes sample it.
                S_PIXWAIT: begin
                    if (delay == 24'd0)
                        state <= S_PIX;
                end

                default: state <= S_POWER;
            endcase
        end
    end
endmodule
