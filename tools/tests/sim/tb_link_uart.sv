// SPDX-License-Identifier: GPL-3.0-only
// The desktop link's UART (fpga/desktop/link_uart.sv) on its own, at every
// rate it offers:
//   - back-to-back characters with one stop bit, as the BL616 sends them,
//     from a sender up to 3% fast or slow, all received in order with no
//     framing error and no fallback;
//   - transmitted characters with exact bit times (25, 10 or 5 crystal clocks)
//     and a stop bit, read back by an independent receiver;
//   - a character with no stop bit is dropped and counted;
//   - a one-clock low glitch on an idle line is not a character;
//   - above 2 Mbaud, a line low for 3 us is a fallback and 2.8 us is not; at
//     2 Mbaud no low run is.
//
//   tools/tests/test_link_uart.sh
`timescale 1ns/1ps
module tb_link_uart;
    logic sys_clk = 0, clk = 0;
    always #10 sys_clk = ~sys_clk;          // 50 MHz crystal
    always #23.26 clk = ~clk;               // ~21.49 MHz link clock

    logic [1:0] rate = 0;
    logic rxd = 1;
    wire  txd;
    wire [7:0] rx_data;
    wire rx_valid, tx_busy, fallback, framing_error;
    logic [7:0] tx_data = 0;
    logic tx_start = 0;

    link_uart dut (
        .sys_clk(sys_clk), .clk(clk), .rate(rate),
        .rx_data(rx_data), .rx_valid(rx_valid),
        .tx_data(tx_data), .tx_start(tx_start), .tx_busy(tx_busy),
        .fallback(fallback), .framing_error(framing_error),
        .rxd(rxd), .txd(txd)
    );

    integer checks = 0;
    integer got_n = 0, fe_n = 0, fb_n = 0;
    logic [7:0] got [0:1023];
    always @(posedge clk) begin
        if (rx_valid) begin got[got_n] = rx_data; got_n = got_n + 1; end
        if (framing_error) fe_n = fe_n + 1;
        if (fallback) fb_n = fb_n + 1;
    end

    function automatic realtime bit_time(input [1:0] r);
        bit_time = r == 2 ? 100.0 : r == 1 ? 200.0 : 500.0;
    endfunction

    // One character from the "BL616": start, eight data bits LSB first, and
    // a stop bit -- or none, for a framing error.
    task automatic send(input [7:0] b, input realtime bt, input bit stop);
        rxd = 0; #(bt);
        for (integer i = 0; i < 8; i++) begin rxd = b[i]; #(bt); end
        rxd = stop; #(bt);
        rxd = 1;
    endtask

    task automatic settle;
        repeat (20) @(posedge clk);
    endtask

    // Receive: a burst of back-to-back characters at one rate and skew.
    task automatic rx_burst(input [1:0] r, input real skew);
        logic [7:0] sent [0:255];
        integer base_got, base_fe, base_fb;
        realtime bt;
        begin
            rate = r; settle;
            bt = bit_time(r) * (1.0 + skew);
            base_got = got_n; base_fe = fe_n; base_fb = fb_n;
            for (integer i = 0; i < 256; i++) begin
                sent[i] = (i == 0) ? 8'h00 : (i == 1) ? 8'hFF : (i == 2) ? 8'h55 :
                          (i == 3) ? 8'hAA : 8'($urandom);
                send(sent[i], bt, 1);
            end
            settle;
            if (got_n - base_got != 256)
                $fatal(1, "rate %0d skew %0.3f: %0d of 256 characters received", r, skew, got_n - base_got);
            for (integer i = 0; i < 256; i++)
                if (got[base_got + i] !== sent[i])
                    $fatal(1, "rate %0d skew %0.3f: character %0d is %02h, sent %02h",
                           r, skew, i, got[base_got + i], sent[i]);
            if (fe_n != base_fe || fb_n != base_fb)
                $fatal(1, "rate %0d skew %0.3f: %0d framing errors, %0d fallbacks",
                       r, skew, fe_n - base_fe, fb_n - base_fb);
            checks++;
        end
    endtask

    // Transmit: every bit must last exactly the rate's whole number of crystal
    // clocks, and the value read at mid-bit must be the byte.
    task automatic tx_check(input [1:0] r);
        realtime bt, t0, t;
        logic [7:0] b, v;
        begin
            rate = r; settle;
            bt = bit_time(r);
            for (integer k = 0; k < 16; k++) begin
                b = (k == 0) ? 8'h00 : (k == 1) ? 8'hFF : 8'($urandom);
                @(posedge clk);
                if (tx_busy) $fatal(1, "busy before a character");
                fork
                    begin @(negedge txd); t0 = $realtime; end
                    begin
                        tx_data <= b; tx_start <= 1;
                        @(posedge clk); tx_start <= 0;
                        @(posedge clk);
                        if (!tx_busy) $fatal(1, "not busy after tx_start");
                    end
                join
                #(bt / 2);
                if (txd !== 0) $fatal(1, "rate %0d: start bit not held", r);
                for (integer i = 0; i < 8; i++) begin #(bt); v[i] = txd; end
                #(bt);
                if (txd !== 1) $fatal(1, "rate %0d: no stop bit", r);
                if (v !== b) $fatal(1, "rate %0d: sent %02h, line carried %02h", r, b, v);
                // The stop bit ends, and busy drops, after exactly ten bits.
                wait (!tx_busy); t = $realtime - t0;
                if (t < 10 * bt || t > 10 * bt + 200)
                    $fatal(1, "rate %0d: character took %0.1f ns, ten bits are %0.1f", r, t, 10 * bt);
                checks++;
            end
        end
    endtask

    // The edges, exactly: a 0x55 alternates every bit.
    task automatic tx_edges(input [1:0] r);
        realtime bt, last, now;
        begin
            rate = r; settle;
            bt = bit_time(r);
            @(posedge clk);
            fork
                begin @(negedge txd); last = $realtime; end
                begin tx_data <= 8'h55; tx_start <= 1; @(posedge clk); tx_start <= 0; end
            join
            for (integer i = 0; i < 9; i++) begin
                @(txd); now = $realtime;
                if (now - last < bt - 0.01 || now - last > bt + 0.01)
                    $fatal(1, "rate %0d: bit %0d lasted %0.2f ns, not %0.1f", r, i, now - last, bt);
                last = now;
            end
            wait (!tx_busy);
            checks++;
        end
    endtask

    task automatic low_for(input realtime ns);
        rxd = 0; #(ns); rxd = 1; #(2000);
        settle;
    endtask

    initial begin
        #1000;
        // Receive at every rate, nominal and with the sender's clock off.
        for (integer r = 0; r < 3; r++) begin
            rx_burst(r[1:0], 0.0);
            rx_burst(r[1:0], 0.03);
            rx_burst(r[1:0], -0.03);
        end
        // Transmit at every rate.
        for (integer r = 0; r < 3; r++) begin
            tx_check(r[1:0]);
            tx_edges(r[1:0]);
        end
        // A character with no stop bit is dropped and counted, and the next
        // one is still read.
        for (integer r = 0; r < 3; r++) begin
            integer g, f;
            rate = r[1:0]; settle;
            g = got_n; f = fe_n;
            send(8'h3C, bit_time(r[1:0]), 0);
            #(bit_time(r[1:0]) * 2);
            send(8'hC3, bit_time(r[1:0]), 1);
            settle;
            if (fe_n != f + 1 || got_n != g + 1 || got[g] !== 8'hC3)
                $fatal(1, "rate %0d: framing error not dropped and counted (%0d, %0d)", r, fe_n - f, got_n - g);
            checks++;
        end
        // A glitch on an idle line is not a character.
        for (integer r = 0; r < 3; r++) begin
            integer g, f;
            rate = r[1:0]; settle;
            g = got_n; f = fe_n;
            rxd = 0; #(25); rxd = 1; #(2000); settle;
            if (got_n != g || fe_n != f) $fatal(1, "rate %0d: a 25 ns glitch was read", r);
            checks++;
        end
        // The fallback: only above 2 Mbaud, and only past 3 us.
        for (integer r = 1; r < 3; r++) begin
            integer b;
            rate = r[1:0]; settle;
            b = fb_n;
            low_for(2800);
            if (fb_n != b) $fatal(1, "rate %0d: 2.8 us low was taken as a fallback", r);
            low_for(3100);
            if (fb_n != b + 1) $fatal(1, "rate %0d: 3.1 us low was not a fallback", r);
            low_for(50_000);
            if (fb_n != b + 2) $fatal(1, "rate %0d: a long low gave %0d fallbacks, not one", r, fb_n - b - 1);
            checks++;
        end
        begin
            integer b;
            rate = 0; settle;
            b = fb_n;
            low_for(50_000);
            if (fb_n != b) $fatal(1, "a fallback at 2 Mbaud");
            checks++;
        end
        $display("link_uart: PASS (%0d checks)", checks);
        $finish;
    end
    initial begin #50_000_000; $fatal(1, "link_uart test timeout"); end
endmodule
