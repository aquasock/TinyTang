/*
 * tb_btpad_host.c -- ports/bl616/tang_btpad.c, the Classic gamepad host, run
 * against a fake Bluetooth stack on the host.
 *
 * Both of the host's failures on the board were in the glue this exercises,
 * not in a format a pure test could see: dropping a reference the stack owns
 * reset the board when a pad switched off (BLE-021), and reading the link key
 * in the pairing callback saved nothing, because the controller reports
 * pairing complete before it delivers the key (BLE-023).  So the fake counts
 * connection references exactly as Bouffalo SDK 2.3.36 does -- the reference
 * bt_conn_create_br() returns is the stack's on a link this host made, and
 * notify_disconnected() drops it after the callbacks -- and plays events in
 * the orders the board saw.  An unref below zero is counted, not a crash.
 *
 * The real file is included, not a copy (as tb_osd_desk.c does), so the test
 * can look at its slots.  The shell commands block in vTaskDelay(), which is
 * where the fake stack and pad act and where tang_ble.c's background task is
 * emulated: tang_btpad_work() runs when signalled or while a link waits.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "tang_btpad.c"
/* tang_btpad.c's quoted include finds the real tdsh_stdio_redirect.h beside
 * it, which maps printf to the console; this file prints to stdout. */
#undef printf

/* ------------------------------------------------------------ the checks */

static int fails, checks;
static void check(bool ok, const char *what)
{
    checks++;
    if (!ok) {
        fails++;
        printf("  FAIL: %s\n", what);
    }
}

/* ------------------------------------------------------------ the clock */

static uint64_t g_now = 1000;
uint64_t bflb_mtimer_get_time_ms(void) { return g_now; }
TickType_t xTaskGetTickCount(void) { return (TickType_t)g_now; }
BaseType_t xTaskCreate(void (*fn)(void *), const char *name, uint32_t stack_words,
                       void *arg, unsigned priority, void *handle)
{
    (void)fn; (void)name; (void)stack_words; (void)arg; (void)priority; (void)handle;
    return pdFAIL;
}

/* ------------------------------------------------------------ the console */

static char g_out[8192];
static size_t g_out_n;
int tdsh_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(g_out + g_out_n, sizeof g_out - g_out_n, fmt, ap);
    va_end(ap);
    if (n > 0) {
        g_out_n += (size_t)n;
        if (g_out_n >= sizeof g_out) {
            g_out_n = sizeof g_out - 1;
        }
    }
    return n;
}
static void out_clear(void) { g_out_n = 0; g_out[0] = '\0'; }
static bool out_has(const char *s) { return strstr(g_out, s) != NULL; }

/* ------------------------------------------------- connections, as the SDK */

#define MAX_CONNS 4
static struct bt_conn g_conns[MAX_CONNS];
static int g_underflows;

struct bt_conn *bt_conn_ref(struct bt_conn *c)
{
    c->ref++;
    return c;
}

void bt_conn_unref(struct bt_conn *c)
{
    if (c->ref <= 0) {
        g_underflows++;          /* BT_ASSERT(old > 0): the board's reset */
        return;
    }
    if (--c->ref == 0) {
        c->in_use = false;
    }
}

int bt_conn_get_info(const struct bt_conn *c, struct bt_conn_info *info)
{
    info->type = BT_CONN_TYPE_BR;
    info->br.dst = &c->dst;
    return 0;
}

int bt_conn_disconnect(struct bt_conn *c, u8_t reason)
{
    (void)reason;
    c->disconnects++;
    return 0;
}

static struct bt_conn *conn_new(const bt_addr_t *a)
{
    for (int i = 0; i < MAX_CONNS; i++) {
        if (!g_conns[i].in_use) {
            memset(&g_conns[i], 0, sizeof g_conns[i]);
            g_conns[i].in_use = true;
            g_conns[i].ref = 1;          /* bt_conn_add_br */
            g_conns[i].dst = *a;
            return &g_conns[i];
        }
    }
    return NULL;
}

struct bt_conn *bt_conn_lookup_addr_br(const bt_addr_t *peer)
{
    for (int i = 0; i < MAX_CONNS; i++) {
        if (g_conns[i].in_use && memcmp(g_conns[i].dst.val, peer->val, 6) == 0) {
            return bt_conn_ref(&g_conns[i]);
        }
    }
    return NULL;
}

