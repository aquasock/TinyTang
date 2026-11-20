// TinyTang — Classic Bluetooth gamepads: the HID host.
//
// What the board does with a Classic pad, from the evidence in BLE-021 and
// BLE-022:
//
//   * Pairing (`btpad pair`): a Classic inquiry picks the gamepad in pairing
//     mode, an ACL link is made to it, and opening the HID control channel
//     (PSM 0x11) with security level 2 makes the host pair with Secure
//     Simple Pairing.  tang_ble.c's auth callbacks have no passkey ones, so
//     the IO capability is NoInputNoOutput and Just Works is confirmed by the
//     stack.  Then the interrupt channel (PSM 0x13) opens and reports flow.
//   * Keeping it: the host keeps link keys in RAM only on this board, so the
//     key is copied into /sd/root/ble/br_keys.bin when pairing completes and
//     written back into the host's table each time the radio starts.
//   * Reconnecting: a woken pad pages the board, authenticates with the key
//     and opens both channels itself.  The board must not open its own at
//     the same moment -- that collided and timed the link out (0x24) -- so
//     on a link the pad made it waits BTPAD_PAD_OPENS_MS before stepping in.
//   * Reference counting: on a link this host made, the reference
//     bt_conn_create_br() returns is the stack's, and notify_disconnected()
//     drops it after the disconnected callbacks (BLE-021); dropping it here
//     too reset the board.  Only a link the pad made carries a reference of
//     ours.  A failed connect never reaches notify_disconnected(), so there
//     the reference is ours to drop.
//   * Reports: the Switch simple report 0x3F (tang_btpad_codec.h), turned
//     into the pad word the desk layer takes from the dock's pad.
//
// The stack's callbacks run in its receive task and only change state and
// note events here; the card and the channel opening are done by tang_ble.c's
// background task through tang_btpad_work().  The shell commands print.
//
// SPDX-License-Identifier: MIT
#include "tang_btpad.h"

#if !defined(CONFIG_BT_BREDR)

/* The LE-only build has no Classic host: no pad is ever connected. */
uint16_t tang_btpad_pad_word(int pad)
{
    (void)pad;
    return 0;
}

void tang_btpad_poll(tang_mouse_t *m, bool console_mode, tang_wii_keys_t *k)
{
    (void)m;
    (void)console_mode;
    (void)k;
}

#else /* CONFIG_BT_BREDR */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "bflb_mtimer.h"
#include "bluetooth.h"
#include "conn.h"
#include "ff.h"
#include "hci_host.h"
#include "keys.h"
#include "l2cap.h"

#include "tang_btpad_codec.h"
#include "tang_btpad_hooks.h"
#include "tdsh.h"
#include "tdsh_stdio_redirect.h"   /* tdsh_printf */

/* From the stack's internal headers, which are not on the include path.
 * conn_internal.h: the Classic link to an address, with a reference, or NULL. */
struct bt_conn *bt_conn_lookup_addr_br(const bt_addr_t *peer);
/* hci_core.h: this controller's public address, the Classic BD_ADDR too. */
int bt_get_local_public_address(bt_addr_le_t *adv_addr);
/* l2cap_internal.h: a PDU from the ACL pool (pool NULL). */
struct net_buf *bt_l2cap_create_pdu_timeout(struct net_buf_pool *pool,
                                            size_t reserve, s32_t timeout);

#define BTPAD_HCI_PIN_CODE_REPLY 0x040Du   /* Link Control, OCF 0x0D */

#define BTPAD_DIR           "/sd/root/ble"
#define BTPAD_PATH          "/sd/root/ble/br_keys.bin"
#define BTPAD_PSM_CTRL      0x0011u
#define BTPAD_PSM_INTR      0x0013u
#define BTPAD_MTU           672u      /* the BR/EDR default */
#define BTPAD_PAD_OPENS_MS  3000u
#define BTPAD_RAW_MAX       49        /* a Switch full report; 0x3F is 13 */
#define BTPAD_NOTES         8
#define BTPAD_NOTE_LEN      72
#define BTPAD_SCAN_MAX      16

typedef struct {
    tang_btpad_key_t saved;            /* the pairing, on the card */
    struct bt_conn *conn;              /* the link; outbound says whose reference */
    bool outbound;                     /* this host made it */
    bool pairing;                      /* made by a pairing: save the key it gets */
    volatile bool key_due;             /* paired and bonded; the key comes after */
    volatile bool pin_due;             /* the pad asked for a legacy PIN */
    bool used_pin;                     /* this link paired by PIN: the Wii family */
    uint8_t want_addr[6];              /* the pad a pairing is for */
    char want_name[TANG_BTPAD_NAME_LEN];
    struct bt_l2cap_br_chan ctrl, intr;
    volatile bool linked, ctrl_up, intr_up;
    bool ctrl_asked, intr_asked;       /* opened by this host on this link */
    uint64_t linked_ms;
    volatile uint16_t word;
    tang_btpad_input_t in;             /* the latest report, decoded */
    volatile bool have_in;
    tang_wiipad_t wii;                 /* the Wii Classic key map's state; desk task only */
    uint32_t next_wheel_ms;            /* desk task only */
    uint32_t reports;
    uint8_t raw[BTPAD_RAW_MAX];        /* the latest report, for `watch` */
    uint8_t raw_len;
    uint32_t raw_seq;
} btpad_t;

/* Static, because the stack holds pointers to the channels inside. */
static btpad_t s_pad[TANG_BTPAD_SLOTS];
static volatile bool s_save_due;
static bool s_servers;
static const char *s_file_state = "not read";

/* ------------------------------------------------------------ the notes */

/* What happened, for `btpad status` and `watch`: written from the stack's
 * task, read from the shell's. */
