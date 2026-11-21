// TinyTang — decoding the core's reply frames.
//
// The UART gives a byte stream; what every reader actually wants is frames.
// That conversion used to live inside the link module, which put the trickiest
// logic in the file that is hardest to test: the link owns a UART, an
// interrupt, a filesystem and a shell command, so none of it runs anywhere but
// the board.  This is the same state machine as a module with no dependencies,
// so it can be exercised on a host.
//
// It also fixes a shape problem.  The link used to decode inside its waiter and
// discard every frame that was not the one being waited for.  That is fine
// while there is one reader.  The desktop's pointer made it two -- it wants the
// core's unprompted joypad reports -- and a reader that discards on the way
// past would then have eaten the reply another command was waiting for.  So
// frames are decoded into a small queue and a waiter takes what it wants,
// leaving the rest.
//
// Nothing here locks.  The caller decides what protection its context needs.

#ifndef FPGA_FRAMES_H
#define FPGA_FRAMES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The wire format, both directions:
 *
 *     0xAA len_hi len_lo type payload[len-1]
 *
 * The length is big-endian and counts the type byte.  A length high byte of 8
 * or more is not a frame this protocol can carry, so it is treated as noise
 * rather than believed -- which is how the reader resynchronises mid-stream. */
#define FPGA_FRAME_MAGIC 0xAAu
#define FPGA_LEN_HI_MAX  8u

/* Response types worth naming. */
#define FPGA_RESP_CORE_ID 0x01u
#define FPGA_RESP_JOYPAD  0x03u
/* A TinyNES core's AE350 text in link mode: the program's output, a line or
 * up to 128 bytes at a time (TinyNES fpga/ae350_soc/desk_link.sv). */
#define FPGA_RESP_AE350_TEXT 0x20u
/* A core's Wii Classic Controller 1, as the controller sent its last data
 * format 3 report: the four 8-bit stick bytes (LX, RX, LY, RY), then the two
 * button bytes, active low -- report bytes 0 to 3, 6 and 7.  TinyNES sends it
 * every 20 ms while a Classic is present (fpga/ae350_soc/desk_link.sv), so the
 * desktop can read the pad the core holds as it reads the one on its own I2C
 * bus (tang_wiipad.c).  It is kept as the newest, never queued. */
#define FPGA_RESP_WII        0x21u
#define FPGA_RESP_WII_LEN    6u
/* Save persistence: 128 bytes of cartridge save memory, and the acknowledgement
 * of a 128-byte write (address echoed back, then a status byte). */
#define FPGA_RESP_SAVE_DATA 0x10u
#define FPGA_RESP_SAVE_ACK  0x11u

/* How many frames and how much of each are kept.  A waiter's own reply plus a
 * few unprompted joypad reports is the realistic worst case, except for link
 * mode's AE350 text: a battery save comes back as a run of 65-byte frames.
 * When this fills, rx_decode stops feeding it rather than evicting the oldest
 * frame, so the receive ring behind it holds the rest and a burst delays
 * frames instead of losing them.  On 2026-10-14 a burst over an eight-frame
 * queue cost a save its last 82 bytes. */
#define FPGA_FRAMES_MAX   16u
#define FPGA_FRAMES_PAYLOAD_MAX    256u

/* fpga_frames_take() results that are not a length. */
#define FPGA_FRAMES_NONE    (-1)   /* nothing of that type is waiting */
#define FPGA_FRAMES_REFUSED (-2)   /* there was one, but it did not fit */

/* Forget the half-seen frame and empty the queue.  The bytes are gone, so
 * finishing a frame out of the next one's bytes would be worse than losing
 * it. */
void fpga_frames_reset(void);

/* Offer one byte from the stream. */
void fpga_frames_push(uint8_t byte);

/* True while the queue can take another frame.  The link's decoder stops when
 * this is false, leaving the bytes it has not read in the receive ring, so a
 * full queue delays frames instead of discarding the oldest of them. */
bool fpga_frames_space(void);

/* Empty the queue but leave the byte-level decoder exactly as it is, so a frame
 * half-received across this call is still finished correctly.
 *
 * This exists because `fpga_frames_take` removes only the type it was asked
 * for, and the core sends unprompted joypad reports while a command is running.
 * A long command therefore accumulates reports of types it is not waiting on
 * until the queue is full, at which point `fpga_frames_space` goes false and
 * `rx_decode` stops feeding the decoder -- so the reply the caller wants never
 * arrives and the caller times out.  A save dump is 64 to 1024 such exchanges in
 * a row, which is exactly the shape that fills a sixteen-frame queue.
 *
 * Safe to call between exchanges of a strict request/response protocol: nothing
 * legitimate is in flight at that moment, and joypad state is cached separately
 * (`fpga_frames_joypad`), so dropping those frames loses nothing. */
void fpga_frames_flush_queue(void);

/* Take the oldest frame of the given type.
 *
 * Returns its payload length (0 for a header-only frame), FPGA_FRAMES_NONE if
 * no such frame is waiting, or FPGA_FRAMES_REFUSED if one was waiting but
 * could not be delivered.  A frame longer than `cap` is dropped rather than
 * truncated: half a reply is not a reply.  The two failures are told apart so
 * a waiter can give up at once on a refusal instead of waiting out its timeout
 * for a frame that has already been discarded. */
int fpga_frames_take(uint8_t type, uint8_t *out, size_t cap);

/* The newest joypad the core has reported.
 *
 * Kept rather than queued, because the core sends only when the pad changes
 * and a pointer wants the pad's *state*: a button held down must read as held,
 * not as one edge.  Takes no frame from the queue, so the two readers cannot
 * starve each other. */
void fpga_frames_joypad(uint16_t *joy1, uint16_t *joy2);

/* True once a joypad report has ever been seen. */
bool fpga_frames_joypad_seen(void);

/* The newest FPGA_RESP_WII report into `out` (FPGA_RESP_WII_LEN bytes) and
 * how many have arrived since boot into *seq, which only ever counts up, so a
 * reader can tell a new report from the same one read again.  False, leaving
 * both alone, when none has arrived. */
bool fpga_frames_wii(uint8_t out[FPGA_RESP_WII_LEN], uint32_t *seq);

#endif /* FPGA_FRAMES_H */
