// SPDX-License-Identifier: GPL-3.0-only
// The generic I2C engine against a register-aware DS3231-like slave: a read
// of any register, a write, a bus scan and an absent device; then the same
// slave stretching the clock as a Wii controller does, then reading as one
// does (a read after a repeated START, or too soon after the register was
// set, is ACKed and sent as 0xff), and a dead bus (both
// lines held low, as the dock's pull-downs hold an empty port), which must
// end in a NACK and leave both lines released.
`timescale 1ns/1ps

module tb_i2c_master;
    logic clk = 0;
    always #500 clk = ~clk;             // 1 MHz; HALF is 5 clocks

    logic resetn = 0, enable = 0;
    logic [1:0] cmd_op = 0;
    logic [6:0] cmd_addr = 0;
    logic [7:0] cmd_reg = 0;
    logic [4:0] cmd_len = 0;
    logic [63:0] cmd_wdata = 0;
    logic cmd_start = 0;
    logic auto_enable = 0;
    logic [6:0] auto_addr = 0;
    logic [7:0] auto_reg = 0;
    logic [4:0] auto_len = 0;
    wire [63:0] rdata;
    wire busy, done, nack, valid;
    wire [7:0] seq_no, scan_addr, scan_count;
    wire scan_done;
    wire scl_low, sda_low;
    logic sda_in;

    i2c_master #(.CLK_HZ(1_000_000), .I2C_HZ(100_000)) dut (
        .clk(clk), .resetn(resetn), .enable(enable),
        .cmd_op(cmd_op), .cmd_addr(cmd_addr), .cmd_reg(cmd_reg),
        .cmd_len(cmd_len), .cmd_wdata(cmd_wdata), .cmd_start(cmd_start),
        .auto_enable(auto_enable), .auto_addr(auto_addr), .auto_reg(auto_reg),
        .auto_len(auto_len), .rdata(rdata), .busy(busy), .done(done),
        .nack(nack), .valid(valid), .seq_no(seq_no), .scan_addr(scan_addr),
        .scan_count(scan_count), .scan_done(scan_done),
        .scl_low(scl_low), .sda_low(sda_low), .scl_in(scl), .sda_in(sda_in)
    );

    // ------------------------------------------------------------ the bus --
    logic slave_sda_low = 0, slave_scl_low = 0;
    logic slave_present = 1;
    logic bus_dead = 0;
    logic scl_stuck = 0;                // a device holding SCL low for good
    integer stretch_ns = 0;             // the slave's hold on its ACK clock
    logic wii_mode = 0;                 // reads need STOP and 150 us first
    wire scl = (scl_low || slave_scl_low || bus_dead || scl_stuck) ? 1'b0 : 1'b1;
    wire sda = (sda_low || slave_sda_low || bus_dead) ? 1'b0 : 1'b1;

    // SCL, once it rises, stays high for a whole half-period (5 clocks):
    // the engine times the high half from when it reads the line high.
    realtime scl_rose = 0;
    always @(posedge scl) scl_rose = $realtime;
    always @(negedge scl)
        if (busy && $realtime - scl_rose < 5000.0)
            $fatal(1, "SCL high for only %0t", $realtime - scl_rose);
    assign sda_in = sda;

    // ---------------------------------------------------------- the slave --
    logic [7:0] mem [0:255];
    logic [7:0] shift;
    integer bitcnt, nbytes, reg_ptr;
    logic ack_phase, ack_done, addressed, read_op, tx_active, master_nack;
    logic [7:0] tx_shift;
    logic ptr_written = 0, ptr_stopped = 0, blank = 0;
    realtime ptr_stop_time = 0, start_time = 0;
    integer write_probes = 0;           // scan probes with the write bit
    function automatic [7:0] out_byte(input integer a);
        out_byte = blank ? 8'hff : mem[a[7:0]];
    endfunction

    task automatic reset_slave;
        begin
            bitcnt = 0; ack_phase = 0; ack_done = 0; nbytes = 0;
            addressed = 0; read_op = 0; tx_active = 0;
            slave_sda_low = 0; shift = 0;
        end
    endtask

    always @(negedge sda) if (scl) begin  // START
        start_time = $realtime;
        reset_slave();
    end
    always @(posedge sda) if (scl) begin  // STOP
        if (ptr_written) begin
            ptr_stopped = 1;
            ptr_stop_time = $realtime;
        end
        ptr_written = 0;
        reset_slave();
    end

    // Bits, the master's ACK among them, are taken on SCL's rising edge, as
    // a real device takes them: the master releases SDA in the same clock
    // that it pulls SCL low after its ACK, so SDA at the falling edge races.
    always @(posedge scl) begin
        if (bitcnt < 8) begin
            shift <= {shift[6:0], sda};
            bitcnt <= bitcnt + 1;
        end else if (ack_done) begin
            master_nack <= sda;
        end
    end

    always @(negedge scl) begin
        if (bitcnt < 8) begin
            if (tx_active) begin
                slave_sda_low <= ~tx_shift[7];
                tx_shift <= {tx_shift[6:0], 1'b0};
            end else begin
                slave_sda_low <= 1'b0;
            end
        end else if (!ack_done) begin
            if (!addressed && nbytes == 0 && !shift[0] && dut.op_q == 2'd2)
                write_probes = write_probes + 1;
            // ACK the byte just received if it is this device.
            if (!tx_active && slave_present &&
                ((!addressed && shift[7:1] == 7'h68) || addressed)) begin
                if (stretch_ns > 0) begin
                    // Hold SCL, and ACK only late in the hold.
                    slave_scl_low = 1'b1;
                    #(stretch_ns) slave_sda_low = 1'b1;
                    #100 slave_scl_low = 1'b0;
                end else begin
                    slave_sda_low <= 1'b1;
                end
            end else begin
                slave_sda_low <= 1'b0;
            end
            ack_done <= 1'b1;
        end else begin
            slave_sda_low <= 1'b0;
            ack_done <= 1'b0;
            bitcnt <= 0;
            if (!addressed) begin
                if (shift[7:1] == 7'h68 && slave_present) begin
                    addressed <= 1'b1;
                    read_op <= shift[0];
                    nbytes <= 1;
                    if (shift[0]) begin
                        // A read address: the register pointer is already set.
                        // The pause counts from the STOP to this START.
                        blank = wii_mode && !(ptr_stopped &&
                                              start_time - ptr_stop_time >= 150_000.0);
                        ptr_stopped = 0;
                        tx_active <= 1'b1;
                        tx_shift <= {out_byte(reg_ptr) << 1};
                        slave_sda_low <= out_byte(reg_ptr) < 8'h80;   // a 0 in bit 7
                        reg_ptr <= reg_ptr + 1;
                    end
                end
            end else if (read_op) begin
                // The master's ACK bit says whether it wants another byte.
                // A NACK ends the read, so the slave must release SDA for
                // the STOP rather than drive the next byte's first bit.
                if (master_nack) begin
                    tx_active <= 1'b0;
                end else begin
                    tx_shift <= {out_byte(reg_ptr) << 1};
                    slave_sda_low <= out_byte(reg_ptr) < 8'h80;   // a 0 in bit 7
                    reg_ptr <= reg_ptr + 1;
                end
            end else begin
                if (nbytes == 1) begin
                    $display("slave reg=%02x", shift);
                    reg_ptr <= shift;
                    ptr_written = 1;
                    ptr_stopped = 0;
                    nbytes <= 2;
                end else begin
                    $display("slave write mem[%02x]=%02x", reg_ptr, shift);
                    mem[reg_ptr] <= shift;
                    reg_ptr <= reg_ptr + 1;
                    nbytes <= nbytes + 1;
                end
            end
        end
    end

    // ------------------------------------------------------------- checks --
    task automatic wait_done(input integer limit);
        integer n;
        begin
            n = 0;
            while (!done && n < limit) begin
                @(posedge clk);
                n = n + 1;
            end
            if (!done) $fatal(1, "timed out waiting for done");
        end
    endtask

    task automatic do_cmd(input [1:0] op, input [6:0] addr, input [7:0] reg_a,
                          input [4:0] len);
        begin
            @(posedge clk);
            cmd_op = op; cmd_addr = addr; cmd_reg = reg_a; cmd_len = len;
            cmd_start = 1;
            @(posedge clk);
            cmd_start = 0;
        end
    endtask

    task automatic do_write(input [6:0] addr, input [7:0] reg_a,
                            input [4:0] len, input [63:0] data);
        begin
            @(posedge clk);
            cmd_op = 1; cmd_addr = addr; cmd_reg = reg_a; cmd_len = len;
            cmd_wdata = data; cmd_start = 1;
            @(posedge clk);
            cmd_start = 0;
        end
    endtask

    logic wr_dbg = 0;
    integer wdbg = 0;
    always @(posedge clk) begin
        if (wr_dbg && wdbg < 120) begin
            if (wdbg % 5 == 0)
                $display("W t=%0t st=%0d seq=%0d scl=%b sda=%b sl=%b bit=%0d add=%b nb=%0d rp=%02x",
                         $time, dut.state, dut.seq, scl, sda, slave_sda_low,
                         bitcnt, addressed, nbytes, reg_ptr);
            wdbg = wdbg + 1;
        end
    end

    integer i;
    initial begin
        for (i = 0; i < 256; i = i + 1) mem[i] = i[7:0];

        repeat (4) @(posedge clk);
        resetn = 1;
        enable = 1;
        repeat (10) @(posedge clk);

        // Seven-byte read from 0x00.
        do_cmd(0, 7'h68, 8'h00, 5'd7);
        wait_done(4000);
        if (rdata[55:0] !== {mem[6], mem[5], mem[4], mem[3], mem[2], mem[1], mem[0]})
            $fatal(1, "read 0x00 mismatch: %014h", rdata[55:0]);
        if (!valid || nack) $fatal(1, "read status valid=%0d nack=%0d", valid, nack);
        $display("read 0x00: %014h", rdata[55:0]);

        // Write five bytes to 0x20.
        $display("issuing write busy=%b state=%0d", busy, dut.state);
        wr_dbg = 1;
        do_write(7'h68, 8'h20, 5'd5, {8'hA4, 8'hA3, 8'hA2, 8'hA1, 8'hA0});
        $display("after issue busy=%b state=%0d op=%0d addr=%02x reg=%02x len=%0d",
                 busy, dut.state, cmd_op, cmd_addr, cmd_reg, cmd_len);
        wait_done(4000);
        if (nack) $fatal(1, "write NACKed");
        for (i = 0; i < 5; i = i + 1)
            if (mem[8'h20 + i] !== 8'hA0 + i[7:0])
                $fatal(1, "write byte %0d: got %02h", i, mem[8'h20 + i]);
        $display("write 0x20: ok");

        // Scan finds 0x68.
        do_cmd(2, 0, 0, 1);
        wait (scan_done);
        repeat (2) @(posedge clk);
        if (scan_addr !== 8'h68 || scan_count !== 8'd1)
            $fatal(1, "scan: addr=%02h count=%0d", scan_addr, scan_count);
        if (write_probes != 0 || slave_sda_low)
            $fatal(1, "scan: %0d write probes, slave SDA %0d", write_probes, slave_sda_low);
        $display("scan: addr %02h count %0d, read probes only", scan_addr, scan_count);

        // Absent device.
        slave_present = 0;
        do_cmd(0, 7'h68, 8'h00, 5'd1);
        wait_done(4000);
        if (!nack || valid) $fatal(1, "absent: nack=%0d valid=%0d", nack, valid);
        $display("absent: nack ok");
        slave_present = 1;

        // The slave stretches every ACK clock by 40 us, eight half-periods.
        stretch_ns = 40_000;
        for (i = 0; i < 256; i = i + 1) mem[i] = 8'hff - i[7:0];
        do_cmd(0, 7'h68, 8'h00, 5'd7);
        wait_done(20000);
        if (nack || !valid ||
            rdata[55:0] !== {mem[6], mem[5], mem[4], mem[3], mem[2], mem[1], mem[0]})
            $fatal(1, "stretched read: nack=%0d valid=%0d %014h", nack, valid, rdata[55:0]);
        do_write(7'h68, 8'h30, 5'd2, {8'h5B, 8'h5A});
        wait_done(20000);
        if (nack || mem[8'h30] !== 8'h5A || mem[8'h31] !== 8'h5B)
            $fatal(1, "stretched write: nack=%0d %02h %02h", nack, mem[8'h30], mem[8'h31]);
        do_cmd(2, 0, 0, 1);
        wait (scan_done);
        repeat (2) @(posedge clk);
        if (scan_addr !== 8'h68 || scan_count !== 8'd1)
            $fatal(1, "stretched scan: addr=%02h count=%0d", scan_addr, scan_count);
        $display("stretched clock: read, write and scan ok");

        // Read as a Wii controller: the bytes come only after a STOP and a
        // pause, still with the clock stretched.
        wii_mode = 1;
        do_cmd(0, 7'h68, 8'hfa, 5'd6);
        wait_done(30000);
        if (nack || !valid ||
            rdata[47:0] !== {mem[8'hff], mem[8'hfe], mem[8'hfd], mem[8'hfc], mem[8'hfb], mem[8'hfa]})
            $fatal(1, "Wii read: nack=%0d valid=%0d %012h", nack, valid, rdata[47:0]);
        do_cmd(2, 0, 0, 1);
        wait (scan_done);
        repeat (2) @(posedge clk);
        if (scan_addr !== 8'h68 || scan_count !== 8'd1 || write_probes != 0 || slave_sda_low)
            $fatal(1, "Wii scan: addr=%02h count=%0d write probes %0d", scan_addr, scan_count,
                   write_probes);
        $display("Wii-style reads and scan ok");
        wii_mode = 0;
        stretch_ns = 0;

        // A dead bus: each command gives up after 1 ms (1000 clocks here)
        // with a NACK and both lines released, and a scan finds nothing.
        bus_dead = 1;
        do_cmd(0, 7'h68, 8'h00, 5'd1);
        wait_done(1500);
        if (!nack || valid || scl_low || sda_low)
            $fatal(1, "dead bus read: nack=%0d valid=%0d scl_low=%0d sda_low=%0d",
                   nack, valid, scl_low, sda_low);
        do_cmd(2, 0, 0, 1);
        wait_done(1500);
        repeat (2) @(posedge clk);
        if (!scan_done || scan_count !== 8'd0 || !nack || scl_low || sda_low)
            $fatal(1, "dead bus scan: done=%0d count=%0d nack=%0d", scan_done, scan_count, nack);
        $display("dead bus: nack after the timeout, lines released");

        bus_dead = 0;

        // SCL stuck low in the middle of a byte, during a 0 bit, while the
        // engine holds SDA low: the abort must let SDA go too.
        do_cmd(0, 7'h68, 8'h00, 5'd1);
        wait (busy && dut.state == 5'd5 && sda_low);   // S_TX2
        scl_stuck = 1;
        wait_done(1500);
        repeat (2) @(posedge clk);
        if (!nack || valid || scl_low || sda_low)
            $fatal(1, "stuck SCL: nack=%0d valid=%0d scl_low=%0d sda_low=%0d",
                   nack, valid, scl_low, sda_low);
        $display("stuck SCL mid-byte: nack, SDA released");
        scl_stuck = 0;

        // The bus comes back and so does the device.
        do_cmd(0, 7'h68, 8'h00, 5'd7);
        wait_done(4000);
        if (nack || !valid) $fatal(1, "after the dead bus: nack=%0d valid=%0d", nack, valid);
        $display("recovered: read ok");

        $display("i2c_master: PASS");
        $finish;
    end

    initial begin
        #100_000_000;
        $fatal(1, "global timeout");
    end
endmodule
