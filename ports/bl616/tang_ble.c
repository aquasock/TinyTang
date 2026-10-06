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
// `blekbd` is the next step: connect to one BLE keyboard (HID over GATT),
// pair with Just Works and put it in boot protocol.  Boot protocol gives the
// same 8-byte report (modifiers, reserved, six keycodes) the wired keyboard
// link already carries, and tang_ble_keyboard() hands it to the desktop
// layer's poll, which types it exactly as it types the wired link's.
// `blekbd watch` prints the reports as well.  Keys are held in RAM only
// (CONFIG_BT_SETTINGS is 0), so after a reset the keyboard must be put back
// in pairing mode.

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

static void kbd_register_callbacks(void);

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

    kbd_register_callbacks();

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

/* ---- blekbd: one BLE keyboard over HID-over-GATT ---------------------- */

#define KBD_LOG_LINES     48
#define KBD_LOG_LEN       96
#define KBD_MAX_CHRCS     24
#define KBD_MAX_SUBS      8
#define KBD_DEFAULT_SECS  30
#define KBD_MAX_SECS      600

/* Bluetooth callbacks run on the BLE host's tasks.  They only append lines
 * here; the shell task drains and prints them, so the console is only ever
 * written from the shell. */
static char s_log[KBD_LOG_LINES][KBD_LOG_LEN];
static unsigned s_log_head;
static unsigned s_log_tail;
static unsigned s_log_lost;

static void kbd_log(const char *fmt, ...)
{
    char line[KBD_LOG_LEN];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);

    taskENTER_CRITICAL();
    if (s_log_head - s_log_tail >= KBD_LOG_LINES) {
        s_log_tail++;
        s_log_lost++;
    }
    memcpy(s_log[s_log_head % KBD_LOG_LINES], line, KBD_LOG_LEN);
    s_log_head++;
    taskEXIT_CRITICAL();
}

static void kbd_log_drain(void)
{
    char line[KBD_LOG_LEN];
    unsigned lost;

    for (;;) {
        bool have = false;
        taskENTER_CRITICAL();
        lost = s_log_lost;
        s_log_lost = 0;
        if (s_log_tail != s_log_head) {
            memcpy(line, s_log[s_log_tail % KBD_LOG_LINES], KBD_LOG_LEN);
            s_log_tail++;
            have = true;
        }
        taskEXIT_CRITICAL();
        if (lost) {
            tdsh_printf("blekbd: (%u line(s) lost)\r\n", lost);
        }
        if (!have) {
            return;
        }
        tdsh_printf("blekbd: %s\r\n", line);
    }
}

typedef struct {
    uint16_t uuid;
    uint16_t value_handle;
    uint16_t ccc_handle;
    uint8_t props;
} kbd_chrc_t;

typedef enum {
    KBD_IDLE,
    KBD_CONNECTING,
    KBD_SECURING,
    KBD_DISCOVERING,
    KBD_READY,
} kbd_state_t;

static const char *const s_state_names[] = {
    "idle", "connecting", "pairing", "discovering", "ready",
};

static struct bt_conn *s_kbd_conn;
static volatile kbd_state_t s_kbd_state;
static bt_addr_le_t s_kbd_addr;
static bool s_kbd_boot;

static uint16_t s_hids_start;
static uint16_t s_hids_end;
static kbd_chrc_t s_chrcs[KBD_MAX_CHRCS];
static unsigned s_nchrcs;
static uint16_t s_boot_value_handle;

static struct bt_gatt_discover_params s_disc_primary;
static struct bt_gatt_discover_params s_disc_chrc;
static struct bt_gatt_discover_params s_disc_ccc;
static struct bt_gatt_subscribe_params s_subs[KBD_MAX_SUBS];
static unsigned s_nsubs;

static volatile uint32_t s_kbd_reports;
static uint8_t s_kbd_report[8];
/* Reports are logged only while `blekbd` is watching.  The keyboard is an
 * input now, so outside a watch every keystroke would only fill the log ring
 * and come out as "lines lost" at the next command. */
static volatile bool s_kbd_watching;

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

