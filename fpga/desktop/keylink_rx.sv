// SPDX-License-Identifier: GPL-3.0-only
//
// TinyTang keyboard link -- receiver.
//
// TinyTang's keyboard input does not use USB.  The front USB-A port is two
// FPGA pins wired to the connector and nothing else (BRD-003), and the
// keyboard on the other end is an STM32F401 whose USB data pins are ordinary
// GPIOs whose mode the firmware chooses -- Keychron's k2_he board.h puts PA11
// and PA12 in PIN_MODE_ALTERNATE for the OTG peripheral.  Both ends can
// therefore speak a protocol of our choosing on those two wires, and this is
// the FPGA end of it.
//
// It is a fixed-baud UART, one direction per wire, with no addressing and no
// polling discipline imposed by anything but us.  The baud is set where the
// receiver is instantiated, 281250 on every core: the keyboard's 72 MHz
// STM32F401 divides it to exactly 256 clocks, and this end counts its own
// clocks per bit from CLK_HZ, within half a percent.  The default is unused.
//
// Wire assignment:
//   usb1_dp  (D+)  keyboard PA12 -> FPGA    keyboard to Tang
//   usb1_dn  (D-)  FPGA -> keyboard PA11    Tang to keyboard, not used yet
//
// Frame, sent as consecutive 8N1 bytes:
//
//   A5  LEN  payload[LEN]  SUM
//
// LEN is 8 for the only payload defined so far, which is the HID boot
// keyboard report the keyboard already builds internally, so forwarding it
// costs the keyboard side almost nothing:
//
//   payload[0]   modifiers   (bit 0 LCtrl ... bit 7 RGUI)
//   payload[1]   reserved, always 0
//   payload[2:7] up to six concurrent keycodes, 0 = no key
//
// SUM is the 8-bit sum of every preceding byte of the frame, start and length
// included.  A frame that ends early, declares the wrong length, or fails SUM
// is dropped and counted, and the receiver resynchronises on the next A5.
//
// Nothing here is USB: no packets, no tokens, no CRC5 or CRC16, no frames per
// millisecond, and no polling interval imposed by a host controller.  The
// report rate is whatever the keyboard chooses to send.
//
// The UART receiver is written here rather than reusing the link's
// `uart_fixed.v`, whose parameter checks instantiate a module with a string
// argument that Verilator could not elaborate in this receiver's tests
// (`Pin not found: '__pinNumber1'`).  A receiver this small is
// better self-contained.