static char s_note[BTPAD_NOTES][BTPAD_NOTE_LEN];
static volatile unsigned s_note_head;    /* total ever written */

static void note(const char *fmt, ...)
{
    char tmp[BTPAD_NOTE_LEN];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    taskENTER_CRITICAL();
    memcpy(s_note[s_note_head % BTPAD_NOTES], tmp, sizeof tmp);
    s_note_head++;
    taskEXIT_CRITICAL();
}

/* Print the notes written since *seen (the oldest that survive), and move it. */
static void notes_print(unsigned *seen)
{
    const unsigned head = s_note_head;
    if (head - *seen > BTPAD_NOTES) {
        *seen = head - BTPAD_NOTES;
    }
    for (; *seen != head; (*seen)++) {
        char line[BTPAD_NOTE_LEN];
        taskENTER_CRITICAL();
        memcpy(line, s_note[*seen % BTPAD_NOTES], sizeof line);
        taskEXIT_CRITICAL();
        tdsh_printf("btpad: %s\r\n", line);
    }
}

/* ------------------------------------------------------------ the slots */

static void addr_text(const uint8_t a[6], char out[18])
{
    snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", a[5], a[4], a[3], a[2], a[1], a[0]);
}

static bool conn_addr(struct bt_conn *conn, uint8_t out[6])
{
    struct bt_conn_info ci;
    if (bt_conn_get_info(conn, &ci) != 0 || ci.type != BT_CONN_TYPE_BR || ci.br.dst == NULL) {
        return false;
    }
    memcpy(out, ci.br.dst->val, 6);
    return true;
}

static btpad_t *by_conn(struct bt_conn *conn)
{
    for (int i = 0; i < TANG_BTPAD_SLOTS; i++) {
        if (s_pad[i].conn == conn && conn != NULL) {
            return &s_pad[i];
        }
    }
    return NULL;
}

/* The slot a pad at this address belongs to: its saved pairing, or the
 * pairing in progress for it. */
static btpad_t *by_addr(const uint8_t a[6])
{
    for (int i = 0; i < TANG_BTPAD_SLOTS; i++) {
        if (s_pad[i].saved.valid && memcmp(s_pad[i].saved.addr, a, 6) == 0) {
            return &s_pad[i];
        }
    }
    for (int i = 0; i < TANG_BTPAD_SLOTS; i++) {
        if (s_pad[i].pairing && memcmp(s_pad[i].want_addr, a, 6) == 0) {
            return &s_pad[i];
        }
    }
    return NULL;
}

static int slot_no(const btpad_t *p)
{
    return (int)(p - s_pad) + 1;
}

static void key_capture(btpad_t *p);

uint16_t tang_btpad_pad_word(int pad)
{
    if (pad < 0 || pad >= TANG_BTPAD_SLOTS) {
        return 0;
    }
    return s_pad[pad].intr_up ? s_pad[pad].word : 0;
}

/* The controls as the desktop's input (tang_btpad.h).  A report seen for the
 * first time on a link starts from a fresh key map state, so a button held
 * across a reconnection is not a press. */
void tang_btpad_poll(tang_mouse_t *m, bool console_mode, tang_wii_keys_t *k)
{
    const uint32_t now = (uint32_t)bflb_mtimer_get_time_ms();
    for (int i = 0; i < TANG_BTPAD_SLOTS; i++) {
        btpad_t *p = &s_pad[i];
        if (!p->intr_up || !p->have_in) {
            if (p->wii.kind != TANG_WII_KIND_NONE) {
                memset(&p->wii, 0, sizeof p->wii);
            }
            continue;
        }
        tang_btpad_input_t in;
        taskENTER_CRITICAL();
        in = p->in;
        taskEXIT_CRITICAL();
        tang_wii_sample_t s;
        tang_btpad_wii_sample(&in, &s);
        if (p->wii.kind == TANG_WII_KIND_NONE) {
            memset(&p->wii, 0, sizeof p->wii);
            p->wii.kind = TANG_WII_KIND_CLASSIC;
            p->wii.state = TANG_WIIPAD_READY;
            p->wii.btn_prev = s.btn;  /* held already: not a press */
            p->next_wheel_ms = now;
        }
        tang_wii_apply(&p->wii, &s, m, console_mode, now, k);
        tang_btpad_sticks(&in, console_mode, now, &p->next_wheel_ms, m);
    }
}

/* -------------------------------------------------------- the channels */

static btpad_t *by_chan(struct bt_l2cap_chan *chan, bool *intr)
{
    for (int i = 0; i < TANG_BTPAD_SLOTS; i++) {
        if (chan == &s_pad[i].ctrl.chan) {
            *intr = false;
            return &s_pad[i];
        }
        if (chan == &s_pad[i].intr.chan) {
            *intr = true;
            return &s_pad[i];
        }
    }
    return NULL;
}

static void chan_connected(struct bt_l2cap_chan *chan)
{
    bool intr;
    btpad_t *p = by_chan(chan, &intr);
    if (p == NULL) {
        return;
    }
    if (intr) {
        p->intr_up = true;
    } else {
        p->ctrl_up = true;
    }
    key_capture(p);
    note("pad %d: %s channel open", slot_no(p), intr ? "interrupt" : "control");
    tang_ble_btpad_signal();
}

static void chan_disconnected(struct bt_l2cap_chan *chan)
{
    bool intr;
    btpad_t *p = by_chan(chan, &intr);
    if (p == NULL) {
        return;
    }
    if (intr) {
        p->intr_up = false;
        p->have_in = false;
        p->word = 0;                  /* nothing stays held */
    } else {
        p->ctrl_up = false;
    }
    note("pad %d: %s channel closed", slot_no(p), intr ? "interrupt" : "control");
}

