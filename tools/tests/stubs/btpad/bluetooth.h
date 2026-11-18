/* Host stub for tools/tests/tb_btpad_host.c: the Bluetooth host types and
 * calls ports/bl616/tang_btpad.c uses, behaving as the test's fake stack
 * says.  Only what that file needs.
 * SPDX-License-Identifier: MIT */
#ifndef STUB_BT_BLUETOOTH_H
#define STUB_BT_BLUETOOTH_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <errno.h>

typedef uint8_t  u8_t;
typedef uint16_t u16_t;
typedef int32_t  s32_t;

typedef struct { uint8_t val[6]; } bt_addr_t;
typedef struct { uint8_t type; bt_addr_t a; } bt_addr_le_t;

struct bt_br_discovery_result {
    bt_addr_t addr;
    int8_t    rssi;
    uint8_t   cod[3];
    uint8_t   eir[240];
};
struct bt_br_discovery_param {
    uint8_t length;
    bool    limited;
};
typedef void bt_br_discovery_cb_t(struct bt_br_discovery_result *results, size_t count);
int bt_br_discovery_start(const struct bt_br_discovery_param *param,
                          struct bt_br_discovery_result *results, size_t count,
                          bt_br_discovery_cb_t cb);
int bt_br_discovery_stop(void);
int bt_br_set_connectable(bool enable);
#endif
