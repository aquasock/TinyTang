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
    logic [31:0] i2c_cmd=0;
    logic i2c_toggle=0;
    wire [31:0] i2c_status;
    // The USB-A pair as an I2C bus: the adapter's pull-ups on both lines and
    // two devices, the DS3231 at 0x68 and a Wii controller at 0x52, which
    // answer their addresses.  The controller stretches the clock as a real
    // one does: on its ACK clock it holds SCL low and gives the ACK late, so
    // a master that does not wait for SCL reads a NACK.  Off for the VGA
    // checks.
    tri usb_sda, usb_scl;
    logic bus_on=0;
    logic slave_sda_low=0, slave_scl_low=0;
    assign (weak0,weak1) usb_sda=bus_on ? 1'b1:1'bz;
    assign (weak0,weak1) usb_scl=bus_on ? 1'b1:1'bz;
    assign usb_sda=slave_sda_low ? 1'b0:1'bz;
    assign usb_scl=slave_scl_low ? 1'b0:1'bz;
    wire scl_line=usb_scl, sda_line=usb_sda;
    integer slave_bit=0, scl_falls=0, pmod_drives=0;
    logic [7:0] slave_shift=0;
    logic slave_active=0;
    // Set while the sockets are released and the bus runs: no PMOD pin may
    // move then.
    logic watch_pmod=0;
    always @(negedge sda_line) if(bus_on) begin
        #1 if(scl_line===1'b1) begin slave_active=1; slave_bit=0; end // START
    end
    always @(posedge scl_line) if(slave_active && slave_bit<8) begin
        slave_shift={slave_shift[6:0],sda_line}; slave_bit=slave_bit+1;
    end
    always @(negedge scl_line) if(bus_on) begin
        scl_falls=scl_falls+1;
        if(slave_active && slave_bit==8) begin
            slave_bit=9;
            if(slave_shift[7:1]==7'h52) begin
                slave_scl_low=1; #2000 slave_sda_low=1; #100 slave_scl_low=0;
            end else begin
                slave_sda_low=slave_shift[7:1]==7'h68;
            end
        end else if(slave_active && slave_bit==9) begin
            slave_sda_low=0; slave_active=0;
        end
    end
    always @(p0 or p1) if(watch_pmod && (p0!==8'hzz || p1!==8'hzz))
        pmod_drives=pmod_drives+1;
    task automatic scan(output [7:0] count,output [7:0] first);
        begin
            // Poll from the next clock, as the firmware may: the previous
            // scan's done flag must not be taken for this one's.
            i2c_cmd=32'd2; i2c_toggle=~i2c_toggle;
            @(posedge pixel_clk); @(posedge pixel_clk); #1;
            if(!i2c_status[1] || i2c_status[3]) $fatal(1,"I2C: status not busy right after the command");
            wait(i2c_status[3] && !i2c_status[1]);
            count=i2c_status[31:24]; first=i2c_status[23:16];
        end
    endtask
    desktop_pmod #(.I2C_CLK_HZ(2_000_000)) dut(.control_clk(pixel_clk),.pixel_clk(pixel_clk),.sys_clk(pixel_clk),.resetn(resetn),.cx(cx),.cy(cy),
        .rgb(rgb),.socket_word(word),.socket_request(request),.socket_ack(ack),
        .pmod0_io(p0),.pmod1_io(p1),.usb_sda(usb_sda),.usb_scl(usb_scl),
        .oled_cell_we(1'b0),.oled_cell_index(9'd0),.oled_cell_word(16'd0),
        .oled_cursor(17'd0),.oled_frames(),.oled_signature(),
        .i2c_cmd(i2c_cmd),.i2c_cmd_toggle(i2c_toggle),.i2c_wdata(64'd0),
        .i2c_auto(32'd0),.i2c_rdata(),.i2c_res_toggle(),.i2c_status(i2c_status),
        // The PMOD UART's register side, tied idle.  Left unconnected these
        // float to X, and an X on `hold` reaches the transmitter's pin, which
        // is exactly what a personality-8 pin check has to observe.
        .uart_tx_data(8'd0),.uart_tx_push(1'b0),.uart_flush(1'b0),
        .uart_hold(1'b0),.uart_nrst_low(1'b0),.uart_loopback(1'b0),
        .uart_rx_pop(1'b0),.uart_rx_data(),.uart_status());
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
    reg [7:0] found,first;
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
        // I2C on the USB-A pair, with both sockets released: the scan finds
        // both devices, and no PMOD pin moves while it runs.
        apply(0); bus_on=1; #1;
        if(usb_scl!==1'b1 || usb_sda!==1'b1) $fatal(1,"I2C: idle bus not released to the pull-ups");
        watch_pmod=1;
        scan(found,first);
        watch_pmod=0;
        if(scl_falls==0) $fatal(1,"I2C: no SCL on the USB-A D- line");
        if(pmod_drives!=0) $fatal(1,"I2C: a PMOD pin was driven by the USB-A bus");
        if(found!=2 || first!=8'h52)
            $fatal(1,"I2C: scan found %0d device(s), first 0x%02h; expected two from 0x52",found,first);
        // The old I2C and I2C-plus-UART personalities are released sockets.
        apply(16'h0060); check_outputs(16'h0060);
        apply(16'h0070); check_outputs(16'h0070);
        apply(16'h0700); check_outputs(16'h0700);
        // The bus is independent of the sockets: it still runs beside VGA.
        apply(16'h0230); check_outputs(16'h0230);
        scan(found,first);
        if(found!=2 || first!=8'h52)
            $fatal(1,"I2C beside VGA: scan found %0d device(s), first 0x%02h",found,first);
        apply(0); bus_on=0; #1;
        // ------------------------------------------------- personality 8 ----
        // The PMOD UART's pins, checked at pin level rather than through the
        // register window (tb_desktop_uart covers that half).  PMOD-009 says
        // NRST on pin 8 and TX on pin 9, RX read from pin 10, and no flip
        // support.  The last of those is not what the gateware does: the
        // accept mask leaves bits 4 to 13 unconstrained, so a flip bit on a
        // personality-8 word is taken, and apply() then XORs the lane index
        // while uart_rx_raw keeps reading IO7 directly.  These checks pin the
        // behaviour down both ways so that a future core revision which
        // refuses the word fails here and says so.
        check_uart_pins(16'h0080, 0);   // PMOD0: NRST pin 8, TX pin 9
        check_uart_pins(16'h0800, 1);   // PMOD1: the same two pins
        // Bit 12 on a PMOD0 personality-8 word is accepted and moves NRST and
        // TX to pins 2 and 3 while RX stays on pin 10: a half-flipped UART.
        check_uart_pins(16'h1080, 0);
        // Bit 13 flips PMOD1, which this word leaves released, so it changes
        // nothing the module can see.
        check_uart_pins(16'h2080, 0);
        // A socket word the core refuses must not reach the pins at all.
        apply(0);
        resetn=0; @(posedge pixel_clk); #1;
        if(p0!==8'hzz || p1!==8'hzz) $fatal(1,"reset did not release sockets");
        if(usb_sda!==1'bz || usb_scl!==1'bz) $fatal(1,"reset drove the USB-A pair");
        resetn=1;
        $display("desktop PMOD: row permutations, frame commit, blanking, 720p sync, USB-A I2C and personality 8 pins PASS");
        $finish;
    end
    // Personality 8 drives exactly two pins per socket: lane 5 (NRST) and
    // lane 6 (TX out of the FPGA).  Both idle high, which is TX's mark and the
    // module's deasserted reset.  RX is not driven.  `slot` is 0 for PMOD0 and
    // 1 for PMOD1; the bench asks only that the other socket is released.
    task automatic check_uart_pins(input [15:0] w, input integer slot);
        reg [7:0] exp_a,exp_b,en;
        reg [2:0] normal,lane;
        integer i;
        begin
            apply(w);
            en=8'h00;
            // The two enabled lanes, before the flip XOR.
            en[5]=1'b1; en[6]=1'b1;
            // Route them the way the driver's g_pin loop does.
            exp_a=8'hzz; exp_b=8'hzz;
            for(i=0;i<8;i=i+1) begin
                normal=3'(i/2+(i%2)*4);
                lane=normal ^ (w[12] ? 3'd4 : 3'd0);
                if(en[lane]) begin
                    if(slot==0) exp_a[i]=1'b1;
                    else exp_b[i]=1'b1;
                end
            end
            if(p0!==exp_a || p1!==exp_b)
                $fatal(1,"personality 8 pins wrong for %04h: p0=%b p1=%b expected %b %b",
                       w,p0,p1,exp_a,exp_b);
            // Named expectations for the shipped word, so a lane-map change
            // cannot pass by agreeing with its own formula.
            if(w==16'h0080) begin
                if(p0[3]!==1'b1 || p0[5]!==1'b1)
                    $fatal(1,"0x0080 must drive IO3 (pin 8) and IO5 (pin 9)");
                if(p0[7]!==1'bz) $fatal(1,"0x0080 must leave IO7 (pin 10, RX) undriven");
            end
            if(w==16'h1080) begin
                if(p0[3]!==1'bz || p0[5]!==1'bz)
                    $fatal(1,"flip bit 12 no longer moves the UART lanes; the deviation was fixed");
                if(p0[2]!==1'b1 || p0[4]!==1'b1)
                    $fatal(1,"flip bit 12 moved the UART to unexpected pins");
            end
        end
    endtask
    initial begin #25_000_000; $fatal(1,"desktop PMOD test timeout"); end
endmodule
