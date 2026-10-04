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

#endif /* FPGA_FRAMES_H */
