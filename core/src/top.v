module top (
    input  clk,
    output [7:0] cathodes,
	output [7:0] anodes
);

    localparam BITS = 32;
    localparam LOG2DELAY = 20;

    wire bufg_clk;
    BUFG bufgctrl(.I(clk), .O(bufg_clk));

	reg [BITS+LOG2DELAY-1:0] counter = 0;

	always @(posedge bufg_clk)
		counter <= counter + 1;

	segment_displayer s7(
		.clk(bufg_clk),
		.value(counter >> LOG2DELAY),
		.cathodes(cathodes),
		.anodes(anodes)
	);
endmodule

module segment_displayer (
	input clk,
	input [31:0] value,
	output [7:0] cathodes,
	output [7:0] anodes
);

	wire [63:0] segdata;
	genvar i;

	generate
		for (i = 0; i < 8; i = i + 1) begin
			hex_decoder hd0(
				.nybble(value[(i+1)*4-1:i*4]),
				.anodes(segdata[(i+1)*8-1:i*8])
			);
		end
	endgenerate

	wire segclk;
	segment_cdiv scdiv(clk, segclk);

	segment_buf sb(
		.cycle_clk(segclk),
		.data_(segdata),
		.anodes(anodes),
		.cathodes(cathodes)
	);
endmodule
