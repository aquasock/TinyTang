/* Host stub: BR/EDR L2CAP channels and servers.  SPDX-License-Identifier: MIT */
#ifndef STUB_BT_L2CAP_H
#define STUB_BT_L2CAP_H
#include "conn.h"

struct net_buf { uint8_t *data; u16_t len; uint8_t store[64]; };
struct net_buf_pool;
#define K_SECONDS(s) ((s32_t)((s) * 1000))
void *net_buf_add_mem(struct net_buf *buf, const void *mem, size_t len);
void net_buf_unref(struct net_buf *buf);
struct bt_l2cap_chan;
struct bt_l2cap_chan_ops {
    void (*connected)(struct bt_l2cap_chan *chan);
    void (*disconnected)(struct bt_l2cap_chan *chan);
    int (*recv)(struct bt_l2cap_chan *chan, struct net_buf *buf);
};
struct bt_l2cap_chan {
    struct bt_conn *conn;
    const struct bt_l2cap_chan_ops *ops;
    bt_security_t required_sec_level;
    u16_t psm;
};
struct bt_l2cap_br_endpoint { u16_t cid; u16_t mtu; };
struct bt_l2cap_br_chan {
    struct bt_l2cap_chan chan;
    struct bt_l2cap_br_endpoint rx, tx;
};
struct bt_l2cap_server {
    u16_t psm;
    bt_security_t sec_level;
    int (*accept)(struct bt_conn *conn, struct bt_l2cap_chan **chan);
};
int bt_l2cap_br_server_register(struct bt_l2cap_server *server);
int bt_l2cap_chan_send(struct bt_l2cap_chan *chan, struct net_buf *buf);
int bt_l2cap_chan_connect(struct bt_conn *conn, struct bt_l2cap_chan *chan, u16_t psm);
#endif