static struct bt_conn *g_created;
struct bt_conn *bt_conn_create_br(const bt_addr_t *peer, const struct bt_br_conn_param *param)
{
    (void)param;
    struct bt_conn *c = conn_new(peer);
    if (c == NULL) {
        return NULL;
    }
    c->ref++;                            /* set_state(CONNECT): the stack's own */
    c->initial_master = true;            /* the returned one stays the stack's */
    g_created = c;
    return c;
}

/* conn_complete, success: lookup, CONNECTED and the callback, unref. */
static void sdk_connect_ok(struct bt_conn *c)
{
    bt_conn_ref(c);
    tang_btpad_connected(c, 0);
    bt_conn_unref(c);
}

/* conn_complete, failure: lookup; CONNECT -> DISCONNECTED notifies and drops
 * the stack's reference; then the lookup's.  The one create returned is left. */
static void sdk_connect_fail(struct bt_conn *c, u8_t err)
{
    bt_conn_ref(c);
    tang_btpad_connected(c, err);
    bt_conn_unref(c);
    bt_conn_unref(c);
}

/* conn_req then conn_complete for a link the pad makes: the stack holds one. */
static struct bt_conn *sdk_incoming(const bt_addr_t *a)
{
    struct bt_conn *c = conn_new(a);
    c->ref++;                            /* set_state(CONNECT) */
    bt_conn_unref(c);                    /* conn_req's own */
    sdk_connect_ok(c);
    return c;
}

/* The open channels, for the stack to close at a disconnection. */
#define MAX_CHANS 8
static struct bt_l2cap_chan *g_open[MAX_CHANS];

static void chan_opened(struct bt_l2cap_chan *ch)
{
    for (int i = 0; i < MAX_CHANS; i++) {
        if (g_open[i] == NULL) {
            g_open[i] = ch;
            break;
        }
    }
    ch->ops->connected(ch);
}

/* hci_disconn_complete, notify_disconnected, then the cleanup and update work
 * that close the channels and drop the stack's first reference. */
static void sdk_disconnect(struct bt_conn *c, u8_t reason)
{
    bt_conn_ref(c);                      /* lookup */
    bt_conn_ref(c);                      /* set_state(DISCONNECTED), the patch's */
    bt_conn_unref(c);                    /* the lookup's */
    tang_btpad_disconnected(c, reason);
    if (c->initial_master) {
        bt_conn_unref(c);                /* notify_disconnected: the sticky one */
    }
    bt_conn_unref(c);                    /* conn_cleanup: the patch's */
    for (int i = 0; i < MAX_CHANS; i++) {
        if (g_open[i] != NULL && g_open[i]->conn == c) {
            struct bt_l2cap_chan *ch = g_open[i];
            g_open[i] = NULL;
            ch->ops->disconnected(ch);
        }
    }
    bt_conn_unref(c);                    /* conn_update_timeout: the first one */
}

/* ------------------------------------------------- channels and servers */

static struct bt_l2cap_server *g_srv[2];
static int g_srv_count;
int bt_l2cap_br_server_register(struct bt_l2cap_server *server)
{
    if (g_srv_count < 2) {
        g_srv[g_srv_count++] = server;
    }
    return 0;
}

static struct bt_l2cap_chan *g_req[2];   /* requested by the host: ctrl, intr */
static uint64_t g_req_ms[2];
static int g_requests;
int bt_l2cap_chan_connect(struct bt_conn *conn, struct bt_l2cap_chan *chan, u16_t psm)
{
    const int i = psm == 0x13 ? 1 : 0;
    chan->conn = conn;
    chan->psm = psm;
    g_req[i] = chan;
    g_req_ms[i] = g_now;
    g_requests++;
    return 0;
}

/* The pad opens a channel to one of the host's servers. */
static int sdk_pad_opens(struct bt_conn *c, u16_t psm)
{
    for (int i = 0; i < g_srv_count; i++) {
        if (g_srv[i]->psm != psm) {
            continue;
        }
        struct bt_l2cap_chan *ch = NULL;
        const int rc = g_srv[i]->accept(c, &ch);
        if (rc == 0) {
            ch->conn = c;
            ch->psm = psm;
            chan_opened(ch);
        }
        return rc;
    }
    return -ENOENT;
}

static bool g_connectable;
int bt_br_set_connectable(bool enable)
{
    g_connectable = enable;
    return 0;
}

/* ------------------------------------------------------------ the inquiry */

