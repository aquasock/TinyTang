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
    wire cell_we;
    wire [8:0] cell_index;
    wire [15:0] cell_word;
    logic [15:0] cells[0:383];
    integer writes=0;
    always @(posedge clk) if(cell_we) begin
        if(cell_index>=384) $fatal(1,"out-of-range cell write");
        cells[cell_index]=cell_word;writes++;
    end
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
        .desktop_socket_ack(socket_ack),
        .desktop_oled_cell_we(cell_we),.desktop_oled_cell_index(cell_index),
        .desktop_oled_cell_word(cell_word),
        .desktop_oled_frames(32'd0),.desktop_oled_signature(32'd0)
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
    // EXTCTL-002: version, opcode, sequence, address, a count byte, the words
    // and the CRC.  Fault 3 sends the 32-bit count this core once expected.
    task automatic block_write(input [31:0] addr,input integer count,input integer fault,input [7:0] status);
        reg [7:0] bytes[0:269];
        reg [15:0] c;
        reg [31:0] value;
        integer n,header,before_reply,before_writes;
        begin
            transaction_sequence++;
            header=fault==3 ? 12:9;
            n=header+2+count*4;
            bytes[0]=1;bytes[1]=4;bytes[2]=8'(transaction_sequence>>8);bytes[3]=8'(transaction_sequence);
            for(integer i=0;i<4;i++) begin bytes[4+i]=addr[31-i*8 -:8];if(fault==3)bytes[8+i]=8'(count>>(24-i*8));end
            if(fault!=3)bytes[8]=8'(count);
            for(integer i=0;i<count;i++) begin
                value=fault==2 && i==count-1 ? 32'h00010741:32'h00000f41+i;
                for(integer j=0;j<4;j++)bytes[header+i*4+j]=value[31-j*8 -:8];
            end
            c=crc_byte(16'hffff,8'h12);
            for(integer i=0;i<n-2;i++)c=crc_byte(c,bytes[i]);
            if(fault==1)c=c^1;
            bytes[n-2]=c[15:8];bytes[n-1]=c[7:0];
            before_reply=reply_count;before_writes=writes;
            send_byte(8'haa);send_byte(8'((n+1)>>8));send_byte(8'(n+1));send_byte(8'h12);
            for(integer i=0;i<n;i++)send_byte(bytes[i]);
            wait(reply_count>before_reply);
            if(reply[111:104]!=8'h84 || reply[103:96]!=status || reply[95:80]!=16'(transaction_sequence) ||
               reply[79:48]!=addr || reply[47:16]!=(status==0 ? 32'(count):32'd0))
                $fatal(1,"block response mismatch %030h",reply);
            if(writes-before_writes!=(status==0 ? count:0))$fatal(1,"block partial/rejected write");
            if(status==0) for(integer i=0;i<count;i++)
                if(cells[((addr-32'h200)>>2)+i]!==16'h0f41+16'(i))$fatal(1,"block cell order mismatch");
            #1000;
        end
    endtask
    // Replay the bytes the firmware's own encoder produces
    // (gen_desktop_block_frames.cpp) and compare the cells it expects.
    logic [7:0] fw[0:4095];
    logic [15:0] fw_cells[0:383];
    task automatic firmware_frames;
        integer fd,frames,total,at,len,before_reply,before_writes,words;
        reg [31:0] addr;
        reg [15:0] seq;
        begin
            fd=$fopen("firmware-blocks.count","r");
            if(fd==0 || $fscanf(fd,"%d %d",frames,total)!=2)$fatal(1,"no firmware block frames");
            $fclose(fd);
            $readmemh("firmware-blocks.hex",fw);
            $readmemh("firmware-cells.hex",fw_cells);
            at=0;words=0;
            for(integer f=0;f<frames;f++) begin
                len={fw[at+1],fw[at+2]};
                seq={fw[at+6],fw[at+7]};
                addr={fw[at+8],fw[at+9],fw[at+10],fw[at+11]};
                before_reply=reply_count;before_writes=writes;
                for(integer i=0;i<3+len;i++)send_byte(fw[at+i]);
                wait(reply_count>before_reply);
                if(reply[111:104]!=8'h84 || reply[103:96]!=0 || reply[95:80]!=seq ||
                   reply[79:48]!=addr || reply[47:16]!=32'(fw[at+12]))
                    $fatal(1,"firmware block %0d refused: %030h",f,reply);
                if(writes-before_writes!=int'(fw[at+12]))$fatal(1,"firmware block %0d partial",f);
                words+=int'(fw[at+12]);
                at+=3+len;
                #1000;
            end
            if(at!=total)$fatal(1,"firmware frame bytes %0d of %0d",at,total);
            for(integer i=0;i<384;i++)
                if(cells[i]!==fw_cells[i])$fatal(1,"firmware cell %0d is %h, expected %h",i,cells[i],fw_cells[i]);
            $display("desktop UART: %0d firmware block frames, %0d cells PASS",frames,words);
        end
    endtask
    initial begin
        #3000; resetn=1; #3000;
        send_byte(8'haa); send_byte(0); send_byte(1); send_byte(1);
        wait(core_count==1);
        request(0,0,0,0,0,19);
        request(1,4,0,0,0,32'h00010001);
        request(1,8,0,0,0,15);
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
        request(1,32'h100,0,0,0,32'h00180010);
        request(2,32'hc0,32'h0010,0,0,32'h0010);
        request(2,32'hc0,32'h2100,0,0,32'h2100);
        request(2,32'h114,32'h010f17,0,0,32'h010f17);
        request(1,32'h114,0,0,0,32'h010f17);
        request(2,32'h114,32'h010f18,0,6,0);
        request(2,32'h114,32'h011017,0,6,0);
        request(2,32'h200,32'h0f41,0,0,32'h0f41);
        if(writes!=1 || cells[0]!=16'h0f41)$fatal(1,"single cell write failed");
        request(2,32'h200,32'h010f41,0,6,0);
        if(writes!=1)$fatal(1,"invalid cell word mutated memory");
        block_write(32'h200,64,0,0);
        block_write(32'h7fc,1,0,0);
        block_write(32'h200,64,1,3);
        block_write(32'h200,64,2,6);
        block_write(32'h7fc,2,0,4);
        block_write(32'h202,1,0,4);
        block_write(32'h1fc,1,0,4);
        block_write(32'hfffffffc,2,0,4);
        block_write(32'h200,0,0,5);
        block_write(32'h200,64,3,5);
        firmware_frames();
        request(2,32'hc0,0,0,0,0);
        // The original desktop commands still work alongside register replies.
        send_byte(8'haa); send_byte(0); send_byte(2); send_byte(8'h15); send_byte(1);
        #1000;
        if (!wide_on || socket_word!=0 || core_count!=1 || keyboard_count<1)
            $fatal(1,"legacy desktop/keyboard traffic failed");
        $fclose(trace);
        $display("desktop UART: %0d register responses, ID, keyboard traffic and layer enable PASS",reply_count);
        $finish;
    end
    initial begin #60_000_000; $fatal(1,"desktop UART test timeout"); end
endmodule
