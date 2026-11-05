/* Host stub: SPDX-License-Identifier: MIT */
#ifndef STUB_CR_WDG_H
#define STUB_CR_WDG_H
#include <stdint.h>
struct bflb_device_s { int unused; };
struct bflb_wdg_config_s {
    int clock_source, clock_div;
    uint32_t comp_val;
    int mode;
};
#define WDG_CLKSRC_32K 0
#define WDG_MODE_RESET 1
struct bflb_device_s *bflb_device_get_by_name(const char *name);
int bflb_wdg_init(struct bflb_device_s *dev, struct bflb_wdg_config_s *cfg);
int bflb_wdg_start(struct bflb_device_s *dev);
int bflb_wdg_stop(struct bflb_device_s *dev);
int bflb_wdg_reset_countervalue(struct bflb_device_s *dev);
#endif
