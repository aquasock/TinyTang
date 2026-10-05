// TinyTang — the BL616 port of TinyDesk Shell.
//
// This header is the seam between our port and the rest of the firmware.
// TinyDesk Shell itself is not modified: its portable core is compiled from
// third_party/tinydesk-shell and this port supplies the platform API, a
// stdio filesystem and a console.
#ifndef TDSH_BL616_H
#define TDSH_BL616_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Console transport (usb_cdc_bl616.c).  The USB CDC port is the terminal the
 * user sees; the shell renders into it and the PC displays it. */
void tdsh_bl616_console_init(void);
bool tdsh_bl616_console_connected(void);
/* Return the next byte, or -1 when none is waiting.  Never blocks. */
int  tdsh_bl616_console_read_byte(void);
/* Write a block, blocking until the USB stack has taken it.  The bytes also
 * feed the desktop layer's mirror of the console (tang_osd_desk_feed). */
int  tdsh_bl616_console_write(const void *data, size_t length);
/* The same, to the USB console only, leaving the layer's mirror alone. */
int  tdsh_bl616_console_write_usb(const void *data, size_t length);
/* Where the shell's command output goes -- printf, puts, putchar, stdout and
 * stderr.  At the console that is tdsh_bl616_console_write.  While the shell
 * runs in TinyDesk's Terminal window it is the window, and the USB console,
 * but not the layer's mirror. */
int  tdsh_bl616_output(const void *data, size_t length);

/* Filesystem (tdsh_fs_fatfs.c): a stdio shim over FatFS on the SD card. */
int  tdsh_bl616_fs_mount(void);
bool tdsh_bl616_fs_ready(void);
int  tdsh_bl616_fs_last_result(void);   /* FRESULT of the last mount attempt */

/* Shell lifecycle (tdsh_platform_bl616.c). */
int  tdsh_bl616_init(const char *hostname);
int  tdsh_bl616_run(void);
/* The same shell loop, ending when *stop goes true — for a shell hosted by
 * something else, such as TinyDesk's Terminal window. */
int  tdsh_bl616_run_until(volatile const bool *stop);
/* Point the shell's terminal at another byte stream, or back at the console
 * with NULL, NULL.  Only one shell runs at a time, so one redirect is enough. */
void tdsh_bl616_terminal_set_io(int (*read_fn)(void),
                                int (*write_fn)(const void *data, size_t length));
/* The redirected terminal's width, for the line editor to wrap at (TinyDesk
 * Shell 0.1.4's columns()); 0 when it is not known. */
void tdsh_bl616_terminal_set_columns(int cols);

/* The BL616 <-> FPGA UART link and the ROM loader over it (tang_fpga_uart.c). */
int  tdsh_bl616_fpga_register(void);

#endif /* TDSH_BL616_H */
