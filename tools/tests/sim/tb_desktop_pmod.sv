// SPDX-License-Identifier: GPL-3.0-only
`timescale 1ns/1ps
module tb_desktop_pmod;
    logic pixel_clk=0, resetn=0;
    always #6.734 pixel_clk=~pixel_clk;
    logic [10:0] cx=0;
    logic [9:0] cy=0;
    logic [23:0] rgb=24'h123456;
    logic [15:0] word=0;
    logic request=0;
    wire ack;
    tri [7:0] p0,p1;
    desktop_pmod dut(.control_clk(pixel_clk),.pixel_clk(pixel_clk),.resetn(resetn),.cx(cx),.cy(cy),
        .rgb(rgb),.socket_word(word),.socket_request(request),.socket_ack(ack),
        .pmod0_io(p0),.pmod1_io(p1),
        .oled_cell_we(1'b0),.oled_cell_index(9'd0),.oled_cell_word(16'd0),
        .oled_cursor(17'd0),.oled_frames(),.oled_signature());
    task automatic sample(input integer x,input integer y);
        @(negedge pixel_clk); cx=11'(x); cy=10'(y);
        @(posedge pixel_clk); #1;
    endtask
    task automatic apply(input [15:0] next_word);
        reg old_ack;
        begin
            old_ack=ack;
            sample(50,50); word=next_word; request=~request;
            repeat(8) @(negedge pixel_clk);
            if(ack!=old_ack) $fatal(1,"socket change before vertical blank");
            sample(0,720);
            @(negedge pixel_clk); #1;
            if(ack!=request) $fatal(1,"socket acknowledgement absent at frame boundary");
            sample(50,50);
        end
    endtask
    function automatic [7:0] physical(input [7:0] lanes,input bit flip);
        // Explicit board wiring rather than reusing the driver's index formula.
        reg [7:0] ordered;
        begin
            ordered=flip ? {lanes[3:0],lanes[7:4]}:lanes;
            physical={ordered[7],ordered[3],ordered[6],ordered[2],
                      ordered[5],ordered[1],ordered[4],ordered[0]};
        end
    endfunction
    task automatic check_outputs(input [15:0] config_word);
        reg [7:0] a,b,en_a,en_b,exp_a,exp_b;
        bit visible,hs,vs;
        begin
            visible=cx<1280 && cy<720;
            hs=cx>=1390 && cx<1430;
            vs=cy>=725 && cy<730;
            a=config_word[7:4]==2 ? (visible ? 8'h51:8'h00):
                {2'b0,vs,hs,visible ? 4'h3:4'h0};
            b=config_word[11:8]==2 ? (visible ? 8'h51:8'h00):
                {2'b0,vs,hs,visible ? 4'h3:4'h0};
            en_a=config_word[7:4]==2 ? 8'hff:config_word[7:4]==3 ? 8'h3f:8'h00;
            en_b=config_word[11:8]==2 ? 8'hff:config_word[11:8]==3 ? 8'h3f:8'h00;
            a=physical(a,config_word[12]); b=physical(b,config_word[13]);
            en_a=physical(en_a,config_word[12]); en_b=physical(en_b,config_word[13]);
            for(integer i=0;i<8;i=i+1) begin
                exp_a[i]=en_a[i] ? a[i]:1'bz;
                exp_b[i]=en_b[i] ? b[i]:1'bz;
            end
            if(p0!==exp_a || p1!==exp_b)
                $fatal(1,"socket mismatch at %0d,%0d word=%04h got=%b %b expected=%b %b",
                    cx,cy,config_word,p0,p1,exp_a,exp_b);
        end
    endtask
    integer hs_pixels=0,vs_pixels=0,active_pixels=0;
    initial begin
        #100;
        if(p0!==8'hzz || p1!==8'hzz) $fatal(1,"reset drove a PMOD pin");
        resetn=1;
        apply(16'h0230); check_outputs(16'h0230);
        apply(16'h0320); check_outputs(16'h0320);
        apply(16'h1230); check_outputs(16'h1230);
        apply(16'h2320); check_outputs(16'h2320);
        apply(16'h3320); check_outputs(16'h3320);
        apply(0); check_outputs(0);
        apply(16'h0050); check_outputs(16'h0050); // unsupported personality released
        apply(16'h0230);
        // Icarus preserves Z on the physical pin model. Check the entire
        // raster's blanking and sync while changing only the coordinates.
        for(integer y=0;y<750;y=y+1) begin
            for(integer x=0;x<1650;x=x+1) begin
                sample(x,y); check_outputs(16'h0230);
                if(p0[1]===1) hs_pixels=hs_pixels+1; // J2 pin 7 = IO1
                if(p0[3]===1) vs_pixels=vs_pixels+1; // J2 pin 8 = IO3
                if(x<1280 && y<720) active_pixels=active_pixels+1;
            end
        end
        if(hs_pixels!=40*750 || vs_pixels!=5*1650 || active_pixels!=1280*720)
            $fatal(1,"raster mismatch: H=%0d V=%0d active=%0d",hs_pixels,vs_pixels,active_pixels);
        resetn=0; @(posedge pixel_clk); #1;
        if(p0!==8'hzz || p1!==8'hzz) $fatal(1,"reset did not release sockets");
        $display("desktop PMOD: row permutations, frame commit, blanking and 720p sync PASS");
        $finish;
    end
    initial begin #25_000_000; $fatal(1,"desktop PMOD test timeout"); end
endmodule
