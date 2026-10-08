// TinyTang desktop VGA socket driver. SPDX-License-Identifier: GPL-3.0-only
// Raster timing is 1280x720p60, matching the existing HDMI transmitter.
module desktop_pmod (
    input logic pixel_clk, resetn,
    input logic [10:0] cx,
    input logic [9:0] cy,
    input logic [23:0] rgb,
    input logic [15:0] socket_word,
    input logic socket_request,
    output logic socket_ack,
    inout wire [7:0] pmod0_io, pmod1_io
);
    (* ASYNC_REG = "TRUE" *) logic request_meta, request_sync;
    logic [15:0] active_word;
    always_ff @(posedge pixel_clk) begin
        if (!resetn) begin
            request_meta <= 0;
            request_sync <= 0;
            socket_ack <= 0;
            active_word <= 0;
        end else begin
            request_meta <= socket_request;
            request_sync <= request_meta;
            // Commit during vertical blank, after two synchronization stages.
            // The sender holds socket_word until this ack crosses back.
            if (cx == 0 && cy == 720 && request_sync != socket_ack) begin
                active_word <= socket_word;
                socket_ack <= request_sync;
            end
        end
    end

    wire visible = cx < 1280 && cy < 720;
    // nes2hdmi's registered RGB is the pixel sampled by HDMI at the current
    // coordinate. VGA presents that same value, black through blanking.
    wire [3:0] red = visible ? rgb[23:20] : 4'b0;
    wire [3:0] green = visible ? rgb[15:12] : 4'b0;
    wire [3:0] blue = visible ? rgb[7:4] : 4'b0;
    wire hs = cx >= 1390 && cx < 1430;
    wire vs = cy >= 725 && cy < 730;
    wire [7:0] j1 = {blue, red};
    wire [7:0] j2 = {2'b0, vs, hs, green};

    for (genvar slot = 0; slot < 2; slot = slot + 1) begin : g_slot
        wire [3:0] personality = slot == 0 ? active_word[7:4] : active_word[11:8];
        wire flipped = slot == 0 ? active_word[12] : active_word[13];
        wire [7:0] lanes = personality == 2 ? j1 : j2;
        wire [7:0] enables = personality == 2 ? 8'hff : personality == 3 ? 8'h3f : 8'h00;
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
