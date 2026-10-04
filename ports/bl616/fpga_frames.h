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
/* The keyboard link's report: the HID boot layout, modifier byte then six
 * usage codes, byte for byte.  The core sends it when it changes, the same way
 * and for the same reason it sends the pad -- the report is state, and the
 * desktop wants the newest state rather than a queue of transitions. */
#define FPGA_RESP_KEYBOARD 0x08u

/* How many frames and how much of each are kept.  A waiter's own reply plus a
 * few unprompted joypad reports is the realistic worst case. */
#define FPGA_FRAMES_MAX   8u
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

/* The newest keyboard report the core has sent, in HID boot layout: eight
 * bytes, modifier then reserved then six usage codes.  Bytes are copied, not
 * a pointer, because the cache can be updated by the next frame at any time.
 *
 * Returns true when a report arrived since the previous call.  That is not the
 * same as the contents having changed, and the difference is the whole point:
 * the core resends an unchanged report as a heartbeat, so "a report arrived"
 * is what tells the reader the link is alive, while "the contents changed"
 * would be false the entire time a key is held.
 *
 * Kept rather than queued for the same reason as the pad: the core reports
 * state, and a key held down must read as held.  Takes no frame from the
 * queue, so it cannot starve a waiter. */
bool fpga_frames_keyboard(uint8_t out[8]);

/* True once a keyboard report has ever been seen. */
bool fpga_frames_keyboard_seen(void);

#endif /* FPGA_FRAMES_H */
