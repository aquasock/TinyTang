// TinyTang — `blescan`: prove the BL616's Bluetooth LE radio works.
//
// The BL616 has a 2.4 GHz Wi-Fi 6 / BLE 5 radio.  On the Tang Console NEO dock
// its antenna pin (BL616_ANT) runs through L9 (0R) to U35, an IPEX/U.FL jack
// on the underside marked "ANT"; there is no antenna on the board itself.  So
// before building anything on Bluetooth, this answers the first question: does
// the radio receive at all, with or without an antenna on that jack?
//
// It listens for BLE advertisements for a few seconds and prints each device
// seen, strongest first.  Phones, earbuds, watches and BLE controllers all
// advertise, so any of them held near the board is a test signal.
//
// The radio and the BLE stack are started on the first `blescan`, not at boot,
// so a board that never runs it boots exactly as before.  The bring-up order is
// the SDK's own (examples/btble/central): RF parameters, then the controller,
// then the HCI driver and host.  The controller cannot be shut down again, so
// once started it stays up until reset.
//
// `blekbd` and `blemouse` connect one BLE keyboard and one BLE mouse (HID over
// GATT), each on its own connection: pair with Just Works and put the device
// in boot protocol.  The keyboard's boot report (modifiers, reserved, six
// keycodes) is the one the wired keyboard link already carries, and
// tang_ble_keyboard() hands it to the desktop layer's poll, which types it
// exactly as it types the wired link's.  The mouse's boot report (buttons, X,
// Y, and a wheel byte if the mouse adds one) is gathered by tang_ble_mouse()
// and drives the desktop's pointer.  `watch` prints the reports as well.  Keys
// are held in RAM only (CONFIG_BT_SETTINGS is 0), so after a reset each
// device must be put back in pairing mode.

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "bluetooth.h"
#include "conn.h"
#include "gatt.h"
#include "uuid.h"
#include "hci_driver.h"
#include "btble_lib_api.h"
#include "rfparam_adapter.h"

#include "tdsh.h"
#include "tang_ble.h"
#include "tang_pad.h"

int tdsh_printf(const char *fmt, ...);

/* From the SDK's host/hci_core.h, which is not self-contained enough to
 * include on its own. */
int bt_get_local_public_address(bt_addr_le_t *adv_addr);

#define BLE_MAX_DEVICES   24
#define BLE_NAME_LEN      24
#define BLE_DEFAULT_SECS  5
#define BLE_MAX_SECS      60
#define BLE_ENABLE_WAIT_MS 3000

typedef struct {
    bt_addr_le_t addr;
    int8_t rssi_max;
    int8_t rssi_last;
    uint16_t seen;
    char name[BLE_NAME_LEN];
} ble_device_t;

/* Filled from the BLE host's receive task, read by the shell task.  Updates
 * are a few bytes under a critical section; nothing in it blocks. */
static ble_device_t s_devices[BLE_MAX_DEVICES];
static unsigned s_count;
static unsigned s_dropped;
static uint32_t s_reports;

static volatile int s_enable_result = 1;   /* 1 = pending, else bt_enable's err */
static bool s_started;

static void bt_ready(int err)
{
    s_enable_result = err;
}

static bool name_cb(struct bt_data *data, void *user_data)
{
    char *name = user_data;

    if (data->type == BT_DATA_NAME_COMPLETE || data->type == BT_DATA_NAME_SHORTENED) {
        size_t n = data->data_len < BLE_NAME_LEN - 1 ? data->data_len : BLE_NAME_LEN - 1;
        memcpy(name, data->data, n);
        name[n] = '\0';
        return false;
    }
    return true;
}

static void device_found(const bt_addr_le_t *addr, s8_t rssi, u8_t evtype,
                         struct net_buf_simple *buf)
{
    (void)evtype;
    char name[BLE_NAME_LEN] = { 0 };

    bt_data_parse(buf, name_cb, name);

    taskENTER_CRITICAL();
    s_reports++;
    ble_device_t *dev = NULL;
    for (unsigned i = 0; i < s_count; i++) {
        if (bt_addr_le_cmp(&s_devices[i].addr, addr) == 0) {
            dev = &s_devices[i];
            break;
        }
    }
    if (!dev) {
        if (s_count < BLE_MAX_DEVICES) {
            dev = &s_devices[s_count++];
            memset(dev, 0, sizeof(*dev));
            bt_addr_le_copy(&dev->addr, addr);
            dev->rssi_max = rssi;
        } else {
            s_dropped++;
        }
    }
    if (dev) {
        dev->seen++;
        dev->rssi_last = rssi;
        if (rssi > dev->rssi_max) {
            dev->rssi_max = rssi;
        }
        /* The name usually arrives in the scan response, not the first
         * advertisement, so take it whenever one turns up. */
        if (name[0] && !dev->name[0]) {
            memcpy(dev->name, name, BLE_NAME_LEN);
        }
    }
    taskEXIT_CRITICAL();
}

static void hid_register_callbacks(void);

