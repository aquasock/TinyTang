/* TinyTang — the PCA9548A channel map (tang_i2c_mux.h), hardware-free.
 *
 * SPDX-License-Identifier: MIT
 */

#include "tang_i2c_mux.h"
#include "tang_clock.h"

int tang_i2c_mux_target(uint8_t addr)
{
    /* The switch answers its own address upstream of whichever channel is
     * live, so a write to it must never try to select a channel first. */
    if (addr == TANG_I2C_MUX_ADDR) {
        return -1;
    }
    if (addr == TANG_I2C_WII_ADDR) {
        return TANG_I2C_MUX_CH_WII;
    }
    if (addr == TANG_CLOCK_RTC_ADDR) {
        return TANG_I2C_MUX_CH_RTC;
    }
    return -1;
}

const char *tang_i2c_mux_name(int channel)
{
    switch (channel) {
    case TANG_I2C_MUX_CH_WII:
        return "wii";
    case TANG_I2C_MUX_CH_MAG:
        return "mag";
    case TANG_I2C_MUX_CH_RTC:
        return "rtc";
    case TANG_I2C_MUX_CH_SHT45:
        return "sht45";
    default:
        return "unassigned";
    }
}