static void kbd_log_boot_report(const uint8_t *r)
{
    static const char *const mods[] = {
        "LCtrl", "LShift", "LAlt", "LGui", "RCtrl", "RShift", "RAlt", "RGui",
    };
    char text[KBD_LOG_LEN];
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
    kbd_log("%s", text);
}

static void kbd_setup_step_done(struct bt_conn *conn, const void *op);

static u8_t kbd_notify(struct bt_conn *conn, struct bt_gatt_subscribe_params *params,
                       const void *data, u16_t length)
{
    if (!data) {
        /* The SDK's BFLB_BLE_PATCH_NOTIFY_WRITE_CCC_RSP calls this with no
         * data when the CCC write completes, as well as on failure or
         * unsubscribe, so it is reported but the subscription is kept. */
        kbd_log("CCC write for 0x%04X completed", params->value_handle);
        kbd_setup_step_done(conn, params);
        return BT_GATT_ITER_CONTINUE;
    }

    const uint8_t *p = data;
    s_kbd_reports++;
    if (params->value_handle == s_boot_value_handle && length >= 8) {
        taskENTER_CRITICAL();
        memcpy(s_kbd_report, p, 8);
        taskEXIT_CRITICAL();
        if (s_kbd_watching) {
            kbd_log_boot_report(p);
        }
    } else if (s_kbd_watching) {
        char text[KBD_LOG_LEN];
        int n = snprintf(text, sizeof(text), "report 0x%04X len %u:",
                         params->value_handle, length);
        for (unsigned i = 0; i < length && n < (int)sizeof(text) - 3; i++) {
            n += snprintf(text + n, sizeof(text) - n, " %02X", p[i]);
        }
        kbd_log("%s", text);
    }
    return BT_GATT_ITER_CONTINUE;
}

static kbd_chrc_t *kbd_find_chrc(uint16_t uuid)
{
    for (unsigned i = 0; i < s_nchrcs; i++) {
        if (s_chrcs[i].uuid == uuid) {
            return &s_chrcs[i];
        }
    }
    return NULL;
}

#define KBD_MAX_READS 10

static struct bt_gatt_read_params s_reads[KBD_MAX_READS];
static unsigned s_nreads;

/* Setup after discovery is a sequence of ATT operations: every subscription,
 * then the read-backs.  They are issued one at a time, each from the
 * previous one's completion callback.  Issuing them all at once from a host
 * callback exhausts the ATT transmit buffers, and the allocation then waits
 * forever on the very task that would free them. */
static unsigned s_setup_sub;
static unsigned s_setup_read;
static bool s_setup_active;
static const void *s_setup_pending;

static u8_t kbd_read_done(struct bt_conn *conn, u8_t err,
                          struct bt_gatt_read_params *params,
                          const void *data, u16_t length)
{
    if (err) {
        kbd_log("read 0x%04X failed (ATT 0x%02X)", params->single.handle, err);
    } else if (data) {
        const uint8_t *p = data;
        kbd_log("read 0x%04X: %u byte(s) %02X %02X", params->single.handle, length,
                length > 0 ? p[0] : 0, length > 1 ? p[1] : 0);
    }
    kbd_setup_step_done(conn, params);
    return BT_GATT_ITER_STOP;
}

static void kbd_setup_next(struct bt_conn *conn)
{
    while (s_setup_active) {
        if (s_setup_sub < s_nsubs) {
            struct bt_gatt_subscribe_params *sp = &s_subs[s_setup_sub++];
            s_setup_pending = sp;
            int err = bt_gatt_subscribe(conn, sp);
            if (err == 0) {
                return;
            }
            kbd_log("subscribe 0x%04X failed (%d)", sp->value_handle, err);
            continue;
        }
        if (s_setup_read < s_nreads) {
            struct bt_gatt_read_params *rp = &s_reads[s_setup_read++];
            s_setup_pending = rp;
            int err = bt_gatt_read(conn, rp);
            if (err == 0) {
                return;
            }
            kbd_log("read 0x%04X not sent (%d)", rp->single.handle, err);
            continue;
        }
        s_setup_active = false;
        s_setup_pending = NULL;
        s_kbd_state = KBD_READY;
        kbd_log("ready - type on the keyboard");
    }
}

