// Implementation of HDMI packet ECC calculation.
// By Sameer Puri https://github.com/sameer
// source: https://github.com/hdl-util/hdmi/
// Dual-licensed under Apache License 2.0 and MIT License.

// converted to Verilog for hsdaoh

module packet_assembler (
	clk_pixel,
	reset,
	data_island_period,
	header, // See Table 5-8 Packet Types
	sub,
	packet_data, // See Figure 5-4 Data Island Packet and ECC Structure
	counter
);
	input wire clk_pixel;
	input wire reset;
	input wire data_island_period;
	input wire [23:0] header;
	input wire [223:0] sub;
	output wire [8:0] packet_data;
	output reg [4:0] counter = 5'd0;

	// 32 pixel wrap-around counter. See Section 5.2.3.4 for further information.
	always @(posedge clk_pixel)
	begin
		if (reset)
			counter <= 5'd0;
		else if (data_island_period)
			counter <= counter + 5'd1;
	end
	// BCH packets 0 to 3 are transferred two bits at a time, see Section 5.2.3.4 for further information.
	wire [5:0] counter_t2 = {counter, 1'b0};
	wire [5:0] counter_t2_p1 = {counter, 1'b1};

	// Initialize parity bits to 0
	reg [39:0] parity = 40'h0000000000;

	wire [63:0] bch [3:0];
	assign bch[0] = {parity[0+:8], sub[0+:56]};
	assign bch[1] = {parity[8+:8], sub[56+:56]};
	assign bch[2] = {parity[16+:8], sub[112+:56]};
	assign bch[3] = {parity[24+:8], sub[168+:56]};
	wire [31:0] bch4 = {parity[32+:8], header};
	assign packet_data = {bch[3][counter_t2_p1], bch[2][counter_t2_p1], bch[1][counter_t2_p1], bch[0][counter_t2_p1], bch[3][counter_t2], bch[2][counter_t2], bch[1][counter_t2], bch[0][counter_t2], bch4[counter]};

	// See Figure 5-5 Error Correction Code generator. Generalization of a CRC with binary BCH.
	// See https://web.archive.org/web/20190520020602/http://hamsterworks.co.nz/mediawiki/index.php/Minimal_HDMI#Computing_the_ECC for an explanation of the implementation.
	// See https://en.wikipedia.org/wiki/BCH_code#Systematic_encoding:_The_message_as_a_prefix for further information.
	function automatic [7:0] next_ecc;
		input [7:0] ecc, next_bch_bit;
	begin
		next_ecc = (ecc >> 1) ^ (ecc[0] ^ next_bch_bit ? 8'b10000011 : 8'd0);
	end
	endfunction

	wire [7:0] parity_next [4:0];

	// The parity needs to be calculated 2 bits at a time for blocks 0 to 3.
	// There's 56 bits being sent 2 bits at a time over TMDS channels 1 & 2, so the parity bits wouldn't be ready in time otherwise.
	wire [31:0] parity_next_next;

	genvar i;
	generate
		for (i = 0; i < 5; i = i + 1) begin : parity_calc
			if (i == 4) begin : genblk1
				assign parity_next[i] = next_ecc(parity[i * 8+:8], header[counter]);
			end
			else begin : genblk1
				assign parity_next[i] = next_ecc(parity[i * 8+:8], sub[(i * 56) + counter_t2]);
				assign parity_next_next[i * 8+:8] = next_ecc(parity_next[i], sub[(i * 56) + counter_t2_p1]);
			end
		end
	endgenerate
	always @(posedge clk_pixel)
	begin
		if (reset)
			parity <= 40'h0000000000;
		else if (data_island_period) begin
			if (counter < 5'd28) begin  // Compute ECC only on subpacket data, not on itself
				parity[0+:32] <= parity_next_next;
				if (counter < 5'd24)  // Header only has 24 bits, whereas subpackets have 56 and 56 / 2 = 28.
					parity[32+:8] <= parity_next[4];
			end
			else if (counter == 5'd31)
				parity <= 40'h0000000000; // Reset ECC for next packet
		end
		else
			parity <= 40'h0000000000;
	end
endmodule
