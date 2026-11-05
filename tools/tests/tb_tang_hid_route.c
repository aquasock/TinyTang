/* TinyTang — host test for Bluetooth HID report routing (tang_hid_route.c).
 *
 * The decision this covers is the one part of the Bluetooth path that fails
 * silently: a report sent to the wrong parser raises nothing anywhere, it just
 * does nothing, and the console still shows the reports arriving.  Two of this
 * project's recorded facts live in it (BLE-019 and the Logitech case in
 * tang_ble.c's notify callback), so both are pinned here by name.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>

#include "tang_hid_route.h"

static int failures;

#define CHECK(x)                                                              \
    do {                                                                      \
        if (!(x)) {                                                           \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x);                \
            failures++;                                                       \
        }                                                                     \
    } while (0)

#define CHECK_EQ(a, b)                                                        \
    do {                                                                      \
        long _a = (long)(a), _b = (long)(b);                                   \
        if (_a != _b) {                                                       \
            printf("FAIL %s:%d: %s == %ld, expected %ld\n", __FILE__, __LINE__, \
                   #a, _a, _b);                                               \
            failures++;                                                       \
        }                                                                     \
    } while (0)

#define ROUTE(slot, vh, kbd, mouse, len) \
    tang_hid_route((slot), (vh), (kbd), (mouse), (len))

/* Handles from a real device: the boot input and a report characteristic sit
 * at different values, and the one that matters is which arrives. */
#define BOOT_INPUT   0x0025u
#define REPORT_CHRC  0x001Eu