static void kbd_setup_step_done(struct bt_conn *conn, const void *op)
{
    if (s_setup_active && op == s_setup_pending) {
        kbd_setup_next(conn);
    }
}

static void kbd_add_read(uint16_t handle)
{
    if (!handle || s_nreads >= KBD_MAX_READS) {
        return;
    }
    struct bt_gatt_read_params *rp = &s_reads[s_nreads++];
    memset(rp, 0, sizeof(*rp));
    rp->func = kbd_read_done;
    rp->handle_count = 1;
    rp->single.handle = handle;
}

static void kbd_add_sub(const kbd_chrc_t *c)
{
    if (s_nsubs >= KBD_MAX_SUBS) {
        return;
    }
    struct bt_gatt_subscribe_params *sp = &s_subs[s_nsubs++];
    memset(sp, 0, sizeof(*sp));
    sp->notify = kbd_notify;
    sp->value_handle = c->value_handle;
    sp->ccc_handle = c->ccc_handle;
    sp->value = BT_GATT_CCC_NOTIFY;
}

/* With every characteristic and its CCC known: switch to boot protocol if
 * the keyboard offers it, and subscribe to every notifying input in the HID
 * service (the boot keyboard input and all input reports), so reports are
 * seen whichever way the keyboard actually sends them.  The protocol mode
 * and each CCC are then read back to show what the keyboard accepted. */
static void kbd_setup_reports(struct bt_conn *conn)
{
    kbd_chrc_t *boot = kbd_find_chrc(0x2A22);
    kbd_chrc_t *mode = kbd_find_chrc(0x2A4E);

    for (unsigned i = 0; i < s_nchrcs; i++) {
        kbd_log("  chrc 0x%04X value 0x%04X ccc 0x%04X props 0x%02X",
                s_chrcs[i].uuid, s_chrcs[i].value_handle,
                s_chrcs[i].ccc_handle, s_chrcs[i].props);
    }

    s_nsubs = 0;
    s_nreads = 0;
    s_kbd_boot = false;
    if (boot && boot->ccc_handle && mode) {
        static const uint8_t boot_protocol = 0;
        int err = bt_gatt_write_without_response(conn, mode->value_handle,
                                                 &boot_protocol, 1, false);
        if (err) {
            kbd_log("protocol mode write failed (%d)", err);
        } else {
            s_kbd_boot = true;
        }
        s_boot_value_handle = boot->value_handle;
    }
    for (unsigned i = 0; i < s_nchrcs; i++) {
        const kbd_chrc_t *c = &s_chrcs[i];
        if ((c->uuid == 0x2A22 || c->uuid == 0x2A4D) &&
            (c->props & BT_GATT_CHRC_NOTIFY) && c->ccc_handle) {
            kbd_add_sub(c);
        }
    }
    if (mode) {
        kbd_add_read(mode->value_handle);
    }
    for (unsigned i = 0; i < s_nsubs; i++) {
        kbd_add_read(s_subs[i].ccc_handle);
    }
    kbd_log("%s protocol requested; subscribing to %u input(s)",
            s_kbd_boot ? "boot" : "report", s_nsubs);

    s_setup_sub = 0;
    s_setup_read = 0;
    s_setup_active = true;
    kbd_setup_next(conn);
}

static u8_t kbd_discover_ccc(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                             struct bt_gatt_discover_params *params)
{
    (void)params;
    if (!attr) {
        kbd_setup_reports(conn);
        return BT_GATT_ITER_STOP;
    }
    /* Discovery runs unfiltered (filtering by UUID found nothing on the
     * Logitech K950), so pick the CCCs out here. */
    if (attr->uuid->type != BT_UUID_TYPE_16 || BT_UUID_16(attr->uuid)->val != 0x2902) {
        return BT_GATT_ITER_CONTINUE;
    }
    /* A CCC belongs to the characteristic whose value handle precedes it. */
    kbd_chrc_t *owner = NULL;
    for (unsigned i = 0; i < s_nchrcs; i++) {
        if (s_chrcs[i].value_handle < attr->handle &&
            (!owner || s_chrcs[i].value_handle > owner->value_handle)) {
            owner = &s_chrcs[i];
        }
    }
    if (owner && !owner->ccc_handle) {
        owner->ccc_handle = attr->handle;
    }
    return BT_GATT_ITER_CONTINUE;
}

