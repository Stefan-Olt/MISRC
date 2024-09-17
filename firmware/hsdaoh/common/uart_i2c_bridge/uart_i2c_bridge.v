// UART to I2C and control bridge for hsdaohSDR
// Copyright (C) 2024 by Steve Markgraf <steve@steve-m.de>
// License: MIT

module uart_i2c_bridge

    #(
    parameter CLK_FRE = 27
    )(
	input wire          clk,
	input wire          rst_n,
	input wire          uart_rx,
	output wire         uart_tx,
	inout wire          i2c_sda,
	inout wire          i2c_scl,
	output reg[31:0]    settings = 32'h00080000 // default setting: dual channel
	);

//parameter                        CLK_FRE  = 27;//Mhz
parameter                        UART_FRE = 921600;
reg[7:0]                         tx_data;
reg[7:0]                         tx_str;
reg                              tx_data_valid;
wire                             tx_data_ready;
reg[7:0]                         tx_cnt;
wire[7:0]                        rx_data;
wire                             rx_data_valid;
wire                             rx_data_ready;
reg[31:0]                        wait_cnt;
reg[3:0]                         state;
reg[3:0]                         rx_byte_cnt;
reg[23:0]                        set_tmp;

localparam                       IDLE            =  0;
localparam                       READ_SLAVE_ADDR =  1;
localparam                       READ_VALUE_TO_WRITE = 2;
localparam                       READ_REG_ADDR   =  3;
localparam                       WAIT_I2C_BUSY   =  4;
localparam                       SEND_ACKNOWLEDGE = 5;
localparam                       END_SENDING_ACKNOWLEDGE = 6;
localparam                       SEND_READ_RESULT = 7;
localparam                       RECEIVE_SETTINGS = 8;

// commands
localparam                      CMD_I2C_INIT = "i";
localparam                      CMD_I2C_READ = "r";
localparam                      CMD_I2C_WRITE = "w";
localparam                      CMD_CONFIG = "c";

assign rx_data_ready = 1'b1;

always@(posedge clk or negedge rst_n)
begin
	if(rst_n == 1'b0)
	begin
		wait_cnt <= 32'd0;
		tx_data <= 8'd0;
		state <= IDLE;
		tx_cnt <= 8'd0;
		tx_data_valid <= 1'b0;
		i2c_enable <= 1'b0;
		settings <= 32'h00000000;
		rx_byte_cnt <= 0;
	end
	else

	case(state)
		IDLE:
            begin
                tx_data_valid <= 1'b0;
                if (rx_data_valid == 1'b1)
                begin
                    // receive command
                    case (rx_data)
                        CMD_I2C_READ:
                        begin
                            i2c_read_write <= 1'b1;
                            state <= READ_SLAVE_ADDR;
                        end
                        CMD_I2C_WRITE:
                        begin
                            i2c_read_write <= 1'b0;
                            state <= READ_SLAVE_ADDR;
                        end
                        CMD_I2C_INIT:
                            state <= IDLE;
                        CMD_CONFIG:
                            state <= RECEIVE_SETTINGS;
                    endcase
           //     default:
                end
            end

		RECEIVE_SETTINGS:
			if (rx_data_valid == 1'b1) begin
				if (rx_byte_cnt < 3)
					rx_byte_cnt <= rx_byte_cnt + 1'b1;

				case (rx_byte_cnt)
				0:
					set_tmp[23:16] <= rx_data[7:0];
				1:
					set_tmp[15:8] <= rx_data[7:0];
				2:
					set_tmp[7:0] <= rx_data[7:0];
				3:
				begin
					settings[31:0] <= {set_tmp[23:0], rx_data[7:0]};
					rx_byte_cnt <= 0;
					state <= IDLE;
				end

				default:
					rx_byte_cnt <= 0;
				endcase
			end
		READ_SLAVE_ADDR:
             if(rx_data_valid == 1'b1) begin
                device_address <= rx_data[6:0];
                state <= i2c_read_write ? READ_REG_ADDR : READ_VALUE_TO_WRITE;
             end

       READ_VALUE_TO_WRITE:
            // only for I2C TX: read value to be written
             if(rx_data_valid == 1'b1) begin
                mosi_data <= rx_data;
                state <= READ_REG_ADDR;
             end
       READ_REG_ADDR:
             if(rx_data_valid == 1'b1) begin
                reg_addr <= rx_data;
                
                if (i2c_busy == 0)
                    i2c_enable <= 1'b1;
                else
                    state <= IDLE;

                state <= WAIT_I2C_BUSY;
             end

     WAIT_I2C_BUSY:
        if (i2c_busy == 1) begin
            i2c_enable <= 1'b0;
            state <= SEND_ACKNOWLEDGE;

        end

     SEND_ACKNOWLEDGE:
            begin
            if ((i2c_busy == 0) && tx_data_ready) begin
                tx_data <= {7'b0000000, i2c_acknowledge};
                tx_data_valid <= 1'b1;
                state <= END_SENDING_ACKNOWLEDGE;
              end
            end

     END_SENDING_ACKNOWLEDGE:
            begin
                tx_data_valid <= 1'b0;
                state <= i2c_read_write ? SEND_READ_RESULT : IDLE;
            end

     SEND_READ_RESULT:
        begin
            if ((i2c_busy == 0) && tx_data_ready) begin
                tx_data <= miso_data;
                tx_data_valid <= 1'b1;
                state <= IDLE;
              end
        end

		default:
			state <= IDLE;
	endcase
end


reg i2c_enable;
reg i2c_read_write;
reg[7:0] mosi_data;
reg[7:0] reg_addr;
wire[7:0] miso_data;
reg[6:0] device_address;
wire i2c_busy;
wire i2c_acknowledge;

i2c_master #(.DATA_WIDTH(8),.REGISTER_WIDTH(8),.ADDRESS_WIDTH(7))
        i2c_master_inst(
            .clock                  (clk),
            .reset_n                (rst_n),
            .enable                 (i2c_enable),
            .read_write             (i2c_read_write),
            .mosi_data              (mosi_data),
            .register_address       (reg_addr),
            .device_address         (device_address),
            .divider                (16), // ~400 kHz

            .miso_data              (miso_data),
            .busy                   (i2c_busy),
            .got_acknowledge        (i2c_acknowledge),

            .external_serial_data   (i2c_sda),
            .external_serial_clock  (i2c_scl)
        );

uart_rx#
(
	.CLK_FRE(CLK_FRE),
	.BAUD_RATE(UART_FRE)
) uart_rx_inst
(
	.clk                        (clk                      ),
	.rst_n                      (rst_n                    ),
	.rx_data                    (rx_data                  ),
	.rx_data_valid              (rx_data_valid            ),
	.rx_data_ready              (rx_data_ready            ),
	.rx_pin                     (uart_rx                  )
);

uart_tx#
(
	.CLK_FRE(CLK_FRE),
	.BAUD_RATE(UART_FRE)
) uart_tx_inst
(
	.clk                        (clk                      ),
	.rst_n                      (rst_n                    ),
	.tx_data                    (tx_data                  ),
	.tx_data_valid              (tx_data_valid            ),
	.tx_data_ready              (tx_data_ready            ),
	.tx_pin                     (uart_tx                  )
);
endmodule