static int ble_start(void)
{
    if (s_started) {
        return s_enable_result;
    }

    int32_t rc = rfparam_init(0, NULL, 0);
    if (rc != 0) {
        tdsh_printf("blescan: RF init failed (%ld)\r\n", (long)rc);
        return (int)rc;
    }

    btble_controller_init(configMAX_PRIORITIES - 1);
    hci_driver_init();
    s_enable_result = 1;
    int err = bt_enable(bt_ready);
    s_started = true;
    if (err) {
        s_enable_result = err;
        tdsh_printf("blescan: bt_enable failed (%d)\r\n", err);
        return err;
    }

    for (int waited = 0; s_enable_result == 1 && waited < BLE_ENABLE_WAIT_MS; waited += 20) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    if (s_enable_result == 1) {
        tdsh_printf("blescan: BLE stack did not come up within %d ms\r\n",
                    BLE_ENABLE_WAIT_MS);
        return -1;
    }
    if (s_enable_result != 0) {
        tdsh_printf("blescan: BLE stack failed to start (%d)\r\n", s_enable_result);
        return s_enable_result;
    }

    hid_register_callbacks();

    bt_addr_le_t own;
    bt_get_local_public_address(&own);
    tdsh_printf("blescan: radio up, own address %02X:%02X:%02X:%02X:%02X:%02X\r\n",
                own.a.val[5], own.a.val[4], own.a.val[3],
                own.a.val[2], own.a.val[1], own.a.val[0]);
    return 0;
}

static int cmd_blescan(tdsh_session_t *session, int argc, char **argv)
{
    (void)session;
    int secs = BLE_DEFAULT_SECS;

    if (argc >= 2) {
        secs = atoi(argv[1]);
        if (secs < 1 || secs > BLE_MAX_SECS) {
            tdsh_printf("usage: blescan [seconds 1-%d]\r\n", BLE_MAX_SECS);
            return 1;
        }
    }

    if (ble_start() != 0) {
        return 1;
    }

    taskENTER_CRITICAL();
    s_count = 0;
    s_dropped = 0;
    s_reports = 0;
    taskEXIT_CRITICAL();

    /* Active scan, so devices answer with a scan response (where the name
     * usually is); duplicates are kept so the RSSI keeps updating. */
    struct bt_le_scan_param param = {
        .type = BT_HCI_LE_SCAN_ACTIVE,
        .filter_dup = BT_HCI_LE_SCAN_FILTER_DUP_DISABLE,
        .interval = BT_GAP_SCAN_FAST_INTERVAL,
        .window = BT_GAP_SCAN_FAST_WINDOW,
    };
    int err = bt_le_scan_start(&param, device_found);
    if (err) {
        tdsh_printf("blescan: scan start failed (%d)\r\n", err);
        return 1;
    }
    tdsh_printf("blescan: listening for %d s...\r\n", secs);
    vTaskDelay(pdMS_TO_TICKS(secs * 1000));
    (void)bt_le_scan_stop();

    static ble_device_t snap[BLE_MAX_DEVICES];
    unsigned count, dropped;
    uint32_t reports;
    taskENTER_CRITICAL();
    count = s_count;
    dropped = s_dropped;
    reports = s_reports;
    memcpy(snap, s_devices, count * sizeof(snap[0]));
    taskEXIT_CRITICAL();

    /* Strongest first: the device held next to the board should top the list. */
    for (unsigned i = 1; i < count; i++) {
        ble_device_t t = snap[i];
        unsigned j = i;
        while (j > 0 && snap[j - 1].rssi_max < t.rssi_max) {
            snap[j] = snap[j - 1];
            j--;
        }
        snap[j] = t;
    }

    tdsh_printf("blescan: %u device(s), %lu report(s)\r\n", count, (unsigned long)reports);
    if (count) {
        tdsh_printf("  address            type  best  last  seen  name\r\n");
    }
    for (unsigned i = 0; i < count; i++) {
        const ble_device_t *d = &snap[i];
        tdsh_printf("  %02X:%02X:%02X:%02X:%02X:%02X  %-4s  %4d  %4d  %4u  %s\r\n",
                    d->addr.a.val[5], d->addr.a.val[4], d->addr.a.val[3],
                    d->addr.a.val[2], d->addr.a.val[1], d->addr.a.val[0],
                    d->addr.type == BT_ADDR_LE_PUBLIC ? "pub" : "rand",
                    d->rssi_max, d->rssi_last, d->seen,
                    d->name[0] ? d->name : "-");
    }
    if (dropped) {
        tdsh_printf("  (%u more report(s) from devices past the %d-entry table)\r\n",
                    dropped, BLE_MAX_DEVICES);
    }
    return 0;
}

/* ---- blekbd / blemouse: HID-over-GATT devices ------------------------- */

/* Two slots, one keyboard and one mouse, each its own connection with its
 * own discovery, subscriptions and setup sequence.  The Bluetooth callbacks
 * find their slot by connection.  Only one slot sets up at a time: setup is a
 * chain of ATT requests issued one by one (BLE-005), and two chains at once
 * would put that back in doubt. */

#define HID_LOG_LINES     48
#define HID_LOG_LEN       96
#define HID_MAX_CHRCS     24
#define HID_MAX_SUBS      8
#define HID_MAX_READS     10
#define HID_DEFAULT_SECS  30
#define HID_MAX_SECS      600

