create_clock -name sys_clk -period 37.04  [get_ports {sys_clk}] -add
create_clock -name adc_clk -period 25.00  [get_ports {adc_clk}] -add