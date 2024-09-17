// Implementation of HDMI Spec v1.4a
// By Sameer Puri https://github.com/sameer
// source: https://github.com/hdl-util/hdmi/
// Dual-licensed under Apache License 2.0 and MIT License.

// converted to Verilog and stripped down for hsdaoh,
// changed VIDEO_ID_CODE 16 to minimal timings the MS2130 accepts

module hdmi (
	clk_pixel_x5,
	clk_pixel,
	reset,
	rgb,
	tmds,
	tmds_clock,
	cx,
	cy,
	frame_width,
	frame_height,
	screen_width,
	screen_height
);
	// Defaults to 640x480 which should be supported by almost if not all HDMI sinks.
	// See README.md or CEA-861-D for enumeration of video id codes.
	// Pixel repetition, interlaced scans and other special output modes are not implemented (yet).
	parameter [6:0] VIDEO_ID_CODE = 1;

	// The IT content bit indicates that image samples are generated in an ad-hoc
	// manner (e.g. directly from values in a framebuffer, as by a PC video
	// card) and therefore aren't suitable for filtering or analog
	// reconstruction.  This is probably what you want if you treat pixels
	// as "squares".  If you generate a properly bandlimited signal or obtain
	// one from elsewhere (e.g. a camera), this can be turned off.
	//
	// This flag also tends to cause receivers to treat RGB values as full
	// range (0-255).
	parameter [0:0] IT_CONTENT = 1'b1;

	// Defaults to minimum bit lengths required to represent positions.
	// Modify these parameters if you have alternate desired bit lengths.
	parameter [12:0] BIT_WIDTH = (VIDEO_ID_CODE < 4 ? 10 : (VIDEO_ID_CODE == 4 ? 11 : 12));
	parameter [12:0] BIT_HEIGHT = (VIDEO_ID_CODE == 16 ? 11 : 10);

	// A true HDMI signal sends auxiliary data (i.e. audio, preambles) which prevents it from being parsed by DVI signal sinks.
	// HDMI signal sinks are fortunately backwards-compatible with DVI signals.
	// Enable this flag if the output should be a DVI signal. You might want to do this to reduce resource usage or if you're only outputting video.
	parameter [0:0] DVI_OUTPUT = 1'b0;

	// **All parameters below matter ONLY IF you plan on sending auxiliary data (DVI_OUTPUT == 1'b0)**

	// Starting screen coordinate when module comes out of reset.
	//
	// Setting these to something other than (0, 0) is useful when positioning
	// an external video signal within a larger overall frame (e.g.
	// letterboxing an input video signal). This allows you to synchronize the
	// negative edge of reset directly to the start of the external signal
	// instead of to some number of clock cycles before.
	//
	// You probably don't need to change these parameters if you are
	// generating a signal from scratch instead of processing an
	// external signal.
	parameter [12:0] START_X = 0;
	parameter [12:0] START_Y = 0;

	input wire clk_pixel_x5;
	input wire clk_pixel;
	// synchronous reset back to 0,0
	input wire reset;
	input wire [23:0] rgb;

	// These outputs go to your HDMI port
	output wire [2:0] tmds;
	output wire tmds_clock;

	// All outputs below this line stay inside the FPGA
	// They are used (by you) to pick the color each pixel should have
	// i.e. always_ff @(posedge pixel_clk) rgb <= {8'd0, 8'(cx), 8'(cy)};
	output reg [BIT_WIDTH - 1:0] cx = START_X;
	output reg [BIT_HEIGHT - 1:0] cy = START_Y;

	// The screen is at the upper left corner of the frame.
	// 0,0 = 0,0 in video
	// the frame includes extra space for sending auxiliary data
	output wire [BIT_WIDTH - 1:0] frame_width;
	output wire [BIT_HEIGHT - 1:0] frame_height;
	output wire [BIT_WIDTH - 1:0] screen_width;
	output wire [BIT_HEIGHT - 1:0] screen_height;

	localparam signed [31:0] NUM_CHANNELS = 3;
	reg hsync;
	reg vsync;

	wire [BIT_WIDTH - 1:0] hsync_pulse_start, hsync_pulse_size;
	wire [BIT_HEIGHT - 1:0] vsync_pulse_start, vsync_pulse_size;
	wire invert;

	// See CEA-861-D for more specifics formats described below.
	generate
		case (VIDEO_ID_CODE)
			16, 34: begin : genblk1
				// Those are the patched, minimal timings for video mode 16 that
				// work with the MS2130. The inactive video period has been reduced
				// significantly. See below (mode 99) for the original timings for
				// 1080p
				assign frame_width = 1982;
				assign frame_height = 1084;
				assign screen_width = 1920;
				assign screen_height = 1080;
				assign hsync_pulse_start = 6;
				assign hsync_pulse_size = 4;
				assign vsync_pulse_start = 0;
				assign vsync_pulse_size = 1;
				assign invert = 0;
			end
			99: begin : genblk1
				assign frame_width = 2200;
				assign frame_height = 1125;
				assign screen_width = 1920;
				assign screen_height = 1080;
				assign hsync_pulse_start = 88;
				assign hsync_pulse_size = 44;
				assign vsync_pulse_start = 4;
				assign vsync_pulse_size = 5;
				assign invert = 0;
			end
		endcase
	endgenerate
	always @(*) begin
		hsync <= invert ^ ((cx >= (screen_width + hsync_pulse_start)) && (cx < ((screen_width + hsync_pulse_start) + hsync_pulse_size)));
		// vsync pulses should begin and end at the start of hsync, so special
		// handling is required for the lines on which vsync starts and ends
		if (cy == ((screen_height + vsync_pulse_start) - 1))
			vsync <= invert ^ (cx >= (screen_width + hsync_pulse_start));
		else if (cy == (((screen_height + vsync_pulse_start) + vsync_pulse_size) - 1))
			vsync <= invert ^ (cx < (screen_width + hsync_pulse_start));
		else
			vsync <= invert ^ ((cy >= (screen_height + vsync_pulse_start)) && (cy < ((screen_height + vsync_pulse_start) + vsync_pulse_size)));
	end

	// Wrap-around pixel position counters indicating the pixel to be generated by the user in THIS clock and sent out in the NEXT clock.
	always @(posedge clk_pixel)
	begin
		if (reset) begin
			cx <= START_X;
			cy <= START_Y;
		end
		else begin
			cx <= (cx == (frame_width - 1'b1) ? 0 : cx + 1'b1);
			cy <= (cx == (frame_width - 1'b1) ? (cy == (frame_height - 1'b1) ? 0 : cy + 1'b1) : cy);
		end
	end

	// See Section 5.2
	reg video_data_period = 0;
	always @(posedge clk_pixel)
	begin
		if (reset)
			video_data_period <= 0;
		else
			video_data_period <= (cx < screen_width) && (cy < screen_height);
	end

	reg [2:0] mode = 3'd1;
	reg [23:0] video_data = 24'd0;
	reg [5:0] control_data = 6'd0;
	reg [11:0] data_island_data = 12'd0;

	function automatic [4:0] sv2v_cast_5;
		input reg [4:0] inp;
		sv2v_cast_5 = inp;
	endfunction

	generate
		if (!DVI_OUTPUT) begin : true_hdmi_output
			reg video_guard = 1;
			reg video_preamble = 0;
			always @(posedge clk_pixel)
			begin
				if (reset) begin
					video_guard <= 1;
					video_preamble <= 0;
				end
				else begin
					video_guard <= ((cx >= (frame_width - 2)) && (cx < frame_width)) && ((cy == (frame_height - 1)) || (cy < (screen_height - 1)));
					video_preamble <= ((cx >= (frame_width - 10)) && (cx < (frame_width - 2))) && ((cy == (frame_height - 1)) || (cy < (screen_height - 1)));
				end
			end

			reg [4:0] num_packets_alongside = 1; // patched for hsdaoh: set to minimum required number

			wire data_island_period_instantaneous;
			assign data_island_period_instantaneous = ((num_packets_alongside > 0) && (cx >= (screen_width + 14))) && (cx < ((screen_width + 14) + (num_packets_alongside * 32)));
			wire packet_enable;
			assign packet_enable = data_island_period_instantaneous && (sv2v_cast_5((cx + screen_width) + 18) == 5'd0);

			reg data_island_guard = 0;
			reg data_island_preamble = 0;
			reg data_island_period = 0;
			always @(posedge clk_pixel)
			begin
				if (reset) begin
					data_island_guard <= 0;
					data_island_preamble <= 0;
					data_island_period <= 0;
				end
				else begin
					data_island_guard <= num_packets_alongside > 0 && (
						(cx >= screen_width + 12 && cx < screen_width + 14) /* leading guard */ ||
						(cx >= screen_width + 14 + num_packets_alongside * 32 && cx < screen_width + 14 + num_packets_alongside * 32 + 2) /* trailing guard */
					);
					data_island_preamble <= num_packets_alongside > 0 && cx >= screen_width + 4 && cx < screen_width + 12;
					data_island_period <= data_island_period_instantaneous;
				end
			end

			// See Section 5.2.3.4
			wire [23:0] header;
			wire [223:0] sub;
			wire video_field_end;
			assign video_field_end = (cx == (screen_width - 1'b1)) && (cy == (screen_height - 1'b1));
			wire [4:0] packet_pixel_counter;
			packet_picker #(
				.VIDEO_ID_CODE(VIDEO_ID_CODE)
			) packet_picker(
				.clk_pixel(clk_pixel),
				.reset(reset),
				.video_field_end(video_field_end),
				.packet_enable(packet_enable),
				.packet_pixel_counter(packet_pixel_counter),
				.header(header),
				.sub(sub)
			);
			wire [8:0] packet_data;
			packet_assembler packet_assembler(
				.clk_pixel(clk_pixel),
				.reset(reset),
				.data_island_period(data_island_period),
				.header(header),
				.sub(sub),
				.packet_data(packet_data),
				.counter(packet_pixel_counter)
			);
			always @(posedge clk_pixel)
				if (reset) begin
					mode <= 3'd2;
					video_data <= 24'd0;
					control_data = 6'd0;
					data_island_data <= 12'd0;
				end
				else begin
					mode <= (data_island_guard ? 3'd4 : (data_island_period ? 3'd3 : (video_guard ? 3'd2 : (video_data_period ? 3'd1 : 3'd0))));
					video_data <= rgb;
					control_data <= {1'b0, data_island_preamble, 1'b0, video_preamble || data_island_preamble, vsync, hsync};
					data_island_data[11:4] <= packet_data[8:1];
					data_island_data[3] <= cx != 0;
					data_island_data[2] <= packet_data[0];
					data_island_data[1:0] <= {vsync, hsync};
				end
		end
		else begin : genblk2  // DVI_OUTPUT = 1
			reg video_guard = 1;
			reg video_preamble = 0;
			always @(posedge clk_pixel)
				if (reset) begin
					video_guard <= 1;
					mode <= 3'd0;
					video_data <= 24'd0;
					control_data <= 6'd0;
				end
				else begin
					video_guard <= ((cx >= (frame_width - 2)) && (cx < frame_width)) && ((cy == (frame_height - 1)) || (cy < (screen_height - 1)));
					video_preamble <= ((cx >= (frame_width - 10)) && (cx < (frame_width - 2))) && ((cy == (frame_height - 1)) || (cy < (screen_height - 1)));
					mode <= (video_data_period ? 3'd1 : (video_guard ? 3'd2 : 3'd0));
					video_data <= rgb;
					control_data <= {4'b0000, vsync, hsync};  // ctrl3, ctrl2, ctrl1, ctrl0, vsync, hsync
				end
		end
	endgenerate

	// All logic below relates to the production and output of the 10-bit TMDS code.
	wire [29:0] tmds_internal;
	genvar i;
	generate
		// TMDS code production.
		for (i = 0; i < NUM_CHANNELS; i = i + 1) begin : tmds_gen
			tmds_channel #(.CN(i)) tmds_channel(
				.clk_pixel(clk_pixel),
				.video_data(video_data[(i * 8) + 7:i * 8]),
				.data_island_data(data_island_data[(i * 4) + 3:i * 4]),
				.control_data(control_data[(i * 2) + 1:i * 2]),
				.mode(mode),
				.tmds(tmds_internal[i * 10+:10])
			);
		end
	endgenerate
	serializer #(
		.NUM_CHANNELS(NUM_CHANNELS)
	) serializer(
		.clk_pixel(clk_pixel),
		.clk_pixel_x5(clk_pixel_x5),
		.reset(reset),
		.tmds_internal(tmds_internal),
		.tmds(tmds),
		.tmds_clock(tmds_clock)
	);
endmodule
