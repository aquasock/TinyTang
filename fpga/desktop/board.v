// TinyTang desktop host board. SPDX-License-Identifier: GPL-3.0-only
`define RES_720P
`define GW_IDE
`define MEGA138K
`define PRIMER
`define CONSOLE
`define USB1
`define USB2
`define MENU_CORE
`define DESKTOP_CORE
package configPackage;
    localparam SDRAM_DATA_WIDTH = 16;
    localparam SDRAM_ROW_WIDTH = 13;
    localparam SDRAM_COL_WIDTH = 9;
    localparam SDRAM_BANK_WIDTH = 2;
endpackage
