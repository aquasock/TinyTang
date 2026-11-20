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

#include <string.h>

/* The Bluetooth window shows the pad slots as missing from this build. */
static const char s_no_classic[] = "this firmware has no Classic Bluetooth (LE-only build)";

void tang_btpad_info(tang_btpad_info_t *out)
{
    memset(out, 0, sizeof *out);
    out->pairings = "not in this build";
    for (int i = 0; i < TANG_BTPAD_PADS; i++) {
        out->pad[i].state = "not in this build";
    }
}

const char *tang_btpad_scan_start(void)
{
    return s_no_classic;
}

int tang_btpad_scan_results(tang_btpad_found_t *out, int max)
{
    (void)out;
    (void)max;
    return 0;
}

const char *tang_btpad_pair_start(int pad, const uint8_t addr[6], const char *name)
{
    (void)pad;
    (void)addr;
    (void)name;
    return s_no_classic;
}

const char *tang_btpad_set_on(int pad, bool on)
{
    (void)pad;
    (void)on;
    return s_no_classic;
}

const char *tang_btpad_forget(int pad)
{
    (void)pad;
    return s_no_classic;
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
        if (p->saved.valid && p->saved.off && !p->pairing) {
            /* Turned off in the Bluetooth window or with `btpad off`: the
             * pairing is kept, its reconnections are not taken. */
            note("pad %d: is turned off; its connection was refused", slot_no(p));
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
        any = any || (k[i].valid && !k[i].off);   /* an off pad needs no radio */
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

static bool job_busy(void);
static void job_step(void);

bool tang_btpad_busy(void)
{
    if (job_busy()) {
        return true;
    }
    for (int i = 0; i < TANG_BTPAD_SLOTS; i++) {
        if (waiting(&s_pad[i])) {
            return true;
        }
    }
    return false;
}

void tang_btpad_work(void)
{
    job_step();
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

/* Start a Classic inquiry of `secs` seconds into s_found and return at once:
 * NULL, or why it did not start.  s_found_n stays -1 until it is done. */
static char s_inquiry_why[48];
static const char *inquiry_begin(int secs)
{
    if (s_found == NULL) {
        s_found = calloc(BTPAD_SCAN_MAX, sizeof *s_found);
        if (s_found == NULL) {
            return "not enough memory for an inquiry";
        }
    }
    s_found_n = -1;
    const struct bt_br_discovery_param param = {
        .length = (uint8_t)((secs * 100 + 127) / 128),   /* units of 1.28 s */
        .limited = false,
    };
    const int err = bt_br_discovery_start(&param, s_found, BTPAD_SCAN_MAX, inquiry_done);
    if (err != 0) {
        snprintf(s_inquiry_why, sizeof s_inquiry_why, "the inquiry did not start (%d)", err);
        return s_inquiry_why;
    }
    return NULL;
}

/* The inquiry's count once it is done, or -1 after stopping one that never
 * finished: called when s_found_n is set or its time is up. */
static int inquiry_end(void)
{
    if (s_found_n < 0) {
        (void)bt_br_discovery_stop();
        return -1;
    }
    return s_found_n < BTPAD_SCAN_MAX ? s_found_n : BTPAD_SCAN_MAX;
}

/* The shell's inquiry: start one and wait for it.  The count, or -1. */
static int inquiry(int secs)
{
    const char *why = inquiry_begin(secs);
    if (why != NULL) {
        tdsh_printf("btpad: %s\r\n", why);
        return -1;
    }
    const uint64_t deadline = bflb_mtimer_get_time_ms() + (uint64_t)secs * 1000u + 5000u;
    while (s_found_n < 0 && bflb_mtimer_get_time_ms() < deadline) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    const int n = inquiry_end();
    if (n < 0) {
        tdsh_printf("btpad: the inquiry never finished\r\n");
    }
    return n;
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
 * key (or `fresh` asks for a new one), a reconnection otherwise.  NULL, or why
 * it was not started. */
static const char *link_to(btpad_t *p, const uint8_t a[6], const char *name, bool fresh)
{
    bt_addr_t addr;
    memcpy(addr.val, a, 6);
    struct bt_conn *existing = bt_conn_lookup_addr_br(&addr);
    if (existing != NULL) {
        bt_conn_unref(existing);
        return "already linked to it";
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
        return "the connection was not started";
    }
    p->conn = conn;
    return NULL;
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

/* ------------------------------------------------------- the jobs
 *
 * The Bluetooth window's requests, and the shell's pair, on and off, run as
 * jobs on tang_ble.c's background task, as its LE jobs do: a request claims
 * Bluetooth and returns at once, and the task does the work in steps from
 * tang_btpad_work() without ever blocking, because the same task must keep
 * answering PIN requests and opening channels while a pairing comes up.  The
 * claim is held while the inquiry runs and the link is started, as the shell
 * command always held it, and given back before the pairing is waited for. */

#define BTPAD_JOB_SCAN_SECS  10
#define BTPAD_JOB_PAIR_SECS  40
#define BTPAD_JOB_DROP_MS    5000u     /* for a replaced pad's link to go */

_Static_assert(TANG_BTPAD_PADS == TANG_BTPAD_SLOTS, "tang_btpad.h and the codec disagree");

enum { PH_START, PH_SCAN, PH_DROP, PH_LINK };

static volatile tang_btpad_job_t s_job;
static volatile bool s_job_running;
static volatile bool s_job_failed;
static bool s_job_claimed;
static volatile int s_job_phase;
static int s_job_pad;                  /* -1: the shell's pairing picks the slot */
static int s_job_secs;
static uint8_t s_job_addr[6];
static char s_job_name[TANG_BTPAD_NAME_LEN];
static uint64_t s_job_until;
static volatile uint64_t s_scan_end;   /* while an inquiry runs */
static char s_job_msg[64];

/* The last inquiry's devices, strongest first; taken from the heap at the
 * first inquiry and kept, as the window reads them after the job. */
static tang_btpad_found_t *s_results;
static volatile int s_results_n;

static bool job_busy(void)
{
    return s_job_running;
}

static void job_say(const char *fmt, ...)
{
    char tmp[sizeof s_job_msg];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    taskENTER_CRITICAL();
    memcpy(s_job_msg, tmp, sizeof tmp);
    taskEXIT_CRITICAL();
}

static void job_end(bool failed)
{
    if (s_job_claimed) {
        s_job_claimed = false;
        tang_ble_release();
    }
    s_job_failed = failed;
    s_job_running = false;
}

static const char *job_submit(tang_btpad_job_t kind, int pad, const uint8_t *addr,
                              const char *name, int secs)
{
    if (!tang_ble_claim()) {
        return "Bluetooth is busy; try again when it has finished";
    }
    if (s_job_running) {
        /* A pairing waiting for its pad has given the claim back. */
        tang_ble_release();
        return "a gamepad pairing is still running";
    }
    s_job = kind;
    s_job_pad = pad;
    s_job_secs = secs;
    memset(s_job_addr, 0, sizeof s_job_addr);
    if (addr != NULL) {
        memcpy(s_job_addr, addr, 6);
    }
    memset(s_job_name, 0, sizeof s_job_name);
    if (name != NULL) {
        strncpy(s_job_name, name, sizeof s_job_name - 1);
    }
    s_job_claimed = true;
    s_job_failed = false;
    s_job_phase = PH_START;
    job_say("%s", "");
    s_job_running = true;
    tang_ble_btpad_signal();
    return NULL;
}

/* Delete a slot's pairing: its key in the host and on the card, and its
 * link unless `keep_link` (the caller drops it and waits for it to go). */
static void forget_slot(btpad_t *p, bool keep_link)
{
    if (p->conn != NULL && !keep_link) {
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
}

/* The inquiry is over (or its time is): keep its devices, strongest first. */
static void scan_finish(void)
{
    const int n = inquiry_end();
    if (n < 0) {
        inquiry_free();
        job_say("the inquiry never finished");
        job_end(true);
        return;
    }
    if (s_results == NULL) {
        s_results = calloc(TANG_BTPAD_FOUND_MAX, sizeof *s_results);
    }
    int kept = 0;
    for (int i = 0; s_results != NULL && i < n && kept < TANG_BTPAD_FOUND_MAX; i++) {
        const struct bt_br_discovery_result *r = &s_found[i];
        tang_btpad_found_t f;
        memset(&f, 0, sizeof f);
        memcpy(f.addr, r->addr.val, 6);
        memcpy(f.cod, r->cod, 3);
        f.rssi = r->rssi;
        f.pad = cod_is_pad(r->cod);
        f.peripheral = (r->cod[1] & 0x1F) == 0x05;
        eir_name(r->eir, sizeof r->eir, f.name, sizeof f.name);
        int at = kept++;
        while (at > 0 && s_results[at - 1].rssi < f.rssi) {
            s_results[at] = s_results[at - 1];
            at--;
        }
        s_results[at] = f;
    }
    inquiry_free();
    s_results_n = kept;
    int pads = 0;
    for (int i = 0; i < kept; i++) {
        pads += s_results[i].pad;
    }
    job_say("%d device(s) found, %d gamepad(s)", kept, pads);
    job_end(false);
}

/* Start the link of a pairing; the slot's old link, if any, is gone. */
static void pair_link(btpad_t *p)
{
    char t[18];
    addr_text(s_job_addr, t);
    note("pad %d: pairing %s (%s)", slot_no(p), t, s_job_name[0] ? s_job_name : "no name");
    const char *why = link_to(p, s_job_addr, s_job_name, true);
    if (why != NULL) {
        job_say("%s", why);
        job_end(true);
        return;
    }
    /* The claim covered the start; the wait does not hold it (as the shell
     * command never did), so the window can still be used. */
    s_job_claimed = false;
    tang_ble_release();
    s_job_pad = slot_no(p) - 1;
    s_job_until = bflb_mtimer_get_time_ms() + BTPAD_JOB_PAIR_SECS * 1000u;
    s_job_phase = PH_LINK;
    job_say("pairing %s as pad %d", s_job_name[0] ? s_job_name : t, slot_no(p));
}

static void pair_begin(void)
{
    btpad_t *other = by_addr(s_job_addr);
    btpad_t *p;
    if (s_job_pad < 0) {
        p = other != NULL ? other : free_slot();
        if (p == NULL) {
            job_say("both pad slots are taken; forget one first");
            job_end(true);
            return;
        }
    } else {
        p = &s_pad[s_job_pad];
        if (other != NULL && other != p) {
            job_say("that pad is saved as pad %d; forget it there first", slot_no(other));
            job_end(true);
            return;
        }
    }
    if (p->saved.valid && memcmp(p->saved.addr, s_job_addr, 6) != 0) {
        forget_slot(p, true);         /* the window asked to replace it */
    }
    p->saved.off = false;
    if (p->conn != NULL) {
        /* Its old link must be gone before the slot takes a new one: the
         * disconnected callback finds the slot by the link it holds. */
        (void)bt_conn_disconnect(p->conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        s_job_pad = slot_no(p) - 1;
        s_job_until = bflb_mtimer_get_time_ms() + BTPAD_JOB_DROP_MS;
        s_job_phase = PH_DROP;
        return;
    }
    pair_link(p);
}

static void job_begin(void)
{
    if (s_job != TANG_BTPAD_JOB_FORGET && s_job != TANG_BTPAD_JOB_OFF &&
        tang_ble_radio_start(false) != 0) {
        job_say("the radio did not start; see ble in the Terminal");
        job_end(true);
        return;
    }
    btpad_t *p = s_job_pad >= 0 ? &s_pad[s_job_pad] : NULL;
    switch (s_job) {
    case TANG_BTPAD_JOB_SCAN: {
        const char *why = inquiry_begin(s_job_secs);
        if (why != NULL) {
            job_say("%s", why);
            job_end(true);
            return;
        }
        s_scan_end = bflb_mtimer_get_time_ms() + (uint64_t)s_job_secs * 1000u;
        s_job_until = s_scan_end + 5000u;
        s_job_phase = PH_SCAN;
        job_say("looking for gamepads in pairing mode");
        return;
    }
    case TANG_BTPAD_JOB_PAIR:
        pair_begin();
        return;
    case TANG_BTPAD_JOB_ON:
    case TANG_BTPAD_JOB_OFF: {
        const bool on = s_job == TANG_BTPAD_JOB_ON;
        if (p == NULL || !p->saved.valid) {
            job_say("not paired");
            job_end(true);
            return;
        }
        taskENTER_CRITICAL();
        p->saved.off = !on;
        taskEXIT_CRITICAL();
        if (!on && p->conn != NULL) {
            (void)bt_conn_disconnect(p->conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        }
        s_save_due = true;
        note("pad %d: turned %s", slot_no(p), on ? "on" : "off");
        job_say(on ? "reconnects by itself when woken" : "not reconnecting; pairing kept");
        job_end(false);
        return;
    }
    case TANG_BTPAD_JOB_FORGET:
        if (p != NULL) {
            forget_slot(p, false);
            note("pad %d: forgotten", slot_no(p));
        }
        job_say("pairing deleted");
        job_end(false);
        return;
    case TANG_BTPAD_JOB_NONE:
        break;
    }
    job_end(false);
}

/* One step of the running job, from tang_btpad_work(). */
static void job_step(void)
{
    if (!s_job_running) {
        return;
    }
    const uint64_t now = bflb_mtimer_get_time_ms();
    switch (s_job_phase) {
    case PH_START:
        job_begin();
        break;
    case PH_SCAN:
        if (s_found_n >= 0 || now >= s_job_until) {
            scan_finish();
        }
        break;
    case PH_DROP: {
        btpad_t *p = &s_pad[s_job_pad];
        if (p->conn == NULL) {
            pair_link(p);
        } else if (now >= s_job_until) {
            job_say("pad %d's old link did not go", slot_no(p));
            job_end(true);
        }
        break;
    }
    case PH_LINK: {
        btpad_t *p = &s_pad[s_job_pad];
        if (p->intr_up && !p->pairing) {
            job_say("pad %d is paired and reporting", slot_no(p));
            job_end(false);
        } else if (p->conn == NULL) {
            job_say("pad %d did not finish pairing", slot_no(p));
            job_end(true);
        } else if (now >= s_job_until) {
            job_say("pad %d did not finish pairing in %d s", slot_no(p), BTPAD_JOB_PAIR_SECS);
            job_end(true);
        }
        break;
    }
    }
}

/* ---- the window's calls (tang_btpad.h) */

static bool pad_ok(int pad)
{
    return pad >= 0 && pad < TANG_BTPAD_SLOTS;
}

const char *tang_btpad_scan_start(void)
{
    const char *why = tang_ble_radio_refusal();
    if (why != NULL) {
        return why;
    }
    return job_submit(TANG_BTPAD_JOB_SCAN, -1, NULL, NULL, BTPAD_JOB_SCAN_SECS);
}

int tang_btpad_scan_results(tang_btpad_found_t *out, int max)
{
    if (s_job_running && s_job == TANG_BTPAD_JOB_SCAN) {
        return -1;
    }
    const int n = s_results_n < max ? s_results_n : max;
    if (out != NULL && n > 0 && s_results != NULL) {
        memcpy(out, s_results, (size_t)n * sizeof *out);
    }
    return out != NULL ? (n > 0 ? n : 0) : s_results_n;
}

const char *tang_btpad_pair_start(int pad, const uint8_t addr[6], const char *name)
{
    if (!pad_ok(pad) || addr == NULL) {
        return "no such pad";
    }
    const char *why = tang_ble_radio_refusal();
    if (why != NULL) {
        return why;
    }
    return job_submit(TANG_BTPAD_JOB_PAIR, pad, addr, name, 0);
}

const char *tang_btpad_set_on(int pad, bool on)
{
    if (!pad_ok(pad)) {
        return "no such pad";
    }
    if (on) {
        const char *why = tang_ble_radio_refusal();
        if (why != NULL) {
            return why;
        }
    }
    return job_submit(on ? TANG_BTPAD_JOB_ON : TANG_BTPAD_JOB_OFF, pad, NULL, NULL, 0);
}

const char *tang_btpad_forget(int pad)
{
    if (!pad_ok(pad)) {
        return "no such pad";
    }
    return job_submit(TANG_BTPAD_JOB_FORGET, pad, NULL, NULL, 0);
}

/* The slot's latest note, without its "pad N: ", or "". */
static void last_note(int pad, char *out, size_t cap)
{
    char prefix[8] = "pad 1: ";
    prefix[4] = (char)('1' + pad);    /* pad is 0 or 1 */
    const size_t plen = strlen(prefix);
    out[0] = '\0';
    const unsigned head = s_note_head;
    const unsigned oldest = head > BTPAD_NOTES ? head - BTPAD_NOTES : 0;
    for (unsigned i = head; i-- > oldest;) {
        char line[BTPAD_NOTE_LEN];
        taskENTER_CRITICAL();
        memcpy(line, s_note[i % BTPAD_NOTES], sizeof line);
        taskEXIT_CRITICAL();
        line[sizeof line - 1] = '\0';
        if (strncmp(line, prefix, plen) == 0) {
            snprintf(out, cap, "%s", line + plen);
            return;
        }
    }
}

static const char *pad_state(const btpad_t *p)
{
    if (p->intr_up) {
        return "ready";
    }
    if (p->linked) {
        return "opening channels";
    }
    if (p->conn != NULL) {
        return p->pairing ? "pairing" : "connecting";
    }
    if (!p->saved.valid) {
        return "idle";
    }
    return p->saved.off ? "off" : "waiting";
}

void tang_btpad_info(tang_btpad_info_t *out)
{
    memset(out, 0, sizeof *out);
    out->available = true;
    out->pairings = s_file_state;
    out->job = s_job;
    out->job_running = s_job_running;
    out->job_failed = s_job_failed;
    taskENTER_CRITICAL();
    memcpy(out->job_msg, s_job_msg, sizeof out->job_msg);
    taskEXIT_CRITICAL();
    out->job_msg[sizeof out->job_msg - 1] = '\0';
    const uint64_t now = bflb_mtimer_get_time_ms();
    if (s_job_running && s_job_phase == PH_SCAN && s_scan_end > now) {
        out->scan_ms_left = (uint32_t)(s_scan_end - now);
    }
    for (int i = 0; i < TANG_BTPAD_SLOTS; i++) {
        const btpad_t *p = &s_pad[i];
        tang_btpad_slot_info_t *o = &out->pad[i];
        taskENTER_CRITICAL();
        const tang_btpad_key_t k = p->saved;
        taskEXIT_CRITICAL();
        o->state = pad_state(p);
        o->saved = k.valid;
        if (k.valid) {
            memcpy(o->addr, k.addr, 6);
            snprintf(o->name, sizeof o->name, "%s", k.name);
            o->wii = k.kind == TANG_BTPAD_KIND_WII;
            o->off = k.off;
        } else if (p->conn != NULL) {
            memcpy(o->addr, p->want_addr, 6);
            snprintf(o->name, sizeof o->name, "%s", p->want_name);
        }
        o->linked = p->conn != NULL;
        o->ready = p->intr_up;
        o->reports = p->reports;
        last_note(i, o->last, sizeof o->last);
    }
}

/* ---- the shell's side of the jobs */

/* Print the notes until the job is over, then its outcome.  0 when it worked.
 * Every job ends by itself (an inquiry after its length, a pairing after
 * BTPAD_JOB_PAIR_SECS), so the bound here only keeps the shell from hanging
 * if one ever did not. */
#define BTPAD_JOB_WAIT_MS 120000u
static int job_wait(unsigned *seen)
{
    const uint64_t end = bflb_mtimer_get_time_ms() + BTPAD_JOB_WAIT_MS;
    while (s_job_running) {
        notes_print(seen);
        if (bflb_mtimer_get_time_ms() >= end) {
            tdsh_printf("btpad: still running after %u s; see btpad status\r\n",
                        BTPAD_JOB_WAIT_MS / 1000u);
            return 1;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    notes_print(seen);
    char msg[sizeof s_job_msg];
    taskENTER_CRITICAL();
    memcpy(msg, s_job_msg, sizeof msg);
    taskEXIT_CRITICAL();
    if (msg[0] != '\0') {
        tdsh_printf("btpad: %s\r\n", msg);
    }
    return s_job_failed ? 1 : 0;
}

static int cmd_pair(int secs, const uint8_t *given)
{
    uint8_t a[6];
    char name[TANG_BTPAD_NAME_LEN] = "";
    unsigned seen = s_note_head;
    const char *why;
    if (given != NULL) {
        memcpy(a, given, 6);
    } else {
        tdsh_printf("btpad: looking for a gamepad in pairing mode for about %d s...\r\n", secs);
        why = job_submit(TANG_BTPAD_JOB_SCAN, -1, NULL, NULL, secs);
        if (why != NULL) {
            tdsh_printf("btpad: %s\r\n", why);
            return 1;
        }
        if (job_wait(&seen) != 0) {
            return 1;
        }
        int best = -1;
        for (int i = 0; i < s_results_n; i++) {
            if (s_results[i].pad && (best < 0 || s_results[i].rssi > s_results[best].rssi)) {
                best = i;
            }
        }
        if (best < 0) {
            tdsh_printf("btpad: no gamepad in pairing mode was found\r\n");
            return 1;
        }
        memcpy(a, s_results[best].addr, 6);
        snprintf(name, sizeof name, "%s", s_results[best].name);
    }
    why = job_submit(TANG_BTPAD_JOB_PAIR, -1, a, name, 0);
    if (why != NULL) {
        tdsh_printf("btpad: %s\r\n", why);
        return 1;
    }
    if (job_wait(&seen) != 0) {
        return 1;
    }
    tdsh_printf("btpad: it reconnects by itself when woken\r\n");
    return 0;
}

static int cmd_on(int n, bool on)
{
    unsigned seen = s_note_head;
    const char *why = on ? tang_btpad_set_on(n - 1, true)
                         : job_submit(TANG_BTPAD_JOB_OFF, n - 1, NULL, NULL, 0);
    if (why != NULL) {
        tdsh_printf("btpad: %s\r\n", why);
        return 1;
    }
    return job_wait(&seen);
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
                            p->conn ? "connecting" :
                            p->saved.off ? "off (btpad on N)" : "waiting for the pad";
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
    forget_slot(p, false);
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
    if (strcmp(op, "on") == 0 || strcmp(op, "off") == 0) {
        const int n = argc >= 3 ? atoi(argv[2]) : 0;
        if (n < 1 || n > TANG_BTPAD_SLOTS) {
            tdsh_printf("usage: btpad on|off 1|2\r\n");
            return 1;
        }
        return cmd_on(n, op[1] == 'n');
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
            const char *why = link_to(p, a, NULL, false);
            tang_ble_release();
            if (why != NULL) {
                tdsh_printf("btpad: %s\r\n", why);
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
                " | on|off 1|2 | forget 1|2|all\r\n");
    return 1;
}

#endif /* CONFIG_BT_BREDR */
