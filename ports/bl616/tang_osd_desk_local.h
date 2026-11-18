// SPDX-License-Identifier: MIT
// tang_osd_desk_local.h -- the desk layer drawn by TinyDesk itself.  Apart
// from tang_osd_desk.h so that only what links TinyDesk sees its types.
#ifndef TANG_OSD_DESK_LOCAL_H
#define TANG_OSD_DESK_LOCAL_H

/* TinyDesk draws the layer itself, from cells, while the desktop runs
 * (TinyDesk patch 0005, td_set_local_display): no escape sequences and no
 * emulator in between, and the console's emulator is freed meanwhile.
 * local_begin returns the display for td_set_local_display, or NULL when the
 * layer is off (the desktop then draws to a terminal on the console as
 * before); local_end returns the layer to the console, on a blank grid. */
#include "tinydesk/td.h"
const td_local_display_t *tang_osd_desk_local_begin(void);
void tang_osd_desk_local_end(void);

#endif
