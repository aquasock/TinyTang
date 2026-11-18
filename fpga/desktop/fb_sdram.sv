// SPDX-License-Identifier: GPL-3.0-only
//
// TinyTang: a 1280x720 RGB565 framebuffer in the dock's SDRAM, scanned out at
// 1:1 on the pixel clock.  The successor to pixplane.sv's 640x360 store in
// block RAM: the picture moves to one Winbond W9825G6KH-6 die (32 MB x16,
// TinyTang record PSX-005) and only two line buffers stay on chip.
//
// One clock.  The SDRAM runs on the pixel clock itself, 74.25 MHz, 13.47 ns: no
// new PLL, the scanout and the controller share a domain, and CAS latency 2 is
// well inside the part's margins.  The budget, per 1650-clock raster line: 1280
// words of the next line in eight-word bursts, about 36 clocks of row opening
// and closing, four refreshes of 6 clocks, and what is left drains writes.  The
// link's writes arrive at UART speed, a few per line, through a small FIFO from
// the link's clock; it is the only crossing.
//
// Layout.  Line y starts at word y * 1536 of bank 0 -- three 512-word rows a
// line, of which 2.5 are used -- so a line never shares a row with another and
// the addressing is shifts and adds.  720 lines use 2160 of bank 0's 8192 rows.
//
// Scanout.  Two line buffers, selected by line parity.  At the start of raster
// line cy the controller fetches line cy + 1 into the other buffer (line 0
// during the last line of the frame), so a fetch has a whole line to finish in
// before the four-pixel look-ahead starts reading it.  The look-ahead and the
// colour pipeline are pixplane's and textdisp_wide's: four registers, so the
// layers stay interchangeable in desktop_hdmi.
//
// Timing, in clocks at 74.25 MHz, from the -6 part's datasheet values: tRCD 2
// (18 ns), tRP 2 (18 ns), tRAS 4 (42 ns), tRC 5 (60 ns), tWR 2, tMRD 2, and
// 8192 refreshes per 64 ms.  Four refreshes are owed per line, about 3000 per
// frame against the 2138 the part needs in that time.