static u8_t kbd_discover_chrc(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                              struct bt_gatt_discover_params *params)
{
    (void)params;
    if (!attr) {
        kbd_log("found %u characteristic(s); finding CCC descriptors", s_nchrcs);
        memset(&s_disc_ccc, 0, sizeof(s_disc_ccc));
        s_disc_ccc.uuid = NULL;
        s_disc_ccc.func = kbd_discover_ccc;
        s_disc_ccc.start_handle = s_hids_start + 1;
        s_disc_ccc.end_handle = s_hids_end;
        s_disc_ccc.type = BT_GATT_DISCOVER_DESCRIPTOR;
        int err = bt_gatt_discover(conn, &s_disc_ccc);
        if (err) {
            kbd_log("descriptor discovery failed (%d)", err);
        }
        return BT_GATT_ITER_STOP;
    }
    const struct bt_gatt_chrc *chrc = attr->user_data;
    if (s_nchrcs < KBD_MAX_CHRCS) {
        kbd_chrc_t *c = &s_chrcs[s_nchrcs++];
        c->uuid = chrc->uuid->type == BT_UUID_TYPE_16 ? BT_UUID_16(chrc->uuid)->val : 0;
        c->value_handle = chrc->value_handle;
        c->ccc_handle = 0;
        c->props = chrc->properties;
    }
    return BT_GATT_ITER_CONTINUE;
}

static void kbd_start_chrc_discovery(struct bt_conn *conn)
{
    s_nchrcs = 0;
    memset(&s_disc_chrc, 0, sizeof(s_disc_chrc));
    s_disc_chrc.uuid = NULL;
    s_disc_chrc.func = kbd_discover_chrc;
    s_disc_chrc.start_handle = s_hids_start + 1;
    s_disc_chrc.end_handle = s_hids_end;
    s_disc_chrc.type = BT_GATT_DISCOVER_CHARACTERISTIC;
    int err = bt_gatt_discover(conn, &s_disc_chrc);
    if (err) {
        kbd_log("characteristic discovery failed (%d)", err);
    }
}

/* Every primary service is listed (useful while bringing up a new device);
 * the HID service's range is kept and walked once the list is complete. */
static u8_t kbd_discover_primary(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                 struct bt_gatt_discover_params *params)
{
    (void)params;
    if (!attr) {
        if (!s_hids_end) {
            kbd_log("no HID service on this device");
            return BT_GATT_ITER_STOP;
        }
        kbd_log("HID service 0x%04X-0x%04X", s_hids_start, s_hids_end);
        kbd_start_chrc_discovery(conn);
        return BT_GATT_ITER_STOP;
    }
    const struct bt_gatt_service_val *svc = attr->user_data;
    uint16_t uuid = svc->uuid->type == BT_UUID_TYPE_16 ? BT_UUID_16(svc->uuid)->val : 0;
    kbd_log("  service 0x%04X-0x%04X uuid %s0x%04X", attr->handle, svc->end_handle,
            svc->uuid->type == BT_UUID_TYPE_16 ? "" : "(128-bit) ", uuid);
    if (uuid == 0x1812 && !s_hids_end) {
        s_hids_start = attr->handle;
        s_hids_end = svc->end_handle;
    }
    return BT_GATT_ITER_CONTINUE;
}

static void kbd_start_discovery(struct bt_conn *conn)
{
    s_kbd_state = KBD_DISCOVERING;
    s_hids_start = 0;
    s_hids_end = 0;
    memset(&s_disc_primary, 0, sizeof(s_disc_primary));
    s_disc_primary.uuid = NULL;
    s_disc_primary.func = kbd_discover_primary;
    s_disc_primary.start_handle = 0x0001;
    s_disc_primary.end_handle = 0xFFFF;
    s_disc_primary.type = BT_GATT_DISCOVER_PRIMARY;
    int err = bt_gatt_discover(conn, &s_disc_primary);
    if (err) {
        kbd_log("service discovery failed (%d)", err);
    }
}

