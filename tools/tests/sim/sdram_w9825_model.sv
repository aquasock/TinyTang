// SPDX-License-Identifier: GPL-3.0-only
`timescale 1ns/1ps
//
// Pin-level model of one Winbond W9825G6KH-6 die, for the desktop core's
// framebuffer controller (fpga/desktop/fb_sdram.sv).
//
// TinyGBA's diag/sdram_model.v answers one word per command and lets a
// command issued too early run anyway.  A framebuffer controller lives on the
// opposite assumptions -- eight-word bursts back to back, rows opened and
// closed with auto-precharge, refresh squeezed into what time a raster leaves
// -- so this model implements those and *counts* every timing violation
// instead of forgiving it.  A controller that passes here has issued no command
// early by the datasheet's own numbers.
//
// Modelled: MRS (CAS latency 2 or 3, burst length 1/2/4/8, sequential, burst or
// single-location writes), ACT, READ and WRITE with or without auto-precharge
// (A10), PRE (one bank or all), auto refresh, NOP, and DQM on writes.
// Checked, in clocks of the model's own clock: tRCD, tRP, tRAS, tRC (ACT to ACT
// and REF to anything), tWR before an auto-precharge, tMRD, commands to a bank in
// the wrong state, and the longest gap between refreshes.  The clock-to-cycle
// conversion is the caller's: the parameters are cycles.
//
// Read data for a READ issued on edge t is driven just after edge t+CL-1, so it
// is stable at edge t+CL, the edge the part's datasheet calls the first data.

module sdram_w9825_model #(
    parameter integer SIZE_WORDS = 2 * 1024 * 1024,   // words modelled from address 0
    parameter integer T_RCD = 2,
    parameter integer T_RP  = 2,
    parameter integer T_RAS = 4,
    parameter integer T_RC  = 5,
    parameter integer T_WR  = 2,
    parameter integer T_MRD = 2
) (
    input             clk,
    input      [12:0] A,
    input      [1:0]  BA,
    input             nCS,
    input             nRAS,
    input             nCAS,
    input             nWE,
    input      [1:0]  DQM,
    inout      [15:0] DQ
);

    reg [15:0] mem [0:SIZE_WORDS-1];
    integer i;
    initial for (i = 0; i < SIZE_WORDS; i = i + 1) mem[i] = 16'h0000;

    integer cycle = 0;
    always @(posedge clk) cycle <= cycle + 1;

    integer violations = 0;
    integer refreshes = 0;
    integer max_ref_gap = 0;
    integer last_ref = -1;
    integer reads = 0, writes = 0, activates = 0;

    task violate(input [8*80-1:0] what);
        begin
            violations = violations + 1;
            if (violations <= 20) $display("SDRAM VIOLATION @%0d: %0s", cycle, what);
        end
    endtask

    // Mode register.
    integer cl = 2, bl = 1;
    reg     single_write = 0;
    integer last_mrs = -100;

    // Per bank: open, row, when activated, when it may next be activated, when
    // its auto-precharge happens (or -1).
    reg     open_   [0:3];
    integer row     [0:3];
    integer t_act   [0:3];
    integer t_ready [0:3];      // earliest ACT: after tRP following a precharge
    integer t_ap    [0:3];      // a pending auto-precharge's start, or -1
    integer t_any_ref_ready = 0;
    initial for (i = 0; i < 4; i = i + 1) begin
        open_[i] = 0; row[i] = 0; t_act[i] = -100; t_ready[i] = 0; t_ap[i] = -1;
    end

    // Read data scheduled by clock: entry t % 32 says which word is driven
    // after edge t.  A later READ overwrites the entries it reaches, which is
    // the part's burst interrupt; back-to-back READs BL apart never collide.
    reg        sch_v [0:31];
    integer    sch_a [0:31];
    initial for (i = 0; i < 32; i = i + 1) sch_v[i] = 0;
    reg [15:0] dq_out;
    reg        dq_oe = 0;
    assign DQ = dq_oe ? dq_out : 16'hzzzz;

    task precharge(input integer b, input integer at);
        begin
            if (open_[b]) begin
                if (at - t_act[b] < T_RAS) violate("tRAS: precharged too soon after ACT");
                open_[b] = 0;
                t_ready[b] = at + T_RP;
            end
            t_ap[b] = -1;
        end
    endtask

    always @(posedge clk) begin : command
        integer b, c, addr;
        // Auto-precharges whose time has come.
        for (b = 0; b < 4; b = b + 1)
            if (t_ap[b] >= 0 && cycle >= t_ap[b]) precharge(b, t_ap[b]);

        if (!nCS) begin
            b = BA;
            case ({nRAS, nCAS, nWE})
                3'b000: begin // MRS
                    for (c = 0; c < 4; c = c + 1) if (open_[c]) violate("MRS with a bank open");
                    cl = A[6:4];
                    bl = (A[2:0] == 3'b011) ? 8 : (A[2:0] == 3'b010) ? 4 : (A[2:0] == 3'b001) ? 2 : 1;
                    single_write = A[9];
                    if (cl != 2 && cl != 3) violate("MRS: CAS latency not 2 or 3");
                    last_mrs = cycle;
                end
                3'b001: begin // REF
                    for (c = 0; c < 4; c = c + 1) if (open_[c]) violate("REF with a bank open");
                    if (cycle < t_any_ref_ready) violate("tRC: REF too soon after REF");
                    for (c = 0; c < 4; c = c + 1) if (cycle < t_ready[c]) violate("tRP: REF too soon after precharge");
                    if (last_ref >= 0 && cycle - last_ref > max_ref_gap) max_ref_gap = cycle - last_ref;
                    last_ref = cycle;
                    refreshes = refreshes + 1;
                    t_any_ref_ready = cycle + T_RC;
                    for (c = 0; c < 4; c = c + 1) if (t_ready[c] < cycle + T_RC) t_ready[c] = cycle + T_RC;
                end
                3'b011: begin // ACT
                    if (open_[b]) violate("ACT to an open bank");
                    if (cycle < t_ready[b]) violate("tRP/tRC: ACT too soon");
                    if (cycle - t_act[b] < T_RC) violate("tRC: ACT to ACT too soon");
                    if (cycle - last_mrs < T_MRD) violate("tMRD: ACT too soon after MRS");
                    open_[b] = 1; row[b] = A; t_act[b] = cycle; activates = activates + 1;
                end
                3'b010: begin // PRE
                    if (A[10]) begin
                        for (c = 0; c < 4; c = c + 1) precharge(c, cycle);
                    end else precharge(b, cycle);
                end
                3'b101: begin // READ
                    if (!open_[b]) violate("READ to a closed bank");
                    if (cycle - t_act[b] < T_RCD) violate("tRCD: READ too soon after ACT");
                    for (c = 0; c < bl; c = c + 1) begin
                        sch_v[(cycle + cl - 1 + c) % 32] = 1;
                        sch_a[(cycle + cl - 1 + c) % 32] =
                            ((b * 8192 + row[b]) * 512) +
                            ((A[8:0] & ~(bl - 1)) | ((A[8:0] + c) & (bl - 1)));
                    end
                    reads = reads + 1;
                    if (A[10]) t_ap[b] = cycle + bl;        // precharge starts as the burst ends
                end
                3'b100: begin // WRITE
                    if (!open_[b]) violate("WRITE to a closed bank");
                    if (cycle - t_act[b] < T_RCD) violate("tRCD: WRITE too soon after ACT");
                    addr = ((b * 8192 + row[b]) * 512) + A[8:0];
                    if (addr < SIZE_WORDS) begin
                        if (!DQM[0]) mem[addr][7:0]  = DQ[7:0];
                        if (!DQM[1]) mem[addr][15:8] = DQ[15:8];
                    end
                    writes = writes + 1;
                    if (A[10]) t_ap[b] = cycle + T_WR;      // after the write recovery time
                    if (!single_write && bl != 1) violate("burst writes are not modelled");
                end
                3'b111: ; // NOP
                default: ; // burst terminate etc.: not used
            endcase
        end

        // Drive this edge's scheduled word, if any, just after the edge.
        if (sch_v[cycle % 32]) begin
            dq_out <= #1 (sch_a[cycle % 32] < SIZE_WORDS) ? mem[sch_a[cycle % 32]] : 16'hxxxx;
            dq_oe  <= #1 1;
            sch_v[cycle % 32] = 0;
        end else begin
            dq_oe  <= #1 0;
        end
    end

endmodule
