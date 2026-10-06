// TinyTang — the controller as the desktop's pointer.
//
// The pad is read by the core and relayed here as the joypad word; this turns
// it into the input the desktop already understands -- xterm SGR mouse reports
// -- so nothing in tinydesk has to know a controller exists.  The desktop
// enables mouse reporting itself and parses exactly this form, so the whole
// interface is the one it already speaks.
//
// Bit assignments are not guessable and are pinned here: the core relays the
// SNES/NES pad word in the shift order the controller sends, MSB last, so
//
//     bit 4  up      bit 8  A        (A and B are what the core's own menu
//     bit 5  down    bit 0  B         uses as "choose"; PROT-006)
//     bit 6  left    bit 10 L / LB
//     bit 7  right
//
// The word's own comment calls bits 6 and 7 LT and RT, which is DS2 naming:
// on a SNES pad those are the D-pad's left and right, and the core's NES
// mapping (`{joy1[7:2], ...}`) depends on exactly that.  Confirmed against
// src/controller_snes.v's shift table.
//
// A Bluetooth mouse drives the same pointer (tang_pad_pointer): its movement
// is scaled from counts to cells and its buttons merge with A and B.
//
// Coordinates are cells and one-based on the wire: the desktop subtracts one
// when it parses, so a pointer at cell (0,0) is reported as (1,1).

#ifndef TANG_PAD_H
#define TANG_PAD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The pad word, as the core relays it. */
#define TANG_PAD_UP    0x010u
#define TANG_PAD_DOWN  0x020u
#define TANG_PAD_LEFT  0x040u
#define TANG_PAD_RIGHT 0x080u
#define TANG_PAD_A     0x100u
#define TANG_PAD_B     0x001u
#define TANG_PAD_LB    0x400u

/* A mouse, as the pointer sees it: movement and wheel gathered since the
 * last step, and the buttons as they are now.  HID boot mouse buttons: bit 0
 * left, bit 1 right, bit 2 middle. */
typedef struct {
    uint8_t buttons;
    int     dx, dy;           /* counts; +x right, +y down */
    int     wheel;            /* detents; + is away from the user (scroll up) */
} tang_mouse_t;

#define TANG_MOUSE_LEFT   0x01u
#define TANG_MOUSE_RIGHT  0x02u
#define TANG_MOUSE_MIDDLE 0x04u

/* How many mouse counts move the pointer one cell.  A cell is 16x16 pixels of
 * the 1280x720 frame, so 16 is one count per pixel. */
#ifndef TANG_MOUSE_COUNTS_PER_CELL
#    define TANG_MOUSE_COUNTS_PER_CELL 16
#endif

/* At most this many wheel events go out per step; a faster spin is clamped
 * rather than queued, so the desktop never scrolls on after the wheel stops. */
#define TANG_MOUSE_WHEEL_MAX 4

typedef struct {
    int      cols, rows;      /* the grid the pointer is clamped to */
    int      x, y;            /* pointer cell */
    int      fx, fy;          /* mouse counts not yet a whole cell */
    uint16_t prev;            /* the previous pad word, for edges */
    bool     left_down;       /* button states we have already reported */
    bool     right_down;
    bool     middle_down;
} tang_pad_t;

/* Put the pointer in the middle of the grid. */
void tang_pad_reset(tang_pad_t *p, int cols, int rows);

/* Turn one reading of the pad into desktop input.
 *
 * Writes the escape sequences into `out` and returns how many bytes; sets
 * *toggle when L was pressed on this reading (a rising edge, not a level, so
 * holding it does not flap).  Returns 0 and *toggle == false when nothing
 * happened, which is the common case.
 *
 * `out` should be at least 96 bytes: a step can emit a move and a click. */
int tang_pad_step(tang_pad_t *p, uint16_t joy1, char *out, size_t cap, bool *toggle);

/* The same step with a mouse as well, so the pad and the mouse drive one
 * pointer: the mouse's movement is added to the pad's, its left and right
 * buttons are the pad's A and B (either one holds the button), its middle
 * button is SGR button 1 and its wheel SGR 64 / 65.  `m` may be NULL.
 *
 * `out` should be at least 192 bytes: a step can emit a move, three button
 * changes and TANG_MOUSE_WHEEL_MAX wheel events. */
int tang_pad_pointer(tang_pad_t *p, uint16_t joy1, const tang_mouse_t *m,
                     char *out, size_t cap, bool *toggle);

/* Add one HID boot mouse report -- buttons, then X and Y as signed bytes,
 * then optionally a signed wheel byte -- to `m`.  Movement and wheel
 * accumulate; the buttons are replaced.  A report shorter than three bytes is
 * ignored. */
void tang_mouse_boot_add(tang_mouse_t *m, const uint8_t *report, size_t len);

#endif /* TANG_PAD_H */
