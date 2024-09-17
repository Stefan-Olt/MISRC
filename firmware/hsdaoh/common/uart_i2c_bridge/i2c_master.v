// Tiny but mighty I2C master
// Copyright (c) 2021-2024  Artin Isagholian <artinisagholian@gmail.com>
// https://github.com/0xArt/Tiny_But_Mighty_I2C_Master_Verilog
// License: GPL V3.0
//
// converted to Verilog and extended with the 'got_acknowledge' output for hsdaoh

module i2c_master (
	clock,
	reset_n,
	enable,
	read_write,
	mosi_data,
	register_address,
	device_address,
	divider,
	miso_data,
	busy,
    got_acknowledge,
	external_serial_data,
	external_serial_clock
);

localparam S_IDLE                  =   0;
localparam S_START                 =   1;
localparam S_WRITE_ADDR_W          =   2;
localparam S_CHECK_ACK             =   3;
localparam S_WRITE_REG_ADDR        =   4;
localparam S_RESTART               =   5;
localparam S_WRITE_ADDR_R          =   6;
localparam S_READ_REG              =   7;
localparam S_SEND_NACK             =   8;
localparam S_SEND_STOP             =   9;
localparam S_WRITE_REG_DATA        =   10;
localparam S_WRITE_REG_ADDR_MSB    =   11;
localparam S_WRITE_REG_DATA_MSB    =   12;
localparam S_READ_REG_MSB          =   13;
localparam S_SEND_ACK              =   14;

	parameter DATA_WIDTH = 8;
	parameter REGISTER_WIDTH = 8;
	parameter ADDRESS_WIDTH = 7;
	input wire clock;
	input wire reset_n;
	input wire enable;
	input wire read_write;
	input wire [DATA_WIDTH - 1:0] mosi_data;
	input wire [REGISTER_WIDTH - 1:0] register_address;
	input wire [ADDRESS_WIDTH - 1:0] device_address;
	input wire [15:0] divider;
	output reg [DATA_WIDTH - 1:0] miso_data;
	output reg busy;
    output reg got_acknowledge;
	inout external_serial_data;
	inout external_serial_clock;
	reg [3:0] state;
	reg [3:0] _state;
	reg [3:0] post_state;
	reg [3:0] _post_state;
	reg serial_clock;
	reg _serial_clock;
	reg [ADDRESS_WIDTH:0] saved_device_address;
	reg [ADDRESS_WIDTH:0] _saved_device_address;
	reg [REGISTER_WIDTH - 1:0] saved_register_address;
	reg [REGISTER_WIDTH - 1:0] _saved_register_address;
	reg [DATA_WIDTH - 1:0] saved_mosi_data;
	reg [DATA_WIDTH - 1:0] _saved_mosi_data;
	reg [1:0] process_counter;
	reg [1:0] _process_counter;
	reg [7:0] bit_counter;
	reg [7:0] _bit_counter;
	reg serial_data;
	reg _serial_data;
	reg post_serial_data;
	reg _post_serial_data;
	reg last_acknowledge;
	reg _last_acknowledge;
	reg _saved_read_write;
	reg saved_read_write;
	reg [15:0] divider_counter;
	reg [15:0] _divider_counter;
	reg divider_tick;
	reg [DATA_WIDTH - 1:0] _miso_data;
	reg _busy;
    reg _got_acknowledge;
	reg serial_data_output_enable;
	reg serial_clock_output_enable;
	assign external_serial_clock = (serial_clock_output_enable ? serial_clock : 1'bz);
	assign external_serial_data = (serial_data_output_enable ? serial_data : 1'bz);
	always @(*) begin
		_state = state;
		_post_state = post_state;
		_process_counter = process_counter;
		_bit_counter = bit_counter;
		_last_acknowledge = last_acknowledge;
		_miso_data = miso_data;
		_saved_read_write = saved_read_write;
		_busy = busy;
        _got_acknowledge = got_acknowledge;
		_divider_counter = divider_counter;
		_saved_register_address = saved_register_address;
		_saved_device_address = saved_device_address;
		_saved_mosi_data = saved_mosi_data;
		_serial_data = serial_data;
		_serial_clock = serial_clock;
		_post_serial_data = post_serial_data;
		if (divider_counter == divider) begin
			_divider_counter = 0;
			divider_tick = 1;
		end
		else begin
			_divider_counter = divider_counter + 1;
			divider_tick = 0;
		end
		if ((((state != S_IDLE) && (state != S_CHECK_ACK)) && (state != S_READ_REG)) && (state != S_READ_REG_MSB))
			serial_data_output_enable = 1;
		else
			serial_data_output_enable = 0;
		if (((state != S_IDLE) && (process_counter != 1)) && (process_counter != 2))
			serial_clock_output_enable = 1;
		else
			serial_clock_output_enable = 0;
		case (state)
			S_IDLE: begin
				_process_counter = 0;
				_bit_counter = 0;
				_last_acknowledge = 0;
				_busy = 0;
				_saved_read_write = read_write;
				_saved_register_address = register_address;
				_saved_device_address = {device_address, 1'b0};
				_saved_mosi_data = mosi_data;
				_serial_data = 1;
				_serial_clock = 1;
				if (enable) begin
                    _got_acknowledge = 0; 
					_state = S_START;
					_post_state = S_WRITE_ADDR_W;
					_busy = 1;
				end
			end
			S_START:
				if (divider_tick)
					case (process_counter)
						0: _process_counter = 1;
						1: begin
							_serial_data = 0;
							_process_counter = 2;
						end
						2: begin
							_bit_counter = 8;
							_process_counter = 3;
						end
						3: begin
							_serial_clock = 0;
							_process_counter = 0;
							_state = post_state;
							_serial_data = saved_device_address[ADDRESS_WIDTH];
						end
					endcase
			S_WRITE_ADDR_W:
				if (divider_tick)
					case (process_counter)
						0: begin
							_serial_clock = 1;
							_process_counter = 1;
						end
						1:
							if (external_serial_clock == 1)
								_process_counter = 2;
						2: begin
							_serial_clock = 0;
							_bit_counter = bit_counter - 1;
							_process_counter = 3;
						end
						3: begin
							if (bit_counter == 0) begin
								_post_serial_data = saved_register_address[REGISTER_WIDTH - 1];
								if (REGISTER_WIDTH == 16)
									_post_state = S_WRITE_REG_ADDR_MSB;
								else
									_post_state = S_WRITE_REG_ADDR;
								_state = S_CHECK_ACK;
								_bit_counter = 8;
							end
							else
								_serial_data = saved_device_address[bit_counter - 1];
							_process_counter = 0;
						end
					endcase
			S_CHECK_ACK:
				if (divider_tick)
					case (process_counter)
						0: begin
							_serial_clock = 1;
							_process_counter = 1;
						end
						1:
							if (external_serial_clock == 1) begin
								_last_acknowledge = 0;
								_process_counter = 2;
							end
						2: begin
							_serial_clock = 0;
							if (external_serial_data == 0)
								_last_acknowledge = 1;
							_process_counter = 3;
						end
						3: begin
							if (last_acknowledge == 1) begin
                                _got_acknowledge = 1;
								_last_acknowledge = 0;
								_serial_data = post_serial_data;
								_state = post_state;
							end
							else begin
                                _got_acknowledge = 0;
								_state = S_SEND_STOP;
                            end
							_process_counter = 0;
						end
					endcase
			S_WRITE_REG_ADDR_MSB:
				if (divider_tick)
					case (process_counter)
						0: begin
							_serial_clock = 1;
							_process_counter = 1;
						end
						1:
							if (external_serial_clock == 1) begin
								_last_acknowledge = 0;
								_process_counter = 2;
							end
						2: begin
							_serial_clock = 0;
							_bit_counter = bit_counter - 1;
							_process_counter = 3;
						end
						3: begin
							if (bit_counter == 0) begin
								_post_state = S_WRITE_REG_ADDR;
								_post_serial_data = saved_register_address[7];
								_bit_counter = 8;
								_serial_data = 0;
								_state = S_CHECK_ACK;
							end
							else
								_serial_data = saved_register_address[bit_counter + 7];
							_process_counter = 0;
						end
					endcase
			S_WRITE_REG_ADDR:
				if (divider_tick)
					case (process_counter)
						0: begin
							_serial_clock = 1;
							_process_counter = 1;
						end
						1:
							if (external_serial_clock == 1) begin
								_last_acknowledge = 0;
								_process_counter = 2;
							end
						2: begin
							_serial_clock = 0;
							_bit_counter = bit_counter - 1;
							_process_counter = 3;
						end
						3: begin
							if (bit_counter == 0) begin
								if (read_write == 0) begin
									if (DATA_WIDTH == 16) begin
										_post_state = S_WRITE_REG_DATA_MSB;
										_post_serial_data = saved_mosi_data[15];
									end
									else begin
										_post_state = S_WRITE_REG_DATA;
										_post_serial_data = saved_mosi_data[7];
									end
								end
								else begin
									_post_state = S_RESTART;
									_post_serial_data = 1;
								end
								_bit_counter = 8;
								_serial_data = 0;
								_state = S_CHECK_ACK;
							end
							else
								_serial_data = saved_register_address[bit_counter - 1];
							_process_counter = 0;
						end
					endcase
			S_WRITE_REG_DATA_MSB:
				if (divider_tick)
					case (process_counter)
						0: begin
							_serial_clock = 1;
							_process_counter = 1;
						end
						1:
							if (external_serial_clock == 1) begin
								_last_acknowledge = 0;
								_process_counter = 2;
							end
						2: begin
							_serial_clock = 0;
							_bit_counter = bit_counter - 1;
							_process_counter = 3;
						end
						3: begin
							if (bit_counter == 0) begin
								_state = S_CHECK_ACK;
								_post_state = S_WRITE_REG_DATA;
								_post_serial_data = saved_mosi_data[7];
								_bit_counter = 8;
								_serial_data = 0;
							end
							else
								_serial_data = saved_mosi_data[bit_counter + 7];
							_process_counter = 0;
						end
					endcase
			S_WRITE_REG_DATA:
				if (divider_tick)
					case (process_counter)
						0: begin
							_serial_clock = 1;
							_process_counter = 1;
						end
						1:
							if (external_serial_clock == 1) begin
								_last_acknowledge = 0;
								_process_counter = 2;
							end
						2: begin
							_serial_clock = 0;
							_bit_counter = bit_counter - 1;
							_process_counter = 3;
						end
						3: begin
							if (bit_counter == 0) begin
								_state = S_CHECK_ACK;
								_post_state = S_SEND_STOP;
								_post_serial_data = 0;
								_bit_counter = 8;
								_serial_data = 0;
							end
							else
								_serial_data = saved_mosi_data[bit_counter - 1];
							_process_counter = 0;
						end
					endcase
			S_RESTART:
				if (divider_tick)
					case (process_counter)
						0: begin
                            _process_counter = 1;
                            end
						1: begin
							_process_counter = 2;
							_serial_clock = 1;
						    end
						2: begin
                            _process_counter = 3;
                            end
						3: begin
							_state = S_START;
							_post_state = S_WRITE_ADDR_R;
							_saved_device_address[0] = 1;
							_process_counter = 0;
						end
					endcase
			S_WRITE_ADDR_R:
				if (divider_tick)
					case (process_counter)
						0: begin
							_serial_clock = 1;
							_process_counter = 1;
						end
						1:
							if (external_serial_clock == 1) begin
								_last_acknowledge = 0;
								_process_counter = 2;
							end
						2: begin
							_serial_clock = 0;
							_bit_counter = bit_counter - 1;
							_process_counter = 3;
						end
						3: begin
							if (bit_counter == 0) begin
								if (DATA_WIDTH == 16) begin
									_post_state = S_READ_REG_MSB;
									_post_serial_data = 0;
								end
								else begin
									_post_state = S_READ_REG;
									_post_serial_data = 0;
								end
								_state = S_CHECK_ACK;
								_bit_counter = 8;
							end
							else
								_serial_data = saved_device_address[bit_counter - 1];
							_process_counter = 0;
						end
					endcase
			S_READ_REG_MSB:
				if (divider_tick)
					case (process_counter)
						0: begin
							_serial_clock = 1;
							_process_counter = 1;
						end
						1:
							if (external_serial_clock == 1) begin
								_last_acknowledge = 0;
								_process_counter = 2;
							end
						2: begin
							_serial_clock = 0;
							_miso_data[bit_counter + 7] = external_serial_data;
							_bit_counter = bit_counter - 1;
							_process_counter = 3;
						end
						3: begin
							if (bit_counter == 0) begin
								_post_state = S_READ_REG;
								_state = S_SEND_ACK;
								_bit_counter = 8;
								_serial_data = 0;
							end
							_process_counter = 0;
						end
					endcase
			S_READ_REG:
				if (divider_tick)
					case (process_counter)
						0: begin
							_serial_clock = 1;
							_process_counter = 1;
						end
						1:
							if (external_serial_clock == 1) begin
								_last_acknowledge = 0;
								_process_counter = 2;
							end
						2: begin
							_serial_clock = 0;
							_miso_data[bit_counter - 1] = external_serial_data;
							_bit_counter = bit_counter - 1;
							_process_counter = 3;
						end
						3: begin
							if (bit_counter == 0) begin
								_state = S_SEND_NACK;
								_serial_data = 0;
							end
							_process_counter = 0;
						end
					endcase
			S_SEND_NACK:
				if (divider_tick)
					case (process_counter)
						0: begin
							_serial_clock = 1;
							_serial_data = 1;
							_process_counter = 1;
						end
						1:
							if (external_serial_clock == 1) begin
								_last_acknowledge = 0;
								_process_counter = 2;
							end
						2: begin
							_process_counter = 3;
							_serial_clock = 0;
						end
						3: begin
							_state = S_SEND_STOP;
							_process_counter = 0;
							_serial_data = 0;
						end
					endcase
			S_SEND_ACK:
				if (divider_tick)
					case (process_counter)
						0: begin
							_serial_clock = 1;
							_process_counter = 1;
							_serial_data = 0;
						end
						1:
							if (external_serial_clock == 1) begin
								_last_acknowledge = 0;
								_process_counter = 2;
							end
						2: begin
							_process_counter = 3;
							_serial_clock = 0;
						end
						3: begin
							_state = post_state;
							_process_counter = 0;
						end
					endcase
			S_SEND_STOP:
				if (divider_tick)
					case (process_counter)
						0: begin
							_serial_clock = 1;
							_process_counter = 1;
						end
						1:
							if (external_serial_clock == 1) begin
								_last_acknowledge = 0;
								_process_counter = 2;
							end
						2: begin
							_process_counter = 3;
							_serial_data = 1;
						end
						3: _state = S_IDLE;
					endcase
		endcase
	end
	always @(posedge clock)
		if (!reset_n) begin
			state <= S_IDLE;
			post_state <= S_IDLE;
			process_counter <= 0;
			bit_counter <= 0;
			last_acknowledge <= 0;
			miso_data <= 0;
			saved_read_write <= 0;
			divider_counter <= 0;
			saved_device_address <= 0;
			saved_register_address <= 0;
			saved_mosi_data <= 0;
			serial_clock <= 0;
			serial_data <= 0;
			saved_mosi_data <= 0;
			post_serial_data <= 0;
			busy <= 0;
            got_acknowledge <= 0;
		end
		else begin
			state <= _state;
			post_state <= _post_state;
			process_counter <= _process_counter;
			bit_counter <= _bit_counter;
			last_acknowledge <= _last_acknowledge;
			miso_data <= _miso_data;
			saved_read_write <= _saved_read_write;
			divider_counter <= _divider_counter;
			saved_device_address <= _saved_device_address;
			saved_register_address <= _saved_register_address;
			saved_mosi_data <= _saved_mosi_data;
			serial_clock <= _serial_clock;
			serial_data <= _serial_data;
			post_serial_data <= _post_serial_data;
			busy <= _busy;
            got_acknowledge <= _got_acknowledge;
		end
endmodule
