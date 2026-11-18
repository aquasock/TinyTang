/* Host stub: the HCI command calls the PIN reply uses.
 * SPDX-License-Identifier: MIT */
#ifndef STUB_BT_HCI_HOST_H
#define STUB_BT_HCI_HOST_H
#include "l2cap.h"
struct net_buf *bt_hci_cmd_create(u16_t opcode, u8_t param_len);
int bt_hci_cmd_send_sync(u16_t opcode, struct net_buf *buf, struct net_buf **rsp);
#endif
