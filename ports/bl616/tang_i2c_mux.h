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
 *   channel 7  MLX90614 IR thermometer, 0x5A
 *
 * Channels 1 to 5 are unassigned: the LIS3MDL magnetometer that named channel
 * 1 was removed from the bench, so it no longer has a name or a constant here.
 * The MLX90614 answers its address on channel 7 but returns no register data
 * yet, so the channel carries the part's name and nothing reads it.
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
#define TANG_I2C_MUX_CH_IR    7

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
