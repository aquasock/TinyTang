// TinyTang desktop VGA socket driver. SPDX-License-Identifier: GPL-3.0-only
// Raster timing is 1280x720p60, matching the existing HDMI transmitter.
module desktop_pmod (
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
    // sides meet on a command toggle and a result toggle, and the pins cross
    // into the pixel domain.
    input  logic [31:0] i2c_cmd,
    input  logic        i2c_cmd_toggle,
    input  logic [63:0]  i2c_wdata,
    input  logic [31:0] i2c_auto,
    output logic [63:0]  i2c_rdata,
    output logic         i2c_res_toggle,
    output logic [31:0] i2c_status,
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
    // the board's interleaved row mapping irrelevant.  A four-wire cable uses
    // only the top row (pins 3 and 4), which are lanes 0 and 1.
    wire i2c_on0 = active_word[7:4] == 6;
    wire i2c_on1 = active_word[11:8] == 6;
    wire i2c_enable = i2c_on0 || i2c_on1;
    wire i2c_flip = i2c_on0 ? active_word[12] : active_word[13];
    wire i2c_sda_raw = i2c_on0
                     ? (i2c_flip ? pmod0_io[2] : pmod0_io[0])
                     : i2c_on1 ? (i2c_flip ? pmod1_io[2] : pmod1_io[0])
                     : 1'b1;
    wire i2c_scl_low, i2c_sda_low;
    wire [63:0] i2c_rdata_p;
    wire i2c_busy_p, i2c_done_p, i2c_nack_p, i2c_valid_p, i2c_scan_done_p;
    wire [7:0] i2c_seq_p, i2c_scan_addr_p, i2c_scan_count_p;

    // The command toggle crosses into the pixel domain; its data fields are
    // held stable by desktop_regs, so the engine latches them safely.
    logic [2:0] cmd_sync;
    wire cmd_pulse;
    always_ff @(posedge sys_clk) begin
        if (!resetn) cmd_sync <= 3'b0;
        else cmd_sync <= {cmd_sync[1:0], i2c_cmd_toggle};
    end
    assign cmd_pulse = cmd_sync[1] ^ cmd_sync[2];

    i2c_master #(.CLK_HZ(74_250_000), .I2C_HZ(100_000)) i2c (
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

    logic i2c_valid_l, i2c_nack_l, i2c_scan_done_l, i2c_busy_s;
    logic [7:0] i2c_seq_l, i2c_scan_addr_l, i2c_scan_count_l;
    always_ff @(posedge control_clk) begin
        if (!resetn) begin
            res_sync <= 3'b0; busy_sync <= 2'b0; i2c_busy_s <= 1'b0;
            i2c_valid_l <= 1'b0; i2c_nack_l <= 1'b0;
            i2c_scan_done_l <= 1'b0;
            i2c_seq_l <= 8'd0; i2c_scan_addr_l <= 8'd0;
            i2c_scan_count_l <= 8'd0;
        end else begin
            res_sync <= {res_sync[1:0], res_toggle_p};
            busy_sync <= {busy_sync[0], i2c_busy_p};
            i2c_busy_s <= busy_sync[1];
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
                         i2c_scan_done_l, i2c_nack_l, i2c_busy_s, i2c_valid_l};

    logic [7:0] i2c_enables;
    always_comb begin
        i2c_enables = 8'b0;
        // Lanes 0/4 are socket pins 3/9; lanes 1/5 are pins 4/10.
        i2c_enables[0] = i2c_flip ? i2c_scl_low : i2c_sda_low;
        i2c_enables[4] = i2c_flip ? i2c_scl_low : i2c_sda_low;
        i2c_enables[1] = i2c_flip ? i2c_sda_low : i2c_scl_low;
        i2c_enables[5] = i2c_flip ? i2c_sda_low : i2c_scl_low;
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
                         : personality == 3 ? j2 : personality == 6 ? 8'b0 : 8'b0;
        wire [7:0] enables = personality == 1 ? oled_enables : personality == 2 ? 8'hff
                           : personality == 3 ? 8'h3f : personality == 6 ? i2c_enables : 8'h00;
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
