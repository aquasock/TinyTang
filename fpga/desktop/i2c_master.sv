// TinyTang — a generic I2C master for the PMOD socket's I2C personality.
//
// The first client is the desktop clock's RTC (an Adafruit DS3231 at 0x68, or
// an MCP79410 at 0x6F), but nothing here is RTC-specific: a transaction names
// a device address, a register, a length of 1..16 bytes and a direction, and
// the engine runs START / address / register / data / repeated START / read N
// / STOP on the two open-drain pins.  A bus scan is the same machine with an
// address probe and no data.
//
// The engine is clocked by the board's 50 MHz crystal clock (sys_clk).  The
// register file's side reaches it through a command toggle and a result
// toggle, so the 21.49 MHz control domain never shares a clock with it.
//
// SPDX-License-Identifier: GPL-3.0-only

module i2c_master #(
    // 50 MHz on the desktop core; HALF is one half-period of SCL, 100 kHz
    // there.
    parameter integer CLK_HZ = 50_000_000,
    parameter integer I2C_HZ = 100_000
) (
    input  logic clk,
    input  logic resetn,

    // The socket says it is the I2C personality.  When low the bus is
    // released and no transaction runs.
    input  logic enable,

    // One transaction.  `cmd_start` is a one-clock pulse in this domain; the
    // fields and wdata are held stable by the caller until the engine takes
    // them.  A command offered while busy is ignored, so the caller polls
    // `busy` (and turns the automatic read off while it does).
    input  logic [1:0]   cmd_op,       // 0 read, 1 write, 2 scan
    input  logic [6:0]   cmd_addr,
    input  logic [7:0]   cmd_reg,
    input  logic [4:0]   cmd_len,      // 1..8
    input  logic [63:0]  cmd_wdata,
    input  logic         cmd_start,

    // The once-a-second RTC read, if enabled.
    input  logic         auto_enable,
    input  logic [6:0]   auto_addr,
    input  logic [7:0]   auto_reg,
    input  logic [4:0]   auto_len,

    output logic [63:0]  rdata,
    output logic         busy,
    output logic         done,
    output logic         nack,
    output logic         valid,
    output logic [7:0]   seq_no,
    output logic [7:0]   scan_addr,
    output logic [7:0]   scan_count,
    output logic         scan_done,

    // Open-drain pins: 1 drives low, 0 releases the line.
    output logic scl_low,
    output logic sda_low,
    input  logic sda_in
);

localparam integer HALF = CLK_HZ / (2 * I2C_HZ);

localparam logic [1:0] OP_READ  = 2'd0;
localparam logic [1:0] OP_WRITE = 2'd1;
localparam logic [1:0] OP_SCAN  = 2'd2;

typedef enum logic [4:0] {
    S_IDLE   = 5'd0,
    S_START1 = 5'd1,
    S_START2 = 5'd2,
    S_START3 = 5'd3,
    S_TX1    = 5'd4,
    S_TX2    = 5'd5,
    S_TX3    = 5'd6,
    S_TXAK1  = 5'd7,
    S_TXAK2  = 5'd8,
    S_TXAK3  = 5'd9,
    S_RX1    = 5'd10,
    S_RX2    = 5'd11,
    S_RX3    = 5'd12,
    S_ACK1   = 5'd13,
    S_ACK2   = 5'd14,
    S_ACK3   = 5'd15,
    S_STOP1  = 5'd16,
    S_STOP2  = 5'd17,
    S_STOP3  = 5'd18,
    S_DONE   = 5'd19,
    S_LOAD   = 5'd20
} state_t;

state_t state;
logic [15:0] half_cnt;
logic tick;

logic [2:0] bit_cnt;
logic [7:0] shift;
logic [1:0] op_q;
logic [6:0] addr_q;
logic [7:0] reg_q;
logic [4:0] len_q;
logic [4:0] seq;
logic [7:0] wbuf [0:7];
logic [7:0] rbuf [0:7];
logic [7:0] scan_addr_q;
logic [7:0] scan_count_q;
logic [7:0] scan_first_q;
logic [7:0] seq_q;
logic scan_done_q;
logic nack_q;
logic valid_q;
logic busy_q;

logic [1:0] sda_sync;
logic [1:0] enable_sync;
logic [31:0] auto_cnt;
logic auto_tick;

// ------------------------------------------------------------ timebases --
always @(posedge clk) begin
    if (!resetn) begin
        half_cnt <= 16'd0;
        tick     <= 1'b0;
    end else if (half_cnt == HALF - 1) begin
        half_cnt <= 16'd0;
        tick     <= 1'b1;
    end else begin
        half_cnt <= half_cnt + 16'd1;
        tick     <= 1'b0;
    end
end

always @(posedge clk) begin
    if (!resetn) begin
        auto_cnt  <= 32'd0;
        auto_tick <= 1'b0;
    end else if (!enable_sync[1] || !auto_enable) begin
        auto_cnt  <= 32'd0;
        auto_tick <= 1'b0;
    end else if (auto_cnt == CLK_HZ - 1) begin
        auto_cnt  <= 32'd0;
        auto_tick <= 1'b1;
    end else begin
        auto_cnt  <= auto_cnt + 32'd1;
        auto_tick <= 1'b0;
    end