static int chan_recv(struct bt_l2cap_chan *chan, struct net_buf *buf)
{
    bool intr;
    btpad_t *p = by_chan(chan, &intr);
    if (p == NULL || !intr) {
        return 0;                     /* control: handshakes, nothing to read */
    }
    p->reports++;
    const uint8_t n = (uint8_t)(buf->len < BTPAD_RAW_MAX ? buf->len : BTPAD_RAW_MAX);
    if (n != p->raw_len || memcmp(p->raw, buf->data, n) != 0) {
        taskENTER_CRITICAL();
        memcpy(p->raw, buf->data, n);
        p->raw_len = n;
        p->raw_seq++;
        taskEXIT_CRITICAL();
    }
    tang_btpad_input_t in;
    if (tang_btpad_switch_decode(buf->data, buf->len, &in)) {
        taskENTER_CRITICAL();
        p->in = in;
        p->have_in = true;
        taskEXIT_CRITICAL();
        p->word = tang_btpad_word(&in);
    }
    return 0;
}

static const struct bt_l2cap_chan_ops s_chan_ops = {
    .connected = chan_connected,
    .disconnected = chan_disconnected,
    .recv = chan_recv,
};

/* A channel the stack does not hold, made ready to connect or accept. */
static void chan_reset(struct bt_l2cap_br_chan *c)
{
    memset(c, 0, sizeof *c);
    c->chan.ops = &s_chan_ops;
    c->chan.required_sec_level = BT_SECURITY_L2;
    c->rx.mtu = BTPAD_MTU;
}

static int accept(struct bt_conn *conn, struct bt_l2cap_chan **chan, bool intr)
{
    btpad_t *p = by_conn(conn);
    if (p == NULL) {
        return -EACCES;
    }
    if (intr ? p->intr_up : p->ctrl_up) {
        return -EBUSY;
    }
    struct bt_l2cap_br_chan *c = intr ? &p->intr : &p->ctrl;
    chan_reset(c);
    *chan = &c->chan;
    return 0;
}

static int accept_ctrl(struct bt_conn *conn, struct bt_l2cap_chan **chan)
{
    return accept(conn, chan, false);
}

static int accept_intr(struct bt_conn *conn, struct bt_l2cap_chan **chan)
{
    return accept(conn, chan, true);
}

static struct bt_l2cap_server s_srv_ctrl = {
    .psm = BTPAD_PSM_CTRL, .sec_level = BT_SECURITY_L2, .accept = accept_ctrl,
};
static struct bt_l2cap_server s_srv_intr = {
    .psm = BTPAD_PSM_INTR, .sec_level = BT_SECURITY_L2, .accept = accept_intr,
};

/* ------------------------------------------------- the connection callbacks */

bool tang_btpad_is_br(struct bt_conn *conn)
{
    struct bt_conn_info ci;
    return bt_conn_get_info(conn, &ci) == 0 && ci.type == BT_CONN_TYPE_BR;
}

void tang_btpad_connected(struct bt_conn *conn, u8_t err)
{
    btpad_t *p = by_conn(conn);
    if (err) {
        if (p != NULL) {
            note("pad %d: connect failed (HCI 0x%02X)", slot_no(p), err);
            /* The stack dropped its own reference; the one
             * bt_conn_create_br() returned is ours here (see the top). */
            bt_conn_unref(conn);
            p->conn = NULL;
            p->pairing = false;
        }
        return;
    }
    if (p == NULL) {
        uint8_t a[6];
        p = conn_addr(conn, a) ? by_addr(a) : NULL;
        if (p == NULL || p->conn != NULL) {
            /* Not a pad of ours, or a second link to one: let it go. */
            char t[18] = "?";
            if (conn_addr(conn, a)) {
                addr_text(a, t);
            }
            note("%s connected but is not a saved pad; disconnecting", t);
            (void)bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
            return;
        }
        p->conn = bt_conn_ref(conn);
        p->outbound = false;
        note("pad %d: the pad connected", slot_no(p));
    }
    p->ctrl_asked = p->intr_asked = false;
    p->linked_ms = bflb_mtimer_get_time_ms();
    p->linked = true;
    tang_ble_btpad_signal();
}

void tang_btpad_disconnected(struct bt_conn *conn, u8_t reason)
{
    btpad_t *p = by_conn(conn);
    if (p == NULL) {
        return;
    }
    note("pad %d: link down (reason 0x%02X), %lu report(s)", slot_no(p), reason,
         (unsigned long)p->reports);
    p->linked = false;
    p->ctrl_up = p->intr_up = false;
    p->have_in = false;
    p->word = 0;
    p->conn = NULL;
    p->pairing = false;
    p->key_due = false;
    p->pin_due = false;
    if (!p->outbound) {
        bt_conn_unref(conn);          /* ours; an outbound one is the stack's */
    }
}

/* Keep the key a pairing produced.  Not in the pairing callback itself: the
 * controller reports Simple Pairing Complete before Link Key Notification, so
 * the key is not in the host's table yet when the stack says the pairing is
 * done (the first pairing on the board saved nothing for that reason).  It
 * is there once encryption is up, which is where this runs, and again when
 * a channel opens in case that comes first. */