module fb_sdram #(
    parameter integer W = 1280,
    parameter integer H = 720,
    parameter integer STRIDE = 1536,       // words per line
    parameter integer INIT_WAIT = 15000,   // clocks before the first command: > 200 us
    parameter integer READ_DELAY = 2,      // READ issued to the first capture: 2 in simulation (CL2, the part on the inverse clock); the board may differ
    parameter integer T_RCD = 2,
    parameter integer T_RP  = 2,
    parameter integer T_RC  = 5,
    parameter integer T_WR  = 2,
    parameter integer T_MRD = 2
) (
    // Write side, on the link's clock.
    input  wire        clk_w,
    input  wire        we,
    input  wire [10:0] wx,
    input  wire [9:0]  wy,
    input  wire [15:0] wdata,

    // Controller and scanout, on the pixel clock.
    input  wire        clk,
    input  wire        resetn,             // synchronous to clk
    input  wire [10:0] cx,
    input  wire [9:0]  cy,
    input  wire [10:0] frame_width,
    input  wire [9:0]  frame_height,
    output reg  [23:0] rgb,

    // The SDRAM.  Its clock is the caller's: the inverse of clk.
    output reg  [12:0] sd_a,
    output reg  [1:0]  sd_ba,
    output reg         sd_cs_n,
    output reg         sd_ras_n,
    output reg         sd_cas_n,
    output reg         sd_we_n,
    output reg  [1:0]  sd_dqm,
    inout  wire [15:0] sd_dq,

    // Diagnostics.
    output reg         ready,              // initialised
    output reg         underrun,           // sticky: a line was shown before its fetch ended
    output reg         dropped,            // sticky: a write found the FIFO full
    output reg  [10:0] fetch_max           // the longest fetch, in clocks
);

    // ------------------------------------------------------------ write FIFO
    //
    // 64 entries of {y, x, pixel}, Gray-coded pointers synchronised through two
    // flops each way.  Writes drain only between fetches, and a fetch holds the
    // part for up to about 17.5 us, so the FIFO must hold what arrives meanwhile:
    // two pixels at today's 2 Mbaud, and far more from a faster link or a
    // stress test.  A write into a full FIFO is dropped and counted in `dropped`.
    localparam integer FA = 6;
    reg [36:0] fifo [0:(1<<FA)-1];
    reg [FA:0] wbin = 0, wgray = 0, rbin = 0, rgray = 0;
    reg [FA:0] rgray_w1 = 0, rgray_w2 = 0, wgray_r1 = 0, wgray_r2 = 0;

    wire [FA:0] wbin_next  = wbin + 1'b1;
    wire [FA:0] wgray_next = wbin_next ^ (wbin_next >> 1);
    wire        full = wgray_next == {~rgray_w2[FA:FA-1], rgray_w2[FA-2:0]};

    reg dropped_w = 1'b0;           // sticky, on the write side's clock
    always @(posedge clk_w) begin
        rgray_w1 <= rgray;
        rgray_w2 <= rgray_w1;
        if (we && full) dropped_w <= 1'b1;
        if (we && !full) begin
            fifo[wbin[FA-1:0]] <= {wy, wx, wdata};
            wbin  <= wbin_next;
            wgray <= wgray_next;
        end
    end

    reg dropped_r1 = 1'b0;
    always @(posedge clk) begin
        wgray_r1 <= wgray;
        wgray_r2 <= wgray_r1;
        dropped_r1 <= dropped_w;
        dropped  <= dropped_r1;
    end
    wire        empty = rgray == wgray_r2;
    wire [36:0] head  = fifo[rbin[FA-1:0]];

    // ------------------------------------------------------------ commands
    localparam [3:0] C_NOP = 4'b0111, C_ACT = 4'b0011, C_READ = 4'b0101,
                     C_WRITE = 4'b0100, C_PRE = 4'b0010, C_REF = 4'b0001,
                     C_MRS = 4'b0000, C_DESEL = 4'b1111;

    // CAS latency 2, burst of eight, sequential, single-location writes.
    localparam [12:0] MODE = {3'b000, 1'b1, 2'b00, 3'b010, 1'b0, 3'b011};

    localparam [3:0] S_INIT = 0, S_PREALL = 1, S_INITREF = 2, S_MRS = 3,
                     S_IDLE = 4, S_FETCH = 5, S_REF = 6, S_WRITE = 7;
    reg [3:0]  state;
    reg [15:0] tick;
    reg [3:0]  n;                   // refreshes left in initialisation

    reg [15:0] dq_o;
    reg        dq_oe;
    assign sd_dq = dq_oe ? dq_o : 16'hzzzz;
    reg [15:0] dq_r;                // read data, registered at our edge

    // What the raster asks for.
    reg        fetch_pending;
    reg [9:0]  fetch_line;
    reg [3:0]  ref_owed;
    reg [1:0]  seg;                 // row of the line: 0, 1, 2
    reg [10:0] fetch_len;           // clocks the current fetch has taken

    // Read data in flight: a READ issued now names where its eight words go.
    reg        rp_v [0:READ_DELAY-1];
    reg [11:0] rp_d [0:READ_DELAY-1];
    reg [3:0]  cap_n;
    reg [11:0] cap_i;
    reg [9:0]  cap_line;            // the line being captured
    reg        cap_last;            // the burst in capture is the line's last
    reg        rp_last [0:READ_DELAY-1];
    reg [9:0]  ready_line [0:1];    // the line each buffer holds complete
    reg        armed;               // a whole frame has been fetched: check for underrun

    // Line buffers, by line parity.
    reg [15:0] lb [0:2*W-1];
    reg        lb_we;
    reg [11:0] lb_wa;
    reg [15:0] lb_wd;

    wire [8:0] seg_reads = (seg == 2'd2) ? 9'd32 : 9'd64;   // 256 or 512 words
    wire [8:0] read_k = (tick - T_RCD) >> 3;                 // which READ of the row
    wire       read_tick = tick >= T_RCD && tick[2:0] == T_RCD[2:0] && read_k < seg_reads;
    wire [15:0] seg_end = T_RCD + 8 * (seg_reads - 1) + 8 + T_RP + 1;

    wire [19:0] fetch_row = fetch_line * 3 + seg;           // bank 0 row
    wire [20:0] waddr = head[36:27] * STRIDE + head[26:16];  // y * STRIDE + x

    // ------------------------------------------------------------ scanout
    localparam integer LAT = 4;
    wire        wrap = cx >= frame_width - 11'(LAT);
    wire [10:0] ax   = wrap ? cx + 11'(LAT) - frame_width : cx + 11'(LAT);
    wire [9:0]  ay   = !wrap                   ? cy
                     : (cy == frame_height - 1) ? 10'd0
                     :                            cy + 10'd1;
    wire        in_screen = (ax < 11'(W)) && (ay < 10'(H));
    wire [11:0] raddr = in_screen ? ((ay[0] ? 12'(W) : 12'd0) + {1'b0, ax}) : 12'd0;


    integer k;

    always @(posedge clk) begin
        lb_we <= 1'b0;
        dq_r  <= sd_dq;
        {sd_cs_n, sd_ras_n, sd_cas_n, sd_we_n} <= C_NOP;
        sd_dqm <= 2'b00;
        dq_oe  <= 1'b0;

        // The raster's demands, sampled at the start of every line.
        if (ready && cx == 11'd0) begin
            if (cy + 10'd1 < H[9:0]) begin
                fetch_pending <= 1'b1;
                fetch_line    <= cy + 10'd1;
            end else if (cy == frame_height - 10'd1) begin
                fetch_pending <= 1'b1;
                fetch_line    <= 10'd0;
            end
            ref_owed <= (ref_owed > 4'd11) ? 4'd15 : ref_owed + 4'd4;
        end

        // Read data: a READ from READ_DELAY clocks ago starts eight captures.
        for (k = READ_DELAY - 1; k > 0; k = k - 1) begin
            rp_v[k]    <= rp_v[k-1];
            rp_d[k]    <= rp_d[k-1];
            rp_last[k] <= rp_last[k-1];
        end
        rp_v[0] <= 1'b0;
        if (cap_n != 4'd0) begin
            lb_we <= 1'b1;
            lb_wa <= cap_i;
            lb_wd <= dq_r;
            cap_i <= cap_i + 12'd1;
            cap_n <= cap_n - 4'd1;
            if (cap_n == 4'd1 && cap_last) begin
                ready_line[cap_line[0]] <= cap_line;
                if (cap_line == 10'd0) armed <= 1'b1;
            end
        end
        if (rp_v[READ_DELAY-1]) begin
            cap_i    <= rp_d[READ_DELAY-1];
            cap_n    <= 4'd8;
            cap_last <= rp_last[READ_DELAY-1];
            // The pipeline does not carry the line; captures run in issue
            // order, so it is the line being fetched, or the one just fetched.
        end

        // A line is shown before its fetch finished: the look-ahead has reached
        // its first pixel and the buffer does not hold it yet.
        if (armed && in_screen && ax == 11'd0 && ready_line[ay[0]] != ay)
            underrun <= 1'b1;

        if (!resetn) begin
            state <= S_INIT;
            tick  <= 16'd0;
            ready <= 1'b0;
            fetch_pending <= 1'b0;
            ref_owed <= 4'd0;
            underrun <= 1'b0;
            armed <= 1'b0;
            fetch_max <= 11'd0;
            cap_n <= 4'd0;
            rbin <= 0;
            rgray <= 0;
            ready_line[0] <= 10'h3FF;
            ready_line[1] <= 10'h3FF;
            for (k = 0; k < READ_DELAY; k = k + 1) rp_v[k] <= 1'b0;
            {sd_cs_n, sd_ras_n, sd_cas_n, sd_we_n} <= C_DESEL;
        end else begin
            tick <= tick + 16'd1;
            case (state)
                S_INIT: begin
                    {sd_cs_n, sd_ras_n, sd_cas_n, sd_we_n} <= C_DESEL;
                    if (tick == INIT_WAIT) begin
                        state <= S_PREALL;
                        tick  <= 16'd0;
                    end
                end
                S_PREALL: begin
                    if (tick == 16'd0) begin
                        {sd_cs_n, sd_ras_n, sd_cas_n, sd_we_n} <= C_PRE;
                        sd_a <= 13'h0400;           // A10: all banks
                    end else if (tick == T_RP + 1) begin
                        state <= S_INITREF;
                        tick  <= 16'd0;
                        n     <= 4'd8;
                    end
                end
                S_INITREF: begin
                    if (tick == 16'd0) begin
                        {sd_cs_n, sd_ras_n, sd_cas_n, sd_we_n} <= C_REF;
                    end else if (tick == T_RC + 1) begin
                        tick <= 16'd0;
                        n    <= n - 4'd1;
                        if (n == 4'd1) state <= S_MRS;
                    end
                end
                S_MRS: begin
                    if (tick == 16'd0) begin
                        {sd_cs_n, sd_ras_n, sd_cas_n, sd_we_n} <= C_MRS;
                        sd_a  <= MODE;
                        sd_ba <= 2'b00;
                    end else if (tick == T_MRD + 1) begin
                        state <= S_IDLE;
                        ready <= 1'b1;
                    end
                end
                S_IDLE: begin
                    tick <= 16'd0;
                    if (fetch_pending) begin
                        state <= S_FETCH;
                        fetch_pending <= 1'b0;
                        seg <= 2'd0;
                        fetch_len <= 11'd0;
                        cap_line <= fetch_line;
                    end else if (ref_owed != 4'd0) begin
                        state <= S_REF;
                        ref_owed <= ref_owed - 4'd1;
                    end else if (!empty) begin
                        state <= S_WRITE;
                    end
                end
                S_FETCH: begin
                    fetch_len <= fetch_len + 11'd1;
                    if (tick == 16'd0) begin
                        {sd_cs_n, sd_ras_n, sd_cas_n, sd_we_n} <= C_ACT;
                        sd_ba <= 2'b00;
                        sd_a  <= fetch_row[12:0];
                    end else if (read_tick) begin
                        {sd_cs_n, sd_ras_n, sd_cas_n, sd_we_n} <= C_READ;
                        sd_ba <= 2'b00;
                        // A10 on the row's last READ: auto-precharge.
                        sd_a  <= {2'b00, read_k == seg_reads - 9'd1, 1'b0, read_k[5:0], 3'b000};
                        rp_v[0]    <= 1'b1;
                        rp_d[0]    <= (fetch_line[0] ? 12'(W) : 12'd0) + {1'b0, seg, 9'd0} +
                                      {3'b000, read_k[5:0], 3'b000};
                        rp_last[0] <= seg == 2'd2 && read_k == seg_reads - 9'd1;
                    end else if (tick == seg_end) begin
                        tick <= 16'd0;
                        if (seg == 2'd2) begin
                            state <= S_IDLE;
                            if (fetch_len > fetch_max) fetch_max <= fetch_len;
                        end else begin
                            seg <= seg + 2'd1;
                        end
                    end
                end
                S_REF: begin
                    if (tick == 16'd0) begin
                        {sd_cs_n, sd_ras_n, sd_cas_n, sd_we_n} <= C_REF;
                    end else if (tick == T_RC + 1) begin
                        state <= S_IDLE;
                    end
                end
                S_WRITE: begin
                    if (tick == 16'd0) begin
                        if (head[26:16] < W[10:0] && head[36:27] < H[9:0]) begin
                            {sd_cs_n, sd_ras_n, sd_cas_n, sd_we_n} <= C_ACT;
                            sd_ba <= 2'b00;
                            sd_a  <= {1'b0, waddr[20:9]};
                        end else begin
                            // Off the picture: dropped.
                            state <= S_IDLE;
                            rbin  <= rbin + 1'b1;
                            rgray <= (rbin + 1'b1) ^ ((rbin + 1'b1) >> 1);
                        end
                    end else if (tick == T_RCD) begin
                        {sd_cs_n, sd_ras_n, sd_cas_n, sd_we_n} <= C_WRITE;
                        sd_ba  <= 2'b00;
                        sd_a   <= {2'b00, 1'b1, 1'b0, waddr[8:0]};   // A10: auto-precharge
                        sd_dqm <= 2'b00;
                        dq_o   <= head[15:0];
                        dq_oe  <= 1'b1;
                    end else if (tick == T_RCD + T_WR + T_RP + 1) begin
                        state <= S_IDLE;
                        rbin  <= rbin + 1'b1;
                        rgray <= (rbin + 1'b1) ^ ((rbin + 1'b1) >> 1);
                    end
                end
                default: state <= S_IDLE;
            endcase
        end
    end

    always @(posedge clk) begin
        if (lb_we) lb[lb_wa] <= lb_wd;
    end

    reg [11:0] raddr_r;
    reg [15:0] pix_r, pix_rr;
    always @(posedge clk) begin
        raddr_r <= raddr;
        pix_r   <= lb[raddr_r];
        pix_rr  <= pix_r;
        rgb     <= {pix_rr[15:11], pix_rr[15:13],
                    pix_rr[10:5],  pix_rr[10:9],
                    pix_rr[4:0],   pix_rr[4:2]};
    end

endmodule
