// TMDS serializer
// By Sameer Puri https://github.com/sameer
// source: https://github.com/hdl-util/hdmi/
// Dual-licensed under Apache License 2.0 and MIT License.

// converted to Verilog for hsdaoh
// currently only contains the GOWIN OSER10 primitives
// TODO: add back everything from https://github.com/hdl-util/hdmi/blob/master/src/serializer.sv

module serializer (
	clk_pixel,
	clk_pixel_x5,
	reset,
	tmds_internal,
	tmds,
	tmds_clock
);
	parameter signed [31:0] NUM_CHANNELS = 3;
	parameter real VIDEO_RATE = 0;
	input wire clk_pixel;
	input wire clk_pixel_x5;
	input wire reset;
	input wire [(NUM_CHANNELS * 10) - 1:0] tmds_internal;
	output wire [2:0] tmds;
	output wire tmds_clock;
	OSER10 gwSer0(
		.Q(tmds[0]),
		.D0(tmds_internal[0]),
		.D1(tmds_internal[1]),
		.D2(tmds_internal[2]),
		.D3(tmds_internal[3]),
		.D4(tmds_internal[4]),
		.D5(tmds_internal[5]),
		.D6(tmds_internal[6]),
		.D7(tmds_internal[7]),
		.D8(tmds_internal[8]),
		.D9(tmds_internal[9]),
		.PCLK(clk_pixel),
		.FCLK(clk_pixel_x5),
		.RESET(reset)
	);
	OSER10 gwSer1(
		.Q(tmds[1]),
		.D0(tmds_internal[10]),
		.D1(tmds_internal[11]),
		.D2(tmds_internal[12]),
		.D3(tmds_internal[13]),
		.D4(tmds_internal[14]),
		.D5(tmds_internal[15]),
		.D6(tmds_internal[16]),
		.D7(tmds_internal[17]),
		.D8(tmds_internal[18]),
		.D9(tmds_internal[19]),
		.PCLK(clk_pixel),
		.FCLK(clk_pixel_x5),
		.RESET(reset)
	);
	OSER10 gwSer2(
		.Q(tmds[2]),
		.D0(tmds_internal[20]),
		.D1(tmds_internal[21]),
		.D2(tmds_internal[22]),
		.D3(tmds_internal[23]),
		.D4(tmds_internal[24]),
		.D5(tmds_internal[25]),
		.D6(tmds_internal[26]),
		.D7(tmds_internal[27]),
		.D8(tmds_internal[28]),
		.D9(tmds_internal[29]),
		.PCLK(clk_pixel),
		.FCLK(clk_pixel_x5),
		.RESET(reset)
	);
	assign tmds_clock = clk_pixel;
endmodule
