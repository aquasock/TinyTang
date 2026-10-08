// SPDX-License-Identifier: GPL-3.0-only
`timescale 1ns/1ps
module tb_desktop_uart;
    logic clk=0, pixel_clk=0, resetn=0, uart_rx=1;
    always #23.265 clk=~clk;
    always #6.734 pixel_clk=~pixel_clk;
    wire uart_tx;
    wire [15:0] socket_word;
    wire socket_request;
    logic socket_ack=0;
    // Endpoint tests use a delayed acknowledgement, not a combinational echo.
    // The actual asynchronous/frame-boundary transfer is exercised separately.
    integer ack_delay=0;
    always @(posedge clk) begin
        if (!resetn) begin socket_ack<=0; ack_delay<=0; end
        else if (socket_request != socket_ack) begin
            if (ack_delay == 31) begin socket_ack<=socket_request; ack_delay<=0; end
            else ack_delay<=ack_delay+1;
        end
    end
    wire [6:0] wide_x, wide_ch;
    wire [5:0] wide_y;
    wire [14:0] wide_fg, wide_bg;
    wire wide_we, wide_on;
    iosys_bl616 #(.FREQ(21_492_000), .CORE_ID(16'h0054)) dut (
        .clk(clk), .hclk(pixel_clk), .resetn(resetn),
        .overlay(), .overlay_x(8'd0), .overlay_y(8'd0), .overlay_color(),
        .wide_x(wide_x), .wide_y(wide_y), .wide_ch(wide_ch),
        .wide_fg(wide_fg), .wide_bg(wide_bg), .wide_we(wide_we), .wide_on(wide_on),
        .joy1(12'd0), .joy2(12'd0), .link_mods(8'd0), .link_keys(48'd0),
        .hid1(), .hid2(), .rom_loading(), .rom_do(), .rom_do_valid(),
        .mgmt_address(), .mgmt_read(), .mgmt_readdata(16'd0),
        .mgmt_write(), .mgmt_writedata(), .fdd_request(2'd0),
        .kbd_data(), .kbd_data_valid(), .core_config(),
        .uart_rx(uart_rx), .uart_tx(uart_tx),
        .desktop_socket_word(socket_word), .desktop_socket_request(socket_request),
        .desktop_socket_ack(socket_ack)
    );
    function automatic [15:0] crc_byte(input [15:0] old, input [7:0] b);
        reg [15:0] c;
        begin
            c=old^{b,8'b0};
            for (integer k=0;k<8;k=k+1) c=c[15] ? (c<<1)^16'h1021 : c<<1;
            crc_byte=c;
        end
    endfunction
    task automatic send_byte(input [7:0] b);
        uart_rx=0; #500;
        for (integer bitno=0;bitno<8;bitno=bitno+1) begin uart_rx=b[bitno]; #500; end
        uart_rx=1; #1000;
    endtask
    integer reply_count=0, core_count=0, keyboard_count=0;
    logic [119:0] reply;
    integer trace;
    logic [7:0] captured;
    integer parse_phase=0, remaining=0, payload_index=0;
    logic [7:0] frame_type;
    logic [119:0] assembly;
    initial begin
        trace=$fopen("desktop-replies.hex","w");
        forever begin
            @(negedge uart_tx); #750;
            for (integer bitno=0;bitno<8;bitno=bitno+1) begin
                captured[bitno]=uart_tx;
                if (bitno<7) #500;
            end
            #500;
            if (uart_tx !== 1) $fatal(1,"bad UART stop bit");
            case (parse_phase)
                0: if (captured==8'haa) parse_phase=1;
                1: begin remaining=int'(captured)*256; parse_phase=2; end
                2: begin remaining=remaining+int'(captured); parse_phase=3; end
                3: begin frame_type=captured; remaining=remaining-1; payload_index=0;
                    assembly=0; parse_phase=4; end
                4: begin
                    if (frame_type==16) assembly={assembly[111:0],captured};
                    if (frame_type==1 && captured!=8'h54) $fatal(1,"wrong desktop ID");
                    payload_index=payload_index+1;
                    remaining=remaining-1;
                    if (remaining==0) begin
                        if (frame_type==16) begin
                            if (payload_index!=15) $fatal(1,"wrong response length");
                            reply=assembly; reply_count=reply_count+1;
                            $fdisplay(trace,"%030h",reply);
                        end else if (frame_type==1) core_count=core_count+1;
                        else if (frame_type==8) keyboard_count=keyboard_count+1;
                        parse_phase=0;
                    end
                end
            endcase
        end
    end
    integer transaction_sequence=0;
    task automatic request(input [7:0] op,input [31:0] addr,input [31:0] data,
                           input integer fault,input [7:0] status,input [31:0] expected);
        reg [111:0] p;
        reg [15:0] c;
        integer before_reply, length;
        begin
            transaction_sequence=transaction_sequence+1;
            p={fault==2 ? 8'h02:8'h01,op,16'(transaction_sequence),addr,data,16'b0};
            c=crc_byte(16'hffff,8'h10);
            for (integer i=0;i<12;i=i+1) c=crc_byte(c,p[111-i*8 -:8]);
            p[15:0]=fault==1 ? c^16'h0001:c;
            length=fault==3 ? 14:fault==4 ? 16:15;
            before_reply=reply_count;
            send_byte(8'haa); send_byte(0); send_byte(8'(length)); send_byte(16);
            for (integer i=0;i<length-1;i=i+1)
                send_byte(i<14 ? p[111-i*8 -:8]:8'h00);
            wait(reply_count>before_reply);
            if (reply[119:112]!=1 || reply[111:104]!=(op|8'h80) ||
                reply[103:96]!=status || reply[95:80]!=16'(transaction_sequence) ||
                reply[79:48]!=addr || reply[47:16]!=expected)
                $fatal(1,"response mismatch: %030h, transaction_sequence %d",reply,transaction_sequence);
            if (socket_request!=socket_ack) $fatal(1,"reply preceded socket acknowledgement");
            #1000;
        end
    endtask
    initial begin
        #3000; resetn=1; #3000;
        send_byte(8'haa); send_byte(0); send_byte(1); send_byte(1);
        wait(core_count==1);
        request(0,0,0,0,0,3);
        request(1,4,0,0,0,32'h00010000);
        request(1,8,0,0,0,13);
        request(1,32'hc0,0,0,0,0);
        request(2,32'hc0,32'h0230,0,0,32'h0230);
        request(1,32'hc0,0,0,0,32'h0230);
        request(2,32'hc0,32'h0320,1,3,0);
        request(2,32'hc0,32'h0320,2,1,0);
        request(2,32'hc0,32'h0320,3,5,0);
        request(2,32'hc0,32'h0320,4,5,0);
        request(2,32'hc0,32'h2410,0,6,0);
        request(1,32'hc1,0,0,4,0);
        request(3,0,0,0,2,0);
        request(1,32'hc0,0,0,0,32'h0230);
        request(2,32'hc0,32'h3320,0,0,32'h3320);
        request(2,32'hc0,0,0,0,0);
        // A command with no register payload must reject and recover too.
        send_byte(8'haa); send_byte(0); send_byte(1); send_byte(16);
        wait(reply_count==17);
        if(reply[119:16]!={8'h01,8'h80,8'h05,16'h0000,32'h0,32'h0})
            $fatal(1,"empty-payload rejection failed");
        // The original desktop commands still work alongside register replies.
        send_byte(8'haa); send_byte(0); send_byte(2); send_byte(8'h15); send_byte(1);
        #1000;
        if (!wide_on || socket_word!=0 || core_count!=1 || keyboard_count<1)
            $fatal(1,"legacy desktop/keyboard traffic failed");
        $fclose(trace);
        $display("desktop UART: %0d register responses, ID, keyboard traffic and layer enable PASS",reply_count);
        $finish;
    end
    initial begin #20_000_000; $fatal(1,"desktop UART test timeout"); end
endmodule
