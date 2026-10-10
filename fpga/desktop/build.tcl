# TinyTang desktop core, GW5AST-138 revision C.
# SPDX-License-Identifier: GPL-3.0-only
set_device GW5AST-LV138PG484AC1/I0 -device_version C
add_file src/desktop/board.v
add_file -type cst src/desktop/desktop.cst
add_file -type sdc src/desktop/desktop.sdc
foreach source {
    src/pll/gowin_pll_27.v src/pll/gowin_pll_hdmi.v src/pll/gowin_pll_nes.v
    src/keylink_rx.sv src/iosys/iosys_bl616.v src/iosys/uart_fixed.v
    src/iosys/textdisp_wide.sv src/nes2hdmi.sv src/nestang_top.sv
    src/desktop/clock_monitor.sv
    src/desktop/desktop_regs.sv src/desktop/desktop_pmod.sv
    src/desktop/desktop_oled.sv src/desktop/oled_panel.sv src/desktop/oled_spi.sv
    src/hdmi2/audio_clock_regeneration_packet.sv src/hdmi2/audio_info_frame.sv
    src/hdmi2/audio_sample_packet.sv src/hdmi2/auxiliary_video_information_info_frame.sv
    src/hdmi2/hdmi.sv src/hdmi2/packet_assembler.sv src/hdmi2/packet_picker.sv
    src/hdmi2/serializer.sv src/hdmi2/source_product_description_info_frame.sv
    src/hdmi2/tmds_channel.sv
} { add_file -type verilog $source }
set_option -synthesis_tool gowinsynthesis
set_option -top_module nestang_top
set_option -output_base_name desktop
set_option -verilog_std sysv2017
set_option -rw_check_on_ram 1
set_option -use_mspi_as_gpio 1
set_option -use_ready_as_gpio 1
set_option -use_done_as_gpio 1
set_option -use_i2c_as_gpio 1
set_option -use_cpu_as_gpio 1
set_option -use_sspi_as_gpio 1
set_option -multi_boot 1
set placement 2
if {$argc > 0} { set placement [lindex $argv 0] }
set_option -place_option $placement
run all
