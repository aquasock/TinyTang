// SPDX-License-Identifier: MIT
// The pipelined TMDS encoder (fpga/desktop/hdmi/tmds_channel.sv) against the
// library's original, unpipelined one (tmds_channel_ref, taken from the tree
// before the change by tools/tests/test_tmds_pipeline.sh): on every channel,
// for random runs of every mode -- long video runs, so the disparity counter
// wanders both ways, control periods, guard bands and data islands -- the new
// output must be the original's exactly one pixel clock later.
`timescale 1ns/1ps
module tb_tmds_pipeline;
    logic clk = 0;
    always #5 clk = ~clk;
    logic [7:0] video;
    logic [3:0] island;
    logic [1:0] ctrl;
    logic [2:0] mode;
    wire  [9:0] got [0:2];
    wire  [9:0] ref_out [0:2];
    genvar c;
    generate for (c = 0; c < 3; c++) begin : ch
        tmds_channel     #(.CN(c)) dut (.clk_pixel(clk), .video_data(video), .data_island_data(island),
                                        .control_data(ctrl), .mode(mode), .tmds(got[c]));
        tmds_channel_ref #(.CN(c)) ref_ (.clk_pixel(clk), .video_data(video), .data_island_data(island),
                                         .control_data(ctrl), .mode(mode), .tmds(ref_out[c]));
    end endgenerate

    logic [9:0] ref_d [0:2];
    integer cycles = 0, checked = 0, video_cycles = 0, run = 0;
    always @(posedge clk) begin
        for (int k = 0; k < 3; k++) ref_d[k] <= ref_out[k];
        if (cycles > 2) begin
            for (int k = 0; k < 3; k++)
                if (got[k] !== ref_d[k])
                    $fatal(1, "channel %0d cycle %0d: %b, the original one clock earlier gave %b",
                           k, cycles, got[k], ref_d[k]);
            checked++;
        end
        cycles++;
    end

    initial begin
        video = 0; island = 0; ctrl = 0; mode = 0;
        repeat (200000) begin
            @(negedge clk);
            if (run == 0) begin
                // A new period: mostly video, as on screen; its length random.
                case ($urandom % 8)
                    0, 1:    mode = 3'd0;
                    2:       mode = 3'd2;
                    3:       mode = 3'd3;
                    4:       mode = 3'd4;
                    default: mode = 3'd1;
                endcase
                run = (mode == 3'd1) ? 1 + $urandom % 400 : 1 + $urandom % 40;
            end
            run--;
            // Pixels: sometimes flat runs (long disparity drift), sometimes noise.
            if (($urandom % 16) != 0) video = ($urandom % 4 == 0) ? video : 8'($urandom);
            island = 4'($urandom);
            ctrl = 2'($urandom);
            if (mode == 3'd1) video_cycles++;
        end
        $display("tmds pipeline: %0d cycles on 3 channels match the original one clock later (%0d video) PASS",
                 checked, video_cycles);
        $finish;
    end
endmodule
