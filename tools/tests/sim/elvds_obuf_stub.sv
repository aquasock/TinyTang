// A stand-in for Gowin's LVDS output buffer, for simulation only.
//
// The real one drives the physical TMDS pairs and does not exist on a host.
// Nothing this test checks leaves the chip -- it looks at the colour the
// compositor chose -- so this just carries the signal through and inverts it
// for the complementary pin, which is what the buffer does.
module ELVDS_OBUF (
    input  I,
    output O,
    output OB
);

    assign O  = I;
    assign OB = ~I;

endmodule