static struct bt_br_discovery_result g_found[2];
static int g_found_n;
int bt_br_discovery_start(const struct bt_br_discovery_param *param,
                          struct bt_br_discovery_result *results, size_t count,
                          bt_br_discovery_cb_t cb)
{
    (void)param;
    const size_t n = (size_t)g_found_n < count ? (size_t)g_found_n : count;
    memcpy(results, g_found, n * sizeof *results);
    cb(results, n);
    return 0;
}
int bt_br_discovery_stop(void) { return 0; }

/* ------------------------------------------------------------ the key table */

#define MAX_KEYS 8
static struct { bool used; struct bt_keys_link_key k; } g_keys[MAX_KEYS];

struct bt_keys_link_key *bt_keys_find_link_key(const bt_addr_t *a)
{
    for (int i = 0; i < MAX_KEYS; i++) {
        if (g_keys[i].used && memcmp(g_keys[i].k.addr.val, a->val, 6) == 0) {
            return &g_keys[i].k;
        }
    }
    return NULL;
}

struct bt_keys_link_key *bt_keys_get_link_key(const bt_addr_t *a)
{
    struct bt_keys_link_key *k = bt_keys_find_link_key(a);
    if (k != NULL) {
        return k;
    }
    for (int i = 0; i < MAX_KEYS; i++) {
        if (!g_keys[i].used) {
            memset(&g_keys[i], 0, sizeof g_keys[i]);
            g_keys[i].used = true;
            g_keys[i].k.addr = *a;
            return &g_keys[i].k;
        }
    }
    return NULL;
}

void bt_keys_link_key_clear_addr(const bt_addr_t *a)
{
    for (int i = 0; i < MAX_KEYS; i++) {
        if (g_keys[i].used && memcmp(g_keys[i].k.addr.val, a->val, 6) == 0) {
            g_keys[i].used = false;
        }
    }
}

static void keys_clear_all(void) { memset(g_keys, 0, sizeof g_keys); }

/* Link Key Notification: the key arrives in the host's table. */
static const uint8_t k_key[16] = { 0x07, 0x8f, 0xb2, 0xe8, 0xbc, 0xcd, 0x81, 0xa9,
                                   0x6e, 0xfd, 0x7b, 0x46, 0xe7, 0x69, 0x44, 0x86 };
static void sdk_link_key(const bt_addr_t *a)
{
    struct bt_keys_link_key *k = bt_keys_get_link_key(a);
    k->flags = 0;
    memcpy(k->val, k_key, 16);
}

/* ------------------------------------------------------------ the card */

static uint8_t g_file[256];
static UINT g_file_len;
static bool g_file_exists;
static int g_saves;

FRESULT f_open(FIL *fp, const TCHAR *path, BYTE mode)
{
    (void)path;
    fp->mode = mode;
    fp->at = 0;
    if (mode & FA_CREATE_ALWAYS) {
        g_file_exists = true;
        g_file_len = 0;
        g_saves++;
        return FR_OK;
    }
    return g_file_exists ? FR_OK : FR_NO_FILE;
}
FRESULT f_read(FIL *fp, void *buff, UINT btr, UINT *br)
{
    UINT n = g_file_len - fp->at < btr ? g_file_len - fp->at : btr;
    memcpy(buff, g_file + fp->at, n);
    fp->at += n;
    *br = n;
    return FR_OK;
}
FRESULT f_write(FIL *fp, const void *buff, UINT btw, UINT *bw)
{
    memcpy(g_file + fp->at, buff, btw);
    fp->at += btw;
    g_file_len = fp->at;
    *bw = btw;
    return FR_OK;
}
FRESULT f_close(FIL *fp) { (void)fp; return FR_OK; }
FRESULT f_mkdir(const TCHAR *path) { (void)path; return FR_OK; }

/* ------------------------------------------------- tang_ble.c's side */

static bool g_radio_up;
static bool g_signal;
static int g_claims;
int tang_ble_radio_start(bool verbose)
{
    (void)verbose;
    if (!g_radio_up) {
        g_radio_up = true;
        tang_btpad_radio_up();
    }
    return 0;
}
bool tang_ble_claim(void) { g_claims++; return true; }
void tang_ble_release(void) { g_claims--; }
void tang_ble_btpad_signal(void) { g_signal = true; }

