// SPDX-License-Identifier: MIT
//
// Minimal HDMI (DVI) test core for the Tang Console 138K, GW5AST-138 rev C.
//
// It isolates the desktop core's HDMI hardware path and nothing else, so the
// open flow and the vendor flow can be compared on one small design:
//
//   sys_clk 50 MHz -> pll_27 -> 27 MHz -> pll_hdmi -> hclk5 (5x pixel clock)
//   hclk5 -> CLKDIV /5 -> hclk (pixel clock)
//   3 x OSER10 (PCLK hclk, FCLK hclk5) + the pixel clock itself
//   -> 4 x ELVDS_OBUF on the desktop core's TMDS pins
//
// The PLL wrappers are the desktop core's own gowin_pll_27.v and
// gowin_pll_hdmi.v (tools/build_hdmi_test.sh copies them from the
// reconstructed nestang tree), and the serialiser and output-buffer wiring
// matches nestang's serializer.sv (GW_IDE branch) and nes2hdmi.sv.  The
// picture is eight colour bars, DVI only: no data islands, no audio.
//
// MODE_480P selects 640x480@60 (pixel 25.2 MHz, bit clock 126 MHz, with a
// pll_hdmi variant the build script generates) instead of 1280x720@60 (74.25
// and 371.25 MHz).  The build script sets it; nothing else differs.

module hdmi_test (
    input        sys_clk,
    output       tmds_clk_p,
    output       tmds_clk_n,
    output [2:0] tmds_d_p,
    output [2:0] tmds_d_n
);

parameter MODE_480P = 0;

localparam H_ACTIVE = MODE_480P ? 640  : 1280;
localparam H_FP     = MODE_480P ? 16   : 110;
localparam H_SYNC   = MODE_480P ? 96   : 40;
localparam H_BP     = MODE_480P ? 48   : 220;
localparam V_ACTIVE = MODE_480P ? 480  : 720;
localparam V_FP     = MODE_480P ? 10   : 5;
localparam V_SYNC   = MODE_480P ? 2    : 5;
localparam V_BP     = MODE_480P ? 33   : 20;
localparam SYNC_POS = MODE_480P ? 0    : 1;     // 480p syncs are negative
localparam H_TOTAL  = H_ACTIVE + H_FP + H_SYNC + H_BP;
localparam V_TOTAL  = V_ACTIVE + V_FP + V_SYNC + V_BP;
localparam BAR      = H_ACTIVE / 8;

// ---------------------------------------------------------------- clocks --
wire clk27, hclk5, hclk;

gowin_pll_27 pll_27 (.clkin(sys_clk), .clkout0(clk27));
gowin_pll_hdmi pll_hdmi (.clkin(clk27), .clkout(hclk5));

