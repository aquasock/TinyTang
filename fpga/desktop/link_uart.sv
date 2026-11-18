// TinyTang desktop link UART: the BL616 wire at 2, 5 or 10 Mbaud.
//
// The bit timing runs on the 50 MHz crystal (sys_clk), where each rate is a
// whole number of clocks -- 25, 10 and 5 -- and so are the BL616's (40 MHz
// over 20, 8 and 4).  Nothing is fractional at either end, and the pixel clock,
// the core's tightest, carries none of it.  The link logic stays on `clk`
// (21.49 MHz); bytes cross by toggle handshakes:
//
//   rx  each received byte is held in rx_buf and announced by flipping
//       rx_tog.  The next byte cannot land for a whole character, 50 crystal
//       clocks at 10 Mbaud, against about 7 for the crossing.
//   tx  tx_start (one `clk` pulse while tx_busy is low) takes tx_data and flips
//       tx_req; the crystal side sends the byte and copies tx_req into tx_ack
//       after the stop bit.  tx_busy is tx_req != tx_ack as seen on `clk`, so
//       it covers the whole character, stop bit included -- which is what a
//       rate change waits for.
//
// The rate is `rate` from the `clk` side (0: 2 Mbaud, 1: 5, 2: 10).  It is
// taken at the start of each character, so a change between characters is
// clean.  The receiver samples the middle of each bit: a start edge seen at
// crystal clock t is sampled at t + (div-1)/2, then every div clocks.
//
// The way back to 2 Mbaud.  While the rate is above 2 Mbaud, a line held low
// for LOW_CLOCKS (3 us) is not a character at that rate -- the longest low
// run a 5 Mbaud character has is 1.8 us -- so it is taken as the BL616 talking
// at 2 Mbaud, which a single 0x00 does (4.5 us low), and `fallback` pulses.
// The link drops to 2 Mbaud on it.  So whatever the BL616 believes after a
// reset or a failed switch, it can always get the core back by sending zeros
// at 2 Mbaud, which every core's frame parser ignores between frames.
//
// SPDX-License-Identifier: GPL-3.0-only

