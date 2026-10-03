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
/* Write a block, blocking until the USB stack has taken it. */
int  tdsh_bl616_console_write(const void *data, size_t length);

/* Filesystem (tdsh_fs_fatfs.c): a stdio shim over FatFS on the SD card. */
int  tdsh_bl616_fs_mount(void);
bool tdsh_bl616_fs_ready(void);
int  tdsh_bl616_fs_last_result(void);   /* FRESULT of the last mount attempt */

/* Shell lifecycle (tdsh_platform_bl616.c). */
int  tdsh_bl616_init(const char *hostname);
int  tdsh_bl616_run(void);

#endif /* TDSH_BL616_H */