/* Bluetooth callbacks run on the BLE host's tasks.  They only append lines
 * here; the shell task drains and prints them, so the console is only ever
 * written from the shell.  Each line carries its command's name. */
static char s_log[HID_LOG_LINES][HID_LOG_LEN];
static unsigned s_log_head;
static unsigned s_log_tail;
static unsigned s_log_lost;

static void log_line(const char *line)
{
    taskENTER_CRITICAL();
    if (s_log_head - s_log_tail >= HID_LOG_LINES) {
        s_log_tail++;
        s_log_lost++;
    }
    memcpy(s_log[s_log_head % HID_LOG_LINES], line, HID_LOG_LEN);
    s_log_head++;
    taskEXIT_CRITICAL();
}

static void log_drain(void)
{
    char line[HID_LOG_LEN];
    unsigned lost;

    for (;;) {
        bool have = false;
        taskENTER_CRITICAL();
        lost = s_log_lost;
        s_log_lost = 0;
        if (s_log_tail != s_log_head) {
            memcpy(line, s_log[s_log_tail % HID_LOG_LINES], HID_LOG_LEN);
            s_log_tail++;
            have = true;
        }
        taskEXIT_CRITICAL();
        if (lost) {
            tdsh_printf("ble: (%u line(s) lost)\r\n", lost);
        }
        if (!have) {
            return;
        }
        tdsh_printf("%s\r\n", line);
    }
}

typedef struct {
    uint16_t uuid;
    uint16_t value_handle;
    uint16_t ccc_handle;
    uint8_t props;
} hid_chrc_t;

typedef enum {
    HID_IDLE,
    HID_CONNECTING,
    HID_SECURING,
    HID_DISCOVERING,
    HID_READY,
} hid_state_t;

static const char *const s_state_names[] = {
    "idle", "connecting", "pairing", "discovering", "ready",
};

typedef struct {
    const char *cmd;              /* "blekbd" or "blemouse": the log prefix */
    uint16_t boot_uuid;           /* Boot Keyboard / Boot Mouse Input */
    const char *ready_hint;

    struct bt_conn *conn;
    volatile hid_state_t state;
    bt_addr_le_t addr;
    bool boot;                    /* boot protocol was written */
    volatile bool watching;       /* log every report while `watch` runs */
    volatile uint32_t reports;

    uint16_t hids_start;
    uint16_t hids_end;
    hid_chrc_t chrcs[HID_MAX_CHRCS];
    unsigned nchrcs;
    uint16_t boot_value_handle;

    struct bt_gatt_discover_params disc_primary;
    struct bt_gatt_discover_params disc_chrc;
    struct bt_gatt_discover_params disc_ccc;
    struct bt_gatt_subscribe_params subs[HID_MAX_SUBS];
    unsigned nsubs;
    struct bt_gatt_read_params reads[HID_MAX_READS];
    unsigned nreads;

    /* Setup after discovery is a sequence of ATT operations: every
     * subscription, then the read-backs.  They are issued one at a time, each
     * from the previous one's completion callback.  Issuing them all at once
     * from a host callback exhausts the ATT transmit buffers, and the
     * allocation then waits forever on the very task that would free them. */
    unsigned setup_sub;
    unsigned setup_read;
    bool setup_active;
    const void *setup_pending;
} hid_dev_t;

enum { HID_KBD, HID_MOUSE, HID_SLOTS };

static hid_dev_t s_hid[HID_SLOTS] = {
    [HID_KBD] = { .cmd = "blekbd", .boot_uuid = 0x2A22,
                  .ready_hint = "type on the keyboard" },
    [HID_MOUSE] = { .cmd = "blemouse", .boot_uuid = 0x2A33,
                    .ready_hint = "move the mouse" },
};

/* The keyboard's current boot report, and the mouse's movement and wheel
 * since the desk layer last asked, with its buttons now.  Written by the
 * notify callback, read by tang_ble_keyboard() and tang_ble_mouse(). */
static uint8_t s_kbd_report[8];
static tang_mouse_t s_mouse;

static void hid_log(const hid_dev_t *d, const char *fmt, ...)
{
    char line[HID_LOG_LEN];
    int n = snprintf(line, sizeof(line), "%s: ", d->cmd);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line + n, sizeof(line) - (size_t)n, fmt, ap);
    va_end(ap);
    log_line(line);
}

static hid_dev_t *hid_by_conn(const struct bt_conn *conn)
{
    for (int i = 0; i < HID_SLOTS; i++) {
        if (s_hid[i].conn && s_hid[i].conn == conn) {
            return &s_hid[i];
        }
    }
    return NULL;
}

/* The input a slot hands the desk layer, cleared so nothing stays held. */
static void hid_clear_input(const hid_dev_t *d)
{
    taskENTER_CRITICAL();
    if (d == &s_hid[HID_KBD]) {
        memset(s_kbd_report, 0, sizeof(s_kbd_report));
    } else {
        memset(&s_mouse, 0, sizeof(s_mouse));
    }
    taskEXIT_CRITICAL();
}

