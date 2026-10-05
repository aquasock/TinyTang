// TinyTang — the frame transport to a loaded Tang core.
//
// One link, one protocol, several users.  Everything that talks to a core in
// the FPGA goes through here: the core-ID probe, the cartridge loader, and the
// text OSD.  Splitting it out is what keeps the OSD from having to know about
// UART pins or the RX ring, and keeps the loader from having to know about
// text.
//
// The wire format, in both directions, is
//
//     0xAA len_hi len_lo type payload[len-1]
//
// where the length is big-endian and counts the type byte.  FPGA_FRAME_MAX is
// the ceiling the core's receiver imposes, not a buffer size: it drops any
// frame whose length high byte is 8 or more and goes back to hunting for the
// next magic byte.
//
// This header carries no state.  The implementation owns the UART, the
// interrupts and the lock; callers only see frames.

#ifndef TANG_FPGA_LINK_H
#define TANG_FPGA_LINK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* --------------------------------------------------- commands to the core */

/* Single-byte frames (payload length zero). */
#define FPGA_CMD_CORE_ID    0x01u   /* reply: FPGA_RESP_CORE_ID + core id */

/* OSD text page: 32 columns by 28 rows of 8x8 cells. */
#define FPGA_CMD_OSD_CURSOR 0x04u   /* x, y                         */
#define FPGA_CMD_OSD_TEXT   0x05u   /* characters, from the cursor   */
#define FPGA_CMD_OVERLAY    0x08u   /* one byte; bit 0 shows/hides   */

/* Desktop layer: 80 columns by 45 rows, per-cell colour.  Additive -- a core
 * that does not implement these ignores the frame types, which is what keeps
 * nand2mario's other cores and every stock host working.  The layer is shown
 * only while the overlay above is asserted, so the existing overlay control
 * hides it too. */
#define FPGA_CMD_DESK_CURSOR 0x13u  /* x, y                          */
#define FPGA_CMD_DESK_CELLS  0x14u  /* 5 bytes per cell, from cursor */
#define FPGA_CMD_DESK_CTRL   0x15u  /* one byte; bit 0 enables       */

/* Cartridge loading. */
#define FPGA_CMD_SET_LOAD   0x06u   /* one byte; 0 starts the core   */
#define FPGA_CMD_ROM_DATA   0x07u   /* ROM bytes                     */

/* ------------------------------------------------- responses from the core
 *
 * The frame format and the response types live with the decoder that reads
 * them (`fpga_frames.h`), so the protocol has one definition rather than two
 * that can drift apart. */

#include "fpga_frames.h"

/* The core's receiver ceiling: the length high byte must be < 8. */
#define FPGA_FRAME_MAX      2047u

/* ------------------------------------------------------------------- API */

/* Bring the link up.  Idempotent, and safe to call from any command.  Returns
 * 0, or -1 if UART1 could not be started. */
int  tang_fpga_link_open(void);

/* The transmit lock.  Frames from different commands must not interleave on
 * the wire, so a caller that sends more than one frame takes this across the
 * whole sequence.  It is a FreeRTOS mutex, not a critical section: interrupts
 * stay enabled, because the RX interrupt is what keeps the core's replies from
 * overflowing the 32-byte FIFO. */
void tang_fpga_lock(void);
void tang_fpga_unlock(void);

/* Throw away anything already in the RX ring, so a stale reply cannot be
 * mistaken for the answer to the frame about to be sent. */
void tang_fpga_drain(void);

/* Send one frame.  The caller holds the lock -- one rule for every frame,
 * because a multi-frame sequence must not have another command's frames
 * interleaved into it, and a lock taken per frame would allow exactly that.
 * Returns 0, or -1 if a byte was refused (the TX FIFO stayed full for 100 ms)
 * or the frame would exceed FPGA_FRAME_MAX. */
int  tang_fpga_frame(uint8_t type, const uint8_t *payload, size_t length);

/* Wait for a response frame of the given type, copying its payload into `out`.
 * Frames of other types are parsed and discarded, so the core's periodic
 * joypad traffic cannot desynchronise the reader.  Takes no lock: it only
 * reads the RX ring, and a sender waiting for an answer must not be holding
 * the transmit lock while it waits.  Returns the payload length, or -1 on
 * timeout or if the payload would not fit. */
int  tang_fpga_wait(uint8_t want_type, uint8_t *out, size_t cap,
                    uint32_t timeout_ms);

/* Change the link's rate, for a core that negotiates one (Phosphor's extended
 * protocol switches between 2 and 5 Mbaud around a file stream, EXTCTL-001).
 * The caller holds the lock and has already had the core's agreement: the core
 * switches after its response, and this switches this end to match.  Bytes in
 * the RX ring are dropped, since anything half-received spans the change.
 * Returns 0, or -1 if the link is not open. */
int      tang_fpga_set_baud(uint32_t baud);
uint32_t tang_fpga_baud(void);

/* The newest joypad the core has reported.
 *
 * The core sends this unprompted whenever the pad changes and at most every
 * 20 ms, which makes it a change feed rather than a readable register -- so the
 * link keeps the latest state and hands it out on demand.  It is sticky: a
 * caller polling for a button held down sees it held, not one edge.
 *
 * This is not a `wait`: it never blocks and it never takes the transmit lock,
 * so it is safe to call from a task that is also driving the pad. */
void tang_fpga_joypad(uint16_t *joy1, uint16_t *joy2);

/* The newest keyboard report the core has sent, in HID boot layout: modifier
 * byte, reserved byte, then six usage codes.
 *
 * Returns true when a report arrived since the previous call, which is how the
 * caller tells a held key from a dead link: the core resends an unchanged
 * report as a heartbeat, so "a report arrived" stays true while a key is held
 * and goes false when the link stops.
 *
 * Sticky, like the pad above and for the same reason: the core reports state,
 * so a key held down reads as held rather than as one edge.  Never blocks and
 * never takes the transmit lock, so it is safe from the same task that drives
 * the pointer. */
bool tang_fpga_keyboard(uint8_t out[8]);

#endif /* TANG_FPGA_LINK_H */
