// A stand-in for the audio clock regeneration packet, for simulation only.
//
// The real module cannot be compiled for simulation: it computes its constants
// with a cast in a constant expression, which cannot be folded.  Nothing this
// test checks involves audio -- the composite picks a colour, and the audio
// packets ride alongside it in the data islands -- so this binds the same
// ports and does nothing.
module audio_clock_regeneration_packet
#(
    parameter real VIDEO_RATE = 25.2E6,
    parameter int  AUDIO_RATE = 48e3
)
(
    input  logic        clk_pixel,
    input  logic        clk_audio,
    output logic        clk_audio_counter_wrap = 0,
    output logic [23:0] header,
    output logic [55:0] sub [3:0]
);

    initial begin
        clk_audio_counter_wrap = 0;
        header = 24'h0;
        for (int i = 0; i < 4; i++) sub[i] = 56'h0;
    end

endmodule
