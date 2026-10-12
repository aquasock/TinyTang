# HDMI test core, GW5AST-138 revision C, Gowin reference build only.
# SPDX-License-Identifier: MIT
set_device GW5AST-LV138PG484AC1/I0 -device_version C
add_file -type verilog hdmi_test.v
add_file -type verilog gowin_pll_27.v
add_file -type verilog gowin_pll_hdmi.v
add_file -type cst hdmi_test.cst
add_file -type sdc hdmi_test.sdc
set_option -synthesis_tool gowinsynthesis
set_option -top_module hdmi_test
set_option -output_base_name hdmi_test
set_option -verilog_std v2001
set_option -use_mspi_as_gpio 1
set_option -use_ready_as_gpio 1
set_option -use_done_as_gpio 1
set_option -use_i2c_as_gpio 1
set_option -use_cpu_as_gpio 1
set_option -use_sspi_as_gpio 1
set_option -multi_boot 1
run all
