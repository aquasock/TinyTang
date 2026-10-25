/* TinyTang — host test for the PCA9548A channel map (tang_i2c_mux.c).
 *
 * The map is fixed for this bench (core-log entry 41): 0 Wii adapter,
 * 1 magnetometer, 6 RTC, 7 SHT45, switch at 0x70.  What this locks down is the
 * routing rule the shell and the clock depend on — a known device always
 * selects its own channel, and anything else stays on the selected one.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>

#include "tang_clock.h"
#include "tang_i2c_mux.h"

static int failures;

#define CHECK(x)                                                              \
    do {                                                                      \
        if (!(x)) {                                                           \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x);                \
            failures++;                                                       \
        }                                                                     \
    } while (0)

static int has_channel(int channel)
{
    const char *n = tang_i2c_mux_name(channel);
    return n != NULL && strcmp(n, "unassigned") != 0;
}

int main(void)
{
    /* Devices the firmware knows select their own channel. */
    CHECK(tang_i2c_mux_target(TANG_I2C_WII_ADDR) == TANG_I2C_MUX_CH_WII);
    CHECK(tang_i2c_mux_target(TANG_CLOCK_RTC_ADDR) == TANG_I2C_MUX_CH_RTC);

    /* The switch answers upstream of every channel: selecting a channel
     * before addressing it would make the write talk to the wrong segment. */
    CHECK(tang_i2c_mux_target(TANG_I2C_MUX_ADDR) == -1);

    /* Everything else is the caller's channel, chosen with `tangi2c mux`. */
    CHECK(tang_i2c_mux_target(0x00) == -1);
    CHECK(tang_i2c_mux_target(0x40) == -1);
    CHECK(tang_i2c_mux_target(0x68 + 1) == -1);
    CHECK(tang_i2c_mux_target(0x7f) == -1);

    /* The bench's map is inside the part's eight channels, and no two devices
     * share one — which is the reason the switch is in the path at all. */
    CHECK(TANG_I2C_MUX_CH_WII >= 0 && TANG_I2C_MUX_CH_WII < 8);
    CHECK(TANG_I2C_MUX_CH_MAG >= 0 && TANG_I2C_MUX_CH_MAG < 8);
    CHECK(TANG_I2C_MUX_CH_RTC >= 0 && TANG_I2C_MUX_CH_RTC < 8);
    CHECK(TANG_I2C_MUX_CH_SHT45 >= 0 && TANG_I2C_MUX_CH_SHT45 < 8);
    CHECK(TANG_I2C_MUX_CH_WII != TANG_I2C_MUX_CH_MAG);
    CHECK(TANG_I2C_MUX_CH_WII != TANG_I2C_MUX_CH_RTC);
    CHECK(TANG_I2C_MUX_CH_WII != TANG_I2C_MUX_CH_SHT45);
    CHECK(TANG_I2C_MUX_CH_MAG != TANG_I2C_MUX_CH_RTC);
    CHECK(TANG_I2C_MUX_CH_MAG != TANG_I2C_MUX_CH_SHT45);
    CHECK(TANG_I2C_MUX_CH_RTC != TANG_I2C_MUX_CH_SHT45);

    /* The values from the bench record, so a transcription slip is a failure
     * here rather than a mystery on the board. */
    CHECK(TANG_I2C_MUX_CH_WII == 0);
    CHECK(TANG_I2C_MUX_CH_MAG == 1);
    CHECK(TANG_I2C_MUX_CH_RTC == 6);
    CHECK(TANG_I2C_MUX_CH_SHT45 == 7);
    CHECK(TANG_I2C_MUX_ADDR == 0x70);
    CHECK(TANG_I2C_WII_ADDR == 0x52);
    CHECK(TANG_CLOCK_RTC_ADDR == 0x68);
    CHECK(TANG_I2C_MUX_ADDR != TANG_I2C_WII_ADDR);
    CHECK(TANG_I2C_MUX_ADDR != TANG_CLOCK_RTC_ADDR);

    /* Every channel has a name, and the four in use are distinct so a status
     * line or a scan row cannot be misread. */
    for (int ch = 0; ch < 8; ch++) {
        CHECK(tang_i2c_mux_name(ch) != NULL);
    }
    CHECK(strcmp(tang_i2c_mux_name(-1), "unassigned") == 0);
    CHECK(strcmp(tang_i2c_mux_name(8), "unassigned") == 0);
    CHECK(has_channel(TANG_I2C_MUX_CH_WII));
    CHECK(has_channel(TANG_I2C_MUX_CH_MAG));
    CHECK(has_channel(TANG_I2C_MUX_CH_RTC));
    CHECK(has_channel(TANG_I2C_MUX_CH_SHT45));
    CHECK(strcmp(tang_i2c_mux_name(TANG_I2C_MUX_CH_WII),
                 tang_i2c_mux_name(TANG_I2C_MUX_CH_RTC)) != 0);
    CHECK(strcmp(tang_i2c_mux_name(TANG_I2C_MUX_CH_MAG),
                 tang_i2c_mux_name(TANG_I2C_MUX_CH_SHT45)) != 0);
    CHECK(strcmp(tang_i2c_mux_name(TANG_I2C_MUX_CH_RTC),
                 tang_i2c_mux_name(TANG_I2C_MUX_CH_SHT45)) != 0);

    /* A channel with nothing on it is named as unassigned, not as a device. */
    CHECK(strcmp(tang_i2c_mux_name(2), "unassigned") == 0);
    CHECK(strcmp(tang_i2c_mux_name(3), "unassigned") == 0);
    CHECK(strcmp(tang_i2c_mux_name(4), "unassigned") == 0);
    CHECK(strcmp(tang_i2c_mux_name(5), "unassigned") == 0);

    if (failures) {
        printf("tb_i2c_mux: %d failure(s)\n", failures);
        return 1;
    }
    printf("tb_i2c_mux: all checks passed\n");
    return 0;
}
