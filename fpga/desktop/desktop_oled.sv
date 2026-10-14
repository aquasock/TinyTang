// SPDX-License-Identifier: GPL-3.0-only
// A separate text terminal in one of three fonts; SPI scanout uses no
// desktop bitmap.  font_mode 0 is Graphics, the original 4x4 glyphs in a
// 24x16 grid; 1 is Standard, the desktop's 8x8 font in 12x8; 2 is Compact,
// original 6x8 glyphs in 16x8.  The cell store keeps a stride of 24 in every
// mode, so a cell's index is row*24+column whichever grid is showing.
module desktop_oled (
    input logic control_clk, pixel_clk, resetn, selected,
    input logic [1:0] font_mode,
    input logic cell_we,
    input logic [8:0] cell_index,
    input logic [15:0] cell_word,
    input logic [16:0] cursor_word,
    output logic [7:0] lane_o, lane_oe,
    output logic [31:0] frames, signature
);
    `include "oled_font.vh"
    `include "oled_font_compact.vh"
    `include "oled_font_standard.vh"
    logic [15:0] cells[0:383];
    initial for (integer i=0;i<384;i=i+1) cells[i]=16'h0720;
    always_ff @(posedge control_clk)
        if(cell_we && cell_index<384) cells[cell_index]<=cell_word;

    function automatic [15:0] palette(input [3:0] c);
        case(c)
            0:palette=16'h0000; 1:palette=16'hc800;
            2:palette=16'h0660; 3:palette=16'hce60;
            4:palette=16'h001d; 5:palette=16'hc819;
            6:palette=16'h0679; 7:palette=16'he73c;
            8:palette=16'h7bef; 9:palette=16'hf800;
            10:palette=16'h07e0; 11:palette=16'hffe0;
            12:palette=16'h5aff; 13:palette=16'hf81f;
            14:palette=16'h07ff; 15:palette=16'hffff;
        endcase
    endfunction
    wire [6:0] x;
    wire [5:0] y;
    // Compact cells are six pixels wide: (x*171)>>10 is x/6 exactly for
    // every x from 0 to 95, without a divider.
    wire [13:0] x_sixths=x*14'd171;
    wire [4:0] compact_col=x_sixths[13:10];
    wire [2:0] compact_gx=x-{compact_col,2'b0}-{compact_col,1'b0};
    logic [4:0] col;
    logic [3:0] row;
    logic [2:0] gx_now,gy_now;
    always_comb
        case(font_mode)
            2'd1: begin col={2'b0,x[6:3]}; row={1'b0,y[5:3]}; gx_now=x[2:0]; gy_now=y[2:0]; end
            2'd2: begin col=compact_col; row={1'b0,y[5:3]}; gx_now=compact_gx; gy_now=y[2:0]; end
            default: begin col=x[6:2]; row=y[5:2]; gx_now={1'b0,x[1:0]}; gy_now={1'b0,y[1:0]}; end
        endcase
    wire [8:0] address={5'b0,row}*9'd24+{4'b0,col};
    logic [15:0] cell_q, pixel;
    logic [2:0] gx,gy;
    logic [1:0] mode_q;
    // Held cursor bus uses the same two-flop toggle acknowledgement as the
    // socket declaration (see desktop_pmod); it changes only at vertical blank,
    // and so does font_mode.
    logic cursor_here;
    wire [7:0] ch=cell_q[7:0];
    wire [15:0] glyph4=oled_glyph(ch);
    wire [47:0] glyph6=oled_glyph_compact(ch);
    // The desktop's 8x8 font, converted by tools/make_oled_font.py; DEL, which
    // the terminal sends for any character it cannot show, is a box in all
    // three sets.
    wire [63:0] glyph8=oled_glyph_standard(ch);
    logic ink;
    always_comb
        case(mode_q)
            2'd1: ink=glyph8[6'd63-{gy,3'b0}-{3'b0,gx}];
            2'd2: ink=glyph6[6'd47-{gy,2'b0}-{gy,1'b0}-{3'b0,gx}];
            default: ink=glyph4[15-{gy[1:0],gx[1:0]}];
        endcase
    always_ff @(posedge pixel_clk) begin
        cell_q<=cells[address];
        gx<=gx_now; gy<=gy_now; mode_q<=font_mode;
        cursor_here<=cursor_word[16] && cursor_word[7:0]=={3'b0,col} &&
            cursor_word[15:8]=={4'b0,row};
        pixel<=palette((ink ^ cursor_here) ? cell_q[11:8]:cell_q[15:12]);
    end
    wire cs_n,mosi,sck,dc,res_n,vccen,pmoden,frame_start,px_strobe;
    wire [15:0] sampled_pixel;
    // 75 rounds 74.25 MHz upward for minimum power delays; divider 6 gives
    // 161.6 ns SCK periods, satisfying the documented 150 ns minimum.
    oled_panel #(.CLK_MHZ(75),.SPI_DIV(6)) panel (
        .clk(pixel_clk),.rst(!resetn || !selected),.px_x(x),.px_y(y),.px_data(pixel),
        .cs_n(cs_n),.mosi(mosi),.sck(sck),.dc(dc),.res_n(res_n),
        .vccen(vccen),.pmoden(pmoden),.frame_start(frame_start),
        .px_strobe(px_strobe),.sampled_pixel(sampled_pixel)
    );
    assign lane_o={pmoden,vccen,res_n,dc,sck,1'b0,mosi,cs_n};
    assign lane_oe=8'hfb;

    function automatic [31:0] crc_byte(input [31:0] old,input [7:0] byte_value);
        reg [31:0] c;
        begin
            c=old^{24'b0,byte_value};
            for(integer k=0;k<8;k=k+1)c=c[0] ? (c>>1)^32'hedb88320:c>>1;
            crc_byte=c;
        end
    endfunction
    logic [31:0] crc,frame_counter,frame_gray,panel_signature;
    always_ff @(posedge pixel_clk) begin
        if(!resetn) begin
            crc<=32'hffffffff; frame_counter<=0; frame_gray<=0; panel_signature<=0;
        end else if(!selected) crc<=32'hffffffff;
        else begin
            if(px_strobe) crc<=crc_byte(crc_byte(crc,sampled_pixel[15:8]),sampled_pixel[7:0]);
            if(frame_start) begin
                panel_signature<=~crc;
                crc<=32'hffffffff;
                frame_counter<=frame_counter+32'd1;
                frame_gray<=((frame_counter+32'd1)>>1)^(frame_counter+32'd1);
            end
        end
    end
    (* ASYNC_REG="TRUE" *) logic [31:0] gray_meta,gray_sync;
    logic [31:0] gray_seen;
    function automatic [31:0] from_gray(input [31:0] g);
        reg [31:0] b;
        begin b[31]=g[31];for(integer i=30;i>=0;i=i-1)b[i]=b[i+1]^g[i];from_gray=b;end
    endfunction
    // The CRC is held between frames. Observe its Gray-coded frame event
    // through synchronizers before sampling that stable bundled value.
    always_ff @(posedge control_clk) begin
        if(!resetn) begin
            gray_meta<=0;gray_sync<=0;gray_seen<=0;frames<=0;signature<=0;
        end else begin
            gray_meta<=frame_gray;gray_sync<=gray_meta;
            if(gray_sync!=gray_seen) begin
                gray_seen<=gray_sync;frames<=from_gray(gray_sync);signature<=panel_signature;
            end
        end
    end
endmodule