end

always @(posedge clk) begin
    if (!resetn) begin
        sda_sync    <= 2'b11;
        enable_sync <= 2'b00;
    end else begin
        sda_sync    <= (sda_sync << 1) | {1'b0, sda_in};
        enable_sync <= (enable_sync << 1) | {1'b0, enable};
    end
end

wire ack_now = sda_sync[1];

// ------------------------------------------------------------- next byte --
logic [7:0] tx_byte;
always @(*) begin
    tx_byte = 8'hFF;
    if (op_q == OP_SCAN) begin
        tx_byte = {scan_addr_q[6:0], 1'b0};
    end else begin
        case (seq)
            5'd0: tx_byte = {addr_q, 1'b0};
            5'd1: tx_byte = reg_q;
            5'd2: tx_byte = (op_q == OP_READ) ? {addr_q, 1'b1} : wbuf[0];
            5'd3: tx_byte = wbuf[1];
            5'd4: tx_byte = wbuf[2];
            5'd5: tx_byte = wbuf[3];
            5'd6: tx_byte = wbuf[4];
            5'd7: tx_byte = wbuf[5];
            5'd8: tx_byte = wbuf[6];
            5'd9: tx_byte = wbuf[7];
            default: tx_byte = 8'hFF;
        endcase
    end
end

// ------------------------------------------------------------- the engine --
always @(posedge clk) begin
    if (!resetn) begin
        state        <= S_IDLE;
        bit_cnt      <= 3'd0;
        shift        <= 8'd0;
        op_q         <= OP_READ;
        addr_q       <= 7'd0;
        reg_q        <= 8'd0;
        len_q        <= 5'd7;
        seq          <= 5'd0;
        scan_addr_q  <= 8'd0;
        scan_count_q <= 8'd0;
        scan_first_q <= 8'd0;
        seq_q        <= 8'd0;
        scan_done_q  <= 1'b0;
        nack_q       <= 1'b0;
        valid_q      <= 1'b0;
        busy_q       <= 1'b0;
        done         <= 1'b0;
        scl_low      <= 1'b0;
        sda_low      <= 1'b0;
        for (int i = 0; i < 8; i++) begin
            wbuf[i] <= 8'd0;
            rbuf[i] <= 8'd0;
        end
    end else begin
        done <= 1'b0;

        // A personality change must release the bus, not leave a line driven.
        if (!enable_sync[1]) begin
            state   <= S_IDLE;
            busy_q  <= 1'b0;
            scl_low <= 1'b0;
            sda_low <= 1'b0;
        end else begin
            // A command can start at any clock; the bus phases advance on the
            // half-period tick.
            if (!busy_q) begin
                if (cmd_start) begin
                    op_q   <= cmd_op;
                    addr_q <= cmd_addr;
                    reg_q  <= cmd_reg;
                    len_q  <= (cmd_len == 0 || cmd_len > 8) ? 5'd8 : cmd_len;
                    seq    <= 5'd0;
                    busy_q <= 1'b1;
                    nack_q <= 1'b0;
                    valid_q <= 1'b0;
                    for (int i = 0; i < 8; i++) begin
                        wbuf[i] <= cmd_wdata[i * 8 +: 8];
                    end
                    if (cmd_op == OP_SCAN) begin
                        scan_addr_q  <= 8'h08;
                        scan_count_q <= 8'd0;
                        scan_first_q <= 8'd0;
                        scan_done_q  <= 1'b0;
                    end
                    state <= S_START1;
                end else if (auto_tick) begin
                    op_q   <= OP_READ;
                    addr_q <= auto_addr;
                    reg_q  <= auto_reg;
                    len_q  <= (auto_len == 0 || auto_len > 8) ? 5'd8 : auto_len;
                    seq    <= 5'd0;
                    busy_q <= 1'b1;
                    nack_q <= 1'b0;
                    valid_q <= 1'b0;
                    state  <= S_START1;
                end
            end

            // Load a byte at SCL low, one clock after the sequencer asks.
            if (state == S_LOAD) begin
                shift   <= tx_byte;
                bit_cnt <= 3'd7;
                state   <= S_TX1;
            end

            if (tick) begin
                case (state)
                    S_IDLE: ;

                    S_START1: begin
                        scl_low <= 1'b0;
                        sda_low <= 1'b0;
                        state   <= S_START2;
                    end
                    S_START2: begin
                        sda_low <= 1'b1;       // START
                        state   <= S_START3;
                    end
                    S_START3: begin
                        scl_low <= 1'b1;
                        state   <= S_LOAD;
                    end

                    S_TX1: begin
                        sda_low <= ~shift[7];
                        scl_low <= 1'b1;
                        state   <= S_TX2;
                    end
                    S_TX2: begin
                        scl_low <= 1'b0;
                        state   <= S_TX3;
                    end
                    S_TX3: begin
                        scl_low <= 1'b1;
                        if (bit_cnt == 3'd0) begin
                            state <= S_TXAK1;
                        end else begin
                            bit_cnt <= bit_cnt - 3'd1;
                            shift   <= {shift[6:0], 1'b0};
                            state   <= S_TX1;
                        end
                    end

                    S_TXAK1: begin
                        sda_low <= 1'b0;
                        scl_low <= 1'b1;
                        state   <= S_TXAK2;
                    end
                    S_TXAK2: begin
                        scl_low <= 1'b0;
                        state   <= S_TXAK3;
                    end
                    S_TXAK3: begin
                        scl_low <= 1'b1;
                        case (op_q)
                            OP_READ: begin
                                if (ack_now) begin
                                    nack_q <= 1'b1;
                                    state  <= S_STOP1;
                                end else case (seq)
                                    5'd0: begin seq <= 5'd1; state <= S_LOAD; end
                                    5'd1: begin seq <= 5'd2; state <= S_START1; end
                                    5'd2: begin seq <= 5'd3; bit_cnt <= 3'd7; state <= S_RX1; end
                                    default: state <= S_STOP1;
                                endcase
                            end
                            OP_WRITE: begin
                                if (ack_now) begin
                                    nack_q <= 1'b1;
                                    state  <= S_STOP1;
                                end else if (seq == len_q + 5'd1) begin
                                    state <= S_STOP1;
                                end else begin
                                    seq   <= seq + 5'd1;
                                    state <= S_LOAD;
                                end
                            end
                            default: begin // OP_SCAN
                                if (!ack_now) begin
                                    if (scan_count_q == 8'd0) begin
                                        scan_first_q <= scan_addr_q;
                                    end
                                    scan_count_q <= scan_count_q + 8'd1;
                                end
                                state <= S_STOP1;
                            end
                        endcase
                    end

                    S_RX1: begin
                        sda_low <= 1'b0;
                        scl_low <= 1'b1;
                        state   <= S_RX2;
                    end
                    S_RX2: begin
                        scl_low <= 1'b0;
                        state   <= S_RX3;
                    end
                    S_RX3: begin
                        shift   <= {shift[6:0], ack_now};
                        scl_low <= 1'b1;
                        if (bit_cnt == 3'd0) begin
                            state <= S_ACK1;
                        end else begin
                            bit_cnt <= bit_cnt - 3'd1;
                            state   <= S_RX1;
                        end
                    end

                    S_ACK1: begin
                        // ACK every byte but the last.
                        sda_low <= ~(seq == len_q + 5'd2);
                        scl_low <= 1'b1;
                        state   <= S_ACK2;
                    end
                    S_ACK2: begin
                        scl_low <= 1'b0;
                        state   <= S_ACK3;
                    end
                    S_ACK3: begin
                        scl_low <= 1'b1;
                        sda_low <= 1'b0;
                        case (seq)
                            5'd3:  rbuf[0]  <= shift;
                            5'd4:  rbuf[1]  <= shift;
                            5'd5:  rbuf[2]  <= shift;
                            5'd6:  rbuf[3]  <= shift;
                            5'd7:  rbuf[4]  <= shift;
                            5'd8:  rbuf[5]  <= shift;
                            5'd9:  rbuf[6]  <= shift;
                            5'd10: rbuf[7]  <= shift;
                            default: ;
                        endcase
                        if (seq == len_q + 5'd2) begin
                            seq_q   <= seq_q + 8'd1;
                            valid_q <= 1'b1;
                            state   <= S_STOP1;
                        end else begin
                            seq     <= seq + 5'd1;
                            bit_cnt <= 3'd7;
                            state   <= S_RX1;
                        end
                    end

                    S_STOP1: begin
                        sda_low <= 1'b1;
                        scl_low <= 1'b1;
                        state   <= S_STOP2;
                    end
                    S_STOP2: begin
                        scl_low <= 1'b0;
                        state   <= S_STOP3;
                    end
                    S_STOP3: begin
                        sda_low <= 1'b0;       // STOP
                        if (op_q == OP_SCAN && scan_addr_q != 8'h77) begin
                            scan_addr_q <= scan_addr_q + 8'd1;
                            seq         <= 5'd0;
                            state       <= S_START1;
                        end else begin
                            state <= S_DONE;
                        end
                    end

                    S_DONE: begin
                        busy_q <= 1'b0;
                        done   <= 1'b1;
                        if (op_q == OP_SCAN) begin
                            scan_done_q <= 1'b1;
                        end
                        state <= S_IDLE;
                    end

                    default: state <= S_IDLE;
                endcase
            end
        end
    end
end

// The result window is assembled from the byte buffer.  A read shorter than
// sixteen bytes leaves the rest of the window as it was; the caller knows the
// length it asked for.
assign rdata = {rbuf[7], rbuf[6], rbuf[5], rbuf[4],
                rbuf[3], rbuf[2], rbuf[1], rbuf[0]};
assign busy       = busy_q;
assign nack       = nack_q;
assign valid      = valid_q;
assign seq_no     = seq_q;
assign scan_addr  = scan_first_q;
assign scan_count = scan_count_q;
assign scan_done  = scan_done_q;

endmodule