/* The Wii Classic key map the pad's controls go through (tang_wiipad.c). */
static int g_wii_calls;
static tang_wii_sample_t g_wii_last;
void tang_wii_apply(tang_wiipad_t *n, const tang_wii_sample_t *s, tang_mouse_t *m,
                    bool console_mode, uint32_t now_ms, tang_wii_keys_t *k)
{
    (void)n; (void)m; (void)console_mode; (void)now_ms; (void)k;
    g_wii_calls++;
    g_wii_last = *s;
}

/* ------------------------------------------- PINs, HCI and output reports */

static bt_addr_le_t g_own = { 0, { { 0x87, 0xb7, 0xc9, 0x4e, 0x47, 0xc4 } } };
int bt_get_local_public_address(bt_addr_le_t *a)
{
    *a = g_own;
    return 0;
}

static char g_pin_text[17];
static int g_pin_entries;
int bt_conn_auth_pincode_entry(struct bt_conn *conn, const char *pin)
{
    (void)conn;
    strncpy(g_pin_text, pin, sizeof g_pin_text - 1);
    g_pin_entries++;
    return 0;
}

static struct net_buf g_buf;
void *net_buf_add_mem(struct net_buf *buf, const void *mem, size_t len)
{
    memcpy(buf->store + buf->len, mem, len);
    buf->len = (u16_t)(buf->len + len);
    return buf->store;
}
void net_buf_unref(struct net_buf *buf) { (void)buf; }

static u16_t g_hci_op;
static uint8_t g_hci[32];
static int g_hci_len;
struct net_buf *bt_hci_cmd_create(u16_t opcode, u8_t param_len)
{
    (void)param_len;
    memset(&g_buf, 0, sizeof g_buf);
    g_hci_op = opcode;
    return &g_buf;
}
int bt_hci_cmd_send_sync(u16_t opcode, struct net_buf *buf, struct net_buf **rsp)
{
    (void)rsp;
    g_hci_op = opcode;
    memcpy(g_hci, buf->store, buf->len);
    g_hci_len = buf->len;
    return 0;
}

struct net_buf *bt_l2cap_create_pdu_timeout(struct net_buf_pool *pool, size_t reserve, s32_t timeout)
{
    (void)pool; (void)reserve; (void)timeout;
    memset(&g_buf, 0, sizeof g_buf);
    return &g_buf;
}
static uint8_t g_sent[32];
static int g_sent_len;
int bt_l2cap_chan_send(struct bt_l2cap_chan *chan, struct net_buf *buf)
{
    (void)chan;
    memcpy(g_sent, buf->store, buf->len);
    g_sent_len = buf->len;
    return buf->len;
}

/* ------------------------------------------------------------ the pad */

/* What the fake pad does while a command waits, the orders the board saw. */
enum { PAD_IDLE, PAD_PAIRS, PAD_FAILS, PAD_PIN };
static int g_pad_mode;
static bool g_pad_secured;
static bt_addr_t g_pad_addr = { { 0x7f, 0x4d, 0x17, 0xd8, 0x17, 0xe4 } };

static void pad_tick(void)
{
    if (g_pad_mode == PAD_FAILS && g_created != NULL) {
        struct bt_conn *c = g_created;
        g_created = NULL;
        sdk_connect_fail(c, 0x04);
        return;
    }
    if (g_pad_mode != PAD_PAIRS && g_pad_mode != PAD_PIN) {
        return;
    }
    if (g_created != NULL) {
        struct bt_conn *c = g_created;
        g_created = NULL;
        sdk_connect_ok(c);
        return;
    }
    if (g_req[0] != NULL) {
        struct bt_l2cap_chan *ch = g_req[0];
        g_req[0] = NULL;
        if (g_pad_mode == PAD_PIN && !g_pad_secured) {
            /* A Wii pad: a PIN request first; the pad waits for the answer,
             * which the background task sends on a later tick. */
            if (g_pin_entries == 0 && g_hci_len == 0) {
                g_req[0] = ch;
                static bool asked;
                if (!asked) {
                    asked = true;
                    tang_btpad_pincode(ch->conn, false);
                }
                return;
            }
        }
        if (!g_pad_secured) {
            /* The control channel needs security.  The two pairing styles end
             * differently, and the host must survive both (core-log entry 60).
             * Secure Simple Pairing (the 8BitDo) reports pairing complete,
             * then delivers the key, then brings encryption up.  A legacy PIN
             * pairing (the Wii U Pro) never reports pairing complete at all:
             * AUTH_COMPLETE only raises encryption, so all the host gets is the
             * key and the encryption change. */
            if (g_pad_mode != PAD_PIN) {
                tang_btpad_paired(ch->conn, true, 0);
            }
            sdk_link_key(&ch->conn->dst);     /* the key for this link's pad */
            tang_btpad_security(ch->conn, BT_SECURITY_L2, 0);
            g_pad_secured = true;
        }
        chan_opened(ch);
        return;
    }
    if (g_req[1] != NULL) {
        struct bt_l2cap_chan *ch = g_req[1];
        g_req[1] = NULL;
        chan_opened(ch);
    }
}