static void kbd_connected(struct bt_conn *conn, u8_t err)
{
    if (conn != s_kbd_conn) {
        return;
    }
    if (err) {
        kbd_log("connect failed (HCI 0x%02X)", err);
        bt_conn_unref(s_kbd_conn);
        s_kbd_conn = NULL;
        s_kbd_state = KBD_IDLE;
        return;
    }
    kbd_log("connected; requesting encryption");
    s_kbd_state = KBD_SECURING;
    int rc = bt_conn_set_security(conn, BT_SECURITY_L2);
    if (rc) {
        kbd_log("set_security failed (%d)", rc);
    }
}

static void kbd_disconnected(struct bt_conn *conn, u8_t reason)
{
    if (conn != s_kbd_conn) {
        return;
    }
    kbd_log("disconnected (HCI 0x%02X)", reason);
    bt_conn_unref(s_kbd_conn);
    s_kbd_conn = NULL;
    s_kbd_state = KBD_IDLE;
    /* Whatever was held is released: tang_ble_keyboard() reports nothing
     * from here on, and a stale report must not come back on a reconnect. */
    taskENTER_CRITICAL();
    memset(s_kbd_report, 0, sizeof(s_kbd_report));
    taskEXIT_CRITICAL();
    s_boot_value_handle = 0;
    s_nsubs = 0;
    s_setup_active = false;
    s_setup_pending = NULL;
}

static void kbd_security_changed(struct bt_conn *conn, bt_security_t level,
                                 enum bt_security_err err)
{
    if (conn != s_kbd_conn) {
        return;
    }
    if (err) {
        kbd_log("security failed (level %d, err %d)", level, err);
        return;
    }
    kbd_log("encrypted (level %d)", level);
    if (level >= BT_SECURITY_L2 && s_kbd_state == KBD_SECURING) {
        kbd_start_discovery(conn);
    }
}

static void kbd_pairing_complete(struct bt_conn *conn, bool bonded)
{
    if (conn == s_kbd_conn) {
        kbd_log("paired (%s)", bonded ? "bonded, in RAM only" : "not bonded");
    }
}

static void kbd_pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
    if (conn == s_kbd_conn) {
        kbd_log("pairing failed (%d)", reason);
    }
}

static struct bt_conn_cb s_conn_cb = {
    .connected = kbd_connected,
    .disconnected = kbd_disconnected,
    .security_changed = kbd_security_changed,
};

/* Only completion callbacks, no passkey ones, so the stack reports
 * NoInputNoOutput and pairs with Just Works. */
static struct bt_conn_auth_cb s_auth_cb = {
    .pairing_complete = kbd_pairing_complete,
    .pairing_failed = kbd_pairing_failed,
};

static void kbd_register_callbacks(void)
{
    bt_conn_cb_register(&s_conn_cb);
    int err = bt_conn_auth_cb_register(&s_auth_cb);
    if (err) {
        tdsh_printf("blekbd: auth callback registration failed (%d)\r\n", err);
    }
}

static bool kbd_parse_addr(const char *text, bt_addr_le_t *addr)
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

