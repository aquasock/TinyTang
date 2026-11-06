// TinyTang — what the board comes up as.  See tang_boot_mode.h.
//
// SPDX-License-Identifier: MIT

#include "tang_boot_mode.h"

#include <stdio.h>

bool tang_boot_wants_desktop(const char *path)
{
    if (path == NULL || path[0] == '\0') {
        /* No path to check means no way to ask for the console, and the
         * desktop is the default this exists to protect. */
        return true;
    }
    /* A path that opens is the marker; one that does not is the desktop.
     *
     * On the board this is FatFS, where f_open on a directory returns
     * FR_DENIED, so a directory named no-desktop cannot disable the desktop.
     * That is a property of the filesystem rather than of this function --
     * glibc happily opens a directory for reading -- and it is why the test
     * pins only the two cases that hold on both: absent and present. */
    FILE *probe = fopen(path, "r");
    if (probe == NULL) {
        return true;                 /* absent, or unreadable: the desktop */
    }
    fclose(probe);
    return false;                    /* the marker is there: stay at the prompt */
}