/* vTaskDelay: time passes, the pad acts, and the background task runs. */
void vTaskDelay(TickType_t ticks)
{
    g_now += ticks;
    pad_tick();
    if (g_signal || tang_btpad_busy()) {
        g_signal = false;
        tang_btpad_work();
    }
}

static void run_ms(unsigned ms)
{
    for (unsigned t = 0; t < ms; t += 50) {
        vTaskDelay(50);
    }
}

static void reset_requests(void)
{
    g_req[0] = g_req[1] = NULL;
    g_requests = 0;
}

static int cmd(const char *a1, const char *a2)
{
    char *argv[3] = { "btpad", (char *)a1, (char *)a2 };
    out_clear();
    return tang_btpad_cmd(NULL, a2 ? 3 : 2, argv);
}

/* A report on pad 1's interrupt channel. */
static void report(uint8_t b1, uint8_t b2, uint8_t hat)
{
    uint8_t r[13] = { 0xa1, 0x3f, b1, b2, hat, 0xf0, 0x7f, 0x00, 0x80, 0x00, 0x80, 0xf0, 0x7f };
    struct net_buf nb = { .data = r, .len = sizeof r };
    s_pad[0].intr.chan.ops->recv(&s_pad[0].intr.chan, &nb);
}

int main(void)
{
    /* ---- 1. pairing: complete before the key, as on the board ---------- */
    tang_ble_radio_start(true);
    check(g_srv_count == 2 && g_connectable, "the radio start did not register servers and page scan");
    g_pad_mode = PAD_PAIRS;
    int rc = cmd("E4:17:D8:17:4D:7F", NULL);
    check(rc == 0, "pairing by address did not finish");
    check(s_pad[0].saved.valid, "the pairing was not kept (the key read before it arrived?)");
    check(memcmp(s_pad[0].saved.key, k_key, 16) == 0, "the kept key is not the notified one");
    run_ms(100);
    check(g_file_exists && g_saves >= 1, "the pairing was not written to the card");
    {
        tang_btpad_key_t k[TANG_BTPAD_SLOTS];
        check(tang_btpad_keys_decode(g_file, g_file_len, k) == 0 && k[0].valid &&
              memcmp(k[0].key, k_key, 16) == 0, "the card's file does not hold the key");
    }
    check(s_pad[0].intr_up && s_pad[0].ctrl_up, "the channels are not both open");
    check(g_claims == 0, "the Bluetooth claim was not released");

    /* Reports: the word, the key map, and nothing left held after. */
    report(0x02, 0x00, 0x08);
    check(tang_btpad_pad_word(0) == TANG_PAD_A, "A is not in pad 1's word");
    tang_mouse_t m = { 0 };
    tang_wii_keys_t keys = { 0 };
    tang_btpad_poll(&m, false, &keys);
    check(g_wii_calls == 1 && g_wii_last.left, "the poll did not give the key map A as the left button");
    report(0x00, 0x10, 0x08);
    check(tang_btpad_pad_word(0) == TANG_PAD_LB, "home is not LB");

    /* ---- 2. the pad switches off: a link this host made ---------------- */
    struct bt_conn *c1 = s_pad[0].conn;
    g_pad_mode = PAD_IDLE;
    sdk_disconnect(c1, 0x08);
    check(g_underflows == 0, "a reference was dropped twice (the board reset of BLE-021)");
    check(c1->ref == 0, "a reference was leaked on an outbound link");
    check(s_pad[0].conn == NULL && !s_pad[0].linked, "the slot still holds the link");
    check(tang_btpad_pad_word(0) == 0, "a button stayed held after the link went");
    tang_btpad_poll(&m, false, &keys);
    check(g_wii_calls == 1, "a pad that is gone was still polled");

    /* ---- 3. woken, the pad connects and opens its own channels --------- */
    reset_requests();
    struct bt_conn *c2 = sdk_incoming(&g_pad_addr);
    check(c2->ref == 2, "the host did not take a reference on a link the pad made");
    check(s_pad[0].linked, "a saved pad's link was not taken");
    run_ms(1000);
    check(g_requests == 0, "the host opened a channel while the pad was opening its own");
    check(sdk_pad_opens(c2, 0x11) == 0 && sdk_pad_opens(c2, 0x13) == 0,
          "the servers refused the pad's channels");
    check(s_pad[0].intr_up, "the pad's channels did not make it report");
    run_ms(4000);
    check(g_requests == 0, "the host opened a channel after the pad had");
    sdk_disconnect(c2, 0x08);
    check(g_underflows == 0 && c2->ref == 0, "the references of a link the pad made do not balance");

    /* ---- 4. woken, the pad stays quiet: the host steps in after 3 s ---- */
    reset_requests();
    g_pad_mode = PAD_PAIRS;              /* opens what the host asks for */
    g_pad_secured = true;                /* already bonded: no new pairing */
    const uint64_t linked_at = g_now;
    struct bt_conn *c3 = sdk_incoming(&g_pad_addr);
    g_pad_mode = PAD_IDLE;               /* silent for now */
    run_ms(2900);
    check(g_requests == 0, "the host did not wait for the pad before opening channels");
    g_pad_mode = PAD_PAIRS;
    run_ms(600);
    check(g_requests == 2 && g_req_ms[0] - linked_at >= BTPAD_PAD_OPENS_MS,
          "the host did not open both channels itself after the wait");
    check(s_pad[0].intr_up, "the channels the host opened do not report");
    g_pad_mode = PAD_IDLE;
    sdk_disconnect(c3, 0x08);
    check(g_underflows == 0 && c3->ref == 0, "the references of a quiet pad's link do not balance");

    /* ---- 5. a reconnection that fails --------------------------------- */
    g_pad_mode = PAD_FAILS;
    rc = cmd("E4:17:D8:17:4D:7F", NULL);
    check(rc != 0, "a failed connection was reported as working");
    struct bt_conn *c4 = &g_conns[0];
    for (int i = 0; i < MAX_CONNS; i++) {
        if (g_conns[i].dst.val[0] == 0x7f && g_conns[i].ref != 0) {
            c4 = &g_conns[i];
        }
    }
    check(g_underflows == 0, "a failed connection dropped a reference twice");
    check(c4->ref == 0, "a failed connection leaked its reference");
    check(s_pad[0].conn == NULL && s_pad[0].saved.valid, "a failed connection lost the slot's pairing");
    g_pad_mode = PAD_IDLE;

    /* ---- 6. a stranger connects --------------------------------------- */
    const bt_addr_t other = { { 1, 2, 3, 4, 5, 6 } };
    struct bt_conn *c5 = sdk_incoming(&other);
    check(c5->disconnects == 1, "a device that is not a saved pad was not let go");
    check(c5->ref == 1, "the host took a reference to a stranger's link");
    check(s_pad[1].conn == NULL && s_pad[0].conn == NULL, "a stranger took a slot");
    sdk_disconnect(c5, 0x13);
    check(g_underflows == 0 && c5->ref == 0, "a stranger's link did not balance");

    /* ---- 7. at boot: the keys back from the card ---------------------- */
    keys_clear_all();
    memset(&s_pad[0].saved, 0, sizeof s_pad[0].saved);
    check(tang_btpad_load(), "a saved pad on the card was not loaded");
    check(s_pad[0].saved.valid && memcmp(s_pad[0].saved.key, k_key, 16) == 0, "the loaded key is wrong");
    tang_btpad_radio_up();
    const struct bt_keys_link_key *k = bt_keys_find_link_key(&g_pad_addr);
    check(k != NULL && memcmp(k->val, k_key, 16) == 0, "the key was not put back into the host's table");
    check(g_srv_count == 2, "a second radio start registered the servers again");

    /* ---- 8. forget ---------------------------------------------------- */
    rc = cmd("forget", "1");
    run_ms(100);
    check(rc == 0 && !s_pad[0].saved.valid, "forget did not clear the slot");
    check(bt_keys_find_link_key(&g_pad_addr) == NULL, "forget left the key in the host's table");
    {
        tang_btpad_key_t kk[TANG_BTPAD_SLOTS];
        check(tang_btpad_keys_decode(g_file, g_file_len, kk) == 0 && !kk[0].valid,
              "forget did not save the empty slot to the card");
    }
    cmd("status", NULL);
    check(out_has("pad 1: empty"), "status does not show the forgotten slot as empty");

    /* ---- 9. a Wii pad: legacy PIN, the board's address lowest byte first */
    g_pad_mode = PAD_PIN;
    g_pad_secured = false;
    g_pin_entries = 0;
    g_hci_len = 0;
    reset_requests();
    rc = cmd("18:2A:7B:45:5C:91", NULL);
    check(rc == 0, "a pad that pairs by PIN did not finish pairing");
    check(g_pin_entries == 1 && memcmp(g_pin_text, g_own.a.val, 6) == 0 && strlen(g_pin_text) == 6,
          "the PIN is not this board's address, lowest byte first");
    check(s_pad[0].saved.valid && s_pad[0].saved.kind == TANG_BTPAD_KIND_WII,
          "a PIN-paired pad was not kept as the Wii family");
    /* btpad send: the bytes go out on the interrupt channel as given. */
    {
        char *argv[6] = { "btpad", "send", "1", "a2", "12", "34" };
        out_clear();
        rc = tang_btpad_cmd(NULL, 6, argv);
        check(rc == 0 && g_sent_len == 3 && g_sent[0] == 0xa2 && g_sent[1] == 0x12 &&
              g_sent[2] == 0x34, "btpad send did not send the bytes given");
    }
    g_pad_mode = PAD_IDLE;
    sdk_disconnect(s_pad[0].conn, 0x13);
    check(g_underflows == 0, "the PIN-paired link's references do not balance");

    /* ---- 10. a Wii pad woken: it opens nothing, the host must open both
     * The board's Wii U Pro connects, opens no channel and drops the link with
     * reason 0x13 about a second later, eight times in fifteen seconds, so the
     * 3 s wait that suits a Switch-mode pad loses the link (core-log entry 60). */
    {
        const bt_addr_t wii = { { 0x91, 0x5c, 0x45, 0x7b, 0x2a, 0x18 } };
        g_pad_mode = PAD_IDLE;
        g_pad_secured = true;         /* bonded already: no new pairing */
        reset_requests();
        const uint64_t linked_at = g_now;
        struct bt_conn *c6 = sdk_incoming(&wii);
        g_pad_mode = PAD_PAIRS;       /* accepts the channels the host opens */
        check(s_pad[0].linked, "a woken Wii pad's link was not taken");
        run_ms(200);
        check(g_requests == 2,
              "the host waited for a Wii pad to open channels it never opens");
        check(g_req_ms[0] - linked_at < BTPAD_PAD_OPENS_MS,
              "the Wii pad's channels were opened behind the Switch pad's wait");
        check(s_pad[0].intr_up, "the Wii pad's channels do not report");
        g_pad_mode = PAD_IDLE;
        sdk_disconnect(c6, 0x13);
        check(g_underflows == 0, "the Wii pad's references do not balance");
    }

    /* With a 0x00 in the address the SDK's strlen would cut the PIN short:
     * the raw HCI reply carries the pad's address, length 6 and the PIN. */
    cmd("forget", "1");
    run_ms(100);
    g_own.a.val[2] = 0x00;
    g_pad_mode = PAD_PIN;
    g_pad_secured = false;
    g_pin_entries = 0;
    g_hci_len = 0;
    reset_requests();
    /* The fake pad's PIN request is one-shot and spent: this run pairs on a
     * key, and the PIN request is made by hand once the link is up. */
    const uint8_t wiiu[6] = { 0x91, 0x5c, 0x45, 0x7b, 0x2a, 0x18 };
    g_pad_secured = true;             /* the pairing itself is not the point */
    g_pad_mode = PAD_PAIRS;
    rc = cmd("18:2A:7B:45:5C:91", NULL);
    tang_btpad_pincode(s_pad[0].conn, false);
    run_ms(100);
    check(g_pin_entries == 0 && g_hci_op == 0x040D && g_hci_len == 23,
          "a PIN with a zero byte did not go as a raw HCI reply");
    check(memcmp(g_hci, wiiu, 6) == 0 && g_hci[6] == 6 && memcmp(g_hci + 7, g_own.a.val, 6) == 0,
          "the raw PIN reply is not the pad's address, length 6 and the board's address");

    printf("btpad host: %s (%d checks)\n", fails ? "FAIL" : "PASS", checks);
    return fails ? 1 : 0;
}
