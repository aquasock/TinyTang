// SPDX-License-Identifier: GPL-3.0-only
// A separate 24x16 text terminal; SPI scanout uses no desktop bitmap.
module desktop_oled (
    input logic control_clk, pixel_clk, resetn, selected,
    input logic cell_we,
    input logic [8:0] cell_index,
    input logic [15:0] cell_word,
    input logic [16:0] cursor_word,
    output logic [7:0] lane_o, lane_oe,
    output logic [31:0] frames, signature
);
    `include "oled_font.vh"
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
    wire [8:0] address={5'b0,y[5:2]}*9'd24+{4'b0,x[6:2]};
    logic [15:0] cell_q, pixel;
    logic [1:0] gx,gy;
    // Held cursor bus uses the same two-flop toggle acknowledgement as the
    // socket declaration (see desktop_pmod); it changes only at vertical blank.
    logic cursor_here;
    wire [15:0] glyph=oled_glyph(cell_q[7:0]);
    wire ink=glyph[15-{gy,gx}];
    always_ff @(posedge pixel_clk) begin
        cell_q<=cells[address];
        gx<=x[1:0]; gy<=y[1:0];
        cursor_here<=cursor_word[16] && cursor_word[7:0]==x[6:2] &&
            cursor_word[15:8]==y[5:2];
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