static const char *kbd_key_name(uint8_t code, char *buf)
{
    static const char *const special[] = {
        [0x28] = "Enter", [0x29] = "Esc", [0x2A] = "Bksp", [0x2B] = "Tab",
        [0x2C] = "Space", [0x2D] = "-", [0x2E] = "=", [0x2F] = "[",
        [0x30] = "]", [0x31] = "\\", [0x33] = ";", [0x34] = "'",
        [0x35] = "`", [0x36] = ",", [0x37] = ".", [0x38] = "/",
        [0x39] = "Caps", [0x4F] = "Right", [0x50] = "Left", [0x51] = "Down",
        [0x52] = "Up",
    };

    if (code >= 0x04 && code <= 0x1D) {
        buf[0] = (char)('a' + code - 0x04);
        buf[1] = '\0';
        return buf;
    }
    if (code >= 0x1E && code <= 0x27) {
        buf[0] = (char)(code == 0x27 ? '0' : '1' + code - 0x1E);
        buf[1] = '\0';
        return buf;
    }
    if (code >= 0x3A && code <= 0x45) {
        snprintf(buf, 8, "F%d", code - 0x39);
        return buf;
    }
    if (code < sizeof(special) / sizeof(special[0]) && special[code]) {
        return special[code];
    }
    snprintf(buf, 8, "0x%02X", code);
    return buf;
}

static void kbd_log_boot_report(const hid_dev_t *d, const uint8_t *r)
{
    static const char *const mods[] = {
        "LCtrl", "LShift", "LAlt", "LGui", "RCtrl", "RShift", "RAlt", "RGui",
    };
    char text[HID_LOG_LEN];
    char name[8];
    int n = snprintf(text, sizeof(text), "%02X %02X %02X %02X %02X %02X %02X %02X |",
                     r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7]);

    for (int b = 0; b < 8 && n < (int)sizeof(text); b++) {
        if (r[0] & (1u << b)) {
            n += snprintf(text + n, sizeof(text) - n, " %s", mods[b]);
        }
    }
    for (int i = 2; i < 8 && n < (int)sizeof(text); i++) {
        if (r[i]) {
            n += snprintf(text + n, sizeof(text) - n, " %s", kbd_key_name(r[i], name));
        }
    }
    if (n < (int)sizeof(text) && !(r[0] | r[2] | r[3] | r[4] | r[5] | r[6] | r[7])) {
        snprintf(text + n, sizeof(text) - n, " (released)");
    }
    hid_log(d, "%s", text);
}

static void mouse_log_boot_report(const hid_dev_t *d, const uint8_t *r, u16_t length)
{
    hid_log(d, "len %u buttons %c%c%c dx %+d dy %+d wheel %+d", length,
            (r[0] & TANG_MOUSE_LEFT) ? 'L' : '-',
            (r[0] & TANG_MOUSE_MIDDLE) ? 'M' : '-',
            (r[0] & TANG_MOUSE_RIGHT) ? 'R' : '-',
            (int8_t)r[1], (int8_t)r[2], length >= 4 ? (int8_t)r[3] : 0);
}

static void hid_setup_step_done(struct bt_conn *conn, const void *op);

static u8_t hid_notify(struct bt_conn *conn, struct bt_gatt_subscribe_params *params,
                       const void *data, u16_t length)
{
    hid_dev_t *d = hid_by_conn(conn);
    if (!d) {
        return BT_GATT_ITER_CONTINUE;
    }
    if (!data) {
        /* The SDK's BFLB_BLE_PATCH_NOTIFY_WRITE_CCC_RSP calls this with no
         * data when the CCC write completes, as well as on failure or
         * unsubscribe, so it is reported but the subscription is kept. */
        hid_log(d, "CCC write for 0x%04X completed", params->value_handle);
        hid_setup_step_done(conn, params);
        return BT_GATT_ITER_CONTINUE;
    }

    const uint8_t *p = data;
    d->reports++;
    if (params->value_handle == d->boot_value_handle && d == &s_hid[HID_KBD] &&
        length >= 8) {
        taskENTER_CRITICAL();
        memcpy(s_kbd_report, p, 8);
        taskEXIT_CRITICAL();
        if (d->watching) {
            kbd_log_boot_report(d, p);
        }
    } else if (params->value_handle == d->boot_value_handle && d == &s_hid[HID_MOUSE] &&
               length >= 3) {
        taskENTER_CRITICAL();
        tang_mouse_boot_add(&s_mouse, p, length);
        taskEXIT_CRITICAL();
        if (d->watching) {
            mouse_log_boot_report(d, p, length);
        }
    } else if (d->watching) {
        char text[HID_LOG_LEN];
        int n = snprintf(text, sizeof(text), "report 0x%04X len %u:",
                         params->value_handle, length);
        for (unsigned i = 0; i < length && n < (int)sizeof(text) - 3; i++) {
            n += snprintf(text + n, sizeof(text) - n, " %02X", p[i]);
        }
        hid_log(d, "%s", text);
    }
    return BT_GATT_ITER_CONTINUE;
}

static hid_chrc_t *hid_find_chrc(hid_dev_t *d, uint16_t uuid)
{
    for (unsigned i = 0; i < d->nchrcs; i++) {
        if (d->chrcs[i].uuid == uuid) {
            return &d->chrcs[i];
        }
    }
    return NULL;
}

