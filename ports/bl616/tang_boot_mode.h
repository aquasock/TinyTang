// TinyTang — what the board comes up as.
//
// SPDX-License-Identifier: MIT
//
// The operator's decision: this is a desktop first and a console second, so the
// board comes up in TinyDesk.  The shell is one Exit away, and `desktop` brings
// the desktop back.
//
// The escape hatch is a file, not a reflash: an empty `/root/scripts/no-desktop`
// on the card leaves the prompt in place.  That keeps the property `boot.tdsh`
// has always had -- the card decides, nothing is reflashed to change it -- and
// it is the state a card needs while it is being brought up, or before a
// keyboard is paired and the desktop could not be driven.
//
// It lives in its own module so the decision can be host-tested.  The platform
// file it is called from needs the shell, the console, the radio and the card
// to link at all, so a test of the decision could not reach it there.

#ifndef TANG_BOOT_MODE_H
#define TANG_BOOT_MODE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Where the marker is looked for, in the port's FatFS terms. */
#define TANG_NO_DESKTOP_PATH "/sd/root/scripts/no-desktop"

/* True when the board should bring TinyDesk up after the boot script.
 *
 * Looks for `path` -- pass TANG_NO_DESKTOP_PATH from the firmware, and a
 * temporary path from a test.  Whether the path *opens* is the whole signal;
 * its contents are never read.  On FatFS -- the card -- f_open refuses a
 * directory, so a directory of that name cannot disable the desktop; glibc
 * opens one, so that particular case is a property of the filesystem and not
 * something this function promises. */
bool tang_boot_wants_desktop(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* TANG_BOOT_MODE_H */
