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

/* Cartridge loading. */
#define FPGA_CMD_SET_LOAD   0x06u   /* one byte; 0 starts the core   */
#define FPGA_CMD_ROM_DATA   0x07u   /* ROM bytes                     */

/* ------------------------------------------------- responses from the core */

#define FPGA_RESP_CORE_ID   0x01u

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

#endif /* TANG_FPGA_LINK_H */