module link_uart #(
    parameter integer LOW_CLOCKS = 150      // 3 us of crystal clocks
) (
    input  logic       sys_clk,             // 50 MHz crystal
    input  logic       clk,                 // the link's logic clock
    // `clk` side
    input  logic [1:0] rate,
    output logic [7:0] rx_data,
    output logic       rx_valid,            // one `clk` pulse per byte
    input  logic [7:0] tx_data,
    input  logic       tx_start,
    output logic       tx_busy,
    output logic       fallback,            // one `clk` pulse
    output logic       framing_error,       // one `clk` pulse per bad stop bit
    // pins
    input  logic       rxd,
    output logic       txd
);
    function automatic [4:0] bit_clocks(input [1:0] r);
        case (r)
            2'd1:    bit_clocks = 5'd10;
            2'd2:    bit_clocks = 5'd5;
            default: bit_clocks = 5'd25;
        endcase
    endfunction

    // ------------------------------------------------------- crystal side
    (* ASYNC_REG = "TRUE" *) logic [1:0] rate_m = 2'd0, rate_s = 2'd0;
    (* ASYNC_REG = "TRUE" *) logic rx_m = 1'b1, rx_s = 1'b1;
    always_ff @(posedge sys_clk) begin
        rate_m <= rate;
        rate_s <= rate_m;
        rx_m   <= rxd;
        rx_s   <= rx_m;
    end

    // Receiver
    localparam logic [1:0] R_IDLE = 2'd0, R_DATA = 2'd1, R_STOP = 2'd2, R_HIGH = 2'd3;
    logic [1:0] r_state = R_IDLE;
    logic [4:0] r_div = 5'd25;
    logic [4:0] r_cnt = 5'd0;
    logic [3:0] r_bit = 4'd0;           // 0: start bit, 1..8: data
    logic [7:0] r_shift = 8'd0;
    logic [7:0] rx_buf = 8'd0;
    logic       rx_tog = 1'b0;
    logic       fe_tog = 1'b0;
    always_ff @(posedge sys_clk) begin
        if (r_state != R_IDLE && r_cnt != 5'd0)
            r_cnt <= r_cnt - 5'd1;
        case (r_state)
            R_IDLE: if (!rx_s) begin
                r_div   <= bit_clocks(rate_s);
                r_cnt   <= (bit_clocks(rate_s) - 5'd1) / 5'd2 - 5'd1;
                r_bit   <= 4'd0;
                r_state <= R_DATA;
            end
            R_DATA: if (r_cnt == 5'd0) begin
                r_cnt <= r_div - 5'd1;
                if (r_bit == 4'd0) begin
                    if (rx_s) r_state <= R_IDLE;    // a glitch, not a start bit
                end else
                    r_shift <= {rx_s, r_shift[7:1]};
                if (r_bit == 4'd8) r_state <= R_STOP;
                r_bit <= r_bit + 4'd1;
            end
            R_STOP: if (r_cnt == 5'd0) begin
                if (rx_s) begin
                    rx_buf  <= r_shift;
                    rx_tog  <= ~rx_tog;
                    r_state <= R_IDLE;
                end else begin
                    fe_tog  <= ~fe_tog;             // no stop bit: dropped
                    r_state <= R_HIGH;
                end
            end
            R_HIGH: if (rx_s) r_state <= R_IDLE;
        endcase
    end

    // The long-low detector.
    logic [7:0] low_cnt = 8'd0;
    logic       fb_tog = 1'b0;
    always_ff @(posedge sys_clk) begin
        if (rx_s)
            low_cnt <= 8'd0;
        else if (low_cnt != 8'hFF)
            low_cnt <= low_cnt + 8'd1;
        if (!rx_s && low_cnt == LOW_CLOCKS[7:0] - 8'd1 && rate_s != 2'd0)
            fb_tog <= ~fb_tog;
    end

    // Transmitter
    (* ASYNC_REG = "TRUE" *) logic req_m = 1'b0, req_s = 1'b0;
    logic [7:0] tx_hold = 8'd0;          // `clk` side, stable while busy
    logic       tx_req = 1'b0;
    logic       tx_ack = 1'b0;
    logic       t_busy = 1'b0;
    logic [4:0] t_div = 5'd25;
    logic [4:0] t_cnt = 5'd0;
    logic [3:0] t_bit = 4'd0;
    logic [9:0] t_shift = 10'h3FF;
    always_ff @(posedge sys_clk) begin
        req_m <= tx_req;
        req_s <= req_m;
        if (!t_busy) begin
            if (req_s != tx_ack) begin
                t_busy  <= 1'b1;
                t_div   <= bit_clocks(rate_s);
                t_cnt   <= bit_clocks(rate_s) - 5'd1;
                t_bit   <= 4'd0;
                t_shift <= {1'b1, tx_hold, 1'b0};
            end
        end else if (t_cnt != 5'd0) begin
            t_cnt <= t_cnt - 5'd1;
        end else begin
            t_cnt   <= t_div - 5'd1;
            t_shift <= {1'b1, t_shift[9:1]};
            t_bit   <= t_bit + 4'd1;
            if (t_bit == 4'd9) begin            // the stop bit has had its time
                t_busy <= 1'b0;
                tx_ack <= req_s;
            end
        end
    end
    always_ff @(posedge sys_clk) txd <= t_busy ? t_shift[0] : 1'b1;

    // ----------------------------------------------------------- `clk` side
    (* ASYNC_REG = "TRUE" *) logic [2:0] rx_sync = 3'd0, fe_sync = 3'd0, fb_sync = 3'd0;
    (* ASYNC_REG = "TRUE" *) logic ack_m = 1'b0, ack_s = 1'b0;
    always_ff @(posedge clk) begin
        rx_sync <= {rx_sync[1:0], rx_tog};
        fe_sync <= {fe_sync[1:0], fe_tog};
        fb_sync <= {fb_sync[1:0], fb_tog};
        ack_m   <= tx_ack;
        ack_s   <= ack_m;
        rx_valid      <= rx_sync[2] ^ rx_sync[1];
        framing_error <= fe_sync[2] ^ fe_sync[1];
        fallback      <= fb_sync[2] ^ fb_sync[1];
        if (rx_sync[2] ^ rx_sync[1]) rx_data <= rx_buf;
        if (tx_start && !tx_busy) begin
            tx_hold <= tx_data;
            tx_req  <= ~tx_req;
        end
    end
    assign tx_busy = tx_req != ack_s;
endmodule