reg       resetn = 1'b0;
reg [7:0] reset_cnt = 8'd255;
always @(posedge sys_clk) begin
    reset_cnt <= reset_cnt == 8'd0 ? 8'd0 : reset_cnt - 8'd1;
    if (reset_cnt == 8'd0)
        resetn <= 1'b1;
end

CLKDIV #(.DIV_MODE("5")) div5 (
    .CLKOUT(hclk),
    .HCLKIN(hclk5),
    .RESETN(resetn),
    .CALIB(1'b0)
);

reg [2:0] rst_sync = 3'b111;
always @(posedge hclk or negedge resetn)
    if (!resetn) rst_sync <= 3'b111;
    else         rst_sync <= {rst_sync[1:0], 1'b0};
wire reset = rst_sync[2];

// ---------------------------------------------------------------- raster --
reg [11:0] hcnt = 12'd0;
reg [11:0] vcnt = 12'd0;
always @(posedge hclk)
    if (reset) begin
        hcnt <= 12'd0;
        vcnt <= 12'd0;
    end else if (hcnt == H_TOTAL - 1) begin
        hcnt <= 12'd0;
        vcnt <= vcnt == V_TOTAL - 1 ? 12'd0 : vcnt + 12'd1;
    end else
        hcnt <= hcnt + 12'd1;

wire h_sync = hcnt >= H_ACTIVE + H_FP && hcnt < H_ACTIVE + H_FP + H_SYNC;
wire v_sync = vcnt >= V_ACTIVE + V_FP && vcnt < V_ACTIVE + V_FP + V_SYNC;
wire active = hcnt < H_ACTIVE && vcnt < V_ACTIVE;

// white, yellow, cyan, green, magenta, red, blue, black
wire [2:0] bar = hcnt < BAR     ? 3'd0 : hcnt < 2 * BAR ? 3'd1 :
                 hcnt < 3 * BAR ? 3'd2 : hcnt < 4 * BAR ? 3'd3 :
                 hcnt < 5 * BAR ? 3'd4 : hcnt < 6 * BAR ? 3'd5 :
                 hcnt < 7 * BAR ? 3'd6 : 3'd7;
wire r_on = bar == 3'd0 || bar == 3'd1 || bar == 3'd4 || bar == 3'd5;
wire g_on = bar == 3'd0 || bar == 3'd1 || bar == 3'd2 || bar == 3'd3;
wire b_on = bar == 3'd0 || bar == 3'd2 || bar == 3'd4 || bar == 3'd6;

reg       de = 1'b0, hs = 1'b0, vs = 1'b0;
reg [7:0] red = 8'd0, green = 8'd0, blue = 8'd0;
always @(posedge hclk) begin
    de    <= active;
    hs    <= SYNC_POS ? h_sync : ~h_sync;
    vs    <= SYNC_POS ? v_sync : ~v_sync;
    red   <= r_on ? 8'd192 : 8'd16;
    green <= g_on ? 8'd192 : 8'd16;
    blue  <= b_on ? 8'd192 : 8'd16;
end

// ------------------------------------------------------- encode, serialise --
wire [9:0] tmds_ch [0:2];
tmds_encoder enc0 (.clk(hclk), .reset(reset), .de(de), .ctrl({vs, hs}), .d(blue),  .q(tmds_ch[0]));
tmds_encoder enc1 (.clk(hclk), .reset(reset), .de(de), .ctrl(2'b00),    .d(green), .q(tmds_ch[1]));
tmds_encoder enc2 (.clk(hclk), .reset(reset), .de(de), .ctrl(2'b00),    .d(red),   .q(tmds_ch[2]));

wire [2:0] tmds;
genvar i;
generate
    for (i = 0; i < 3; i = i + 1) begin : ser
        OSER10 gwSer (
            .Q(tmds[i]),
            .D0(tmds_ch[i][0]), .D1(tmds_ch[i][1]), .D2(tmds_ch[i][2]),
            .D3(tmds_ch[i][3]), .D4(tmds_ch[i][4]), .D5(tmds_ch[i][5]),
            .D6(tmds_ch[i][6]), .D7(tmds_ch[i][7]), .D8(tmds_ch[i][8]),
            .D9(tmds_ch[i][9]),
            .PCLK(hclk),
            .FCLK(hclk5),
            .RESET(reset)
        );
    end
endgenerate

ELVDS_OBUF tmds_bufds [3:0] (
    .I({hclk, tmds}),
    .O({tmds_clk_p, tmds_d_p}),
    .OB({tmds_clk_n, tmds_d_n})
);

endmodule

// DVI 1.0 TMDS encoder (section 3.3.3); q[0] is the first bit on the wire.
module tmds_encoder (
    input            clk,
    input            reset,
    input            de,
    input      [1:0] ctrl,
    input      [7:0] d,
    output reg [9:0] q
);

wire [3:0] n1d = d[0] + d[1] + d[2] + d[3] + d[4] + d[5] + d[6] + d[7];
wire use_xnor = n1d > 4'd4 || (n1d == 4'd4 && d[0] == 1'b0);

wire [8:0] qm;
assign qm[0] = d[0];
genvar k;
generate
    for (k = 1; k < 8; k = k + 1) begin : stage
        assign qm[k] = use_xnor ? ~(qm[k-1] ^ d[k]) : (qm[k-1] ^ d[k]);
    end
endgenerate
assign qm[8] = ~use_xnor;

wire [3:0] n1q = qm[0] + qm[1] + qm[2] + qm[3] + qm[4] + qm[5] + qm[6] + qm[7];
wire signed [4:0] diff = $signed({1'b0, n1q}) - $signed(5'd8 - {1'b0, n1q});  // N1 - N0
wire signed [4:0] two_qm8  = qm[8] ? 5'sd2 : 5'sd0;
wire signed [4:0] two_nqm8 = qm[8] ? 5'sd0 : 5'sd2;

reg signed [4:0] cnt = 5'sd0;
always @(posedge clk)
    if (reset) begin
        cnt <= 5'sd0;
        q   <= 10'd0;
    end else if (!de) begin
        cnt <= 5'sd0;
        case (ctrl)
            2'b00: q <= 10'b1101010100;
            2'b01: q <= 10'b0010101011;
            2'b10: q <= 10'b0101010100;
            2'b11: q <= 10'b1010101011;
        endcase
    end else if (cnt == 5'sd0 || diff == 5'sd0) begin
        q   <= {~qm[8], qm[8], qm[8] ? qm[7:0] : ~qm[7:0]};
        cnt <= qm[8] ? cnt + diff : cnt - diff;
    end else if ((cnt > 5'sd0 && diff > 5'sd0) || (cnt < 5'sd0 && diff < 5'sd0)) begin
        q   <= {1'b1, qm[8], ~qm[7:0]};
        cnt <= cnt + two_qm8 - diff;
    end else begin
        q   <= {1'b0, qm[8], qm[7:0]};
        cnt <= cnt - two_nqm8 + diff;
    end

endmodule
