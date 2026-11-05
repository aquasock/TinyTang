// TinyTang — which parser a Bluetooth HID notification belongs to.
// See tang_hid_route.h for the rules and the two facts behind them.
//
// SPDX-License-Identifier: MIT

#include "tang_hid_route.h"

#include <stdbool.h>

/* A handle of 0 is "not discovered yet", not a match: the ATT handle range
 * starts at 1, so a report can never legitimately arrive on 0, and matching it
 * would route during the window before discovery has named the inputs. */
static bool handle_matches(uint16_t configured, uint16_t value_handle)
{
    return configured != 0 && value_handle == configured;
}

/* The boot shapes, by role.  A combo carries both. */
static bool slot_has_keyboard(int slot)
{
    return slot == TANG_HID_SLOT_KBD || slot == TANG_HID_SLOT_COMBO;
}

static bool slot_has_mouse(int slot)
{
    return slot == TANG_HID_SLOT_MOUSE || slot == TANG_HID_SLOT_COMBO;
}

static bool is_boot_keyboard_length(uint16_t length)
{
    return length == TANG_HID_BOOT_KBD_LEN;
}

static bool is_boot_mouse_length(uint16_t length)
{
    return length == TANG_HID_BOOT_MOUSE_LEN ||
           length == TANG_HID_BOOT_MOUSE_LEN_WHEEL;
}

tang_hid_route_t tang_hid_route(int slot, uint16_t value_handle,
                                uint16_t kbd_handle, uint16_t mouse_handle,
                                uint16_t length)
{
    /* An unknown slot carries no role, so only an exact handle match could
     * route it.  It answers NONE rather than matching, which is a deliberate
     * difference from the inline code this was extracted from: there the
     * handle comparison did not consult the slot at all, so an out-of-range
     * slot could still route by handle.  The caller derives the slot from a
     * pointer into its own array, so the case cannot arise; returning NONE is
     * the safer answer because the caller indexes arrays with that slot and a
     * routed report for slot 3 would write past them. */
    if (!slot_has_keyboard(slot) && !slot_has_mouse(slot)) {
        return TANG_HID_ROUTE_NONE;
    }

    const bool kbd_report =
        handle_matches(kbd_handle, value_handle) ||
        (slot_has_keyboard(slot) && is_boot_keyboard_length(length));
    const bool mouse_report =
        handle_matches(mouse_handle, value_handle) ||
        (slot_has_mouse(slot) && is_boot_mouse_length(length));

    /* The keyboard is tested first, and in a combo that is load-bearing: an
     * 8-byte report is a keyboard report whatever handle it arrives on, so the
     * length can override a mouse handle match.  That is safe only because boot
     * protocol gives a mouse three or four bytes and never eight -- a device
     * that sent an 8-byte mouse report would be outside the protocol it
     * negotiated.  Stated here because a refactor that swapped these two
     * branches would look harmless and would route every combo keyboard report
     * nowhere. */
    if (kbd_report && length >= TANG_HID_BOOT_KBD_LEN) {
        return TANG_HID_ROUTE_KEYBOARD;
    }
    if (mouse_report && length >= TANG_HID_BOOT_MOUSE_LEN) {
        return TANG_HID_ROUTE_MOUSE;
    }
    return TANG_HID_ROUTE_NONE;
}

const char *tang_hid_route_name(tang_hid_route_t route)
{
    switch (route) {
    case TANG_HID_ROUTE_KEYBOARD:
        return "keyboard";
    case TANG_HID_ROUTE_MOUSE:
        return "mouse";
    default:
        return "none";
    }
}
