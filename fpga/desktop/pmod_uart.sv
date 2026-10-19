// TinyTang PMOD UART: an 8N1 byte link on a socket's lower row.
// SPDX-License-Identifier: GPL-3.0-only
//
// Personality 7 (`i2c_uart`) gives a socket an I2C bus on the upper row and
// this UART on the lower row.  The register window is in the control clock;
// the shifters run in the 50 MHz crystal clock.  A byte crosses each way on a
// held-data toggle -- the handshake desktop_pmod already uses for the I2C
// command and result -- so there is no dual-clock FIFO and no shared memory.
//
// The baud divisor is CLK_HZ / BAUD rounded to the nearest clock: at 50 MHz
// and 115200 that is 434 clocks, 0.007 % from exact.  The receiver samples at
// half a bit after the start edge and then every full bit, so a small divisor
// error does not accumulate within a byte.
module pmod_uart #(
    // The shifter clock, sys_clk: the board's 50 MHz crystal.
    parameter integer CLK_HZ = 50_000_000,
    parameter integer BAUD   = 115_200,
    // Bytes the host may queue, and bytes the module may send before the
    // host drains them.  One RYLR998 packet is at most 240 bytes plus the
    // +RCV wrapper, so the receive FIFO holds two.
    parameter integer TX_DEPTH = 256,
    parameter integer RX_DEPTH = 512
) (
    input  logic        control_clk, clk, resetn,
    // Register window, control clock.
    input  logic [7:0]  tx_data,
    input  logic        tx_push,
    input  logic        flush,
    input  logic        hold,
    input  logic        loopback,
    output logic [7:0]  rx_data,
    input  logic        rx_pop,
    output logic [31:0] status,
    // Pin side.
    input  logic        rx_pin,
    output logic        tx_pin
);
    localparam integer DIV  = (CLK_HZ + BAUD / 2) / BAUD;
    localparam integer HALF = (DIV > 1) ? DIV / 2 : 1;
    localparam integer TX_AW = $clog2(TX_DEPTH);
    localparam integer RX_AW = $clog2(RX_DEPTH);
    localparam integer TX_CW = TX_AW + 1;
    localparam integer RX_CW = RX_AW + 1;

    // ------------------------------------------------- control clock side --
    // The FIFOs and their pointers are here; the shifters are in clk.  A byte
    // in flight stays at the FIFO head until the engine acknowledges it.
    logic [7:0] tx_mem [0:TX_DEPTH-1];
    logic [7:0] rx_mem [0:RX_DEPTH-1];
    logic [TX_AW-1:0] tx_wp, tx_rp;
    logic [RX_AW-1:0] rx_wp, rx_rp;
    logic [TX_CW-1:0] tx_count;
    logic [RX_CW-1:0] rx_count;
    logic tx_inflight, tx_abandon, rx_overrun;
    logic [7:0] tx_hold;
    logic tx_req, tx_ack_p, rx_ack;
    logic [2:0] tx_ack_sync, rx_tog_sync;

    // Held by the shifter side until its acknowledge crosses back.
    logic [7:0] rx_hold_sys;
    logic rx_tog;
    logic tx_ack;

    assign rx_data = rx_mem[rx_rp];

    wire tx_ack_edge = tx_ack_sync[1] ^ tx_ack_sync[2];
    wire rx_tog_edge = rx_tog_sync[1] ^ rx_tog_sync[2];
    // `hold` lets the host fill the FIFO first, so a command leaves the pin
    // back-to-back instead of one byte per register transaction.
    wire tx_load = !tx_inflight && !tx_abandon && !flush && !hold && (tx_count != 0);
    wire pop_tx = tx_inflight && tx_ack_edge && !tx_abandon;
    wire push_tx = tx_push && !flush && (tx_count != TX_DEPTH);
    wire pop_rx = rx_pop && (rx_count != 0);
    wire push_rx = rx_tog_edge && (rx_count != RX_DEPTH);

    always_ff @(posedge control_clk) begin
        if (!resetn) begin
            tx_wp <= '0; tx_rp <= '0; tx_count <= '0;
            rx_wp <= '0; rx_rp <= '0; rx_count <= '0;
            tx_inflight <= 0; tx_abandon <= 0; rx_overrun <= 0;
            tx_hold <= 0; tx_req <= 0; tx_ack_p <= 0; rx_ack <= 0;
            tx_ack_sync <= 0; rx_tog_sync <= 0;
        end else begin
            tx_ack_sync <= {tx_ack_sync[1:0], tx_ack};
            rx_tog_sync <= {rx_tog_sync[1:0], rx_tog};

            if (tx_load) begin
                tx_hold <= tx_mem[tx_rp];
                tx_req <= ~tx_req;
                tx_inflight <= 1;
            end
            if (tx_inflight && tx_ack_edge) begin
                tx_inflight <= 0;
                tx_abandon <= 0;
            end
            if (pop_tx) begin
                tx_rp <= tx_rp + 1'b1;
            end
            if (push_tx) begin
                tx_mem[tx_wp] <= tx_data;
                tx_wp <= tx_wp + 1'b1;
            end
            // One count update so a push and a pop in the same clock cannot
            // lose each other.
            case ({push_tx, pop_tx})
                2'b10: tx_count <= tx_count + 1'b1;
                2'b01: tx_count <= tx_count - 1'b1;
                default: ;
            endcase

            if (rx_tog_edge) begin
                rx_ack <= ~rx_ack;
            end
            if (push_rx) begin
                rx_mem[rx_wp] <= rx_hold_sys;
                rx_wp <= rx_wp + 1'b1;
            end else if (rx_tog_edge) begin
                rx_overrun <= 1'b1;
            end
            if (pop_rx) begin
                rx_rp <= rx_rp + 1'b1;
            end
            case ({push_rx, pop_rx})
                2'b10: rx_count <= rx_count + 1'b1;
                2'b01: rx_count <= rx_count - 1'b1;
                default: ;
            endcase

            if (flush) begin
                tx_wp <= '0; tx_rp <= '0; tx_count <= '0;
                rx_wp <= '0; rx_rp <= '0; rx_count <= '0;
                rx_overrun <= 0;
                // A byte already on the wire still finishes; its acknowledge
                // must not pop the emptied FIFO.
                if (tx_inflight && !tx_ack_edge) begin
                    tx_abandon <= 1'b1;
                end
            end
        end
    end

    assign status = { {(12-TX_CW){1'b0}}, tx_count,
                      {(12-RX_CW){1'b0}}, rx_count,
                      4'b0, tx_inflight, rx_overrun,
                      (tx_count == TX_DEPTH), (rx_count != 0) };

    // ------------------------------------------------------ pin clock side --
    logic [1:0] rx_meta, loop_sync;
    logic rx_line_q;
    logic [2:0] tx_req_sync, rx_ack_sync;
    logic [7:0] rx_shift;
    logic [9:0] tx_shift;
    logic [15:0] rx_div, tx_div;
    logic [2:0] rx_bit;
    logic [3:0] tx_bit;
    logic rx_wait, tx_busy;

    typedef enum logic [1:0] {
        RX_IDLE, RX_START, RX_DATA, RX_STOP
    } rx_state_t;
    rx_state_t rx_state;

    // The pin is asynchronous; loopback uses the transmitter directly.
    wire rx_line = loop_sync[1] ? tx_pin : rx_meta[1];

    always_ff @(posedge clk) begin
        if (!resetn) begin
            rx_meta <= 0; loop_sync <= 0; rx_line_q <= 1;
            tx_req_sync <= 0; rx_ack_sync <= 0;
            rx_shift <= 0; tx_shift <= 10'h3ff;
            rx_div <= 0; tx_div <= 0; rx_bit <= 0; tx_bit <= 0;
            rx_wait <= 0; rx_tog <= 0; tx_ack <= 0; tx_busy <= 0;
            rx_state <= RX_IDLE;
        end else begin
            rx_meta <= {rx_meta[0], rx_pin};
            loop_sync <= {loop_sync[0], loopback};
            rx_line_q <= rx_line;
            tx_req_sync <= {tx_req_sync[1:0], tx_req};
            rx_ack_sync <= {rx_ack_sync[1:0], rx_ack};

            // A byte is presented on a toggle edge; its data is stable.
            if (tx_req_sync[1] ^ tx_req_sync[2]) begin
                if (!tx_busy) begin
                    tx_shift <= {1'b1, tx_hold, 1'b0};
                    tx_div <= 16'(DIV);
                    tx_bit <= 0;
                    tx_busy <= 1;
                end
            end
            if (tx_busy) begin
                if (tx_div <= 1) begin
                    tx_div <= 16'(DIV);
                    if (tx_bit == 9) begin
                        tx_busy <= 0;
                        tx_ack <= ~tx_ack;
                    end else begin
                        tx_shift <= {1'b1, tx_shift[9:1]};
                        tx_bit <= tx_bit + 1'b1;
                    end
                end else begin
                    tx_div <= tx_div - 1'b1;
                end
            end

            // The receive acknowledge frees the receiver for the next start.
            if (rx_ack_sync[1] ^ rx_ack_sync[2]) begin
                rx_wait <= 0;
            end
            case (rx_state)
                RX_IDLE: begin
                    if (!rx_line_q && !rx_wait) begin
                        rx_div <= 16'(HALF);
                        rx_state <= RX_START;
                    end
                end
                RX_START: begin
                    if (rx_div <= 1) begin
                        if (!rx_line_q) begin
                            rx_bit <= 0;
                            rx_div <= 16'(DIV);
                            rx_state <= RX_DATA;
                        end else begin
                            rx_state <= RX_IDLE;
                        end
                    end else begin
                        rx_div <= rx_div - 1'b1;
                    end
                end
                RX_DATA: begin
                    if (rx_div <= 1) begin
                        rx_shift[rx_bit] <= rx_line_q;
                        rx_div <= 16'(DIV);
                        if (rx_bit == 7) begin
                            rx_state <= RX_STOP;
                        end else begin
                            rx_bit <= rx_bit + 1'b1;
                        end
                    end else begin
                        rx_div <= rx_div - 1'b1;
                    end
                end
                RX_STOP: begin
                    if (rx_div <= 1) begin
                        rx_hold_sys <= rx_shift;
                        rx_tog <= ~rx_tog;
                        rx_wait <= 1;
                        rx_state <= RX_IDLE;
                    end else begin
                        rx_div <= rx_div - 1'b1;
                    end
                end
                default: rx_state <= RX_IDLE;
            endcase
        end
    end

    assign tx_pin = tx_busy ? tx_shift[0] : 1'b1;
endmodule