static void key_capture(btpad_t *p)
{
    if (!p->key_due || p->conn == NULL) {
        return;
    }
    uint8_t a[6];
    struct bt_keys_link_key *k = NULL;
    if (conn_addr(p->conn, a)) {
        bt_addr_t addr;
        memcpy(addr.val, a, 6);
        k = bt_keys_find_link_key(&addr);
    }
    if (k == NULL) {
        return;                       /* not yet; a later event tries again */
    }
    p->key_due = false;
    /* A pad that paired by PIN is of the Wii family; a Switch-mode pad uses
     * Secure Simple Pairing.  Its name is not a test: a Wii pad may send none
     * in an inquiry. */
    tang_btpad_key_t s = { .valid = true,
                           .kind = p->used_pin ? TANG_BTPAD_KIND_WII : TANG_BTPAD_KIND_SWITCH,
                           .flags = k->flags };
    memcpy(s.addr, a, 6);
    memcpy(s.key, k->val, 16);
    if (p->pairing && p->want_name[0]) {
        memcpy(s.name, p->want_name, sizeof s.name);
    } else {
        memcpy(s.name, p->saved.name, sizeof s.name);
    }
    taskENTER_CRITICAL();
    p->saved = s;
    taskEXIT_CRITICAL();
    p->pairing = false;
    s_save_due = true;
    note("pad %d: paired; saving the key to the card", slot_no(p));
    tang_ble_btpad_signal();
}

void tang_btpad_security(struct bt_conn *conn, bt_security_t level, enum bt_security_err err)
{
    btpad_t *p = by_conn(conn);
    if (p == NULL) {
        return;
    }
    if (err != 0) {
        note("pad %d: security failed (%d)", slot_no(p), (int)err);
        return;
    }
    (void)level;
    key_capture(p);
}

void tang_btpad_paired(struct bt_conn *conn, bool bonded, int failed)
{
    btpad_t *p = by_conn(conn);
    if (p == NULL) {
        return;
    }
    if (failed) {
        note("pad %d: pairing failed (%d)", slot_no(p), failed);
        return;
    }
    if (!bonded) {
        note("pad %d: paired without bonding; nothing to keep", slot_no(p));
        return;
    }
    p->key_due = true;
    key_capture(p);
}

/* ------------------------------------------------------- the card and boot */

/* One file object and buffer for both: loading and saving both run in
 * tang_ble.c's background task, never at once. */
static FIL s_file;
static uint8_t s_file_buf[TANG_BTPAD_FILE_LEN];

static void keys_save(void)
{
    FIL *const fp = &s_file;
    uint8_t *const buf = s_file_buf;
    tang_btpad_key_t k[TANG_BTPAD_SLOTS];
    taskENTER_CRITICAL();
    for (int i = 0; i < TANG_BTPAD_SLOTS; i++) {
        k[i] = s_pad[i].saved;
    }
    taskEXIT_CRITICAL();
    const size_t n = tang_btpad_keys_encode(k, buf);
    (void)f_mkdir(BTPAD_DIR);                  /* FR_EXIST is fine */
    UINT put = 0;
    FRESULT r = f_open(fp, BTPAD_PATH, FA_CREATE_ALWAYS | FA_WRITE);
    if (r == FR_OK) {
        r = f_write(fp, buf, (UINT)n, &put);
        const FRESULT c = f_close(fp);
        if (r == FR_OK) {
            r = c;
        }
    }
    s_file_state = (r == FR_OK && put == n) ? "saved to the card" : "save to the card failed";
}

bool tang_btpad_load(void)
{
    FIL *const fp = &s_file;
    uint8_t *const buf = s_file_buf;
    tang_btpad_key_t k[TANG_BTPAD_SLOTS];
    UINT got = 0;
    if (f_open(fp, BTPAD_PATH, FA_READ) != FR_OK) {
        s_file_state = "none on the card";
        return false;
    }
    const FRESULT r = f_read(fp, buf, TANG_BTPAD_FILE_LEN, &got);
    (void)f_close(fp);
    if (r != FR_OK || tang_btpad_keys_decode(buf, got, k) != 0) {
        s_file_state = "unreadable; ignored";
        return false;
    }
    bool any = false;
    for (int i = 0; i < TANG_BTPAD_SLOTS; i++) {
        s_pad[i].saved = k[i];
        any = any || k[i].valid;
    }
    s_file_state = "loaded from the card";
    return any;
}

/* A saved pad's key, put into the host's table as if it had just paired. */
static void key_restore(const btpad_t *p)
{
    if (!p->saved.valid) {
        return;
    }
    bt_addr_t addr;
    memcpy(addr.val, p->saved.addr, 6);
    struct bt_keys_link_key *k = bt_keys_get_link_key(&addr);
    if (k != NULL) {
        k->flags = p->saved.flags;
        memcpy(k->val, p->saved.key, 16);
    }
}

void tang_btpad_radio_up(void)
{
    for (int i = 0; i < TANG_BTPAD_SLOTS; i++) {
        key_restore(&s_pad[i]);
    }
    if (!s_servers) {
        const int a = bt_l2cap_br_server_register(&s_srv_ctrl);
        const int b = bt_l2cap_br_server_register(&s_srv_intr);
        if (a != 0 || b != 0) {
            note("HID PSM servers not registered (%d, %d)", a, b);
        }
        s_servers = true;
    }
    /* Page scan: a woken pad connects to the board itself. */
    (void)bt_br_set_connectable(true);
}

/* ------------------------------------------------------ the background task */

static bool waiting(const btpad_t *p)
{
    return p->linked && p->conn != NULL &&
           (p->pin_due ||
            (!p->ctrl_up && !p->ctrl_asked) || (p->ctrl_up && !p->intr_up && !p->intr_asked));
}

void tang_btpad_pincode(struct bt_conn *conn, bool highsec)
{
    (void)highsec;
    btpad_t *p = by_conn(conn);
    if (p == NULL) {
        return;                       /* not ours: the request times out */
    }
    note("pad %d: asked for a PIN (legacy pairing)", slot_no(p));
    p->pin_due = true;
    p->used_pin = true;
    /* A key worth keeping is coming, and this is the only notice of it: a
     * legacy pairing never reaches tang_btpad_paired(), because the stack
     * calls pairing_complete only from its SSP path (bt_conn_ssp_auth_complete
     * in ssp_complete) while PIN pairing ends in the AUTH_COMPLETE event, whose
     * handler only calls link_encr().  Without this the key is never taken, the
     * slot never leaves its pairing state, and `btpad pair` reports a pad that
     * actually connected as one that did not finish pairing. */
    p->key_due = true;
    tang_ble_btpad_signal();
}

