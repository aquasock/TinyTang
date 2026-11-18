/* Host stub: the BR/EDR link key table.  SPDX-License-Identifier: MIT */
#ifndef STUB_BT_KEYS_H
#define STUB_BT_KEYS_H
#include "bluetooth.h"
struct bt_keys_link_key { bt_addr_t addr; u8_t flags; u8_t val[16]; };
struct bt_keys_link_key *bt_keys_get_link_key(const bt_addr_t *addr);
struct bt_keys_link_key *bt_keys_find_link_key(const bt_addr_t *addr);
void bt_keys_link_key_clear_addr(const bt_addr_t *addr);
#endif
