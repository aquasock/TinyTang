// TinyTang desktop VGA socket driver. SPDX-License-Identifier: GPL-3.0-only
// Raster timing is 1280x720p60, matching the existing HDMI transmitter.
module desktop_pmod #(
    // The I2C engine's clock, sys_clk: the board's 50 MHz crystal.
    parameter integer I2C_CLK_HZ = 50_000_000,
    // The PMOD UART's rate on personality 7.
    parameter integer UART_BAUD = 115_200
) (
    input logic control_clk, pixel_clk, sys_clk, resetn,
    input logic [10:0] cx,
    input logic [9:0] cy,
    input logic [23:0] rgb,
    input logic [15:0] socket_word,
    input logic socket_request,
    input logic oled_cell_we,
    input logic [8:0] oled_cell_index,
    input logic [15:0] oled_cell_word,
    input logic [16:0] oled_cursor,
    input logic [1:0] oled_font,
    output logic [31:0] oled_frames, oled_signature,
    output logic socket_ack,
    // Generic I2C register interface.  The command and write data are in the
    // control clock; the engine is on the 50 MHz crystal clock, so the two
    // sides meet on a command toggle and a result toggle.
    input  logic [31:0] i2c_cmd,
    input  logic        i2c_cmd_toggle,
    input  logic [63:0]  i2c_wdata,
    input  logic [31:0] i2c_auto,
    output logic [63:0]  i2c_rdata,
    output logic         i2c_res_toggle,
    output logic [31:0] i2c_status,
    // PMOD UART (personality 7), control clock.
    input  logic [7:0]  uart_tx_data,
    input  logic        uart_tx_push,
    input  logic        uart_flush,
    input  logic        uart_hold,
    input  logic        uart_nrst_low,
    input  logic        uart_loopback,
    input  logic        uart_rx_pop,
    output logic [7:0]  uart_rx_data,
    output logic [31:0] uart_status,
    inout wire [7:0] pmod0_io, pmod1_io
);
    (* ASYNC_REG = "TRUE" *) logic request_meta, request_sync;
    logic [15:0] active_word;
    logic [16:0] active_cursor;
    logic [1:0] active_font;
    always_ff @(posedge pixel_clk) begin
        if (!resetn) begin
            request_meta <= 0;
            request_sync <= 0;
            socket_ack <= 0;
            active_word <= 0;
            active_cursor <= 0;
            active_font <= 0;
        end else begin
            request_meta <= socket_request;
            request_sync <= request_meta;
            // Commit during vertical blank, after two synchronization stages.
            // The sender holds socket_word until this ack crosses back.
            if (cx == 0 && cy == 720 && request_sync != socket_ack) begin
                active_word <= socket_word;
                active_cursor <= oled_cursor;
                active_font <= oled_font;
                socket_ack <= request_sync;
            end
        end
    end

    // The compositor registers RGB from cx/cy. Register its timing on the
    // same edge, as HDMI does with video_data_period; raw timing would expose
    // the previous line's blanking glyph at x=0 and drop the last active pixel.
    logic visible, hs, vs;
    always_ff @(posedge pixel_clk) begin
        if (!resetn) begin
            visible <= 0;
            hs <= 0;
            vs <= 0;
        end else begin
            visible <= cx < 1280 && cy < 720;
            hs <= cx >= 1390 && cx < 1430;
            vs <= cy >= 725 && cy < 730;
        end
    end
    wire [3:0] red = visible ? rgb[23:20] : 4'b0;
    wire [3:0] green = visible ? rgb[15:12] : 4'b0;
    wire [3:0] blue = visible ? rgb[7:4] : 4'b0;
    wire [7:0] j1 = {blue, red};
    wire [7:0] j2 = {2'b0, vs, hs, green};
    // ------------------------------------------------------ PMOD I2C bus --
    // The I2C personality is a bus, not a device.  A 2x4 Pmod makes each
    // signal column a pair of pins, so both lanes of a pair carry the same
    // open-drain signal; the `flip` bit then only chooses which pair is SCL
    // and which is SDA.  Driving both lanes of a pair identically also makes
    // the board's interleaved row mapping irrelevant.  An I2C Pmod puts SCL on
    // pin 3 and SDA on pin 4, beside GND on pin 5, which are lanes 2 and 3
    // (lane n is Digilent pin n+1, as the OLED's CS on lane 0 and SCK on lane
    // 3 show); the bottom row's pins 9 and 10 are lanes 6 and 7.  SDA is read
    // back from IO6 (lane 3, pin 4), or from IO4 (lane 2, pin 3) when flipped;
    // with both rows identical the socket's row swap does not change that.
    wire i2c_on0 = active_word[7:4] == 6;
    wire i2c_on1 = active_word[11:8] == 6;
    // Personality 7 is the same I2C engine on the upper row only, with the
    // UART on the lower row.  One engine serves either personality.
    wire uart_on0 = active_word[7:4] == 7;
    wire uart_on1 = active_word[11:8] == 7;
    wire i2c_enable = i2c_on0 || i2c_on1 || uart_on0 || uart_on1;
    wire i2c_flip = i2c_on0 ? active_word[12] : active_word[13];
    // Personality 6 reads SDA from the same row's duplicate; personality 7
    // reads it from pin 4, or pin 10 when the module's rows are mirrored.
    wire i2c_sda_raw = i2c_on0
                     ? (i2c_flip ? pmod0_io[4] : pmod0_io[6])
                     : i2c_on1 ? (i2c_flip ? pmod1_io[4] : pmod1_io[6])
                     : uart_on0 ? (active_word[12] ? pmod0_io[7] : pmod0_io[6])
                     : uart_on1 ? (active_word[13] ? pmod1_io[7] : pmod1_io[6])
                     : 1'b1;
    // The module's TXD arrives on pin 10, or on pin 4 when mirrored.
    wire uart_rx_raw = uart_on0
                     ? (active_word[12] ? pmod0_io[6] : pmod0_io[7])
                     : uart_on1 ? (active_word[13] ? pmod1_io[6] : pmod1_io[7])
                     : 1'b1;
    wire i2c_scl_low, i2c_sda_low;
    wire [63:0] i2c_rdata_p;
    wire i2c_busy_p, i2c_done_p, i2c_nack_p, i2c_valid_p, i2c_scan_done_p;
    wire [7:0] i2c_seq_p, i2c_scan_addr_p, i2c_scan_count_p;

    // The command toggle crosses into the sys_clk domain; its data fields are
    // held stable by desktop_regs, so the engine latches them safely.
    logic [2:0] cmd_sync;
    wire cmd_pulse;
    always_ff @(posedge sys_clk) begin
        if (!resetn) cmd_sync <= 3'b0;
        else cmd_sync <= {cmd_sync[1:0], i2c_cmd_toggle};
    end
    assign cmd_pulse = cmd_sync[1] ^ cmd_sync[2];

    i2c_master #(.CLK_HZ(I2C_CLK_HZ), .I2C_HZ(100_000)) i2c (
        .clk(sys_clk), .resetn(resetn), .enable(i2c_enable),
        .cmd_op(i2c_cmd[1:0]), .cmd_addr(i2c_cmd[14:8]),
        .cmd_reg(i2c_cmd[23:16]), .cmd_len(i2c_cmd[28:24]),
        .cmd_wdata(i2c_wdata), .cmd_start(cmd_pulse),
        .auto_enable(i2c_auto[31]), .auto_addr(i2c_auto[6:0]),
        .auto_reg(i2c_auto[15:8]), .auto_len(i2c_auto[20:16]),
        .rdata(i2c_rdata_p), .busy(i2c_busy_p), .done(i2c_done_p),
        .nack(i2c_nack_p), .valid(i2c_valid_p), .seq_no(i2c_seq_p),
        .scan_addr(i2c_scan_addr_p), .scan_count(i2c_scan_count_p),
        .scan_done(i2c_scan_done_p),
        .scl_low(i2c_scl_low), .sda_low(i2c_sda_low), .sda_in(i2c_sda_raw)
    );

    // Personality 7's byte link.  Its register side is in the control clock
    // and its shifters in sys_clk; the two lane outputs cross into the pixel
    // domain below.
    wire uart_tx_line;
    pmod_uart #(.CLK_HZ(I2C_CLK_HZ), .BAUD(UART_BAUD)) uart (
        .control_clk(control_clk), .clk(sys_clk), .resetn(resetn),
        .tx_data(uart_tx_data), .tx_push(uart_tx_push), .flush(uart_flush),
        .hold(uart_hold), .loopback(uart_loopback), .rx_data(uart_rx_data), .rx_pop(uart_rx_pop),
        .status(uart_status),
        .rx_pin(uart_rx_raw), .tx_pin(uart_tx_line)
    );
    logic uart_tx_meta, uart_tx_px, uart_nrst_meta, uart_nrst_px;
    always_ff @(posedge pixel_clk) begin
        if (!resetn) begin
            uart_tx_meta <= 1'b1; uart_tx_px <= 1'b1;
            uart_nrst_meta <= 1'b0; uart_nrst_px <= 1'b0;
        end else begin
            uart_tx_meta <= uart_tx_line; uart_tx_px <= uart_tx_meta;
            uart_nrst_meta <= uart_nrst_low; uart_nrst_px <= uart_nrst_meta;
        end
    end

    // The result toggle crosses back; the data is static until the next
    // transaction, so a latch on the edge is enough.  Busy is a live level
    // and is synchronized separately.
    logic res_toggle_p;
    always_ff @(posedge sys_clk) begin
        if (!resetn) res_toggle_p <= 1'b0;
        else if (i2c_done_p) res_toggle_p <= ~res_toggle_p;
    end

    logic [2:0] res_sync;
    logic [1:0] busy_sync;

    // A command is busy from the control clock it is issued in until its
    // result is latched here.  The engine's own busy arrives through a
    // separate synchronizer and can fall a clock before the result toggle
    // lands, and a scan's done flag would otherwise still be the previous
    // scan's, so a poller would take the old result as the new one.
    logic i2c_valid_l, i2c_nack_l, i2c_scan_done_l, i2c_busy_s;
    logic [7:0] i2c_seq_l, i2c_scan_addr_l, i2c_scan_count_l;
    logic cmd_seen, cmd_pending;
    always_ff @(posedge control_clk) begin
        if (!resetn) begin
            res_sync <= 3'b0; busy_sync <= 2'b0; i2c_busy_s <= 1'b0;
            cmd_seen <= 1'b0; cmd_pending <= 1'b0;
            i2c_valid_l <= 1'b0; i2c_nack_l <= 1'b0;
            i2c_scan_done_l <= 1'b0;
            i2c_seq_l <= 8'd0; i2c_scan_addr_l <= 8'd0;
            i2c_scan_count_l <= 8'd0;
        end else begin
            res_sync <= {res_sync[1:0], res_toggle_p};
            busy_sync <= {busy_sync[0], i2c_busy_p};
            i2c_busy_s <= busy_sync[1];
            cmd_seen <= i2c_cmd_toggle;
            if (i2c_cmd_toggle != cmd_seen) begin
                cmd_pending <= 1'b1;
                i2c_scan_done_l <= 1'b0;
            end else if (res_sync[1] ^ res_sync[2]) begin
                cmd_pending <= 1'b0;
            end
            if (res_sync[1] ^ res_sync[2]) begin
                i2c_valid_l <= i2c_valid_p;
                i2c_nack_l <= i2c_nack_p;
                i2c_scan_done_l <= i2c_scan_done_p;
                i2c_seq_l <= i2c_seq_p;
                i2c_scan_addr_l <= i2c_scan_addr_p;
                i2c_scan_count_l <= i2c_scan_count_p;
            end
        end
    end

    assign i2c_rdata = i2c_rdata_p;
    assign i2c_res_toggle = res_toggle_p;
    assign i2c_status = {i2c_scan_count_l, i2c_scan_addr_l, i2c_seq_l, 4'b0,
                         i2c_scan_done_l, i2c_nack_l, i2c_busy_s || cmd_pending,
                         i2c_valid_l};

    logic [7:0] i2c_enables;
    always_comb begin
        i2c_enables = 8'b0;
        // Lanes 2/6 are socket pins 3/9 (SCL); lanes 3/7 are pins 4/10 (SDA).
        i2c_enables[2] = i2c_flip ? i2c_sda_low : i2c_scl_low;
        i2c_enables[6] = i2c_flip ? i2c_sda_low : i2c_scl_low;
        i2c_enables[3] = i2c_flip ? i2c_scl_low : i2c_sda_low;
        i2c_enables[7] = i2c_flip ? i2c_scl_low : i2c_sda_low;
    end

    logic [7:0] uart_lanes, uart_enables;
    always_comb begin
        uart_lanes = 8'b0;
        uart_enables = 8'b0;
        // Upper row: I2C on pins 3 and 4 only.  An I2C Pmod that ties its two
        // rows together must not be seated in this personality, because the
        // lower row is the UART's here.
        uart_enables[2] = i2c_scl_low;   // SCL, pin 3
        uart_enables[3] = i2c_sda_low;   // SDA, pin 4
        // Lower row: NRST on pin 8 and TX on pin 9; RX (pin 10) stays input.
        // The register bit asserts reset, so the pin carries its inverse.
        uart_lanes[5] = ~uart_nrst_px; uart_enables[5] = 1'b1;
        uart_lanes[6] = uart_tx_px;   uart_enables[6] = 1'b1;
    end

    wire [7:0] oled_lanes, oled_enables;
    desktop_oled terminal_panel (
        .control_clk(control_clk),.pixel_clk(pixel_clk),.resetn(resetn),
        .selected(active_word[7:4]==1 || active_word[11:8]==1),
        .font_mode(active_font),
        .cell_we(oled_cell_we),.cell_index(oled_cell_index),.cell_word(oled_cell_word),
        .cursor_word(active_cursor),.lane_o(oled_lanes),.lane_oe(oled_enables),
        .frames(oled_frames),.signature(oled_signature)
    );

    for (genvar slot = 0; slot < 2; slot = slot + 1) begin : g_slot
        wire [3:0] personality = slot == 0 ? active_word[7:4] : active_word[11:8];
        wire flipped = slot == 0 ? active_word[12] : active_word[13];
        wire [7:0] lanes = personality == 1 ? oled_lanes : personality == 2 ? j1
                         : personality == 3 ? j2 : personality == 6 ? 8'b0
                         : personality == 7 ? uart_lanes : 8'b0;
        wire [7:0] enables = personality == 1 ? oled_enables : personality == 2 ? 8'hff
                           : personality == 3 ? 8'h3f : personality == 6 ? i2c_enables
                           : personality == 7 ? uart_enables : 8'h00;
        for (genvar io = 0; io < 8; io = io + 1) begin : g_pin
            // The dock interleaves rows: even IO numbers are pins 1-4.
            localparam integer NORMAL_LANE = io/2 + (io%2)*4;
            wire [2:0] lane = 3'(NORMAL_LANE) ^ (flipped ? 3'd4 : 3'd0);
            if (slot == 0)
                assign pmod0_io[io] = enables[lane] ? lanes[lane] : 1'bz;
            else
                assign pmod1_io[io] = enables[lane] ? lanes[lane] : 1'bz;
        end
    end
endmodule
