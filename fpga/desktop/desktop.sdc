// TinyTang desktop clocks. SPDX-License-Identifier: GPL-3.0-only
create_clock -name sys_clk -period 20 [get_nets {sys_clk}]
create_clock -name clk -period 46.53 [get_nets {clk}]
create_clock -name hclk5 -period 2.6936 [get_nets {hclk5}]