/* The Wii PIN for pairing from the pad's sync button: this host's own
 * address, lowest byte first, as six binary bytes (BLE-022, the btmon
 * capture of BlueZ answering it).  The SDK's bt_conn_auth_pincode_entry()
 * measures the PIN with strlen, so it is used only when no byte is 0x00;
 * otherwise the HCI PIN Code Request Reply is sent here with its length. */
static void pin_reply(btpad_t *p)
{
    bt_addr_le_t own;
    uint8_t a[6];
    if (bt_get_local_public_address(&own) != 0 || !conn_addr(p->conn, a)) {
        note("pad %d: no address to answer the PIN with", slot_no(p));
        return;
    }
    const uint8_t *pin = own.a.val;
    int rc;
    if (memchr(pin, 0, 6) == NULL) {
        char text[7];
        memcpy(text, pin, 6);
        text[6] = '\0';
        rc = bt_conn_auth_pincode_entry(p->conn, text);
    } else {
        struct net_buf *buf = bt_hci_cmd_create(BTPAD_HCI_PIN_CODE_REPLY, 23);
        if (buf == NULL) {
            note("pad %d: no buffer for the PIN reply", slot_no(p));
            return;
        }
        uint8_t cp[23] = { 0 };
        memcpy(cp, a, 6);             /* the pad's address */
        cp[6] = 6;                    /* PIN length */
        memcpy(cp + 7, pin, 6);
        net_buf_add_mem(buf, cp, sizeof cp);
        rc = bt_hci_cmd_send_sync(BTPAD_HCI_PIN_CODE_REPLY, buf, NULL);
    }
    note("pad %d: answered the PIN with this board's address (%d)", slot_no(p), rc);
}

bool tang_btpad_busy(void)
{
    for (int i = 0; i < TANG_BTPAD_SLOTS; i++) {
        if (waiting(&s_pad[i])) {
            return true;
        }
    }
    return false;
}

void tang_btpad_work(void)
{
    if (s_save_due) {
        s_save_due = false;
        keys_save();
    }
    const uint64_t now = bflb_mtimer_get_time_ms();
    for (int i = 0; i < TANG_BTPAD_SLOTS; i++) {
        btpad_t *p = &s_pad[i];
        if (!waiting(p)) {
            continue;
        }
        if (p->pin_due) {
            p->pin_due = false;
            pin_reply(p);
            continue;
        }
        /* On a link the pad made, a Switch-mode pad opens the channels itself;
         * step in only if it has not after BTPAD_PAD_OPENS_MS (BLE-021: opening
         * them at the same moment collided with the pad's own connect and took
         * the link down with 0x24).  A Wii-family pad does not: the Wii U Pro
         * connects, opens nothing and drops the link about a second later with
         * reason 0x13, eight times in fifteen seconds on the board with no
         * channel ever opened, so the host must open both channels at once
         * (core-log entry 60). */
        const bool pad_opens = !p->outbound && p->saved.kind != TANG_BTPAD_KIND_WII;
        if (pad_opens && now - p->linked_ms < BTPAD_PAD_OPENS_MS) {
            continue;
        }
        if (!p->ctrl_up && !p->ctrl_asked) {
            p->ctrl_asked = true;
            chan_reset(&p->ctrl);
            const int rc = bt_l2cap_chan_connect(p->conn, &p->ctrl.chan, BTPAD_PSM_CTRL);
            if (rc != 0) {
                note("pad %d: control channel not opened (%d)", slot_no(p), rc);
            }
        } else if (p->ctrl_up && !p->intr_up && !p->intr_asked) {
            p->intr_asked = true;
            chan_reset(&p->intr);
            const int rc = bt_l2cap_chan_connect(p->conn, &p->intr.chan, BTPAD_PSM_INTR);
            if (rc != 0) {
                note("pad %d: interrupt channel not opened (%d)", slot_no(p), rc);
            }
        }
    }
}

/* ------------------------------------------------------------- the inquiry */

static struct bt_br_discovery_result *s_found;   /* BTPAD_SCAN_MAX, per inquiry */
static volatile int s_found_n = -1;               /* -1 while it runs */

static void inquiry_done(struct bt_br_discovery_result *results, size_t count)
{
    (void)results;
    s_found_n = (int)count;
}

/* The device's name from its Extended Inquiry Response: complete (0x09) or
 * shortened (0x08). */
static void eir_name(const uint8_t *eir, size_t len, char *out, size_t cap)
{
    out[0] = '\0';
    for (size_t i = 0; i + 1 < len && eir[i] != 0;) {
        const size_t field = eir[i];
        if (i + 1 + field > len) {
            break;
        }
        const uint8_t type = eir[i + 1];
        if (type == 0x08 || type == 0x09) {
            size_t n = field - 1;
            if (n >= cap) {
                n = cap - 1;
            }
            memcpy(out, &eir[i + 2], n);
            out[n] = '\0';
            if (type == 0x09) {
                return;
            }
        }
        i += 1 + field;
    }
}

/* The Class of Device's major class, and for a peripheral its kind. */
static const char *cod_kind(const uint8_t cod[3])
{
    const unsigned major = cod[1] & 0x1F;
    const unsigned minor = (cod[0] >> 2) & 0x3F;
    switch (major) {
    case 0x01: return "computer";
    case 0x02: return "phone";
    case 0x04: return "audio";
    case 0x05:
        switch (minor & 0x0F) {
        case 0x01: return "joystick";
        case 0x02: return "gamepad";
        case 0x03: return "remote";
        default: break;
        }
        switch (minor >> 4) {
        case 0x01: return "keyboard";
        case 0x02: return "mouse";
        case 0x03: return "kbd+mouse";
        default: return "peripheral";
        }
    default: return "-";
    }
}