static u8_t hid_read_done(struct bt_conn *conn, u8_t err,
                          struct bt_gatt_read_params *params,
                          const void *data, u16_t length)
{
    hid_dev_t *d = hid_by_conn(conn);
    if (!d) {
        return BT_GATT_ITER_STOP;
    }
    if (err) {
        hid_log(d, "read 0x%04X failed (ATT 0x%02X)", params->single.handle, err);
    } else if (data) {
        const uint8_t *p = data;
        hid_log(d, "read 0x%04X: %u byte(s) %02X %02X", params->single.handle, length,
                length > 0 ? p[0] : 0, length > 1 ? p[1] : 0);
    }
    hid_setup_step_done(conn, params);
    return BT_GATT_ITER_STOP;
}

static void hid_setup_next(hid_dev_t *d)
{
    while (d->setup_active) {
        if (d->setup_sub < d->nsubs) {
            struct bt_gatt_subscribe_params *sp = &d->subs[d->setup_sub++];
            d->setup_pending = sp;
            int err = bt_gatt_subscribe(d->conn, sp);
            if (err == 0) {
                return;
            }
            hid_log(d, "subscribe 0x%04X failed (%d)", sp->value_handle, err);
            continue;
        }
        if (d->setup_read < d->nreads) {
            struct bt_gatt_read_params *rp = &d->reads[d->setup_read++];
            d->setup_pending = rp;
            int err = bt_gatt_read(d->conn, rp);
            if (err == 0) {
                return;
            }
            hid_log(d, "read 0x%04X not sent (%d)", rp->single.handle, err);
            continue;
        }
        d->setup_active = false;
        d->setup_pending = NULL;
        d->state = HID_READY;
        hid_log(d, "ready - %s", d->ready_hint);
    }
}

static void hid_setup_step_done(struct bt_conn *conn, const void *op)
{
    hid_dev_t *d = hid_by_conn(conn);
    if (d && d->setup_active && op == d->setup_pending) {
        hid_setup_next(d);
    }
}

static void hid_add_read(hid_dev_t *d, uint16_t handle)
{
    if (!handle || d->nreads >= HID_MAX_READS) {
        return;
    }
    struct bt_gatt_read_params *rp = &d->reads[d->nreads++];
    memset(rp, 0, sizeof(*rp));
    rp->func = hid_read_done;
    rp->handle_count = 1;
    rp->single.handle = handle;
}

static void hid_add_sub(hid_dev_t *d, const hid_chrc_t *c)
{
    if (d->nsubs >= HID_MAX_SUBS) {
        return;
    }
    struct bt_gatt_subscribe_params *sp = &d->subs[d->nsubs++];
    memset(sp, 0, sizeof(*sp));
    sp->notify = hid_notify;
    sp->value_handle = c->value_handle;
    sp->ccc_handle = c->ccc_handle;
    sp->value = BT_GATT_CCC_NOTIFY;
    /* Volatile: dropped from the host's list on disconnect.  For a bonded
     * peer the host otherwise keeps the subscription linked and re-sends it
     * on reconnect, while setup here memsets and re-subscribes these same
     * structs on every connection -- rewriting a node still in the host's
     * list, which showed as -EALREADY on the M750's second pairing. */
    atomic_set_bit(sp->flags, BT_GATT_SUBSCRIBE_FLAG_VOLATILE);
}

/* With every characteristic and its CCC known: switch to boot protocol if
 * the device offers this slot's boot input, and subscribe to that input and
 * every notifying input report in the HID service, so reports are seen
 * whichever way the device actually sends them.  The protocol mode and each
 * CCC are then read back to show what the device accepted. */
static void hid_setup_reports(hid_dev_t *d)
{
    hid_chrc_t *boot = hid_find_chrc(d, d->boot_uuid);
    hid_chrc_t *mode = hid_find_chrc(d, 0x2A4E);

    for (unsigned i = 0; i < d->nchrcs; i++) {
        hid_log(d, "  chrc 0x%04X value 0x%04X ccc 0x%04X props 0x%02X",
                d->chrcs[i].uuid, d->chrcs[i].value_handle,
                d->chrcs[i].ccc_handle, d->chrcs[i].props);
    }

    d->nsubs = 0;
    d->nreads = 0;
    d->boot = false;
    if (boot && boot->ccc_handle && mode) {
        static const uint8_t boot_protocol = 0;
        int err = bt_gatt_write_without_response(d->conn, mode->value_handle,
                                                 &boot_protocol, 1, false);
        if (err) {
            hid_log(d, "protocol mode write failed (%d)", err);
        } else {
            d->boot = true;
        }
        d->boot_value_handle = boot->value_handle;
    }
    for (unsigned i = 0; i < d->nchrcs; i++) {
        const hid_chrc_t *c = &d->chrcs[i];
        if ((c->uuid == d->boot_uuid || c->uuid == 0x2A4D) &&
            (c->props & BT_GATT_CHRC_NOTIFY) && c->ccc_handle) {
            hid_add_sub(d, c);
        }
    }
    if (mode) {
        hid_add_read(d, mode->value_handle);
    }
    for (unsigned i = 0; i < d->nsubs; i++) {
        hid_add_read(d, d->subs[i].ccc_handle);
    }
    hid_log(d, "%s protocol requested; subscribing to %u input(s)",
            d->boot ? "boot" : "report", d->nsubs);

    d->setup_sub = 0;
    d->setup_read = 0;
    d->setup_active = true;
    hid_setup_next(d);
}

