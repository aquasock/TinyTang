/* Host stub: connections, counted the way Bouffalo SDK 2.3.36 counts them
 * (BLE-021).  SPDX-License-Identifier: MIT */
#ifndef STUB_BT_CONN_H
#define STUB_BT_CONN_H
#include "bluetooth.h"

#define BT_CONN_TYPE_LE 1
#define BT_CONN_TYPE_BR 2
#define BT_HCI_ERR_REMOTE_USER_TERM_CONN 0x13

typedef enum { BT_SECURITY_L0, BT_SECURITY_L1, BT_SECURITY_L2, BT_SECURITY_L3,
               BT_SECURITY_L4 } bt_security_t;
enum bt_security_err { BT_SECURITY_ERR_SUCCESS = 0, BT_SECURITY_ERR_AUTH_FAIL = 1 };

struct bt_conn {
    int       ref;
    bool      in_use;
    bool      initial_master;   /* BT_CONN_BR_INITIAL_MASTER */
    bt_addr_t dst;
    int       disconnects;      /* bt_conn_disconnect() calls */
};
struct bt_conn_br_info { const bt_addr_t *dst; };
struct bt_conn_info {
    u8_t type;
    struct bt_conn_br_info br;
};
struct bt_br_conn_param { bool allow_role_switch; };
#define BT_BR_CONN_PARAM_DEFAULT (&(struct bt_br_conn_param){ .allow_role_switch = true })

int bt_conn_get_info(const struct bt_conn *conn, struct bt_conn_info *info);
struct bt_conn *bt_conn_ref(struct bt_conn *conn);
void bt_conn_unref(struct bt_conn *conn);
int bt_conn_disconnect(struct bt_conn *conn, u8_t reason);
struct bt_conn *bt_conn_create_br(const bt_addr_t *peer, const struct bt_br_conn_param *param);
int bt_conn_auth_pincode_entry(struct bt_conn *conn, const char *pin);
#endif