static bool cod_is_pad(const uint8_t cod[3])
{
    const unsigned minor = (cod[0] >> 2) & 0x0F;
    return (cod[1] & 0x1F) == 0x05 && (minor == 0x01 || minor == 0x02);
}

/* A Classic inquiry of `secs` seconds into s_found.  The count, or -1. */
static int inquiry(int secs)
{
    if (s_found == NULL) {
        s_found = calloc(BTPAD_SCAN_MAX, sizeof *s_found);
        if (s_found == NULL) {
            tdsh_printf("btpad: not enough memory for an inquiry\r\n");
            return -1;
        }
    }
    s_found_n = -1;
    const struct bt_br_discovery_param param = {
        .length = (uint8_t)((secs * 100 + 127) / 128),   /* units of 1.28 s */
        .limited = false,
    };
    const int err = bt_br_discovery_start(&param, s_found, BTPAD_SCAN_MAX, inquiry_done);
    if (err != 0) {
        tdsh_printf("btpad: inquiry did not start (%d)\r\n", err);
        return -1;
    }
    const uint64_t deadline = bflb_mtimer_get_time_ms() + (uint64_t)secs * 1000u + 5000u;
    while (s_found_n < 0 && bflb_mtimer_get_time_ms() < deadline) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (s_found_n < 0) {
        (void)bt_br_discovery_stop();
        tdsh_printf("btpad: the inquiry never finished\r\n");
        return -1;
    }
    return s_found_n < BTPAD_SCAN_MAX ? s_found_n : BTPAD_SCAN_MAX;
}

static void inquiry_free(void)
{
    free(s_found);
    s_found = NULL;
}

int tang_btscan_cmd(tdsh_session_t *session, int argc, char **argv)
{
    (void)session;
    int secs = 10;
    if (argc >= 2) {
        secs = atoi(argv[1]);
        if (secs < 2 || secs > 60) {
            tdsh_printf("usage: btscan [seconds 2-60]\r\n");
            return 1;
        }
    }
    if (!tang_ble_claim()) {
        tdsh_printf("btscan: Bluetooth is busy; try again\r\n");
        return 1;
    }
    if (tang_ble_radio_start(true) != 0) {
        tang_ble_release();
        return 1;
    }
    tdsh_printf("btscan: Classic inquiry for about %d s...\r\n", secs);
    const int n = inquiry(secs);
    tang_ble_release();
    if (n < 0) {
        inquiry_free();
        return 1;
    }
    tdsh_printf("btscan: %d device(s)\r\n", n);
    if (n > 0) {
        tdsh_printf("  address            rssi  class     kind        name\r\n");
    }
    for (int i = 0; i < n; i++) {
        const struct bt_br_discovery_result *r = &s_found[i];
        char name[40], t[18];
        eir_name(r->eir, sizeof r->eir, name, sizeof name);
        addr_text(r->addr.val, t);
        tdsh_printf("  %s  %4d  %02x%02x%02x  %-10s  %s\r\n", t, r->rssi,
                    r->cod[2], r->cod[1], r->cod[0], cod_kind(r->cod), name[0] ? name : "-");
    }
    inquiry_free();
    return 0;
}

/* -------------------------------------------------------------- the commands */

static bool parse_addr(const char *text, uint8_t out[6])
{
    unsigned b[6];
    char tail;
    if (sscanf(text, "%2x:%2x:%2x:%2x:%2x:%2x%c", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5],
               &tail) != 6) {
        return false;
    }
    for (int i = 0; i < 6; i++) {
        out[5 - i] = (uint8_t)b[i];
    }
    return true;
}

/* Make a link to the pad at `a` for slot `p`: a pairing when it has no saved
 * key (or `fresh` asks for a new one), a reconnection otherwise. */
static int link_to(btpad_t *p, const uint8_t a[6], const char *name, bool fresh)
{
    bt_addr_t addr;
    memcpy(addr.val, a, 6);
    struct bt_conn *existing = bt_conn_lookup_addr_br(&addr);
    if (existing != NULL) {
        bt_conn_unref(existing);
        tdsh_printf("btpad: already linked to it\r\n");
        return -1;
    }
    const bool pairing = fresh || !p->saved.valid || memcmp(p->saved.addr, a, 6) != 0;
    if (pairing) {
        bt_keys_link_key_clear_addr(&addr);    /* pair afresh, not on an old key */
        memcpy(p->want_addr, a, 6);
        memset(p->want_name, 0, sizeof p->want_name);
        if (name != NULL) {
            strncpy(p->want_name, name, sizeof p->want_name - 1);
        }
    }
    p->pairing = pairing;
    p->key_due = false;
    p->pin_due = false;
    p->used_pin = false;
    p->reports = 0;
    p->linked = false;
    p->outbound = true;
    /* The link's callbacks find the slot by this pointer, and the connection
     * cannot complete before the controller has paged the pad, so it is set
     * long before they run. */
    struct bt_conn *conn = bt_conn_create_br(&addr, BT_BR_CONN_PARAM_DEFAULT);
    if (conn == NULL) {
        p->pairing = false;
        tdsh_printf("btpad: the connection was not started\r\n");
        return -1;
    }
    p->conn = conn;
    return 0;
}

/* Print notes until the slot's interrupt channel is open, it fails, or the
 * time runs out.  0 when the pad is reporting. */