static u8_t hid_discover_ccc(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                             struct bt_gatt_discover_params *params)
{
    (void)params;
    hid_dev_t *d = hid_by_conn(conn);
    if (!d) {
        return BT_GATT_ITER_STOP;
    }
    if (!attr) {
        hid_setup_reports(d);
        return BT_GATT_ITER_STOP;
    }
    /* Discovery runs unfiltered (filtering by UUID found nothing on the
     * Logitech K950, BLE-003), so pick the CCCs out here. */
    if (attr->uuid->type != BT_UUID_TYPE_16 || BT_UUID_16(attr->uuid)->val != 0x2902) {
        return BT_GATT_ITER_CONTINUE;
    }
    /* A CCC belongs to the characteristic whose value handle precedes it. */
    hid_chrc_t *owner = NULL;
    for (unsigned i = 0; i < d->nchrcs; i++) {
        if (d->chrcs[i].value_handle < attr->handle &&
            (!owner || d->chrcs[i].value_handle > owner->value_handle)) {
            owner = &d->chrcs[i];
        }
    }
    if (owner && !owner->ccc_handle) {
        owner->ccc_handle = attr->handle;
    }
    return BT_GATT_ITER_CONTINUE;
}

static u8_t hid_discover_chrc(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                              struct bt_gatt_discover_params *params)
{
    (void)params;
    hid_dev_t *d = hid_by_conn(conn);
    if (!d) {
        return BT_GATT_ITER_STOP;
    }
    if (!attr) {
        hid_log(d, "found %u characteristic(s); finding CCC descriptors", d->nchrcs);
        memset(&d->disc_ccc, 0, sizeof(d->disc_ccc));
        d->disc_ccc.uuid = NULL;
        d->disc_ccc.func = hid_discover_ccc;
        d->disc_ccc.start_handle = d->hids_start + 1;
        d->disc_ccc.end_handle = d->hids_end;
        d->disc_ccc.type = BT_GATT_DISCOVER_DESCRIPTOR;
        int err = bt_gatt_discover(conn, &d->disc_ccc);
        if (err) {
            hid_log(d, "descriptor discovery failed (%d)", err);
        }
        return BT_GATT_ITER_STOP;
    }
    const struct bt_gatt_chrc *chrc = attr->user_data;
    if (d->nchrcs < HID_MAX_CHRCS) {
        hid_chrc_t *c = &d->chrcs[d->nchrcs++];
        c->uuid = chrc->uuid->type == BT_UUID_TYPE_16 ? BT_UUID_16(chrc->uuid)->val : 0;
        c->value_handle = chrc->value_handle;
        c->ccc_handle = 0;
        c->props = chrc->properties;
    }
    return BT_GATT_ITER_CONTINUE;
}

static void hid_start_chrc_discovery(hid_dev_t *d)
{
    d->nchrcs = 0;
    memset(&d->disc_chrc, 0, sizeof(d->disc_chrc));
    d->disc_chrc.uuid = NULL;
    d->disc_chrc.func = hid_discover_chrc;
    d->disc_chrc.start_handle = d->hids_start + 1;
    d->disc_chrc.end_handle = d->hids_end;
    d->disc_chrc.type = BT_GATT_DISCOVER_CHARACTERISTIC;
    int err = bt_gatt_discover(d->conn, &d->disc_chrc);
    if (err) {
        hid_log(d, "characteristic discovery failed (%d)", err);
    }
}

/* Every primary service is listed (useful while bringing up a new device);
 * the HID service's range is kept and walked once the list is complete. */
static u8_t hid_discover_primary(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                 struct bt_gatt_discover_params *params)
{
    (void)params;
    hid_dev_t *d = hid_by_conn(conn);
    if (!d) {
        return BT_GATT_ITER_STOP;
    }
    if (!attr) {
        if (!d->hids_end) {
            hid_log(d, "no HID service on this device");
            return BT_GATT_ITER_STOP;
        }
        hid_log(d, "HID service 0x%04X-0x%04X", d->hids_start, d->hids_end);
        hid_start_chrc_discovery(d);
        return BT_GATT_ITER_STOP;
    }
    const struct bt_gatt_service_val *svc = attr->user_data;
    uint16_t uuid = svc->uuid->type == BT_UUID_TYPE_16 ? BT_UUID_16(svc->uuid)->val : 0;
    hid_log(d, "  service 0x%04X-0x%04X uuid %s0x%04X", attr->handle, svc->end_handle,
            svc->uuid->type == BT_UUID_TYPE_16 ? "" : "(128-bit) ", uuid);
    if (uuid == 0x1812 && !d->hids_end) {
        d->hids_start = attr->handle;
        d->hids_end = svc->end_handle;
    }
    return BT_GATT_ITER_CONTINUE;
}

