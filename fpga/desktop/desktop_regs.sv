// TinyTang desktop register endpoint. SPDX-License-Identifier: GPL-3.0-only
// Validate complete CRC-protected requests before any cell or socket write.
module desktop_regs (
    input logic clk, resetn, start, byte_valid, finish, block_request,
    input logic [7:0] byte_data,
    input logic [15:0] frame_length,
    input logic response_taken,
    output logic response_ready,
    output logic [119:0] response,
    output logic [15:0] socket_word,
    output logic socket_request,
    input logic socket_ack,
    output logic oled_cell_we,
    output logic [8:0] oled_cell_index,
    output logic [15:0] oled_cell_word,
    output logic [16:0] oled_cursor,
    input logic [31:0] oled_frames, oled_signature
);
    function automatic [15:0] crc_byte(input [15:0] old,input [7:0] data);
        reg [15:0] c;
        begin
            c=old^{data,8'b0};
            for(integer k=0;k<8;k=k+1)c=c[15] ? (c<<1)^16'h1021:c<<1;
            crc_byte=c;
        end
    endfunction
    localparam [2:0] IDLE=0,CHECK=1,WAIT_ACK=2,SEAL=3,READY=4,APPLY=5;
    logic [2:0] state;
    logic [111:0] packet;
    logic [10:0] received;
    logic [15:0] length_q,request_crc,crc_tail,reply_crc;
    logic [3:0] seal_index;
    logic is_block,invalid_word;
    logic [31:0] assembly,words[0:63];
    logic [6:0] apply_index;
    (* ASYNC_REG="TRUE" *) logic ack_meta,ack_sync;
    always_ff @(posedge clk) begin
        if(!resetn) begin ack_meta<=0;ack_sync<=0;end
        else begin ack_meta<=socket_ack;ack_sync<=ack_meta;end
    end
    wire [7:0] opcode=packet[103:96];
    wire [31:0] address=packet[79:48],write_data=packet[47:16];
    wire [3:0] p0=write_data[7:4],p1=write_data[11:8];
    wire supported_word=(write_data&32'hffffc00f)==0 &&
        ((p0<=1 && p1<=1)||(p0==2 && p1==3)||(p0==3 && p1==2));
    wire cell_address=address>=32'h200 && address<32'h800 && address[1:0]==0;
    wire [31:0] last_address=address+((write_data-32'd1)<<2);
    logic [7:0] status;
    logic [31:0] read_data;
    always_comb begin
        status=0;read_data=0;
        if((!is_block && (length_q!=15 || received!=14)) ||
           (is_block && (write_data<1 || write_data>64 ||
            length_q!=15+(write_data<<2) || received!=length_q-1)))status=5;
        else if(packet[111:104]!=1)status=1;
        else if(request_crc!=crc_tail)status=3;
        else if(is_block) begin
            if(opcode!=4)status=2;
            else if(!cell_address || last_address<address || last_address>=32'h800)status=4;
            else if(invalid_word)status=6;
            else read_data=write_data;
        end else if(opcode==0)read_data=32'h13; // read32, write32, validated block write
        else if(opcode==1) begin
            case(address)
                32'h00:read_data=32'h00544453;
                32'h04:read_data=32'h00010001; // desktop ABI 1.1
                32'h08:read_data=32'h0000000f; // none, OLED, VGA J1/J2
                32'hc0:read_data={16'b0,socket_word};
                32'h100:read_data=32'h00180010; // fixed 24 columns, 16 rows
                32'h108:read_data=oled_frames;
                32'h10c:read_data=oled_signature;
                32'h114:read_data={15'b0,oled_cursor};
                default:status=4;
            endcase
        end else if(opcode==2) begin
            if(address==32'hc0) begin
                if(!supported_word)status=6;else read_data=write_data;
            end else if(address==32'h114) begin
                if(write_data[31:17]!=0 || write_data[7:0]>=24 || write_data[15:8]>=16)status=6;
                else read_data=write_data;
            end else if(cell_address) begin
                if(write_data[31:16]!=0)status=6;else read_data=write_data;
            end else status=4;
        end else status=2;
    end
    always_ff @(posedge clk) begin
        if(!resetn) begin
            state<=IDLE;packet<=0;received<=0;length_q<=0;
            request_crc<=16'hffff;crc_tail<=0;reply_crc<=16'hffff;
            seal_index<=0;response<=0;response_ready<=0;
            socket_word<=0;socket_request<=0;is_block<=0;invalid_word<=0;
            assembly<=0;apply_index<=0;oled_cell_we<=0;
            oled_cell_index<=0;oled_cell_word<=0;oled_cursor<=0;
        end else begin
            oled_cell_we<=0;
            if(start && state==IDLE) begin
                packet<=0;received<=0;length_q<=frame_length;
                is_block<=block_request;invalid_word<=0;assembly<=0;crc_tail<=0;
                request_crc<=crc_byte(16'hffff,block_request ? 8'h12:8'h10);
                if(frame_length==1)state<=CHECK;
            end
            if(byte_valid && state==IDLE) begin
                if(received<12)packet[111-received*8 -:8]<=byte_data;
                if(received<2047)received<=received+11'd1;
                crc_tail<={crc_tail[7:0],byte_data};
                if({5'b0,received}+16'd3<length_q)request_crc<=crc_byte(request_crc,byte_data);
                if(is_block && received>=12 && received<268 &&
                   {5'b0,received}+16'd3<length_q) begin
                    assembly<={assembly[23:0],byte_data};
                    if(received[1:0]==3) begin
                        words[(received-11'd12)>>2]<={assembly[23:0],byte_data};
                        if(assembly[23:8]!=0)invalid_word<=1;
                    end
                end
                if(finish)state<=CHECK;
            end
            case(state)
                CHECK: begin
                    response[119:16]<={8'h01,(opcode|8'h80),status,packet[95:80],address,read_data};
                    response_ready<=0;reply_crc<=crc_byte(16'hffff,8'h10);seal_index<=0;
                    if(status==0 && is_block) begin apply_index<=0;state<=APPLY;end
                    else if(status==0 && opcode==2) begin
                        if(address==32'hc0 || address==32'h114) begin
                            if(address==32'hc0)socket_word<=write_data[15:0];
                            else oled_cursor<=write_data[16:0];
                            socket_request<=~socket_request;state<=WAIT_ACK;
                        end else begin
                            oled_cell_we<=1;oled_cell_index<=(address-32'h200)>>2;
                            oled_cell_word<=write_data[15:0];state<=SEAL;
                        end
                    end else state<=SEAL;
                end
                APPLY: begin
                    oled_cell_we<=1;
                    oled_cell_index<=((address-32'h200)>>2)+apply_index;
                    oled_cell_word<=words[apply_index][15:0];
                    if({25'b0,apply_index}+32'd1==write_data)state<=SEAL;
                    else apply_index<=apply_index+7'd1;
                end
                WAIT_ACK:if(ack_sync==socket_request)state<=SEAL;
                SEAL: begin
                    reply_crc<=crc_byte(reply_crc,response[119-seal_index*8 -:8]);
                    if(seal_index==12) begin
                        response[15:0]<=crc_byte(reply_crc,response[23:16]);
                        response_ready<=1;state<=READY;
                    end else seal_index<=seal_index+4'd1;
                end
                READY:if(response_taken)begin response_ready<=0;state<=IDLE;end
                default:;
            endcase
        end
    end
endmodule
