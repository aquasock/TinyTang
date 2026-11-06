/* TinyTang — host test for what the board comes up as (tang_boot_mode.c).
 *
 * The rule is the operator's: this is a desktop first and a console second, so
 * the board brings TinyDesk up unless the card carries a marker file.  What
 * matters here is that the *file's existence* is the signal and nothing else --
 * its contents are never read, an unreadable path is not a marker, and a
 * missing path still means the desktop.  Each of those is a way the decision
 * could go wrong quietly: a card whose marker was ignored comes up in a desktop
 * with no keyboard to drive it, and looks like a hang.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "tang_boot_mode.h"

static int failures;

#define CHECK(x)                                                              \
    do {                                                                      \
        if (!(x)) {                                                           \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x);                \
            failures++;                                                       \
        }                                                                     \
    } while (0)

int main(void)
{
    char dir[] = "/tmp/bootmode-XXXXXX";
    char marker[160];
    if (mkdtemp(dir) == NULL) {
        printf("FAIL: could not make a temp dir\n");
        return 1;
    }
    snprintf(marker, sizeof marker, "%s/no-desktop", dir);

    /* No marker: the desktop.  This is the case every ordinary card hits. */
    remove(marker);
    CHECK(tang_boot_wants_desktop(marker) == true);

    /* An empty marker: the prompt.  The file's existence is the whole signal,
     * which is what lets a user make one with `touch`. */
    {
        FILE *f = fopen(marker, "w");
        CHECK(f != NULL);
        if (f) fclose(f);
    }
    CHECK(tang_boot_wants_desktop(marker) == false);

    /* Contents are never read: a marker full of text behaves the same. */
    {
        FILE *f = fopen(marker, "w");
        if (f) { fputs("anything at all\nsecond line\n", f); fclose(f); }
    }
    CHECK(tang_boot_wants_desktop(marker) == false);

    /* A directory of that name is NOT asserted here, and the reason is worth
     * keeping: on the board's FatFS, f_open on a directory returns FR_DENIED,
     * so it cannot masquerade as a marker; on glibc, fopen on a directory
     * succeeds, so the same call reports a marker.  The two platforms really do
     * differ, so asserting either one here would pin a host accident rather
     * than the firmware's promise.  What is asserted is the pair that holds on
     * both: absent means desktop, an opened file means prompt. */
    remove(marker);

    /* A path in a directory that does not exist is absent, not an error. */
    {
        char deep[200];
        snprintf(deep, sizeof deep, "%s/nope/no-desktop", dir);
        CHECK(tang_boot_wants_desktop(deep) == true);
    }

    /* No path at all still means the desktop: a caller that cannot say how to
     * ask for the console must not accidentally hold the board at a prompt. */
    CHECK(tang_boot_wants_desktop(NULL) == true);
    CHECK(tang_boot_wants_desktop("") == true);

    /* The path the firmware passes is the port's FatFS view of the card, and
     * it has to be under /sd because that is what the shell's root maps to. */
    CHECK(strcmp(TANG_NO_DESKTOP_PATH, "/sd/root/scripts/no-desktop") == 0);
    CHECK(strncmp(TANG_NO_DESKTOP_PATH, "/sd/", 4) == 0);

    remove(marker);
    rmdir(dir);

    if (failures) {
        printf("tb_boot_mode: %d failure(s)\n", failures);
        return 1;
    }
    printf("tb_boot_mode: all checks passed\n");
    return 0;
}
