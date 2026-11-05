/* TinyTang — the PCA9548A switch on the USB-A I2C bus.
 *
 * The desktop core's bus (BRD-011) reaches the card's devices through a TI
 * PCA9548A at 0x70: one upstream pair, eight downstream channels, and one
 * channel live at a time.  It is a switch, not a second master — every
 * transaction still comes from whichever core holds H13/G13, and the switch
 * only decides which segment that master sees.
 *
 * The map is fixed for this bench rather than read from /tinytang.ini
 * (core-log entry 41), because the ini contract admits one key, `layout`:
 *
 *   channel 0  Adafruit Wii adapter, 0x52
 *   channel 6  DS3231 RTC, 0x68
 *   channel 7  Adafruit MPR121 12-key cap-touch breakout, 0x5A
 *
 * Channels 1 to 5 are unassigned and free for another device: a part added to
 * one of them is reached today with `tangi2c mux <n>` and the read/write
 * commands, and needs an entry here only to be routed automatically by
 * address or to have a name in `tangi2c mux`.
 *
 * Channel 7 held an MLX90614 IR thermometer until 2026-10-30, when the user
 * replaced it with the MPR121 (core-log entry 83).  The address, 0x5a, is the
 * same, which is why the constant kept its old name for a while; it is
 * `..._TOUCH` now and the old spelling remains as an alias so nothing that
 * still names it breaks.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef TANG_I2C_MUX_H
#define TANG_I2C_MUX_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TANG_I2C_MUX_ADDR     0x70u
#define TANG_I2C_WII_ADDR     0x52u

#define TANG_I2C_MUX_CH_WII   0
#define TANG_I2C_MUX_CH_RTC   6
#define TANG_I2C_MUX_CH_TOUCH 7
/* The MPR121 replaced an MLX90614 on this channel; keep the old spelling
 * working so a caller that still names it does not break. */
#define TANG_I2C_MUX_CH_IR    TANG_I2C_MUX_CH_TOUCH

/* The channel a transaction to `addr` has to have selected, or -1 to leave the
 * switch where it is: the switch itself answers upstream of every channel, and
 * any other address belongs to whichever channel the caller selected. */
int tang_i2c_mux_target(uint8_t addr);

/* A channel's name for the shell. */
const char *tang_i2c_mux_name(int channel);

/* Point the switch at the Wii adapter before a core that drives H13/G13 itself
 * (a game core) takes the bus.  Best effort: no switch, no desktop core, or a
 * refused write leaves the bus as it was. */
void tang_i2c_mux_for_game_core(void);

#ifdef __cplusplus
}
#endif

#endif