int main(void)
{
    /* ------------------------------------------- the keyboard slot ------
     * Its boot input routes by handle; the same shape on a report
     * characteristic routes by length, which is the Logitech case. */
    CHECK_EQ(ROUTE(TANG_HID_SLOT_KBD, BOOT_INPUT, BOOT_INPUT, 0, 8),
             TANG_HID_ROUTE_KEYBOARD);
    CHECK_EQ(ROUTE(TANG_HID_SLOT_KBD, REPORT_CHRC, 0, 0, 8),
             TANG_HID_ROUTE_KEYBOARD);
    /* A keyboard slot carries no mouse role, so a 3- or 4-byte report is not
     * its business. */
    CHECK_EQ(ROUTE(TANG_HID_SLOT_KBD, REPORT_CHRC, 0, 0, 3), TANG_HID_ROUTE_NONE);
    CHECK_EQ(ROUTE(TANG_HID_SLOT_KBD, REPORT_CHRC, 0, 0, 4), TANG_HID_ROUTE_NONE);

    /* ---------------------------------------------- the mouse slot ------
     * A Logitech M750 in the mouse slot reports on its report characteristic
     * and never on the boot input; a strict handle match left its movement
     * unread (BLE-009, and the comment in tang_ble.c). */
    CHECK_EQ(ROUTE(TANG_HID_SLOT_MOUSE, REPORT_CHRC, 0, 0, 4),
             TANG_HID_ROUTE_MOUSE);
    CHECK_EQ(ROUTE(TANG_HID_SLOT_MOUSE, REPORT_CHRC, 0, 0, 3),
             TANG_HID_ROUTE_MOUSE);
    CHECK_EQ(ROUTE(TANG_HID_SLOT_MOUSE, BOOT_INPUT, 0, BOOT_INPUT, 4),
             TANG_HID_ROUTE_MOUSE);
    /* And a mouse slot carries no keyboard role. */
    CHECK_EQ(ROUTE(TANG_HID_SLOT_MOUSE, REPORT_CHRC, 0, 0, 8), TANG_HID_ROUTE_NONE);

    /* ---------------------------------------------- the combo slot ------
     * The Rii K06 case: boot formats on a report characteristic instead of the
     * boot input.  Layout 0 maps the 8-byte keyboard report to 0x001E and the
     * touchpad's 3- or 4-byte report to 0x002C, and the lengths are what keep
     * them apart on one slot. */
    CHECK_EQ(ROUTE(TANG_HID_SLOT_COMBO, REPORT_CHRC, 0, 0, 8),
             TANG_HID_ROUTE_KEYBOARD);
    CHECK_EQ(ROUTE(TANG_HID_SLOT_COMBO, 0x002Cu, 0, 0, 3), TANG_HID_ROUTE_MOUSE);
    CHECK_EQ(ROUTE(TANG_HID_SLOT_COMBO, 0x002Cu, 0, 0, 4), TANG_HID_ROUTE_MOUSE);

    /* ------------------------------------------- the precedence ---------
     * The keyboard is tested first, so in a combo an 8-byte report is a
     * keyboard report whatever handle it arrives on.  That is safe only
     * because boot protocol gives a mouse three or four bytes and never eight;
     * swapping these branches would look harmless and would route every combo
     * keyboard report nowhere. */
    CHECK_EQ(ROUTE(TANG_HID_SLOT_COMBO, BOOT_INPUT, 0, BOOT_INPUT, 8),
             TANG_HID_ROUTE_KEYBOARD);
    /* The other direction: a 3-byte report on the keyboard's own handle is a
     * mouse report once the 8-byte test has failed. */
    CHECK_EQ(ROUTE(TANG_HID_SLOT_COMBO, BOOT_INPUT, BOOT_INPUT, 0, 3),
             TANG_HID_ROUTE_MOUSE);
    CHECK_EQ(ROUTE(TANG_HID_SLOT_COMBO, BOOT_INPUT, BOOT_INPUT, 0, 4),
             TANG_HID_ROUTE_MOUSE);

    /* -------------------------------------------- length boundaries -----
     * Only the exact boot shapes route by length.  A 7-byte report is neither,
     * and a 9-byte one is not the keyboard shape -- it routes only when it
     * arrives on the configured input, which is a handle match rather than a
     * guess about the format. */
    CHECK_EQ(ROUTE(TANG_HID_SLOT_KBD, REPORT_CHRC, 0, 0, 7), TANG_HID_ROUTE_NONE);
    CHECK_EQ(ROUTE(TANG_HID_SLOT_KBD, REPORT_CHRC, 0, 0, 9), TANG_HID_ROUTE_NONE);
    CHECK_EQ(ROUTE(TANG_HID_SLOT_KBD, BOOT_INPUT, BOOT_INPUT, 0, 9),
             TANG_HID_ROUTE_KEYBOARD);
    CHECK_EQ(ROUTE(TANG_HID_SLOT_MOUSE, REPORT_CHRC, 0, 0, 2), TANG_HID_ROUTE_NONE);
    CHECK_EQ(ROUTE(TANG_HID_SLOT_MOUSE, REPORT_CHRC, 0, 0, 5), TANG_HID_ROUTE_NONE);
    CHECK_EQ(ROUTE(TANG_HID_SLOT_COMBO, REPORT_CHRC, 0, 0, 0), TANG_HID_ROUTE_NONE);
    CHECK_EQ(ROUTE(TANG_HID_SLOT_COMBO, REPORT_CHRC, 0, 0, 1), TANG_HID_ROUTE_NONE);

    /* ------------------------------------------- the undiscovered window --
     * A handle of 0 means "not discovered yet", not a match: the ATT handle
     * range starts at 1, so a report can never arrive on 0, and treating 0 as
     * a match would route during the window before discovery has named the
     * inputs.  A 5-byte report is neither boot shape, so nothing routes it
     * unless handle 0 wrongly matched. */
    CHECK_EQ(ROUTE(TANG_HID_SLOT_MOUSE, 0, 0, 0, 5), TANG_HID_ROUTE_NONE);
    CHECK_EQ(ROUTE(TANG_HID_SLOT_COMBO, 0, 0, 0, 5), TANG_HID_ROUTE_NONE);
    /* The same with a real handle configured but not the one that arrived. */
    CHECK_EQ(ROUTE(TANG_HID_SLOT_MOUSE, 0x0031u, 0, BOOT_INPUT, 5),
             TANG_HID_ROUTE_NONE);

    /* --------------------------------------------- an unknown slot -------
     * The caller indexes per-slot arrays, so an out-of-range slot must decide
     * nothing rather than fall back to a role. */
    CHECK_EQ(ROUTE(3, BOOT_INPUT, BOOT_INPUT, BOOT_INPUT, 8), TANG_HID_ROUTE_NONE);
    CHECK_EQ(ROUTE(3, BOOT_INPUT, BOOT_INPUT, BOOT_INPUT, 3), TANG_HID_ROUTE_NONE);
    CHECK_EQ(ROUTE(-1, BOOT_INPUT, BOOT_INPUT, BOOT_INPUT, 4), TANG_HID_ROUTE_NONE);

    /* ---------------------------------------------------- the names ----- */
    CHECK_EQ(strcmp(tang_hid_route_name(TANG_HID_ROUTE_KEYBOARD), "keyboard"), 0);
    CHECK_EQ(strcmp(tang_hid_route_name(TANG_HID_ROUTE_MOUSE), "mouse"), 0);
    CHECK_EQ(strcmp(tang_hid_route_name(TANG_HID_ROUTE_NONE), "none"), 0);
    /* Any other value still names something, so a log line cannot be empty. */
    CHECK(tang_hid_route_name((tang_hid_route_t)99) != NULL);

    /* ------------------------------------------- the slot numbering -----
     * tang_ble.c static-asserts these against its own HID_KBD/HID_MOUSE/
     * HID_COMBO; asserting them here means a change to either side is caught
     * by whichever build runs first. */
    CHECK_EQ(TANG_HID_SLOT_KBD, 0);
    CHECK_EQ(TANG_HID_SLOT_MOUSE, 1);
    CHECK_EQ(TANG_HID_SLOT_COMBO, 2);

    /* ------------------------------------ every slot and every length ---
     * A sweep, so a length nobody thought to try cannot fall through to a
     * route it should not have.  Only the boot shapes and a report on the
     * configured handle may route; everything else must be none. */
    for (int slot = 0; slot < 3; slot++) {
        for (unsigned len = 0; len <= 16; len++) {
            const tang_hid_route_t r = ROUTE(slot, REPORT_CHRC, 0, 0, len);
            const int kbd_role = (slot == TANG_HID_SLOT_KBD || slot == TANG_HID_SLOT_COMBO);
            const int mouse_role = (slot == TANG_HID_SLOT_MOUSE || slot == TANG_HID_SLOT_COMBO);
            tang_hid_route_t want = TANG_HID_ROUTE_NONE;
            if (kbd_role && len == 8) want = TANG_HID_ROUTE_KEYBOARD;
            else if (mouse_role && (len == 3 || len == 4)) want = TANG_HID_ROUTE_MOUSE;
            if (r != want) {
                printf("FAIL sweep: slot %d len %u gave %d, expected %d\n",
                       slot, len, (int)r, (int)want);
                failures++;
            }
        }
    }

    /* ------------------------------- equivalence with the original ------
     * The function was extracted from tang_ble.c's notify callback, and the
     * point of a refactor is that behaviour does not move.  This transcribes
     * the inline expression as it stood at commit 6c22e7d and compares the two
     * over every input the caller can produce: three slots, four handle
     * classes and every length to 20.
     *
     * Out-of-range slots are excluded because the extracted version answers
     * NONE there where the original could still route by handle -- a
     * deliberate difference, unreachable from the caller and documented in the
     * module.  Comparing it would assert the change rather than the
     * equivalence. */
    {
        static const unsigned handles[4] = {0x0000u, 0x001Eu, 0x0025u, 0x002Cu};
        long compared = 0, mismatches = 0;
        for (int slot = 0; slot <= 2; slot++) {
            for (unsigned vh = 0; vh <= 0x0040u; vh++) {
                for (int ki = 0; ki < 4; ki++) {
                    for (int mi = 0; mi < 4; mi++) {
                        for (unsigned len = 0; len <= 20; len++) {
                            const int kbd_slot =
                                (slot == TANG_HID_SLOT_KBD || slot == TANG_HID_SLOT_COMBO);
                            const int mouse_slot =
                                (slot == TANG_HID_SLOT_MOUSE || slot == TANG_HID_SLOT_COMBO);
                            const int kbd_report =
                                (handles[ki] && vh == handles[ki]) ||
                                (kbd_slot && len == TANG_HID_BOOT_KBD_LEN);
                            const int mouse_report =
                                (handles[mi] && vh == handles[mi]) ||
                                (mouse_slot && (len == TANG_HID_BOOT_MOUSE_LEN ||
                                                len == TANG_HID_BOOT_MOUSE_LEN_WHEEL));
                            int want = TANG_HID_ROUTE_NONE;
                            if (kbd_report && len >= TANG_HID_BOOT_KBD_LEN) {
                                want = TANG_HID_ROUTE_KEYBOARD;
                            } else if (mouse_report && len >= TANG_HID_BOOT_MOUSE_LEN) {
                                want = TANG_HID_ROUTE_MOUSE;
                            }
                            const int got = (int)ROUTE(slot, vh, handles[ki],
                                                       handles[mi], len);
                            compared++;
                            if (got != want) {
                                if (mismatches < 5) {
                                    printf("FAIL equivalence slot=%d vh=0x%02x "
                                           "kbd=0x%02x mouse=0x%02x len=%u: "
                                           "original %d, extracted %d\n",
                                           slot, vh, handles[ki], handles[mi],
                                           len, want, got);
                                }
                                mismatches++;
                            }
                        }
                    }
                }
            }
        }
        CHECK_EQ(mismatches, 0);
        /* A comparison that ran nothing would pass; this pins that it ran. */
        CHECK(compared > 60000);
    }

    if (failures) {
        printf("tb_tang_hid_route: %d failure(s)\n", failures);
        return 1;
    }
    printf("tb_tang_hid_route: all checks passed\n");
    return 0;
}
