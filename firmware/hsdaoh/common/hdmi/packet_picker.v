// Implementation of HDMI packet choice logic.
// By Sameer Puri https://github.com/sameer
// source: https://github.com/hdl-util/hdmi/
// Dual-licensed under Apache License 2.0 and MIT License.

// converted to Verilog and removed packets not required for hsdaoh

module packet_picker (
	clk_pixel,
	reset,
	video_field_end,
	packet_enable,
	packet_pixel_counter,
	header,
	sub
);
	parameter [6:0] VIDEO_ID_CODE = 4;
	parameter [0:0] IT_CONTENT = 1'b0;
	input wire clk_pixel;
	input wire reset;
	input wire video_field_end;
	input wire packet_enable;
	input wire [4:0] packet_pixel_counter;
	output wire [23:0] header;
	output wire [223:0] sub;

	// Connect the current packet type's data to the output.
	reg [7:0] packet_type = 8'd0;
	wire [23:0] headers [255:0];
	wire [223:0] subs [255:0];
	assign header = headers[packet_type];
	assign sub[0+:56] = subs[packet_type][0+:56];
	assign sub[56+:56] = subs[packet_type][56+:56];
	assign sub[112+:56] = subs[packet_type][112+:56];
	assign sub[168+:56] = subs[packet_type][168+:56];

	// NULL packet
	// "An HDMI Sink shall ignore bytes HB1 and HB2 of the Null Packet Header and all bytes of the Null Packet Body."
	assign headers[0] = 24'hxxxx00;
	assign subs[0][0+:56] = 56'bxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx;
	assign subs[0][56+:56] = 56'bxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx;
	assign subs[0][112+:56] = 56'bxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx;
	assign subs[0][168+:56] = 56'bxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx;

	reg [7:0] frame_counter = 8'd0;
	always @(posedge clk_pixel)
	begin
		if (reset)
			frame_counter <= 8'd0;
		else if ((packet_pixel_counter == 5'd31) && (packet_type == 8'h02)) begin
			frame_counter = frame_counter + 8'd4;
			if (frame_counter >= 8'd192)
				frame_counter = frame_counter - 8'd192;
		end
	end

	auxiliary_video_information_info_frame #(
		.VIDEO_ID_CODE(VIDEO_ID_CODE),
		.IT_CONTENT(IT_CONTENT)
	) auxiliary_video_information_info_frame(
		.header(headers[130]),
		.sub(subs[130])
	);

	reg auxiliary_video_information_info_frame_sent = 1'b0;
	always @(posedge clk_pixel)
	begin
		if (reset || video_field_end) begin
			auxiliary_video_information_info_frame_sent <= 1'b0;
			packet_type <= 8'bxxxxxxxx;
		end
		else if (packet_enable) begin
			if (!auxiliary_video_information_info_frame_sent) begin
				packet_type <= 8'h82;
				auxiliary_video_information_info_frame_sent <= 1'b1;
			end
			else
				packet_type <= 8'd0;
		end
	end
endmodule
