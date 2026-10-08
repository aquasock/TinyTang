// SPDX-License-Identifier: GPL-3.0-only
`timescale 1ns/1ps
// Exercise the real compositor and HDMI raster against the physical VGA pins.
// A fixed RGB input cannot reveal the compositor's one-clock pixel latency.
module tb_desktop_video;
    logic clk=0, pixel_clk=0, pixel_x5=0, resetn=0;
    always #23.265 clk=~clk;
    always #6.734 pixel_clk=~pixel_clk;
    always #1.3468 pixel_x5=~pixel_x5;
    logic we=0;
    logic [6:0] wx=0, ch=0;
    logic [5:0] wy=0;
    logic [14:0] fg=0, bg=0;
    wire [10:0] cx;
    wire [9:0] cy;
    wire [23:0] rgb;
    wire ack;
    tri [7:0] p0,p1;
    nes2hdmi video (
        .clk(clk), .resetn(resetn), .color(6'd0), .cycle(9'd0),
        .scanline(9'd0), .sample(16'd0), .aspect_8x7(1'b0),
        .overlay(1'b1), .overlay_x(), .overlay_y(), .overlay_color(15'd0),
        .wide_we(we), .wide_x(wx), .wide_y(wy), .wide_ch(ch),
        .wide_fg(fg), .wide_bg(bg), .wide_on(1'b1),
        .desktop_cx(cx), .desktop_cy(cy), .desktop_rgb(rgb),
        .clk_pixel(pixel_clk), .clk_5x_pixel(pixel_x5),
        .tmds_clk_n(), .tmds_clk_p(), .tmds_d_n(), .tmds_d_p()
    );
    desktop_pmod sockets (
        .pixel_clk(pixel_clk), .resetn(resetn), .cx(cx), .cy(cy), .rgb(rgb),
        .socket_word(16'h0230), .socket_request(1'b1), .socket_ack(ack),
        .pmod0_io(p0), .pmod1_io(p1)
    );
    logic [6:0] chars[0:3599];
    logic [14:0] foreground[0:3599], background[0:3599];
    function automatic [23:0] expected_pixel(input integer x,input integer y);
        integer cell_index;
        logic [7:0] glyph;
        logic [14:0] color;
        begin
            expected_pixel=0;
            if(x<1280 && y<720) begin
                cell_index=(y/16)*80+x/16;
                glyph=FONT[chars[cell_index]][(y/2)%8];
                color=glyph[(x/2)%8] ? foreground[cell_index]:background[cell_index];
                expected_pixel={color[4:0],3'b0,color[9:5],3'b0,color[14:10],3'b0};
            end
        end
    endfunction
    // Independent coordinates for the registered producer and HDMI encoder.
    integer pixel_x=0,pixel_y=0,hdmi_x=0,hdmi_y=0;
    always @(posedge pixel_clk) begin
        pixel_x<=int'(cx); pixel_y<=int'(cy);
        hdmi_x<=pixel_x; hdmi_y<=pixel_y;
    end
    integer checked=0, hdmi_checked=0, failures=0, left_leaks=0;
    logic [23:0] expected;
    logic [11:0] actual;
    initial begin
        #100; resetn=1;
        // Distinct cells detect shifts even inside a line; 'b' at cell 0
        // reproduces the original PROT-009 blanking-glyph leak.
        for(integer i=0;i<3600;i=i+1) begin
            chars[i]=7'(33+(i*13)%94);
            if(i%80==0) chars[i]=i==0 ? 7'd98:7'd42;
            if(i%80==79) chars[i]=7'd95;
            foreground[i]=15'h4000 | 15'(i);
            background[i]=15'((i*37+123)&15'h3fff);
            @(negedge clk);
            wx=7'(i%80); wy=6'(i/80); ch=chars[i];
            fg=foreground[i]; bg=background[i]; we=1;
            @(negedge clk); we=0;
        end
        wait(ack);
        // Start at a real frame wrap with both paths fully warmed up.
        do begin @(posedge pixel_clk); #1; end while(pixel_x!=0 || pixel_y!=0);
        for(integer n=0;n<1650*750;n=n+1) begin
            expected=expected_pixel(pixel_x,pixel_y);
            actual={p1[6],p1[4],p1[2],p1[0],p0[6],p0[4],p0[2],p0[0],
                    p1[7],p1[5],p1[3],p1[1]};
            if(actual!=={expected[23:20],expected[15:12],expected[7:4]} ||
               p0[1]!== (pixel_x>=1390 && pixel_x<1430) ||
               p0[3]!== (pixel_y>=725 && pixel_y<730)) begin
                failures=failures+1;
                if(cx==0 && cy<720 && actual!=0) left_leaks=left_leaks+1;
                if(failures<=6)
                    $display("VGA mismatch: raster %0d,%0d registered pixel %0d,%0d got %03h expected %06h",
                        cx,cy,pixel_x,pixel_y,actual,expected);
            end
            checked=checked+1;
            // Check HDMI separately against the reference image, including
            // every first and last visible pixel. Its correct alignment must
            // not be altered to compensate the VGA defect.
            if(video.hdmi.mode==1) begin
                hdmi_checked=hdmi_checked+1;
                if(video.hdmi.video_data!==expected_pixel(hdmi_x,hdmi_y))
                    $fatal(1,"HDMI pixel mismatch at %0d,%0d",hdmi_x,hdmi_y);
            end
            @(posedge pixel_clk); #1;
        end
        if(checked!=1650*750 || hdmi_checked!=1280*720)
            $fatal(1,"incomplete raster: VGA=%0d HDMI=%0d",checked,hdmi_checked);
        if(failures!=0)
            $fatal(1,"VGA alignment failed: %0d mismatches, %0d left-edge blanking leaks; HDMI passed %0d pixels",
                failures,left_leaks,hdmi_checked);
        $display("desktop video: VGA %0d raster pixels, HDMI %0d visible pixels; no left-edge leak PASS",checked,hdmi_checked);
        $finish;
    end
    initial begin #40_000_000; $fatal(1,"desktop video test timeout"); end
endmodule
