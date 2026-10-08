// TinyTang desktop core register endpoint. SPDX-License-Identifier: GPL-3.0-only
// Version-1 extended TangCore packets; a write is committed only after the
// complete request validates, and its reply waits for the pixel-domain ack.
module desktop_regs (
    input logic clk, resetn,
    input logic start, byte_valid, finish,
    input logic [7:0] byte_data,
    input logic [15:0] frame_length,
    input logic response_taken,
    output logic response_ready,
    output logic [119:0] response,
    output logic [15:0] socket_word,
    output logic socket_request,
    input logic socket_ack
);
    function automatic [15:0] crc_byte(input [15:0] old, input [7:0] data);
        reg [15:0] c;
        begin
            c = old ^ {data, 8'b0};
            for (integer bitno = 0; bitno < 8; bitno = bitno + 1)
                c = c[15] ? (c << 1) ^ 16'h1021 : c << 1;
            crc_byte = c;
        end
    endfunction

    localparam [2:0] IDLE=0, CHECK=1, WAIT_ACK=2, SEAL=3, READY=4;
    logic [2:0] state;
    logic [111:0] packet;
    logic [4:0] received;
    logic [15:0] length_q, request_crc, reply_crc;
    logic [3:0] seal_index;
    (* ASYNC_REG = "TRUE" *) logic ack_meta, ack_sync;
    // Only the toggle crosses through synchronizers; the associated word is
    // held unchanged until acknowledgement returns (bundled-data handshake).
    always_ff @(posedge clk) begin
        if (!resetn) begin
            ack_meta <= 0;
            ack_sync <= 0;
        end else begin
            ack_meta <= socket_ack;
            ack_sync <= ack_meta;
        end
    end

    wire [7:0] opcode = packet[103:96];
    wire [31:0] address = packet[79:48];
    wire [31:0] write_data = packet[47:16];
    wire [3:0] p0 = write_data[7:4], p1 = write_data[11:8];
    wire supported_word = (write_data & 32'hffffc00f) == 0 &&
        ((p0 == 0 && p1 == 0) || (p0 == 2 && p1 == 3) || (p0 == 3 && p1 == 2));
    logic [7:0] status;
    logic [31:0] read_data;
    always_comb begin
        status = 0;
        read_data = 0;
        if (length_q != 15 || received != 14) status = 5;
        else if (packet[111:104] != 1) status = 1;
        else if (request_crc != packet[15:0]) status = 3;
        else if (opcode == 0) read_data = 3; // read32 and write32 only
        else if (opcode == 1) begin
            case (address)
                32'h00: read_data = 32'h00544453; // TDS, desktop identity
                32'h04: read_data = 32'h00010000; // desktop register ABI 1.0
                32'h08: read_data = 32'h0000000d; // none, VGA J1 and VGA J2
                32'hc0: read_data = {16'b0, socket_word};
                default: status = 4;
            endcase
        end else if (opcode == 2) begin
            if (address != 32'hc0) status = 4;
            else if (!supported_word) status = 6;
            else read_data = write_data;
        end else status = 2;
    end

    always_ff @(posedge clk) begin
        if (!resetn) begin
            state <= IDLE;
            packet <= 0;
            received <= 0;
            length_q <= 0;
            request_crc <= 16'hffff;
            reply_crc <= 16'hffff;
            seal_index <= 0;
            response <= 0;
            response_ready <= 0;
            socket_word <= 0;
            socket_request <= 0;
        end else begin
            if (start && state == IDLE) begin
                packet <= 0;
                received <= 0;
                length_q <= frame_length;
                request_crc <= crc_byte(16'hffff, 8'h10);
                if (frame_length == 1) state <= CHECK;
            end
            if (byte_valid && state == IDLE) begin
                if (received < 14) packet[111-received*8 -: 8] <= byte_data;
                if (received < 31) received <= received + 1;
                if (received < 12) request_crc <= crc_byte(request_crc, byte_data);
                if (finish) state <= CHECK;
            end
            case (state)
                CHECK: begin
                    response[119:16] <= {8'h01, (opcode | 8'h80), status,
                        packet[95:80], address, read_data};
                    response_ready <= 0;
                    reply_crc <= crc_byte(16'hffff, 8'h10);
                    seal_index <= 0;
                    if (status == 0 && opcode == 2) begin
                        socket_word <= write_data[15:0];
                        socket_request <= ~socket_request;
                        state <= WAIT_ACK;
                    end else state <= SEAL;
                end
                WAIT_ACK: if (ack_sync == socket_request) state <= SEAL;
                SEAL: begin
                    reply_crc <= crc_byte(reply_crc, response[119-seal_index*8 -: 8]);
                    if (seal_index == 12) begin
                        response[15:0] <= crc_byte(reply_crc, response[23:16]);
                        response_ready <= 1;
                        state <= READY;
                    end else seal_index <= seal_index + 1;
                end
                READY: if (response_taken) begin
                    response_ready <= 0;
                    state <= IDLE;
                end
                default: ;
            endcase
        end
    end
endmodule
