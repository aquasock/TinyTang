// TinyTang — the keyboard as the desktop's typing input.
//
// The core's keyboard link carries the HID boot keyboard report: one modifier
// byte and six usage codes, which is what a boot-protocol keyboard sends and
// what the keyboard end already builds for itself.  The desktop does not want
// usages, though.  It wants the byte stream an ANSI terminal sends, because
// that is what its input parser reads, so this is the translation: usage codes
// in, terminal input bytes out.
//
// The path is the parser's own, and it was chosen to match it rather than to
// fight it.  In `third_party/tinydesk/src/input.c`, `ground_byte()` treats a
// handful of control bytes as named keys and then UTF-8 decodes anything else
// and emits the code point as a key.  So a letter is its own byte, a shifted
// character is its shifted byte, and Ctrl turns a letter into 0x01..0x1A --
// which the parser maps straight back to ctrl+letter.  Nothing here needs a
// modifier the desktop understands, because the byte carries the result
// rather than the intent to produce it.
//
// Auto-repeat is generated here rather than expected from the keyboard.  A USB
// boot keyboard has no repeat of its own: the host decides, from its own
// settings, that a held key should repeat, and the device goes on reporting
// the key as held.  The keyboard link behaves the same way -- it reports state,
// not events -- so without this a held Backspace would delete exactly one
// character, which is not what anyone means by holding Backspace.

#ifndef TANG_KEY_H
#define TANG_KEY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* HID modifier bits, as they arrive in the report's first byte. */
#define TANG_KEY_LCTRL  0x01u
#define TANG_KEY_LSHIFT 0x02u
#define TANG_KEY_LALT   0x04u
#define TANG_KEY_RCTRL  0x10u
#define TANG_KEY_RSHIFT 0x20u
#define TANG_KEY_RALT   0x40u

/* How long a key is held before it starts repeating, and how often after that.
 * The first is the delay a person expects before a key "takes"; the second is
 * the rate.  Both are the values a desktop would use for its own keyboard. */
#ifndef TANG_KEY_REPEAT_DELAY_MS
#    define TANG_KEY_REPEAT_DELAY_MS 500u
#endif
#ifndef TANG_KEY_REPEAT_RATE_MS
#    define TANG_KEY_REPEAT_RATE_MS 50u
#endif

/* How long the link may go without delivering a report before repeat stops.
 *
 * This exists because a boot keyboard reports *state*: a key held down sends
 * one report and then nothing, so repeat has to be generated here, and
 * therefore here cannot tell "still held" from "the link has stopped" by the
 * report alone.  That ambiguity was not theoretical -- when the link died
 * mid-type the last key repeated forever, flooding the desktop with input and
 * repaints until nothing recovered.
 *
 * The core sends a heartbeat when the report has not changed, so silence now
 * means something.  Three missed heartbeats is the threshold: generous enough
 * that a busy link cannot cut a keystroke short, short enough that a dead one
 * goes quiet rather than repeating. */
#ifndef TANG_KEY_REPORT_TIMEOUT_MS
#    define TANG_KEY_REPORT_TIMEOUT_MS 300u
#endif

typedef struct {
    uint8_t  mods;          /* the last report's modifier byte */
    uint8_t  keys[6];       /* the last report's usages, 0 = empty */
    bool     held;          /* a key is down and repeating */
    uint32_t hold_start;    /* when the repeating key went down */
    uint32_t last_repeat;   /* when it last repeated */
    uint32_t last_report;   /* when a report last arrived from the core */
} tang_key_t;

/* Forget everything, as if no report had ever arrived. */
void tang_key_reset(tang_key_t *k);

/* Turn one report into typing.
 *
 * `report_fresh` says whether a new report arrived from the core since the last
 * call -- not whether the contents changed, because a held key's report is
 * identical to the one before it and arrives anyway.  Repeat runs only while
 * reports keep arriving, so a link that has stopped produces silence rather
 * than the last key forever.
 *
 * Appends the bytes the desktop should receive to `out` and returns how many
 * were written, or 0 when this report produced nothing to type.  A report that
 * repeats a held key emits that key only once the repeat delay has passed at
 * `now_ms`, and then at the repeat rate.
 *
 * The caller decides what to do with a full buffer: a byte that does not fit is
 * dropped rather than half-written, and dropping one character is better than
 * desynchronising the parser with half an escape sequence. */
int tang_key_step(tang_key_t *k, uint8_t mods, const uint8_t keys[6],
                  bool report_fresh, uint32_t now_ms, char *out, size_t cap);

#endif /* TANG_KEY_H */
