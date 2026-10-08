// SPDX-License-Identifier: GPL-3.0-only
// From aquasock/Tang-Phosphor 7cf9ede; retained unchanged.
//
// SPI master for the Pmod OLEDrgb's SSD1331 controller.
//
// The SSD1331 is write-only and uses SPI mode 3: SCK idles high, and the
// controller samples MOSI on the rising edge.  Mode 3 launches data on the
// falling edge, so each bit is presented while SCK is low and SCK is then
// returned high for the controller to sample it.  DC is held for the whole
// byte (0 = command, 1 = data).
//
// Chip select is deliberately not driven here.  The datasheet requires CS low
// across a command and its parameters (tCSS/tCSH of 75/60 ns), so the
// sequencer above asserts CS once for a whole sequence rather than per byte.
//
// One SCK period is 2*DIV clocks.  DIV=4 gives 160 ns (6.25 MHz) from the
// 50 MHz board clock, inside the datasheet's 150 ns minimum clock cycle.

module oled_spi #(
    parameter integer DIV = 4
) (
    input  logic       clk,
    input  logic       rst,
    input  logic       start,    // pulse: send `data` with `dc`
    input  logic [7:0] data,
    input  logic       dc,       // 0 = command, 1 = data
    output logic       busy,     // asserted from an accepted start until done
    output logic       done,     // one-cycle pulse when the byte is complete
    output logic       sck,
    output logic       mosi,
    output logic       dc_o
);
    localparam integer HALF   = DIV;        // clocks per SCK half-period
    localparam integer PERIOD = 2 * DIV;

    logic [7:0] shift;
    logic [3:0] cnt;                        // 0 .. PERIOD-1 within one bit
    logic [2:0] bit_i;                      // 0 .. 7

    // Mode 3: SCK high while idle and during the second half of each bit.
    assign sck  = !busy || (cnt >= HALF[3:0]);
    assign mosi = shift[7];

    always_ff @(posedge clk) begin
        if (rst) begin
            shift <= 8'h00;
            cnt   <= 4'd0;
            bit_i <= 3'd0;
            busy  <= 1'b0;
            done  <= 1'b0;
            dc_o  <= 1'b0;
        end else begin
            done <= 1'b0;

            if (!busy) begin
                if (start) begin
                    shift <= data;
                    cnt   <= 4'd0;
                    bit_i <= 3'd0;
                    busy  <= 1'b1;
                    dc_o  <= dc;
                end
            end else if (cnt == PERIOD[3:0] - 4'd1) begin
                cnt <= 4'd0;
                if (bit_i == 3'd7) begin
                    busy <= 1'b0;
                    done <= 1'b1;
                end else begin
                    bit_i <= bit_i + 3'd1;
                    shift <= {shift[6:0], 1'b0};
                end
            end else begin
                cnt <= cnt + 4'd1;
            end
        end
    end
endmodule
