// TinyTang desktop clock monitor. SPDX-License-Identifier: GPL-3.0-only
//
// A cycle counter per clock domain, so the clocks this design runs on can be
// measured on silicon rather than inferred from a timing model.  Core-log
// entry 57 chose the instrument and this is it: no processor, no fabric
// accelerator, just counters a host can read back.
//
// Why counters and not bits.  A single flop only says a clock is alive; a
// counter says at what rate, and the rate is the question.  The design's main
// clock `clk` (21.492 MHz) and pixel clock `hclk` (74.25 MHz) both come from
// PLLs referenced to the 50 MHz `sys_clk` crystal, so counting each against
// `sys_clk` measures whether each is where it should be -- and whether a
// region the fabric route failed to reach is running at all.
//
// Two probes per fabric-routed clock.  `clk` and `hclk` ride general fabric
// on this die, because the chipdb's HCLK interconnect is not modelled, and the
// model claims 943 `clk` sinks fall back to it (see
// evidence/clock-fabric-fallback-origin.txt).  Each of those clocks therefore
// gets *two* counters, A and B, so a host can read both: if the clock reaches
// both regions their deltas agree, and if it does not, one of them stands
// still.  The placer chooses where A and B land; nothing here is pinned, so
// the pair is a spontaneous spread rather than a deliberate one, and where
// they actually landed is read from the place-and-route report.
//
// Domain crossing.  Only `clk` is the register domain -- the counters are read
// through `desktop_regs`, which runs on `clk`.  The `clk` counters are read
// directly.  The `hclk` and `sys_clk` counters are free-running in their own
// domains and cross into `clk` as *gray code*: a gray counter changes one bit
// per increment, so the two-flop synchroniser captures either the old value or
// the new one and never a mixture, which is what a binary counter would hand
// it mid-carry.  The host reads each counter twice and differences the two
// readings, so the synchroniser's constant latency cancels out.
//
// What this cannot do.  A counter proves a *rate*, not a phase: it does not
// see skew, only whether the clock is there and how fast.  The vertical-line
// hunt already measured skew directly (tools/clock-skew-report.py) and found
// the pixel clock at 0.209 ns in every build, so rate is the open question
// this instrument closes.

module clock_monitor (
    input  logic        clk,       // main logic clock, 21.492 MHz: the read domain
    input  logic        hclk,      // pixel clock, 74.25 MHz
    input  logic        ref_clk,   // 50 MHz crystal (sys_clk): the time reference
    output logic [31:0] clk_a,     // `clk` cycles, probe A
    output logic [31:0] clk_b,     // `clk` cycles, probe B
    output logic [31:0] pix_a,     // `hclk` cycles, probe A, synchronised to clk
    output logic [31:0] pix_b,     // `hclk` cycles, probe B, synchronised to clk
    output logic [31:0] ref_a      // `ref_clk` cycles, synchronised to clk
);

    // ---------------------------------------------------------------- gray
    // binary -> gray (one bit per increment) and back.  The reverse is the
    // standard XOR-doubling decode: g ^ (g>>1) ^ (g>>2) ... ^ (g>>31).
    function automatic [31:0] bin2gray(input [31:0] b);
        bin2gray = b ^ (b >> 1);
    endfunction

    function automatic [31:0] gray2bin(input [31:0] g);
        logic [31:0] v;
        begin
            v = g;
            for (int i = 1; i < 32; i = i << 1) v = v ^ (v >> i);
            gray2bin = v;
        end
    endfunction

    // ------------------------------------------------------------- counters
    // Free-running, no reset: a Gowin flop comes up 0 after configuration, and
    // resetting would only add a net to a design that already has to fit.  A
    // 32-bit counter wraps after ~58 s at 74.25 MHz, far longer than the two
    // readings a measurement needs, and the host differences them modulo 2^32.
    logic [31:0] clk_ca, clk_cb;   // in the clk domain
    logic [31:0] pix_ca, pix_cb;   // in the hclk domain
    logic [31:0] ref_c;            // in the ref_clk domain

    always_ff @(posedge clk)     clk_ca <= clk_ca + 32'd1;
    always_ff @(posedge clk)     clk_cb <= clk_cb + 32'd1;
    always_ff @(posedge hclk)    pix_ca <= pix_ca + 32'd1;
    always_ff @(posedge hclk)    pix_cb <= pix_cb + 32'd1;
    always_ff @(posedge ref_clk) ref_c  <= ref_c  + 32'd1;

    assign clk_a = clk_ca;
    assign clk_b = clk_cb;

    // The hclk and ref_clk counters are read from the clk domain.  The gray
    // value is formed in its own domain and taken through two flops; ASYNC_REG
    // tells the placer these two are the synchroniser, so it keeps the first
    // one close to the crossing rather than optimising it away.
    (* ASYNC_REG = "TRUE" *) logic [31:0] pix_g1a, pix_g2a;
    (* ASYNC_REG = "TRUE" *) logic [31:0] pix_g1b, pix_g2b;
    (* ASYNC_REG = "TRUE" *) logic [31:0] ref_g1,  ref_g2;

    always_ff @(posedge clk) begin
        pix_g1a <= bin2gray(pix_ca);
        pix_g2a <= pix_g1a;
        pix_g1b <= bin2gray(pix_cb);
        pix_g2b <= pix_g1b;
        ref_g1  <= bin2gray(ref_c);
        ref_g2  <= ref_g1;
    end

    assign pix_a = gray2bin(pix_g2a);
    assign pix_b = gray2bin(pix_g2b);
    assign ref_a = gray2bin(ref_g2);

endmodule
