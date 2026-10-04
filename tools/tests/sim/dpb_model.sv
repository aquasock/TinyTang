// A behavioural stand-in for Gowin's DPB block, for simulation only.
//
// nestang's legacy text page is built on a generated Gowin BRAM whose
// behaviour Verilator cannot know.  The desktop layer does not use it, but the
// module that decodes the desktop's commands also instantiates the legacy page
// -- they are one `iosys_bl616` -- so a model is needed to exercise the decode
// at all.  This is two independent ports over one memory, which is what DPB is.
//
// The parameters are declared only so the vendor wrapper's `defparam`s resolve;
// none of them change what this model does.  It is deliberately left
// uninitialised: the real one is loaded with the ASCII font from font.mi, and
// nothing this test checks depends on that -- the desktop layer takes its
// glyphs from font.vh directly.
module DPB (
    output [15:0] DOA,
    output [15:0] DOB,
    input         CLKA,
    input         OCEA,
    input         CEA,
    input         RESETA,
    input         WREA,
    input         CLKB,
    input         OCEB,
    input         CEB,
    input         RESETB,
    input         WREB,
    input  [2:0]  BLKSELA,
    input  [2:0]  BLKSELB,
    input  [13:0] ADA,
    input  [13:0] ADB,
    input  [15:0] DIA,
    input  [15:0] DIB
);

    parameter         READ_MODE0 = 1'b0;
    parameter         READ_MODE1 = 1'b0;
    parameter [1:0]   WRITE_MODE0 = 2'b00;
    parameter [1:0]   WRITE_MODE1 = 2'b00;
    parameter integer BIT_WIDTH_0 = 8;
    parameter integer BIT_WIDTH_1 = 8;
    parameter [2:0]   BLK_SEL_0 = 3'b000;
    parameter [2:0]   BLK_SEL_1 = 3'b000;
    parameter         RESET_MODE = "SYNC";
    parameter [255:0] INIT_RAM_00 = 256'h0;
    parameter [255:0] INIT_RAM_01 = 256'h0;
    parameter [255:0] INIT_RAM_02 = 256'h0;
    parameter [255:0] INIT_RAM_03 = 256'h0;
    parameter [255:0] INIT_RAM_04 = 256'h0;
    parameter [255:0] INIT_RAM_05 = 256'h0;
    parameter [255:0] INIT_RAM_06 = 256'h0;
    parameter [255:0] INIT_RAM_07 = 256'h0;
    parameter [255:0] INIT_RAM_08 = 256'h0;
    parameter [255:0] INIT_RAM_09 = 256'h0;
    parameter [255:0] INIT_RAM_0A = 256'h0;
    parameter [255:0] INIT_RAM_0B = 256'h0;
    parameter [255:0] INIT_RAM_0C = 256'h0;
    parameter [255:0] INIT_RAM_0D = 256'h0;
    parameter [255:0] INIT_RAM_0E = 256'h0;
    parameter [255:0] INIT_RAM_0F = 256'h0;
    parameter [255:0] INIT_RAM_10 = 256'h0;
    parameter [255:0] INIT_RAM_11 = 256'h0;
    parameter [255:0] INIT_RAM_12 = 256'h0;
    parameter [255:0] INIT_RAM_13 = 256'h0;
    parameter [255:0] INIT_RAM_14 = 256'h0;
    parameter [255:0] INIT_RAM_15 = 256'h0;
    parameter [255:0] INIT_RAM_16 = 256'h0;
    parameter [255:0] INIT_RAM_17 = 256'h0;
    parameter [255:0] INIT_RAM_18 = 256'h0;
    parameter [255:0] INIT_RAM_19 = 256'h0;
    parameter [255:0] INIT_RAM_1A = 256'h0;
    parameter [255:0] INIT_RAM_1B = 256'h0;
    parameter [255:0] INIT_RAM_1C = 256'h0;
    parameter [255:0] INIT_RAM_1D = 256'h0;
    parameter [255:0] INIT_RAM_1E = 256'h0;
    parameter [255:0] INIT_RAM_1F = 256'h0;
    parameter [255:0] INIT_RAM_20 = 256'h0;
    parameter [255:0] INIT_RAM_21 = 256'h0;
    parameter [255:0] INIT_RAM_22 = 256'h0;
    parameter [255:0] INIT_RAM_23 = 256'h0;
    parameter [255:0] INIT_RAM_24 = 256'h0;
    parameter [255:0] INIT_RAM_25 = 256'h0;
    parameter [255:0] INIT_RAM_26 = 256'h0;
    parameter [255:0] INIT_RAM_27 = 256'h0;
    parameter [255:0] INIT_RAM_28 = 256'h0;
    parameter [255:0] INIT_RAM_29 = 256'h0;
    parameter [255:0] INIT_RAM_2A = 256'h0;
    parameter [255:0] INIT_RAM_2B = 256'h0;
    parameter [255:0] INIT_RAM_2C = 256'h0;
    parameter [255:0] INIT_RAM_2D = 256'h0;
    parameter [255:0] INIT_RAM_2E = 256'h0;
    parameter [255:0] INIT_RAM_2F = 256'h0;
    parameter [255:0] INIT_RAM_30 = 256'h0;
    parameter [255:0] INIT_RAM_31 = 256'h0;
    parameter [255:0] INIT_RAM_32 = 256'h0;
    parameter [255:0] INIT_RAM_33 = 256'h0;
    parameter [255:0] INIT_RAM_34 = 256'h0;
    parameter [255:0] INIT_RAM_35 = 256'h0;
    parameter [255:0] INIT_RAM_36 = 256'h0;
    parameter [255:0] INIT_RAM_37 = 256'h0;
    parameter [255:0] INIT_RAM_38 = 256'h0;
    parameter [255:0] INIT_RAM_39 = 256'h0;
    parameter [255:0] INIT_RAM_3A = 256'h0;
    parameter [255:0] INIT_RAM_3B = 256'h0;
    parameter [255:0] INIT_RAM_3C = 256'h0;
    parameter [255:0] INIT_RAM_3D = 256'h0;
    parameter [255:0] INIT_RAM_3E = 256'h0;
    parameter [255:0] INIT_RAM_3F = 256'h0;

    reg [7:0] mem [0:2047];
    reg [7:0] qa;
    reg [7:0] qb;

    always @(posedge CLKA) begin
        if (CEA) begin
            if (WREA) mem[ADA[10:0]] <= DIA[7:0];
            else      qa <= mem[ADA[10:0]];
        end
    end

    always @(posedge CLKB) begin
        if (CEB) begin
            if (WREB) mem[ADB[10:0]] <= DIB[7:0];
            else      qb <= mem[ADB[10:0]];
        end
    end

    assign DOA = { 8'h00, qa };
    assign DOB = { 8'h00, qb };

endmodule
