// SPDX-License-Identifier: GPL-3.0-only
`timescale 1ns/1ps
module tb_desktop_oled;
    logic clk=0,control_clk=0,resetn=0,selected=0,we=0;
    always #6.734 clk=~clk;
    always #23.265 control_clk=~control_clk;
    logic [8:0] address=0;
    logic [15:0] word=0;
    wire [7:0] pins,enables;
    wire [31:0] frames,signature;
    desktop_oled dut(.control_clk(control_clk),.pixel_clk(clk),.resetn(resetn),.selected(selected),
        .cell_we(we),.cell_index(address),.cell_word(word),.cursor_word(17'h10101),
        .lane_o(pins),.lane_oe(enables),.frames(frames),.signature(signature));
    task automatic write_cell(input [8:0] a,input [15:0] d);
        @(negedge control_clk);address=a;word=d;we=1;
        @(negedge control_clk);we=0;
    endtask
    // Compare the serialized pixels to literal reference glyphs, independent
    // of the DUT font function and cell address/palette pipelines.
    function automatic [15:0] expected_pixel(input integer n);
        integer x,y,cell_no,bitno;
        reg [15:0] glyph,fg,bg;
        begin
            x=n%96;y=n/96;cell_no=(y/4)*24+x/4;bitno=15-(y%4)*4-x%4;
            glyph=0;fg=16'he73c;bg=0;
            if(cell_no==0)begin glyph=16'h69f9;fg=16'hffff;end
            if(cell_no==1)begin glyph=16'heaea;fg=16'h07e0;end
            if(cell_no==383)begin glyph=16'hf99f;fg=16'h07ff;bg=16'hc800;end
            expected_pixel=(glyph[bitno] ^ (cell_no==25)) ? fg:bg;
        end
    endfunction
    logic [7:0] byte_value=0;
    integer bits=0,commands=0,data_bytes=0,file;
    logic [7:0] hi;
    realtime previous_edge=0,power_time=0,vcc_time=0,reset_low=0;
    localparam [359:0] INIT=360'hfd12aea072a100a200a4a83fad8eb00bb131b3f08a648b788c64bb3abe3e870681918250837d2e2500005f3faf;
    localparam [47:0] WINDOW=48'h15005f75003f;
    always @(posedge pins[7]) if(resetn && selected)power_time=$realtime;
    always @(negedge pins[5]) if(resetn && selected) begin
        if($realtime-power_time<20_000_000)$fatal(1,"power delay too short");
        reset_low=$realtime;
    end
    always @(posedge pins[5]) if(reset_low>0 && $realtime-reset_low<3_000)$fatal(1,"reset pulse too short");
    always @(posedge pins[6])vcc_time=$realtime;
    always @(posedge pins[3]) if(resetn && selected && !pins[0]) begin
        if(previous_edge && $realtime-previous_edge<150)$fatal(1,"SCK exceeds SSD1331 limit");
        previous_edge=$realtime;
        byte_value={byte_value[6:0],pins[1]};bits++;
        if(bits==8) begin
            bits=0;
            if(!pins[4]) begin
                if(commands<45) begin
                    if(byte_value!==INIT[359-commands*8 -:8])$fatal(1,"init command %0d mismatch %02h",commands,byte_value);
                    if(commands==44 && $realtime-vcc_time<25_000_000)$fatal(1,"VCC delay too short");
                end else if(byte_value!==WINDOW[47-((commands-45)%6)*8 -:8])$fatal(1,"window command mismatch");
                commands++;
            end else if(data_bytes<12288) begin
                if(commands!=51)$fatal(1,"pixels before complete initialization");
                if((data_bytes%2)==0)hi=byte_value;
                else if({hi,byte_value}!==expected_pixel(data_bytes/2))
                    $fatal(1,"OLED pixel %0d got %04h expected %04h",data_bytes/2,{hi,byte_value},expected_pixel(data_bytes/2));
                $fdisplay(file,"%02h",byte_value);data_bytes++;
            end
        end
    end
    // Separate live-update test changes a pixel between its two SPI bytes.
    logic small_reset=1;
    logic [15:0] live_pixel=16'ha1b2;
    wire small_sck,small_mosi,small_dc,small_cs,small_strobe;
    oled_panel #(.CLK_MHZ(1),.SPI_DIV(6),.W(1),.H(1)) live(
        .clk(clk),.rst(small_reset),.px_data(live_pixel),.px_x(),.px_y(),
        .cs_n(small_cs),.mosi(small_mosi),.sck(small_sck),.dc(small_dc),
        .res_n(),.vccen(),.pmoden(),.frame_start(),.px_strobe(small_strobe),.sampled_pixel());
    always @(posedge clk) if(small_strobe)live_pixel<=16'hc3d4;
    integer small_bits=0,small_bytes=0;
    logic [7:0] small_byte=0,small_hi;
    always @(posedge small_sck)if(!small_reset && !small_cs && small_dc)begin
        small_byte={small_byte[6:0],small_mosi};small_bits++;
        if(small_bits==8)begin
            small_bits=0;
            if(small_bytes==0)small_hi=small_byte;
            if(small_bytes==1 && {small_hi,small_byte}!=16'ha1b2)$fatal(1,"live update split RGB565 pixel");
            small_bytes++;
        end
    end
    initial begin
        file=$fopen("oled-spi.hex","w");
        #500;resetn=1;small_reset=0;
        write_cell(0,16'h0f41);write_cell(1,16'h0a42);write_cell(383,16'h1e7f);
        selected=1;
        wait(data_bytes==12288);
        wait(frames==1);#100;
        $fclose(file);$display("OLED_SPI_CRC %08h",signature);
        if(small_bytes<2)$fatal(1,"live update test incomplete");
        if(enables!==8'hfb)$fatal(1,"OLED NC lane driven");
        selected=0;repeat(10)@(posedge clk);
        if(frames!=1)$fatal(1,"selection reset broke monotonic frame counter");
        $display("OLED: power, SPI timing, init, every pixel, colours, cursor, CRC and coherent live pixel PASS");
        $finish;
    end
    initial begin #210_000_000;$fatal(1,"OLED timeout");end
endmodule