static void hid_start_discovery(hid_dev_t *d)
{
    d->state = HID_DISCOVERING;
    d->hids_start = 0;
    d->hids_end = 0;
    memset(&d->disc_primary, 0, sizeof(d->disc_primary));
    d->disc_primary.uuid = NULL;
    d->disc_primary.func = hid_discover_primary;
    d->disc_primary.start_handle = 0x0001;
    d->disc_primary.end_handle = 0xFFFF;
    d->disc_primary.type = BT_GATT_DISCOVER_PRIMARY;
    int err = bt_gatt_discover(d->conn, &d->disc_primary);
    if (err) {
        hid_log(d, "service discovery failed (%d)", err);
    }
}

static void hid_connected(struct bt_conn *conn, u8_t err)
{
    hid_dev_t *d = hid_by_conn(conn);
    if (!d) {
        return;
    }
    if (err) {
        hid_log(d, "connect failed (HCI 0x%02X)", err);
        bt_conn_unref(d->conn);
        d->conn = NULL;
        d->state = HID_IDLE;
        return;
    }
    hid_log(d, "connected; requesting encryption");
    d->state = HID_SECURING;
    int rc = bt_conn_set_security(conn, BT_SECURITY_L2);
    if (rc) {
        hid_log(d, "set_security failed (%d)", rc);
    }
}

static void hid_disconnected(struct bt_conn *conn, u8_t reason)
{
    hid_dev_t *d = hid_by_conn(conn);
    if (!d) {
        return;
    }
    hid_log(d, "disconnected (HCI 0x%02X)", reason);
    bt_conn_unref(d->conn);
    d->conn = NULL;
    d->state = HID_IDLE;
    /* Whatever was held is released: the desk layer sees nothing from this
     * slot from here on, and stale input must not come back on a reconnect. */
    hid_clear_input(d);
    d->boot_value_handle = 0;
    d->nsubs = 0;
    d->setup_active = false;
    d->setup_pending = NULL;
}

static void hid_security_changed(struct bt_conn *conn, bt_security_t level,
                                 enum bt_security_err err)
{
    hid_dev_t *d = hid_by_conn(conn);
    if (!d) {
        return;
    }
    if (err) {
        hid_log(d, "security failed (level %d, err %d)", level, err);
        return;
    }
    hid_log(d, "encrypted (level %d)", level);
    if (level >= BT_SECURITY_L2 && d->state == HID_SECURING) {
        hid_start_discovery(d);
    }
}

static void hid_pairing_complete(struct bt_conn *conn, bool bonded)
{
    hid_dev_t *d = hid_by_conn(conn);
    if (d) {
        hid_log(d, "paired (%s)", bonded ? "bonded, in RAM only" : "not bonded");
    }
}

static void hid_pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
    hid_dev_t *d = hid_by_conn(conn);
    if (d) {
        hid_log(d, "pairing failed (%d)", reason);
    }
}

static struct bt_conn_cb s_conn_cb = {
    .connected = hid_connected,
    .disconnected = hid_disconnected,
    .security_changed = hid_security_changed,
};

/* Only completion callbacks, no passkey ones, so the stack reports
 * NoInputNoOutput and pairs with Just Works. */
static struct bt_conn_auth_cb s_auth_cb = {
    .pairing_complete = hid_pairing_complete,
    .pairing_failed = hid_pairing_failed,
};

static void hid_register_callbacks(void)
{
    bt_conn_cb_register(&s_conn_cb);
    int err = bt_conn_auth_cb_register(&s_auth_cb);
    if (err) {
        tdsh_printf("ble: auth callback registration failed (%d)\r\n", err);
    }
}

static bool hid_parse_addr(const char *text, bt_addr_le_t *addr)
{
    unsigned v[6];
    if (sscanf(text, "%2x:%2x:%2x:%2x:%2x:%2x", &v[5], &v[4], &v[3],
               &v[2], &v[1], &v[0]) != 6) {
        return false;
    }
    for (int i = 0; i < 6; i++) {
        addr->a.val[i] = (uint8_t)v[i];
    }
    return true;
}

