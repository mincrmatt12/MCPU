/* multiplexes an array of A-G of segments and shows them all */

module segment_buf (
	input [63:0] data_,
	input cycle_clk,
	output [7:0] anodes,
	output [7:0] cathodes 
);

	wire [7:0] data [7:0];
	assign {data[7], data[6], data[5], data[4], data[3], data[2], data[1], data[0]} = data_;

	reg [2:0] current_element = 0;

	always @(posedge cycle_clk)
		current_element <= current_element + 1;
	
	assign cathodes[7:0] = ~data[current_element];
	assign anodes[7:0]   = ~(8'b1 << current_element);

endmodule

module hex_decoder (
	input [3:0] nybble,
	output reg [7:0] anodes 
);

	always @*
		case (nybble)
			4'h0: anodes <= 8'b00111111;
			4'h1: anodes <= 8'b00000110;
			4'h2: anodes <= 8'b01011011;
			4'h3: anodes <= 8'b01001111;
			4'h4: anodes <= 8'b01100110;
			4'h5: anodes <= 8'b01101101;
			4'h6: anodes <= 8'b01111101;
			4'h7: anodes <= 8'b00000111;
			4'h8: anodes <= 8'b01111111;
			4'h9: anodes <= 8'b01100111;
			4'hA: anodes <= 8'b01110111;
			4'hB: anodes <= 8'b01111100;
			4'hC: anodes <= 8'b00111001;
			4'hD: anodes <= 8'b01011110;
			4'hE: anodes <= 8'b01111001;
			4'hF: anodes <= 8'b01110001;
		endcase
endmodule

module segment_cdiv (
	input clk,
	output reg cycle_clk
);

	reg [15:0] divider = 0;
	localparam prescaler = 10000;

	always @(posedge clk) begin
		if (divider == prescaler) divider <= 0;
		else                      divider <= divider + 1;
		if (divider == prescaler) cycle_clk <= ~cycle_clk;
	end

endmodule