static void kbd_watch(int secs)
{
    uint32_t start_reports = s_kbd_reports;
    TickType_t end = xTaskGetTickCount() + pdMS_TO_TICKS(secs * 1000);

    tdsh_printf("blekbd: watching for %d s\r\n", secs);
    s_kbd_watching = true;
    while ((int32_t)(end - xTaskGetTickCount()) > 0) {
        kbd_log_drain();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    s_kbd_watching = false;
    kbd_log_drain();
    tdsh_printf("blekbd: %s, %lu report(s) in this watch\r\n",
                s_state_names[s_kbd_state],
                (unsigned long)(s_kbd_reports - start_reports));
}

bool tang_ble_keyboard(uint8_t out[8])
{
    taskENTER_CRITICAL();
    const bool live = s_kbd_state == KBD_READY;
    if (live) {
        memcpy(out, s_kbd_report, 8);
    } else {
        memset(out, 0, 8);
    }
    taskEXIT_CRITICAL();
    return live;
}

static int kbd_usage(void)
{
    tdsh_printf("usage: blekbd <AA:BB:CC:DD:EE:FF> [pub|rand] [seconds]\r\n"
                "       blekbd watch [seconds]\r\n"
                "       blekbd off\r\n"
                "       blekbd            (status)\r\n");
    return 1;
}

static int cmd_blekbd(tdsh_session_t *session, int argc, char **argv)
{
    (void)session;

    if (argc < 2) {
        kbd_log_drain();
        tdsh_printf("blekbd: %s", s_state_names[s_kbd_state]);
        if (s_kbd_state != KBD_IDLE) {
            tdsh_printf(", %02X:%02X:%02X:%02X:%02X:%02X, %s protocol",
                        s_kbd_addr.a.val[5], s_kbd_addr.a.val[4], s_kbd_addr.a.val[3],
                        s_kbd_addr.a.val[2], s_kbd_addr.a.val[1], s_kbd_addr.a.val[0],
                        s_kbd_boot ? "boot" : "report");
        }
        tdsh_printf(", %lu report(s)\r\n", (unsigned long)s_kbd_reports);
        return 0;
    }

    if (strcmp(argv[1], "watch") == 0) {
        int secs = argc >= 3 ? atoi(argv[2]) : KBD_DEFAULT_SECS;
        if (secs < 1 || secs > KBD_MAX_SECS) {
            return kbd_usage();
        }
        kbd_watch(secs);
        return 0;
    }

    if (strcmp(argv[1], "off") == 0) {
        if (!s_kbd_conn) {
            tdsh_printf("blekbd: not connected\r\n");
            return 0;
        }
        int err = bt_conn_disconnect(s_kbd_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        if (err) {
            tdsh_printf("blekbd: disconnect failed (%d)\r\n", err);
            return 1;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
        kbd_log_drain();
        return 0;
    }

    bt_addr_le_t addr = { .type = BT_ADDR_LE_RANDOM };
    int secs = KBD_DEFAULT_SECS;
    if (!kbd_parse_addr(argv[1], &addr)) {
        return kbd_usage();
    }
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "pub") == 0) {
            addr.type = BT_ADDR_LE_PUBLIC;
        } else if (strcmp(argv[i], "rand") == 0) {
            addr.type = BT_ADDR_LE_RANDOM;
        } else {
            secs = atoi(argv[i]);
            if (secs < 1 || secs > KBD_MAX_SECS) {
                return kbd_usage();
            }
        }
    }

    if (s_kbd_conn) {
        tdsh_printf("blekbd: already %s; use 'blekbd off' first\r\n",
                    s_state_names[s_kbd_state]);
        return 1;
    }
    if (ble_start() != 0) {
        return 1;
    }

    s_kbd_addr = addr;
    s_kbd_boot = false;
    s_boot_value_handle = 0;
    taskENTER_CRITICAL();
    memset(s_kbd_report, 0, sizeof(s_kbd_report));
    taskEXIT_CRITICAL();
    s_kbd_state = KBD_CONNECTING;
    s_kbd_conn = bt_conn_create_le(&addr, BT_LE_CONN_PARAM_DEFAULT);
    if (!s_kbd_conn) {
        s_kbd_state = KBD_IDLE;
        tdsh_printf("blekbd: could not start connection\r\n");
        return 1;
    }
    tdsh_printf("blekbd: connecting to %s (%s)\r\n", argv[1],
                addr.type == BT_ADDR_LE_PUBLIC ? "pub" : "rand");
    kbd_watch(secs);
    return 0;
}

static const tdsh_command_t s_ble_commands[] = {
    { "blescan", "blescan [seconds]",
      "Listen for Bluetooth LE devices and list them by signal strength (dBm)",
      cmd_blescan, 0 },
    { "blekbd", "blekbd <addr> [pub|rand] [seconds] | watch [seconds] | off",
      "Connect a Bluetooth LE keyboard as an input; watch prints its key reports",
      cmd_blekbd, 0 },
};

int tang_ble_register(void)
{
    return tdsh_register_commands(s_ble_commands,
                                  sizeof(s_ble_commands) / sizeof(s_ble_commands[0]));
}