static void hid_watch(hid_dev_t *d, int secs)
{
    uint32_t start_reports = d->reports;
    TickType_t end = xTaskGetTickCount() + pdMS_TO_TICKS(secs * 1000);

    tdsh_printf("%s: watching for %d s\r\n", d->cmd, secs);
    d->watching = true;
    while ((int32_t)(end - xTaskGetTickCount()) > 0) {
        log_drain();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    d->watching = false;
    log_drain();
    tdsh_printf("%s: %s, %lu report(s) in this watch\r\n", d->cmd,
                s_state_names[d->state],
                (unsigned long)(d->reports - start_reports));
}

bool tang_ble_keyboard(uint8_t out[8])
{
    taskENTER_CRITICAL();
    const bool live = s_hid[HID_KBD].state == HID_READY;
    if (live) {
        memcpy(out, s_kbd_report, 8);
    } else {
        memset(out, 0, 8);
    }
    taskEXIT_CRITICAL();
    return live;
}

bool tang_ble_mouse(tang_mouse_t *out)
{
    taskENTER_CRITICAL();
    const bool live = s_hid[HID_MOUSE].state == HID_READY;
    if (live) {
        *out = s_mouse;
    } else {
        memset(out, 0, sizeof(*out));
    }
    /* Movement and wheel are handed over once; the buttons stay as they are. */
    s_mouse.dx = 0;
    s_mouse.dy = 0;
    s_mouse.wheel = 0;
    taskEXIT_CRITICAL();
    return live;
}

static int hid_usage(const hid_dev_t *d)
{
    tdsh_printf("usage: %s <AA:BB:CC:DD:EE:FF> [pub|rand] [seconds]\r\n"
                "       %s watch [seconds]\r\n"
                "       %s off\r\n"
                "       %s            (status)\r\n",
                d->cmd, d->cmd, d->cmd, d->cmd);
    return 1;
}

static int hid_command(hid_dev_t *d, int argc, char **argv)
{
    if (argc < 2) {
        log_drain();
        tdsh_printf("%s: %s", d->cmd, s_state_names[d->state]);
        if (d->state != HID_IDLE) {
            tdsh_printf(", %02X:%02X:%02X:%02X:%02X:%02X, %s protocol",
                        d->addr.a.val[5], d->addr.a.val[4], d->addr.a.val[3],
                        d->addr.a.val[2], d->addr.a.val[1], d->addr.a.val[0],
                        d->boot ? "boot" : "report");
        }
        tdsh_printf(", %lu report(s)\r\n", (unsigned long)d->reports);
        return 0;
    }

    if (strcmp(argv[1], "watch") == 0) {
        int secs = argc >= 3 ? atoi(argv[2]) : HID_DEFAULT_SECS;
        if (secs < 1 || secs > HID_MAX_SECS) {
            return hid_usage(d);
        }
        hid_watch(d, secs);
        return 0;
    }

    if (strcmp(argv[1], "off") == 0) {
        if (!d->conn) {
            tdsh_printf("%s: not connected\r\n", d->cmd);
            return 0;
        }
        int err = bt_conn_disconnect(d->conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        if (err) {
            tdsh_printf("%s: disconnect failed (%d)\r\n", d->cmd, err);
            return 1;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
        log_drain();
        return 0;
    }

    bt_addr_le_t addr = { .type = BT_ADDR_LE_RANDOM };
    int secs = HID_DEFAULT_SECS;
    if (!hid_parse_addr(argv[1], &addr)) {
        return hid_usage(d);
    }
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "pub") == 0) {
            addr.type = BT_ADDR_LE_PUBLIC;
        } else if (strcmp(argv[i], "rand") == 0) {
            addr.type = BT_ADDR_LE_RANDOM;
        } else {
            secs = atoi(argv[i]);
            if (secs < 1 || secs > HID_MAX_SECS) {
                return hid_usage(d);
            }
        }
    }

    if (d->conn) {
        tdsh_printf("%s: already %s; use '%s off' first\r\n", d->cmd,
                    s_state_names[d->state], d->cmd);
        return 1;
    }
    for (int i = 0; i < HID_SLOTS; i++) {
        const hid_dev_t *o = &s_hid[i];
        if (o == d || !o->conn) {
            continue;
        }
        if (o->state != HID_READY) {
            tdsh_printf("%s: %s is still %s; wait for it to be ready\r\n",
                        d->cmd, o->cmd, s_state_names[o->state]);
            return 1;
        }
        if (bt_addr_le_cmp(&o->addr, &addr) == 0) {
            tdsh_printf("%s: that device is already connected as %s\r\n",
                        d->cmd, o->cmd);
            return 1;
        }
    }
    if (ble_start() != 0) {
        return 1;
    }

    d->addr = addr;
    d->boot = false;
    d->boot_value_handle = 0;
    hid_clear_input(d);
    d->state = HID_CONNECTING;
    d->conn = bt_conn_create_le(&addr, BT_LE_CONN_PARAM_DEFAULT);
    if (!d->conn) {
        d->state = HID_IDLE;
        tdsh_printf("%s: could not start connection\r\n", d->cmd);
        return 1;
    }
    tdsh_printf("%s: connecting to %s (%s)\r\n", d->cmd, argv[1],
                addr.type == BT_ADDR_LE_PUBLIC ? "pub" : "rand");
    hid_watch(d, secs);
    return 0;
}

static int cmd_blekbd(tdsh_session_t *session, int argc, char **argv)
{
    (void)session;
    return hid_command(&s_hid[HID_KBD], argc, argv);
}

static int cmd_blemouse(tdsh_session_t *session, int argc, char **argv)
{
    (void)session;
    return hid_command(&s_hid[HID_MOUSE], argc, argv);
}

static const tdsh_command_t s_ble_commands[] = {
    { "blescan", "blescan [seconds]",
      "Listen for Bluetooth LE devices and list them by signal strength (dBm)",
      cmd_blescan, 0 },
    { "blekbd", "blekbd <addr> [pub|rand] [seconds] | watch [seconds] | off",
      "Connect a Bluetooth LE keyboard as an input; watch prints its key reports",
      cmd_blekbd, 0 },
    { "blemouse", "blemouse <addr> [pub|rand] [seconds] | watch [seconds] | off",
      "Connect a Bluetooth LE mouse as the desktop's pointer; watch prints its reports",
      cmd_blemouse, 0 },
};

int tang_ble_register(void)
{
    return tdsh_register_commands(s_ble_commands,
                                  sizeof(s_ble_commands) / sizeof(s_ble_commands[0]));
}