module keylink_rx #(
    parameter integer CLK_HZ      = 74_250_000,
    parameter integer BAUD        = 750_000,
    parameter [7:0]   START_BYTE  = 8'hA5,
    parameter integer KEYS        = 6,
    // Payload size the receiver will accept: 1 modifier + 1 reserved + KEYS.
    parameter integer PAYLOAD_LEN = 8
) (
    input  logic        clk,
    input  logic        resetn,

    // The keyboard's line, straight from the pin.
    input  logic        rx,

    // Latest accepted report.  Held until the next one arrives.
    output logic [7:0]  o_modifiers,
    output logic [7:0]  o_keys [KEYS],
    // One-cycle pulse when a frame passed its checksum and was latched.
    output logic        o_valid,

    // Diagnostics, in the style the other transports use.
    output logic [31:0] o_frames,
    output logic [31:0] o_bad_checksum,
    output logic [31:0] o_bad_length,
    output logic [31:0] o_truncated
);

    localparam integer CPB = CLK_HZ / BAUD;   // clocks per bit, exact by design

    // ------------------------------------------------------------- UART rx
    // Two-stage synchroniser: rx is a pin, asynchronous to clk_pixel.
    logic [1:0] rx_sync;
    always_ff @(posedge clk) begin
        if (!resetn) rx_sync <= 2'b11;
        else         rx_sync <= {rx_sync[0], rx};
    end

    localparam [1:0] U_IDLE = 2'd0, U_DATA = 2'd1, U_STOP = 2'd2;

    logic [1:0]  u_state;
    logic [31:0] u_cnt;
    logic [2:0]  u_bit;
    logic [7:0]  u_shift;
    logic        rx_ready;
    logic [7:0]  rx_byte;
    logic [31:0] idle_cnt;
    logic        rx_idle;

    always_ff @(posedge clk) begin
        if (!resetn) begin
            u_state  <= U_IDLE;
            u_cnt    <= 32'd0;
            u_bit    <= 3'd0;
            u_shift  <= 8'd0;
            rx_ready <= 1'b0;
            rx_byte  <= 8'd0;
            idle_cnt <= 32'd0;
        end else begin
            rx_ready <= 1'b0;

            // Idle detection: four bit times with no activity means the sender
            // has stopped mid-frame, which is how a torn frame is noticed.
            if (u_state != U_IDLE) idle_cnt <= 32'd0;
            else                   idle_cnt <= idle_cnt + 32'd1;

            case (u_state)
                U_IDLE: begin
                    if (rx_sync[1] == 1'b0) begin   // falling edge = start bit
                        // Skip the start bit and land mid-bit-0: half a bit to
                        // confirm the start, a full bit to clear it.  Sampling
                        // at CPB/2 alone reads the start bit as data bit 0.
                        u_cnt   <= CPB + (CPB / 2);
                        u_bit   <= 3'd0;
                        u_state <= U_DATA;
                    end
                end

                U_DATA: begin
                    if (u_cnt == 32'd0) begin
                        u_shift[u_bit] <= rx_sync[1];
                        // Reload with CPB-1, not CPB: the reload cycle is
                        // already one of the CPB, so CPB here makes the bit
                        // period CPB+1 and the sampling drifts a clock per
                        // bit until the stop check lands in the next byte.
                        u_cnt          <= CPB - 1;
                        if (u_bit == 3'd7) u_state <= U_STOP;
                        else               u_bit   <= u_bit + 3'd1;
                    end else begin
                        u_cnt <= u_cnt - 32'd1;
                    end
                end

                U_STOP: begin
                    if (u_cnt == 32'd0) begin
                        // A high stop bit is the only framing check available;
                        // a low one means we sampled noise, not a byte.
                        if (rx_sync[1] == 1'b1) begin
                            rx_byte  <= u_shift;
                            rx_ready <= 1'b1;
                        end
                        u_state <= U_IDLE;
                    end else begin
                        u_cnt <= u_cnt - 32'd1;
                    end
                end

                default: u_state <= U_IDLE;
            endcase
        end
    end

    assign rx_idle = (idle_cnt >= (4 * CPB)) && (u_state == U_IDLE);

    // ------------------------------------------------------- frame receiver
    localparam [1:0] S_START = 2'd0;
    localparam [1:0] S_LEN   = 2'd1;
    localparam [1:0] S_PAYLD = 2'd2;
    localparam [1:0] S_SUM   = 2'd3;

    localparam integer IDX_W = $clog2(PAYLOAD_LEN);

    logic [1:0]       state;
    logic [7:0]       sum;
    logic [7:0]       len;
    logic [IDX_W-1:0] index;
    logic [7:0]       payload [PAYLOAD_LEN];
    logic             started;   // seen a start byte since the last resynchronise

    always_ff @(posedge clk) begin
        if (!resetn) begin
            state          <= S_START;
            sum            <= 8'd0;
            len            <= 8'd0;
            index          <= '0;
            started        <= 1'b0;
            o_modifiers    <= 8'd0;
            o_valid        <= 1'b0;
            o_frames       <= 32'd0;
            o_bad_checksum <= 32'd0;
            o_bad_length   <= 32'd0;
            o_truncated    <= 32'd0;
            for (int i = 0; i < KEYS; i++) o_keys[i] <= 8'd0;
        end else begin
            o_valid <= 1'b0;

            // A frame that stopped part way is not coming back: resynchronise
            // rather than sit waiting for bytes that will never arrive.
            if (started && rx_idle) begin
                state       <= S_START;
                started     <= 1'b0;
                o_truncated <= o_truncated + 32'd1;
            end else if (rx_ready) begin
                case (state)
                    S_START: begin
                        if (rx_byte == START_BYTE) begin
                            sum     <= rx_byte;
                            started <= 1'b1;
                            state   <= S_LEN;
                        end
                    end

                    S_LEN: begin
                        sum <= sum + rx_byte;
                        len <= rx_byte;
                        if (rx_byte != PAYLOAD_LEN[7:0]) begin
                            o_bad_length <= o_bad_length + 32'd1;
                            started      <= 1'b0;
                            state        <= S_START;
                        end else begin
                            index <= '0;
                            state <= S_PAYLD;
                        end
                    end

                    S_PAYLD: begin
                        sum            <= sum + rx_byte;
                        payload[index] <= rx_byte;
                        if (index == IDX_W'(PAYLOAD_LEN - 1)) begin
                            state <= S_SUM;
                        end else begin
                            index <= index + 1'b1;
                        end
                    end

                    S_SUM: begin
                        state   <= S_START;
                        started <= 1'b0;
                        if (rx_byte == sum) begin
                            o_modifiers <= payload[0];
                            for (int i = 0; i < KEYS; i++)
                                o_keys[i] <= payload[2 + i];
                            o_valid  <= 1'b1;
                            o_frames <= o_frames + 32'd1;
                        end else begin
                            o_bad_checksum <= o_bad_checksum + 32'd1;
                        end
                    end

                    default: begin
                        state   <= S_START;
                        started <= 1'b0;
                    end
                endcase
            end
        end
    end

endmodule