static int link_wait(btpad_t *p, unsigned *seen, int secs)
{
    const uint64_t end = bflb_mtimer_get_time_ms() + (uint64_t)secs * 1000u;
    while (bflb_mtimer_get_time_ms() < end) {
        notes_print(seen);
        if (p->intr_up && !p->pairing) {
            return 0;                 /* reporting, and a pairing's key is kept */
        }
        if (p->conn == NULL) {
            break;                    /* the connect failed or the link dropped */
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    notes_print(seen);
    return -1;
}

static btpad_t *free_slot(void)
{
    for (int i = 0; i < TANG_BTPAD_SLOTS; i++) {
        if (!s_pad[i].saved.valid && s_pad[i].conn == NULL) {
            return &s_pad[i];
        }
    }
    return NULL;
}

static int cmd_pair(int secs, const uint8_t *given)
{
    if (!tang_ble_claim()) {
        tdsh_printf("btpad: Bluetooth is busy; try again\r\n");
        return 1;
    }
    if (tang_ble_radio_start(true) != 0) {
        tang_ble_release();
        return 1;
    }
    uint8_t a[6];
    char name[TANG_BTPAD_NAME_LEN] = "";
    if (given != NULL) {
        memcpy(a, given, 6);
    } else {
        tdsh_printf("btpad: looking for a gamepad in pairing mode for about %d s...\r\n", secs);
        const int n = inquiry(secs);
        int best = -1;
        for (int i = 0; i < n; i++) {
            if (cod_is_pad(s_found[i].cod) && (best < 0 || s_found[i].rssi > s_found[best].rssi)) {
                best = i;
            }
        }
        if (best < 0) {
            inquiry_free();
            tang_ble_release();
            tdsh_printf("btpad: no gamepad in pairing mode was found\r\n");
            return 1;
        }
        memcpy(a, s_found[best].addr.val, 6);
        eir_name(s_found[best].eir, sizeof s_found[best].eir, name, sizeof name);
        inquiry_free();
    }
    char t[18];
    addr_text(a, t);
    btpad_t *p = by_addr(a);
    if (p == NULL) {
        p = free_slot();
    }
    if (p == NULL) {
        tang_ble_release();
        tdsh_printf("btpad: both pad slots are taken; `btpad forget 1` or `2` first\r\n");
        return 1;
    }
    unsigned seen = s_note_head;
    tdsh_printf("btpad: pairing %s (%s) as pad %d\r\n", t, name[0] ? name : "no name", slot_no(p));
    const int rc = link_to(p, a, name, true);
    tang_ble_release();
    if (rc != 0) {
        return 1;
    }
    if (link_wait(p, &seen, 40) != 0) {
        tdsh_printf("btpad: pad %d did not finish pairing\r\n", slot_no(p));
        return 1;
    }
    tdsh_printf("btpad: pad %d is paired and reporting; it reconnects by itself when woken\r\n",
                slot_no(p));
    return 0;
}

static void cmd_status(void)
{
    tdsh_printf("btpad: pairings %s (%s)\r\n", s_file_state, BTPAD_PATH);
    for (int i = 0; i < TANG_BTPAD_SLOTS; i++) {
        const btpad_t *p = &s_pad[i];
        if (!p->saved.valid && p->conn == NULL) {
            tdsh_printf("  pad %d: empty\r\n", i + 1);
            continue;
        }
        char t[18] = "-";
        if (p->saved.valid) {
            addr_text(p->saved.addr, t);
        }
        const char *state = p->intr_up ? "reporting" :
                            p->linked ? "linked, opening channels" :
                            p->conn ? "connecting" : "waiting for the pad";
        /* The kind decides how a woken pad is handled, so it is worth seeing:
         * a Switch pad opens its own channels and a Wii pad does not. */
        const char *kind = !p->saved.valid ? "-" :
                           p->saved.kind == TANG_BTPAD_KIND_WII ? "wii" : "switch";
        tdsh_printf("  pad %d: %s %s, %s, %s, %lu report(s), word 0x%03x\r\n", i + 1, t,
                    p->saved.name[0] ? p->saved.name : "(no name)", kind, state,
                    (unsigned long)p->reports, (unsigned)tang_btpad_pad_word(i));
    }
    unsigned seen = s_note_head > BTPAD_NOTES ? s_note_head - BTPAD_NOTES : 0;
    notes_print(&seen);
}

static void cmd_watch(int secs)
{
    tdsh_printf("btpad: watching for %d s (changed reports, raw)\r\n", secs);
    uint32_t seq[TANG_BTPAD_SLOTS];
    for (int i = 0; i < TANG_BTPAD_SLOTS; i++) {
        seq[i] = s_pad[i].raw_seq;
    }
    unsigned seen = s_note_head;
    const uint64_t end = bflb_mtimer_get_time_ms() + (uint64_t)secs * 1000u;
    while (bflb_mtimer_get_time_ms() < end) {
        notes_print(&seen);
        for (int i = 0; i < TANG_BTPAD_SLOTS; i++) {
            btpad_t *p = &s_pad[i];
            if (p->raw_seq == seq[i]) {
                continue;
            }
            uint8_t raw[BTPAD_RAW_MAX];
            uint8_t n;
            taskENTER_CRITICAL();
            seq[i] = p->raw_seq;
            n = p->raw_len;
            memcpy(raw, p->raw, n);
            taskEXIT_CRITICAL();
            /* The whole report, twelve bytes a row: a 0x34 extension report is
             * 23 bytes and its buttons are at the end, so the old 16-byte cut
             * hid exactly what this command exists to show. */
            /* Twelve bytes plus their spaces and the terminator. */
            char row[3 * 12 + 1];
            const int indent = 25;    /* under the first row's hex column */
            for (uint8_t k = 0; k < n; k += 12) {
                const uint8_t m = (uint8_t)(n - k < 12 ? n - k : 12);
                size_t r = 0;
                for (uint8_t j = 0; j < m; j++) {
                    r += (size_t)snprintf(row + r, sizeof row - r, "%02x ", raw[k + j]);
                }
                if (k == 0) {
                    tdsh_printf("btpad: pad %d word 0x%03x: %s\r\n", i + 1,
                                (unsigned)tang_btpad_pad_word(i), row);
                } else {
                    tdsh_printf("%*s%s\r\n", indent, "", row);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    notes_print(&seen);
}

static void cmd_forget(btpad_t *p)
{
    if (p->conn != NULL) {
        (void)bt_conn_disconnect(p->conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    }
    if (p->saved.valid) {
        bt_addr_t addr;
        memcpy(addr.val, p->saved.addr, 6);
        bt_keys_link_key_clear_addr(&addr);
    }
    taskENTER_CRITICAL();
    memset(&p->saved, 0, sizeof p->saved);
    taskEXIT_CRITICAL();
    p->pairing = false;
    s_save_due = true;
    tang_ble_btpad_signal();
    tdsh_printf("btpad: pad %d forgotten\r\n", slot_no(p));
}

int tang_btpad_cmd(tdsh_session_t *session, int argc, char **argv)
{
    (void)session;
    const char *op = argc >= 2 ? argv[1] : "status";
    if (strcmp(op, "status") == 0) {
        cmd_status();
        return 0;
    }
    if (strcmp(op, "watch") == 0) {
        const int secs = argc >= 3 ? atoi(argv[2]) : 20;
        if (secs < 1 || secs > 600) {
            tdsh_printf("btpad: seconds are 1 to 600\r\n");
            return 1;
        }
        cmd_watch(secs);
        return 0;
    }
    if (strcmp(op, "pair") == 0) {
        const int secs = argc >= 3 ? atoi(argv[2]) : 10;
        if (secs < 2 || secs > 60) {
            tdsh_printf("btpad: seconds are 2 to 60\r\n");
            return 1;
        }
        return cmd_pair(secs, NULL);
    }
    if (strcmp(op, "send") == 0) {
        /* Bytes as given, on pad n's interrupt channel: a HID output report
         * starts with 0xa2 (a Wii Remote's 0x12 sets the report mode, 0x16
         * writes its memory).  For working a pad's protocol out by hand. */
        if (argc < 4) {
            tdsh_printf("usage: btpad send 1|2 <hex bytes>\r\n");
            return 1;
        }
        const int n = atoi(argv[2]);
        if (n < 1 || n > TANG_BTPAD_SLOTS || !s_pad[n - 1].intr_up) {
            tdsh_printf("btpad: pad %s has no open interrupt channel\r\n", argv[2]);
            return 1;
        }
        uint8_t data[32];
        int len = 0;
        for (int i = 3; i < argc && len < (int)sizeof data; i++) {
            char *end;
            const unsigned long v = strtoul(argv[i], &end, 16);
            if (*end != '\0' || v > 0xFF) {
                tdsh_printf("btpad: '%s' is not a hex byte\r\n", argv[i]);
                return 1;
            }
            data[len++] = (uint8_t)v;
        }
        struct net_buf *buf = bt_l2cap_create_pdu_timeout(NULL, 0, K_SECONDS(1));
        if (buf == NULL) {
            tdsh_printf("btpad: no buffer to send with\r\n");
            return 1;
        }
        net_buf_add_mem(buf, data, (size_t)len);
        const int rc = bt_l2cap_chan_send(&s_pad[n - 1].intr.chan, buf);
        if (rc < 0) {
            net_buf_unref(buf);
        }
        tdsh_printf("btpad: sent %d byte(s) to pad %d (%d)\r\n", len, n, rc);
        cmd_watch(2);
        return 0;
    }
    if (strcmp(op, "forget") == 0) {
        if (argc < 3) {
            tdsh_printf("usage: btpad forget 1 | 2 | all\r\n");
            return 1;
        }
        if (strcmp(argv[2], "all") == 0) {
            for (int i = 0; i < TANG_BTPAD_SLOTS; i++) {
                cmd_forget(&s_pad[i]);
            }
            return 0;
        }
        const int n = atoi(argv[2]);
        if (n < 1 || n > TANG_BTPAD_SLOTS) {
            tdsh_printf("btpad: pads are 1 and 2\r\n");
            return 1;
        }
        cmd_forget(&s_pad[n - 1]);
        return 0;
    }
    uint8_t a[6];
    if (parse_addr(op, a)) {
        /* A saved pad: reconnect to it.  Another address: pair with it. */
        btpad_t *p = by_addr(a);
        if (p != NULL && p->saved.valid) {
            if (!tang_ble_claim()) {
                tdsh_printf("btpad: Bluetooth is busy; try again\r\n");
                return 1;
            }
            if (tang_ble_radio_start(true) != 0) {
                tang_ble_release();
                return 1;
            }
            unsigned seen = s_note_head;
            const int rc = link_to(p, a, NULL, false);
            tang_ble_release();
            if (rc != 0) {
                return 1;
            }
            tdsh_printf("btpad: connecting to pad %d\r\n", slot_no(p));
            if (link_wait(p, &seen, 20) != 0) {
                tdsh_printf("btpad: pad %d did not come up (is it on?)\r\n", slot_no(p));
                return 1;
            }
            tdsh_printf("btpad: pad %d is reporting\r\n", slot_no(p));
            return 0;
        }
        return cmd_pair(0, a);
    }
    tdsh_printf("usage: btpad [status] | pair [secs] | <addr> | watch [secs] | send 1|2 <hex>"
                " | forget 1|2|all\r\n");
    return 1;
}

#endif /* CONFIG_BT_BREDR */
