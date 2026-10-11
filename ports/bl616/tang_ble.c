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
#include "semphr.h"
#include "task.h"

#include "bluetooth.h"
#include "conn.h"
#include "gatt.h"
#include "uuid.h"
#include "hci_driver.h"
#include "btble_lib_api.h"
#include "rfparam_adapter.h"

#include "keys.h"
#include "ff.h"
#include "mem.h"

#include "tdsh.h"
#include "tang_ble.h"
#include "tang_ble_bonds.h"
#include "tang_heap.h"
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

/* GAP appearances (Bluetooth Assigned Numbers, 2.6.2): HID category 0x03C0. */
#define BLE_APPEARANCE_KEYBOARD 0x03C1
#define BLE_APPEARANCE_MOUSE    0x03C2

typedef struct {
    bt_addr_le_t addr;
    int8_t rssi_max;
    int8_t rssi_last;
    uint16_t seen;
    uint16_t appearance;     /* 0 if never advertised */
    bool hid;                /* advertised the HID service, 0x1812 */
    char name[BLE_NAME_LEN];
} ble_device_t;

/* Filled from the BLE host's receive task, read by the shell task.  Updates
 * are a few bytes under a critical section; nothing in it blocks. */
static ble_device_t s_devices[BLE_MAX_DEVICES];
static unsigned s_count;
static unsigned s_dropped;
static uint32_t s_reports;

static volatile int s_enable_result = 1;   /* 1 = pending, else bt_enable's err */
/* 0 not started, 1 starting, 2 started (or failed: see s_enable_result).  The
 * stack can be started by a command in the shell and by the boot-time
 * reconnect task, so the start is claimed once and the other caller waits. */
static volatile uint8_t s_start_phase;
/* Free heap just before the radio and stack came up, and just after. */
static uint32_t s_heap_before;
static uint32_t s_heap_after;

/* The Bluetooth window's requests run on the ble task as jobs
 * (tang_ble_scan_start and the rest), and the shell's commands on the shell's
 * task; both use the same code below.  What that code has to say goes to the
 * console from the shell, and is kept as the job's message, for the window to
 * show, from the ble task. */
static TaskHandle_t s_ble_task;
static char s_job_msg[96];

static void ble_say(const char *fmt, ...)
{
    char line[sizeof(s_job_msg)];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    if (s_ble_task == NULL || xTaskGetCurrentTaskHandle() != s_ble_task) {
        tdsh_printf("%s\r\n", line);
        return;
    }
    /* Without the command's name ("blekbd: "): the window shows the slot. */
    const char *text = line;
    const char *colon = strstr(line, ": ");
    if (colon != NULL && colon - line <= 8) {
        text = colon + 2;
    }
    char kept[sizeof(s_job_msg)];
    strncpy(kept, text, sizeof(kept) - 1);
    kept[sizeof(kept) - 1] = '\0';
    taskENTER_CRITICAL();
    memcpy(s_job_msg, kept, sizeof(s_job_msg));
    taskEXIT_CRITICAL();
}

/* One change at a time: a command that scans, pairs or lets a device go, or a
 * job of the window's, claims Bluetooth until it is done, and whichever comes
 * second is refused rather than queued behind a scan the user cannot see. */
static volatile bool s_busy;

static bool ble_claim(void)
{
    taskENTER_CRITICAL();
    const bool ok = !s_busy;
    if (ok) {
        s_busy = true;
    }
    taskEXIT_CRITICAL();
    return ok;
}

static void ble_release(void)
{
    s_busy = false;
}

static void bt_ready(int err)
{
    s_enable_result = err;
}

typedef struct {
    char name[BLE_NAME_LEN];
    uint16_t appearance;
    bool hid;
} ble_adv_t;

static bool adv_cb(struct bt_data *data, void *user_data)
{
    ble_adv_t *adv = user_data;

    if (data->type == BT_DATA_NAME_COMPLETE || data->type == BT_DATA_NAME_SHORTENED) {
        size_t n = data->data_len < BLE_NAME_LEN - 1 ? data->data_len : BLE_NAME_LEN - 1;
        memcpy(adv->name, data->data, n);
        adv->name[n] = '\0';
    } else if (data->type == BT_DATA_GAP_APPEARANCE && data->data_len >= 2) {
        adv->appearance = (uint16_t)(data->data[0] | (data->data[1] << 8));
    } else if (data->type == BT_DATA_UUID16_SOME || data->type == BT_DATA_UUID16_ALL) {
        for (unsigned i = 0; i + 1 < data->data_len; i += 2) {
            if ((data->data[i] | (data->data[i + 1] << 8)) == 0x1812) {
                adv->hid = true;
            }
        }
    }
    return true;
}

static void device_found(const bt_addr_le_t *addr, s8_t rssi, u8_t evtype,
                         struct net_buf_simple *buf)
{
    (void)evtype;
    ble_adv_t adv = { 0 };

    bt_data_parse(buf, adv_cb, &adv);

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
         * advertisement, so take it whenever one turns up; likewise the
         * appearance and the service list. */
        if (adv.name[0] && !dev->name[0]) {
            memcpy(dev->name, adv.name, BLE_NAME_LEN);
        }
        if (adv.appearance) {
            dev->appearance = adv.appearance;
        }
        if (adv.hid) {
            dev->hid = true;
        }
    }
    taskEXIT_CRITICAL();
}

static void hid_register_callbacks(void);
static void hid_mtu_changed(struct bt_conn *conn, int mtu);

/* Bring the radio and the host up, once.  `verbose` prints progress and
 * errors to the console, for the shell's commands; the boot-time reconnect
 * task starts it quietly, and `ble` reports the result. */
static int ble_start_ex(bool verbose)
{
    taskENTER_CRITICAL();
    const uint8_t phase = s_start_phase;
    if (phase == 0) {
        s_start_phase = 1;
    }
    taskEXIT_CRITICAL();
    if (phase != 0) {
        while (s_start_phase == 1) {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        return s_enable_result;
    }

    s_heap_before = kfree_size();
    int32_t rc = rfparam_init(0, NULL, 0);
    if (rc != 0) {
        if (verbose) {
            ble_say("ble: RF init failed (%ld)", (long)rc);
        }
        s_enable_result = (int)rc;
        s_start_phase = 2;
        return (int)rc;
    }

    btble_controller_init(configMAX_PRIORITIES - 1);
    hci_driver_init();
    s_enable_result = 1;
    int err = bt_enable(bt_ready);
    if (err) {
        s_enable_result = err;
        s_start_phase = 2;
        if (verbose) {
            ble_say("ble: bt_enable failed (%d)", err);
        }
        return err;
    }

    for (int waited = 0; s_enable_result == 1 && waited < BLE_ENABLE_WAIT_MS; waited += 20) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    if (s_enable_result == 1) {
        s_enable_result = -1;
        s_start_phase = 2;
        if (verbose) {
            ble_say("ble: BLE stack did not come up within %d ms", BLE_ENABLE_WAIT_MS);
        }
        return -1;
    }
    if (s_enable_result != 0) {
        s_start_phase = 2;
        if (verbose) {
            ble_say("ble: BLE stack failed to start (%d)", s_enable_result);
        }
        return s_enable_result;
    }

    hid_register_callbacks();
    /* BFLB_BLE_MTU_CHANGE_CB is on, so the stack reports the negotiated MTU.
     * The exchange's own completion callback carries no value, which makes
     * this the only place the number is visible. */
    bt_gatt_register_mtu_callback(hid_mtu_changed);
    s_heap_after = kfree_size();
    s_start_phase = 2;

    if (verbose) {
        bt_addr_le_t own;
        bt_get_local_public_address(&own);
        ble_say("ble: radio up, own address %02X:%02X:%02X:%02X:%02X:%02X",
                    own.a.val[5], own.a.val[4], own.a.val[3],
                    own.a.val[2], own.a.val[1], own.a.val[0]);
    }
    return 0;
}

static int ble_start(void)
{
    return ble_start_ex(true);
}

/* Listen for `secs` seconds, filling s_devices afresh, and return a copy
 * sorted strongest first.  The reconnect initiator cannot run during an
 * explicit scan, so it is stopped first and resumed after. */
static ble_device_t s_snap[BLE_MAX_DEVICES];
static unsigned s_snap_count;
static volatile bool s_scanning;
static volatile TickType_t s_scan_until;   /* while scanning: when it ends */
static void ble_auto_stop(void);
static void ble_auto_resume(void);

static int ble_scan(int secs, unsigned *count, unsigned *dropped, uint32_t *reports)
{
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
    s_scanning = true;
    ble_auto_stop();
    int err = bt_le_scan_start(&param, device_found);
    if (err) {
        s_scanning = false;
        ble_auto_resume();
        ble_say("ble: scan start failed (%d)", err);
        return err;
    }
    s_scan_until = xTaskGetTickCount() + pdMS_TO_TICKS(secs * 1000);
    vTaskDelay(pdMS_TO_TICKS(secs * 1000));
    (void)bt_le_scan_stop();
    s_scanning = false;
    ble_auto_resume();

    taskENTER_CRITICAL();
    *count = s_count;
    *dropped = s_dropped;
    *reports = s_reports;
    memcpy(s_snap, s_devices, *count * sizeof(s_snap[0]));
    taskEXIT_CRITICAL();

    /* Strongest first: the device held next to the board should top the list. */
    for (unsigned i = 1; i < *count; i++) {
        ble_device_t t = s_snap[i];
        unsigned j = i;
        while (j > 0 && s_snap[j - 1].rssi_max < t.rssi_max) {
            s_snap[j] = s_snap[j - 1];
            j--;
        }
        s_snap[j] = t;
    }
    taskENTER_CRITICAL();
    s_snap_count = *count;
    taskEXIT_CRITICAL();
    return 0;
}

static const char *ble_kind(const ble_device_t *d)
{
    if (d->appearance == BLE_APPEARANCE_KEYBOARD) {
        return "kbd";
    }
    if (d->appearance == BLE_APPEARANCE_MOUSE) {
        return "mouse";
    }
    return d->hid ? "hid" : "-";
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

    if (!ble_claim()) {
        tdsh_printf("blescan: Bluetooth is busy with the Bluetooth window; try again\r\n");
        return 1;
    }
    if (ble_start() != 0) {
        ble_release();
        return 1;
    }

    unsigned count, dropped;
    uint32_t reports;
    tdsh_printf("blescan: listening for %d s...\r\n", secs);
    const int err = ble_scan(secs, &count, &dropped, &reports);
    ble_release();
    if (err != 0) {
        return 1;
    }

    tdsh_printf("blescan: %u device(s), %lu report(s)\r\n", count, (unsigned long)reports);
    if (count) {
        tdsh_printf("  address            type  best  last  seen  kind   name\r\n");
    }
    for (unsigned i = 0; i < count; i++) {
        const ble_device_t *d = &s_snap[i];
        tdsh_printf("  %02X:%02X:%02X:%02X:%02X:%02X  %-4s  %4d  %4d  %4u  %-5s  %s\r\n",
                    d->addr.a.val[5], d->addr.a.val[4], d->addr.a.val[3],
                    d->addr.a.val[2], d->addr.a.val[1], d->addr.a.val[0],
                    d->addr.type == BT_ADDR_LE_PUBLIC ? "pub" : "rand",
                    d->rssi_max, d->rssi_last, d->seen, ble_kind(d),
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
    HID_WAITING,      /* paired and armed: the host reconnects when it appears */
} hid_state_t;

static const char *const s_state_names[] = {
    "idle", "connecting", "pairing", "discovering", "ready", "waiting",
};

typedef struct {
    const char *cmd;              /* "blekbd" or "blemouse": the log prefix */
    uint16_t boot_uuid;           /* Boot Keyboard / Boot Mouse Input */
    const char *ready_hint;
    uint16_t appearance;          /* what `pair` looks for */

    struct bt_conn *conn;         /* one reference held while set */
    char name[TANG_BOND_NAME_LEN];
    /* The paired device is to be reconnected whenever it is not connected:
     * while waiting it is on the controller's whitelist, and the reconnect
     * initiator connects it when it next advertises (ble_reconnect_update). */
    volatile bool armed;
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
    struct bt_gatt_exchange_params mtu;   /* ATT MTU, asked for once per link */

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
                  .ready_hint = "type on the keyboard",
                  .appearance = BLE_APPEARANCE_KEYBOARD },
    [HID_MOUSE] = { .cmd = "blemouse", .boot_uuid = 0x2A33,
                    .ready_hint = "move the mouse",
                    .appearance = BLE_APPEARANCE_MOUSE },
};

/* The pairings, one per slot, as kept in /sd/ble/bonds.bin.  Written by the
 * pairing callback (keys) and the commands (forget), saved by the task. */
static tang_bond_t s_bonds[TANG_BOND_SLOTS];

/* The background task's work.  Bluetooth callbacks cannot touch the card or
 * issue HCI commands of their own without risking the host's own tasks, so
 * they hand those jobs to this task. */
#define BLE_EV_BOOT 0x1u     /* load the pairings, start the stack, arm */
#define BLE_EV_SAVE 0x2u     /* write s_bonds to the card */
#define BLE_EV_ARM  0x4u     /* arm s_arm_pending, then update the reconnect */
#define BLE_EV_JOB  0x8u     /* run s_job, a request of the Bluetooth window's */
#define BLE_REARM_MS 500
#define BLE_RETRY_MS 1000

static volatile uint8_t s_arm_pending;     /* bit per slot */
static volatile bool s_auto_running;       /* the whitelist initiator is on */
static void ble_signal(uint32_t ev);
static int hid_slot(const hid_dev_t *d) { return (int)(d - s_hid); }

static void ble_arm_later(const hid_dev_t *d)
{
    taskENTER_CRITICAL();
    s_arm_pending |= (uint8_t)(1u << hid_slot(d));
    taskEXIT_CRITICAL();
    ble_signal(BLE_EV_ARM);
}

/* The keyboard's current boot report, and the mouse's movement and wheel
 * since the desk layer last asked, with its buttons now.  Written by the
 * notify callback, read by tang_ble_keyboard() and tang_ble_mouse(). */
static uint8_t s_kbd_report[8];
static tang_mouse_t s_mouse;

/* Each slot's latest line, without the command's name, for the window. */
static char s_slot_msg[HID_SLOTS][HID_LOG_LEN];

static void hid_log(const hid_dev_t *d, const char *fmt, ...)
{
    char line[HID_LOG_LEN];
    int n = snprintf(line, sizeof(line), "%s: ", d->cmd);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line + n, sizeof(line) - (size_t)n, fmt, ap);
    va_end(ap);
    log_line(line);
    taskENTER_CRITICAL();
    memcpy(s_slot_msg[hid_slot(d)], line + n, sizeof(line) - (size_t)n);
    taskEXIT_CRITICAL();
}

static hid_dev_t *hid_by_conn(struct bt_conn *conn)
{
    for (int i = 0; i < HID_SLOTS; i++) {
        if (s_hid[i].conn && s_hid[i].conn == conn) {
            return &s_hid[i];
        }
    }
    /* A reconnection made by the whitelist initiator arrives on a connection
     * object the host created itself: match it to its armed slot by address
     * and take the slot's reference here. */
    const bt_addr_le_t *dst = bt_conn_get_dst(conn);
    for (int i = 0; i < HID_SLOTS; i++) {
        hid_dev_t *d = &s_hid[i];
        if (d->armed && !d->conn && dst && bt_addr_le_cmp(dst, &d->addr) == 0) {
            d->conn = bt_conn_ref(conn);
            return d;
        }
    }
    return NULL;
}

/* The input a slot hands the desk layer, cleared so nothing stays held. */
/* The link starts at the default ATT MTU of 23 -- smaller than the HID Report
 * Map and too small for any input report that is not a keyboard's eight bytes.
 * Nothing here ever asked for more, so every read was capped at 22 and the
 * controller's map was unreachable.  Ask once per connection (the API permits
 * no more than that) and log what the stack says the link settled on. */
static void hid_mtu_changed(struct bt_conn *conn, int mtu)
{
    hid_dev_t *d = hid_by_conn(conn);
    if (d) {
        hid_log(d, "mtu now %d", mtu);
    }
}

static void hid_mtu_exchanged(struct bt_conn *conn, u8_t err,
                              struct bt_gatt_exchange_params *params)
{
    hid_dev_t *d = hid_by_conn(conn);
    if (d && err) {
        hid_log(d, "mtu exchange failed (ATT 0x%02X)", err);
    }
}

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
        if (length <= 4) {
            /* A CCC read-back: two bytes, and the old line said it all. */
            hid_log(d, "read 0x%04X: %u byte(s) %02X %02X", params->single.handle, length,
                    length > 0 ? p[0] : 0, length > 1 ? p[1] : 0);
        } else {
            /* A Report Map is tens of bytes and useless truncated; a log line
             * is 96, so it goes out in chunks with the offset on each. */
            for (unsigned off = 0; off < length; off += 12) {
                char hex[HID_LOG_LEN];
                unsigned n = length - off < 12 ? (unsigned)(length - off) : 12;
                unsigned k = 0;
                for (unsigned i = 0; i < n; i++) {
                    k += (unsigned)snprintf(hex + k, sizeof(hex) - k, "%02X ",
                                            p[off + i]);
                }
                hid_log(d, "read 0x%04X [%u]: %s", params->single.handle,
                        params->single.offset + off, hex);
            }
        }
    }
    /* A value longer than the ATT MTU does not arrive whole, and the stack will
     * not fetch the rest for us: gatt.h is explicit that the caller reads the
     * remainder by handle and offset.  A chunk that filled the MTU means there
     * is probably more, so ask for it from where this one stopped and let the
     * next call land here again; anything shorter ended the value. */
    const u16_t mtu = bt_gatt_get_mtu(conn);
    if (!err && data && length && mtu > 1 && length >= mtu - 1) {
        params->single.offset += length;
        if (bt_gatt_read(conn, params) == 0) {
            return BT_GATT_ITER_STOP;
        }
        hid_log(d, "read 0x%04X continuation not sent", params->single.handle);
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
        /* HID over GATT: a device that considers itself SUSPENDED sends no
         * reports until the host writes Exit Suspend (0x01) to the HID Control
         * Point (0x2A4C), which nothing here ever did.  The Xbox controller is
         * the first device to need it: its input report's CCC is enabled and
         * acknowledged -- 0x001F reads back 0x0001 -- and it still sends
         * nothing, while the K950 and M750 report without it.  Written after
         * the subscriptions, so the device is listening when it resumes. */
        hid_chrc_t *control_point = hid_find_chrc(d, 0x2A4C);
        if (control_point) {
            static const uint8_t exit_suspend = 0x01;
            const int rc = bt_gatt_write_without_response(
                    d->conn, control_point->value_handle, &exit_suspend, 1, false);
            hid_log(d, "exit suspend -> 0x%04X (%d)",
                    control_point->value_handle, rc);
        }
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
    /* The Report Map is the only thing that says which bit of an input report
     * is which button.  A gamepad's report is not a keyboard's, and reading
     * this characteristic is what turns an opaque notification into a control
     * layout.  Nothing here has ever read it. */
    hid_chrc_t *map = hid_find_chrc(d, 0x2A4B);
    if (map) {
        hid_add_read(d, map->value_handle);
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
        /* Only the slots create connections, so this is one a slot let go of
         * while it was being made -- `off` racing a reconnect.  Nobody would
         * set it up or read it, so it is closed rather than left up. */
        if (!err) {
            (void)bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        }
        ble_signal(BLE_EV_ARM);
        return;
    }
    if (err) {
        hid_log(d, "connect failed (HCI 0x%02X)", err);
        bt_conn_unref(d->conn);
        d->conn = NULL;
        if (d->armed) {
            d->state = HID_WAITING;
            ble_arm_later(d);
        } else {
            d->state = HID_IDLE;
            ble_signal(BLE_EV_ARM);       /* an explicit connect ended */
        }
        return;
    }
    if (d->armed && d->state == HID_WAITING) {
        /* The whitelist initiator stops once it has connected something, so
         * it is started again for whatever is still waiting. */
        s_auto_running = false;
    }
    ble_signal(BLE_EV_ARM);
    hid_log(d, "connected; requesting encryption");
    d->state = HID_SECURING;
    int rc = bt_conn_set_security(conn, BT_SECURITY_L2);
    if (rc) {
        hid_log(d, "set_security failed (%d)", rc);
    }
    /* Ask for a wider ATT MTU before discovery, so the Report Map read and any
     * report wider than a keyboard's eight bytes have room.  Sent alongside
     * SMP rather than after it: the two ride different L2CAP channels, so they
     * do not contend for the ATT buffers that setup is careful to serialise. */
    memset(&d->mtu, 0, sizeof(d->mtu));
    d->mtu.func = hid_mtu_exchanged;
    rc = bt_gatt_exchange_mtu(conn, &d->mtu);
    if (rc) {
        hid_log(d, "mtu exchange not sent (%d)", rc);
    }
}

static void hid_disconnected(struct bt_conn *conn, u8_t reason)
{
    hid_dev_t *d = hid_by_conn(conn);
    if (!d) {
        return;
    }
    hid_log(d, "disconnected (HCI 0x%02X)%s", reason,
            d->armed ? "; waiting for it to come back" : "");
    bt_conn_unref(d->conn);
    d->conn = NULL;
    if (d->armed) {
        /* Back on the whitelist, to be reconnected when it next advertises. */
        d->state = HID_WAITING;
        ble_arm_later(d);
    } else {
        d->state = HID_IDLE;
        ble_signal(BLE_EV_ARM);           /* off the whitelist, if it was on */
    }
    /* Whatever was held is released: the desk layer sees nothing from this
     * slot from here on, and stale input must not come back on a reconnect. */
    hid_clear_input(d);
    d->boot_value_handle = 0;
    d->nsubs = 0;
    d->setup_active = false;
    d->setup_pending = NULL;
}

static void hid_drop_key(hid_dev_t *d);

static void hid_security_changed(struct bt_conn *conn, bt_security_t level,
                                 enum bt_security_err err)
{
    hid_dev_t *d = hid_by_conn(conn);
    if (!d) {
        return;
    }
    if (err) {
        hid_log(d, "security failed (level %d, err %d)", level, err);
        /* A device that has re-entered pairing mode, or been reset, holds no
         * key while this board still holds one for it.  The stack then keeps
         * trying to ENCRYPT with a key the device no longer has -- on the Xbox
         * controller that was six failed attempts in a row and then a seventh,
         * and only a manual `forget` ever broke it.
         *
         * Only PIN_OR_KEY_MISSING is acted on.  An UNSPECIFIED failure (err 8)
         * is what this board reports on signal alone, and BLE-001 says to
         * expect it and retry, not to read it as the device refusing -- so
         * dropping the key on it would throw away a good pairing every time the
         * link is marginal, which is exactly what the pad made it do. */
        if (err == BT_SECURITY_ERR_PIN_OR_KEY_MISSING &&
            d->state == HID_SECURING && s_bonds[hid_slot(d)].valid) {
            hid_log(d, "stored key refused; dropping it so the next try pairs");
            hid_drop_key(d);
        }
        return;
    }
    hid_log(d, "encrypted (level %d)", level);
    if (level >= BT_SECURITY_L2 && d->state == HID_SECURING) {
        hid_start_discovery(d);
    }
}

/* Copy the host's key entry for this device into the slot's pairing record.
 * Only what a central needs to encrypt to the device again is kept: its LTK
 * (legacy or LE Secure Connections) and IRK, with their flags. */
static bool hid_capture_bond(hid_dev_t *d, struct bt_conn *conn)
{
    const bt_addr_le_t *dst = bt_conn_get_dst(conn);
    struct bt_keys *k = bt_keys_find_addr(BT_ID_DEFAULT, dst);
    if (!k || !(k->keys & (BT_KEYS_LTK | BT_KEYS_LTK_P256))) {
        return false;
    }
    tang_bond_t b;
    memset(&b, 0, sizeof(b));
    b.valid = true;
    b.addr_type = dst->type;
    memcpy(b.addr, dst->a.val, 6);
    memcpy(b.name, d->name, sizeof(b.name));
    b.enc_size = k->enc_size;
    b.flags = k->flags & (BT_KEYS_AUTHENTICATED | BT_KEYS_SC);
    b.keys = k->keys & (BT_KEYS_LTK | BT_KEYS_LTK_P256 | BT_KEYS_IRK);
    memcpy(b.ltk_rand, k->ltk.rand, 8);
    memcpy(b.ltk_ediv, k->ltk.ediv, 2);
    memcpy(b.ltk_val, k->ltk.val, 16);
    memcpy(b.irk_val, k->irk.val, 16);
    taskENTER_CRITICAL();
    s_bonds[hid_slot(d)] = b;
    taskEXIT_CRITICAL();
    return true;
}

static void hid_pairing_complete(struct bt_conn *conn, bool bonded)
{
    hid_dev_t *d = hid_by_conn(conn);
    if (!d) {
        return;
    }
    if (bonded && hid_capture_bond(d, conn)) {
        hid_log(d, "paired (bonded; saving to the card)");
        ble_signal(BLE_EV_SAVE);
        ble_arm_later(d);
    } else {
        hid_log(d, "paired (%s)", bonded ? "bonded, but no key to keep" : "not bonded");
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

/* ---- pairings on the card, and reconnecting --------------------------- */

#define BLE_BONDS_DIR  "/sd/ble"
#define BLE_BONDS_PATH "/sd/ble/bonds.bin"

static const char *volatile s_bonds_state = "not read";

static void bonds_load(void)
{
    static FIL f;
    static uint8_t buf[TANG_BOND_FILE_LEN + 1];
    tang_bond_t b[TANG_BOND_SLOTS];
    UINT got = 0;

    if (f_open(&f, BLE_BONDS_PATH, FA_READ) != FR_OK) {
        s_bonds_state = "none on the card";
        return;
    }
    const FRESULT r = f_read(&f, buf, sizeof(buf), &got);
    (void)f_close(&f);
    if (r != FR_OK || tang_bonds_decode(buf, got, b) != 0) {
        s_bonds_state = "unreadable, ignored";
        return;
    }
    taskENTER_CRITICAL();
    memcpy(s_bonds, b, sizeof(s_bonds));
    taskEXIT_CRITICAL();
    s_bonds_state = "loaded from the card";
}

static void bonds_save(void)
{
    static FIL f;
    static uint8_t buf[TANG_BOND_FILE_LEN];
    tang_bond_t b[TANG_BOND_SLOTS];

    taskENTER_CRITICAL();
    memcpy(b, s_bonds, sizeof(b));
    taskEXIT_CRITICAL();
    const size_t n = tang_bonds_encode(b, buf);

    (void)f_mkdir(BLE_BONDS_DIR);              /* FR_EXIST is fine */
    UINT put = 0;
    FRESULT r = f_open(&f, BLE_BONDS_PATH, FA_CREATE_ALWAYS | FA_WRITE);
    if (r == FR_OK) {
        r = f_write(&f, buf, (UINT)n, &put);
        const FRESULT c = f_close(&f);
        if (r == FR_OK) {
            r = c;
        }
    }
    s_bonds_state = (r == FR_OK && put == n) ? "saved to the card" : "save to the card failed";
}

static bt_addr_le_t bond_addr(const tang_bond_t *b)
{
    bt_addr_le_t a = { .type = b->addr_type };
    memcpy(a.a.val, b->addr, 6);
    return a;
}

/* Put a pairing back into the host's key table, as if the pairing had just
 * happened, so the next connection encrypts with it instead of pairing. */
static void hid_restore_keys(int i)
{
    tang_bond_t b;
    taskENTER_CRITICAL();
    b = s_bonds[i];
    taskEXIT_CRITICAL();
    if (!b.valid) {
        return;
    }
    const bt_addr_le_t a = bond_addr(&b);
    struct bt_keys *k = bt_keys_get_addr(BT_ID_DEFAULT, &a);
    if (!k) {
        hid_log(&s_hid[i], "no room in the key table for the pairing");
        return;
    }
    k->enc_size = b.enc_size;
    k->flags = b.flags;
    memcpy(k->ltk.rand, b.ltk_rand, 8);
    memcpy(k->ltk.ediv, b.ltk_ediv, 2);
    memcpy(k->ltk.val, b.ltk_val, 16);
    memcpy(k->irk.val, b.irk_val, 16);
    k->keys |= b.keys;
}

/* The reconnect initiator.
 *
 * This SDK builds the host with CONFIG_BT_WHITELIST, so there is no
 * per-device auto-connect: there is one initiator, which connects whichever
 * whitelisted device advertises first and then stops; the whitelist cannot
 * change while it runs; and it cannot run beside an explicit scan or
 * connect.  So this keeps the whitelist equal to the paired devices that are
 * waiting, and starts the initiator again after every connection, every
 * disconnection and every scan.  The task and the shell both call in, so it
 * runs under a lock. */
static SemaphoreHandle_t s_auto_lock;
static uint8_t s_wl_mask;                  /* slots on the whitelist */
static volatile bool s_auto_retry;         /* starting it failed: try again */

static void auto_lock(void)
{
    if (s_auto_lock) {
        xSemaphoreTake(s_auto_lock, portMAX_DELAY);
    }
}

static void auto_unlock(void)
{
    if (s_auto_lock) {
        xSemaphoreGive(s_auto_lock);
    }
}

static bool ble_up(void)
{
    return s_start_phase == 2 && s_enable_result == 0;
}

static void ble_auto_stop(void)
{
    if (!ble_up()) {
        return;
    }
    auto_lock();
    if (s_auto_running) {
        (void)bt_conn_create_auto_stop();
        s_auto_running = false;
    }
    auto_unlock();
}

static void ble_auto_resume(void)
{
    ble_signal(BLE_EV_ARM);
}

static void ble_reconnect_update(void)
{
    if (!ble_up()) {
        return;
    }
    auto_lock();
    s_auto_retry = false;
    bool busy = s_scanning;
    uint8_t want = 0;
    for (int i = 0; i < HID_SLOTS; i++) {
        const hid_dev_t *d = &s_hid[i];
        if (d->state == HID_CONNECTING && !d->armed) {
            busy = true;                   /* an explicit connect is pending */
        }
        if (d->armed && d->state == HID_WAITING && !d->conn) {
            want |= (uint8_t)(1u << i);
        }
    }
    if (busy || (s_auto_running && want == s_wl_mask)) {
        auto_unlock();
        return;
    }
    if (s_auto_running) {
        (void)bt_conn_create_auto_stop();
        s_auto_running = false;
    }
    (void)bt_le_whitelist_clear();
    s_wl_mask = 0;
    for (int i = 0; i < HID_SLOTS; i++) {
        if ((want & (1u << i)) && bt_le_whitelist_add(&s_hid[i].addr) == 0) {
            s_wl_mask |= (uint8_t)(1u << i);
        }
    }
    if (s_wl_mask) {
        const int err = bt_conn_create_auto_le(BT_LE_CONN_PARAM_DEFAULT);
        if (err == 0 || err == -EALREADY) {
            s_auto_running = true;
        } else {
            s_auto_retry = true;
        }
    }
    auto_unlock();
}

/* Mark the slot's paired device to be reconnected, from the task or the
 * shell -- never from a Bluetooth callback, since it issues HCI commands. */
static void hid_arm(int i)
{
    hid_dev_t *d = &s_hid[i];
    tang_bond_t b;
    taskENTER_CRITICAL();
    b = s_bonds[i];
    taskEXIT_CRITICAL();
    if (!b.valid) {
        return;
    }
    if (!d->armed) {
        d->addr = bond_addr(&b);
        memcpy(d->name, b.name, sizeof(d->name));
        d->armed = true;
    }
    if (!d->conn && d->state == HID_IDLE) {
        d->state = HID_WAITING;
    }
    ble_reconnect_update();
}

/* Stop reconnecting and let the device go, keeping its pairing. */
static void hid_off(hid_dev_t *d)
{
    d->armed = false;
    if (!d->conn) {
        d->state = HID_IDLE;
        hid_clear_input(d);
        ble_reconnect_update();            /* off the whitelist */
        return;
    }
    int err = bt_conn_disconnect(d->conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    if (err) {
        ble_say("%s: disconnect failed (%d)", d->cmd, err);
    }
    vTaskDelay(pdMS_TO_TICKS(500));
}

/* Drop the slot's stored key -- on the card and in the host's table -- and
 * leave everything else alone, so the slot stays armed, connects again and
 * pairs afresh.  This is `forget` without the disconnect and the disarm, which
 * are exactly what a failing reconnect must not do. */
static void hid_drop_key(hid_dev_t *d)
{
    const int i = hid_slot(d);
    tang_bond_t b;
    taskENTER_CRITICAL();
    b = s_bonds[i];
    memset(&s_bonds[i], 0, sizeof(s_bonds[i]));
    taskEXIT_CRITICAL();
    if (!b.valid) {
        return;
    }
    if (ble_up()) {
        const bt_addr_le_t a = bond_addr(&b);
        (void)bt_unpair(BT_ID_DEFAULT, &a);
    }
    d->name[0] = '\0';
    ble_signal(BLE_EV_SAVE);
}

/* Let the device go and delete its pairing from the host and the card. */
static void hid_forget(hid_dev_t *d)
{
    hid_off(d);
    hid_drop_key(d);
}

/* At boot: read the pairings, and if there are any, start the stack quietly,
 * put the keys back and wait for the devices.  Run after the boot script, so
 * the radio is not starting while a core is being programmed. */
static void ble_boot(void)
{
    bonds_load();
    bool any = false;
    for (int i = 0; i < HID_SLOTS; i++) {
        any = any || s_bonds[i].valid;
    }
    if (!any || ble_start_ex(false) != 0) {
        return;
    }
    for (int i = 0; i < HID_SLOTS; i++) {
        hid_restore_keys(i);
        hid_arm(i);
    }
}

static void ble_run_job(void);

static void ble_task(void *arg)
{
    (void)arg;
    for (;;) {
        uint32_t ev = 0;
        const TickType_t wait = s_auto_retry ? pdMS_TO_TICKS(BLE_RETRY_MS) : portMAX_DELAY;
        if (xTaskNotifyWait(0, 0xFFFFFFFFu, &ev, wait) != pdTRUE) {
            ble_reconnect_update();        /* the retry */
            continue;
        }
        if (ev & BLE_EV_BOOT) {
            ble_boot();
        }
        if (ev & BLE_EV_SAVE) {
            bonds_save();
        }
        if (ev & BLE_EV_JOB) {
            ble_run_job();
        }
        if (ev & BLE_EV_ARM) {
            /* A short pause, so a device that keeps failing to connect is
             * retried at a walk rather than in a tight loop. */
            vTaskDelay(pdMS_TO_TICKS(BLE_REARM_MS));
            taskENTER_CRITICAL();
            const uint8_t m = s_arm_pending;
            s_arm_pending = 0;
            taskEXIT_CRITICAL();
            for (int i = 0; i < HID_SLOTS; i++) {
                if (m & (1u << i)) {
                    hid_arm(i);
                }
            }
            ble_reconnect_update();
        }
    }
}

static bool ble_task_start(void)
{
    if (!s_auto_lock) {
        s_auto_lock = xSemaphoreCreateMutex();
    }
    if (!s_ble_task) {
        if (xTaskCreate(ble_task, "ble", 2048, NULL, 3, &s_ble_task) != pdPASS) {
            s_ble_task = NULL;
        }
    }
    return s_ble_task != NULL;
}

static void ble_signal(uint32_t ev)
{
    if (s_ble_task) {
        (void)xTaskNotify(s_ble_task, ev, eSetBits);
    }
}

void tang_ble_boot(void)
{
    static bool done;
    if (done) {
        return;
    }
    done = true;
    if (ble_task_start()) {
        ble_signal(BLE_EV_BOOT);
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
    tdsh_printf("usage: %s pair [name]       pair the strongest one advertising (or by name)\r\n"
                "       %s <AA:BB:CC:DD:EE:FF> [pub|rand] [seconds]\r\n"
                "       %s watch [seconds]   print its reports\r\n"
                "       %s off | on          stop / resume reconnecting (pairing kept)\r\n"
                "       %s forget            delete the pairing from the board and the card\r\n"
                "       %s                   status\r\n",
                d->cmd, d->cmd, d->cmd, d->cmd, d->cmd, d->cmd);
    return 1;
}

static void hid_status(const hid_dev_t *d)
{
    tang_bond_t b;
    taskENTER_CRITICAL();
    b = s_bonds[hid_slot(d)];
    taskEXIT_CRITICAL();

    tdsh_printf("%s: %s", d->cmd, s_state_names[d->state]);
    if (d->state != HID_IDLE) {
        tdsh_printf(", %02X:%02X:%02X:%02X:%02X:%02X", d->addr.a.val[5], d->addr.a.val[4],
                    d->addr.a.val[3], d->addr.a.val[2], d->addr.a.val[1], d->addr.a.val[0]);
        if (d->name[0]) {
            tdsh_printf(" %s", d->name);
        }
    }
    if (d->state == HID_READY) {
        tdsh_printf(", %s protocol", d->boot ? "boot" : "report");
    }
    tdsh_printf(", %lu report(s)", (unsigned long)d->reports);
    if (b.valid) {
        tdsh_printf("; paired with %02X:%02X:%02X:%02X:%02X:%02X%s%s%s\r\n",
                    b.addr[5], b.addr[4], b.addr[3], b.addr[2], b.addr[1], b.addr[0],
                    b.name[0] ? " " : "", b.name,
                    d->armed ? ", reconnects by itself" : ", not reconnecting (`on`)");
    } else {
        tdsh_printf("; not paired\r\n");
    }
}

static bool name_has(const char *name, const char *part)
{
    const size_t n = strlen(part);
    for (const char *p = name; *p; p++) {
        size_t i = 0;
        while (i < n && p[i] &&
               (p[i] | 0x20) == (part[i] | 0x20)) {
            i++;
        }
        if (i == n) {
            return true;
        }
    }
    return n == 0;
}

/* Connect to `addr` and pair, into slot `d`, replacing any pairing it had,
 * then watch it for `secs` seconds (none for a job of the window's). */
static int hid_connect(hid_dev_t *d, const bt_addr_le_t *addr, const char *name, int secs)
{
    if (d->conn && !d->armed) {
        ble_say("%s: already %s; use '%s off' first", d->cmd,
                s_state_names[d->state], d->cmd);
        return 1;
    }
    for (int i = 0; i < HID_SLOTS; i++) {
        const hid_dev_t *o = &s_hid[i];
        if (o == d || !o->conn) {
            continue;
        }
        if (o->state != HID_READY && o->state != HID_WAITING) {
            ble_say("%s: %s is still %s; wait for it to be ready",
                    d->cmd, o->cmd, s_state_names[o->state]);
            return 1;
        }
        if (bt_addr_le_cmp(&o->addr, addr) == 0) {
            ble_say("%s: that device is %s's", d->cmd, o->cmd);
            return 1;
        }
    }
    if (ble_start() != 0) {
        return 1;
    }

    /* One device per slot: whatever this slot had paired is let go. */
    hid_forget(d);

    d->addr = *addr;
    d->boot = false;
    d->boot_value_handle = 0;
    memset(d->name, 0, sizeof(d->name));
    if (name) {
        strncpy(d->name, name, sizeof(d->name) - 1);
    }
    hid_clear_input(d);
    d->state = HID_CONNECTING;             /* holds the initiator off */
    ble_auto_stop();
    d->conn = bt_conn_create_le(addr, BT_LE_CONN_PARAM_DEFAULT);
    if (!d->conn) {
        d->state = HID_IDLE;
        ble_auto_resume();
        ble_say("%s: could not start connection", d->cmd);
        return 1;
    }
    char text[18];
    snprintf(text, sizeof(text), "%02X:%02X:%02X:%02X:%02X:%02X",
             addr->a.val[5], addr->a.val[4], addr->a.val[3],
             addr->a.val[2], addr->a.val[1], addr->a.val[0]);
    ble_say("%s: connecting to %s (%s)%s%s", d->cmd, text,
            addr->type == BT_ADDR_LE_PUBLIC ? "pub" : "rand",
            d->name[0] ? " " : "", d->name);
    if (secs > 0) {
        hid_watch(d, secs);                /* the shell's; a job returns at once */
    }
    return 0;
}

#define HID_PAIR_SCAN_SECS 8
#define HID_PAIR_WATCH_SECS 8

static int hid_pair(hid_dev_t *d, const char *part)
{
    if (ble_start() != 0) {
        return 1;
    }
    const char *kind = d->appearance == BLE_APPEARANCE_KEYBOARD ? "keyboard" : "mouse";
    if (part) {
        tdsh_printf("%s: looking for \"%s\" for %d s...\r\n", d->cmd, part, HID_PAIR_SCAN_SECS);
    } else {
        tdsh_printf("%s: looking for a %s in pairing mode for %d s...\r\n", d->cmd, kind,
                    HID_PAIR_SCAN_SECS);
    }
    unsigned count, dropped;
    uint32_t reports;
    if (ble_scan(HID_PAIR_SCAN_SECS, &count, &dropped, &reports) != 0) {
        return 1;
    }
    const ble_device_t *pick = NULL;
    for (unsigned i = 0; i < count && !pick; i++) {
        const ble_device_t *c = &s_snap[i];
        if (part ? !name_has(c->name, part) : c->appearance != d->appearance) {
            continue;
        }
        bool taken = false;
        for (int j = 0; j < HID_SLOTS; j++) {
            taken = taken || (&s_hid[j] != d && s_hid[j].conn &&
                              bt_addr_le_cmp(&s_hid[j].addr, &c->addr) == 0);
        }
        if (!taken) {
            pick = c;
        }
    }
    if (!pick) {
        tdsh_printf("%s: none found; is it in pairing mode and near the board?\r\n", d->cmd);
        for (unsigned i = 0; i < count; i++) {
            const ble_device_t *c = &s_snap[i];
            if (c->hid || c->appearance) {
                tdsh_printf("  seen: %s %s (%d dBm)\r\n", ble_kind(c),
                            c->name[0] ? c->name : "-", c->rssi_max);
            }
        }
        if (!part) {
            tdsh_printf("  a device that does not advertise as a %s can be paired by name: "
                        "%s pair <name>\r\n", kind, d->cmd);
        }
        return 1;
    }
    tdsh_printf("%s: found %s (%d dBm)\r\n", d->cmd, pick->name[0] ? pick->name : "-",
                pick->rssi_max);
    return hid_connect(d, &pick->addr, pick->name, HID_PAIR_WATCH_SECS);
}

static int hid_command_claimed(hid_dev_t *d, int argc, char **argv)
{
    if (argc < 2) {
        log_drain();
        hid_status(d);
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

    if (strcmp(argv[1], "pair") == 0) {
        return hid_pair(d, argc >= 3 ? argv[2] : NULL);
    }

    if (strcmp(argv[1], "off") == 0) {
        if (!d->conn && !d->armed) {
            tdsh_printf("%s: not connected\r\n", d->cmd);
            return 0;
        }
        hid_off(d);
        log_drain();
        return 0;
    }

    if (strcmp(argv[1], "on") == 0) {
        if (!s_bonds[hid_slot(d)].valid) {
            tdsh_printf("%s: not paired; use '%s pair'\r\n", d->cmd, d->cmd);
            return 1;
        }
        if (ble_start() != 0) {
            return 1;
        }
        hid_restore_keys(hid_slot(d));
        hid_arm(hid_slot(d));
        hid_status(d);
        return 0;
    }

    if (strcmp(argv[1], "forget") == 0) {
        hid_forget(d);
        log_drain();
        tdsh_printf("%s: pairing deleted\r\n", d->cmd);
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
    /* The name, if the last scan saw this address. */
    const char *name = NULL;
    for (unsigned i = 0; i < BLE_MAX_DEVICES && !name; i++) {
        if (s_snap[i].seen && bt_addr_le_cmp(&s_snap[i].addr, &addr) == 0 && s_snap[i].name[0]) {
            name = s_snap[i].name;
        }
    }
    return hid_connect(d, &addr, name, secs);
}

static int hid_command(hid_dev_t *d, int argc, char **argv)
{
    /* Status and watch only read; everything else may scan, connect or let a
     * device go, and must not overlap a job of the Bluetooth window's. */
    if (argc < 2 || strcmp(argv[1], "watch") == 0) {
        return hid_command_claimed(d, argc, argv);
    }
    if (!ble_claim()) {
        tdsh_printf("%s: Bluetooth is busy with the Bluetooth window; try again\r\n", d->cmd);
        return 1;
    }
    const int rc = hid_command_claimed(d, argc, argv);
    ble_release();
    return rc;
}

/* ---- the Bluetooth window's jobs ------------------------------------------ */

#define BLE_WINDOW_SCAN_SECS 8
/* Starting the radio takes about 15.8 KB of heap for good (BLE-002, BLE-014);
 * the window will not start it with less than this free. */
#define BLE_START_MIN_FREE   (32u * 1024u)

typedef struct {
    tang_ble_job_t kind;
    int slot;
    bt_addr_le_t addr;
    char name[BLE_NAME_LEN];
} ble_job_t;

static ble_job_t s_job;
static volatile tang_ble_job_t s_job_kind;
static volatile bool s_job_running;
static volatile bool s_job_failed;
static char s_reason[96];

/* On the ble task, holding the claim its request took. */
static void ble_run_job(void)
{
    const ble_job_t j = s_job;
    hid_dev_t *d = &s_hid[j.slot];
    int rc = 0;
    switch (j.kind) {
    case TANG_BLE_JOB_SCAN: {
        rc = ble_start();
        unsigned count = 0, dropped;
        uint32_t reports;
        if (rc == 0) {
            rc = ble_scan(BLE_WINDOW_SCAN_SECS, &count, &dropped, &reports);
        }
        if (rc == 0) {
            ble_say("%u device(s) found", count);
        }
        break;
    }
    case TANG_BLE_JOB_PAIR:
        rc = hid_connect(d, &j.addr, j.name[0] ? j.name : NULL, 0);
        break;
    case TANG_BLE_JOB_ON:
        if (!s_bonds[j.slot].valid) {
            ble_say("not paired");
            rc = 1;
        } else if ((rc = ble_start()) == 0) {
            hid_restore_keys(j.slot);
            hid_arm(j.slot);
            ble_say("reconnects by itself");
        }
        break;
    case TANG_BLE_JOB_OFF:
        hid_off(d);
        ble_say("not reconnecting; pairing kept");
        break;
    case TANG_BLE_JOB_FORGET:
        hid_forget(d);
        ble_say("pairing deleted");
        break;
    case TANG_BLE_JOB_NONE:
        break;
    }
    s_job_failed = rc != 0;
    s_job_running = false;
    ble_release();
}

static const char *job_submit(const ble_job_t *j)
{
    if (s_ble_task == NULL) {
        return "Bluetooth did not start at boot";
    }
    if (!ble_claim()) {
        return "Bluetooth is busy; try again when it has finished";
    }
    s_job = *j;
    s_job_kind = j->kind;
    s_job_failed = false;
    taskENTER_CRITICAL();
    s_job_msg[0] = '\0';
    taskEXIT_CRITICAL();
    s_job_running = true;
    ble_signal(BLE_EV_JOB);
    return NULL;
}

static bool slot_ok(int slot)
{
    return slot >= 0 && slot < HID_SLOTS;
}

const char *tang_ble_scan_start(void)
{
    /* A scan beside a connection still being set up would collide with it,
     * as an explicit connect holds the reconnect initiator off. */
    for (int i = 0; i < HID_SLOTS; i++) {
        const hid_state_t st = s_hid[i].state;
        if (st == HID_CONNECTING || st == HID_SECURING || st == HID_DISCOVERING) {
            snprintf(s_reason, sizeof(s_reason), "wait for the %s to finish %s",
                     i == HID_KBD ? "keyboard" : "mouse", s_state_names[st]);
            return s_reason;
        }
    }
    if (s_start_phase == 2 && s_enable_result != 0) {
        return "the radio failed to start; see ble in the Terminal";
    }
    if (s_start_phase == 0) {
        const uint32_t free_now = kfree_size();
        if (free_now < BLE_START_MIN_FREE) {
            snprintf(s_reason, sizeof(s_reason),
                     "starting the radio needs %u KB free; %lu KB is free",
                     BLE_START_MIN_FREE / 1024u, (unsigned long)(free_now / 1024u));
            return s_reason;
        }
    }
    const ble_job_t j = { .kind = TANG_BLE_JOB_SCAN };
    return job_submit(&j);
}

int tang_ble_scan_results(tang_ble_device_t *out, int max)
{
    int n = 0;
    taskENTER_CRITICAL();
    const unsigned count = s_scanning ? 0 : s_snap_count;
    for (unsigned i = 0; i < count && n < max; i++, n++) {
        const ble_device_t *d = &s_snap[i];
        tang_ble_device_t *o = &out[n];
        memcpy(o->addr, d->addr.a.val, sizeof(o->addr));
        o->addr_type = d->addr.type;
        o->rssi = d->rssi_max;
        o->kind = d->appearance == BLE_APPEARANCE_KEYBOARD ? TANG_BLE_KIND_KEYBOARD
                : d->appearance == BLE_APPEARANCE_MOUSE    ? TANG_BLE_KIND_MOUSE
                : d->hid                                   ? TANG_BLE_KIND_HID
                                                           : TANG_BLE_KIND_OTHER;
        memcpy(o->name, d->name, sizeof(o->name));
        o->name[sizeof(o->name) - 1] = '\0';
    }
    taskEXIT_CRITICAL();
    return n;
}

const char *tang_ble_pair_start(int slot, const tang_ble_device_t *dev)
{
    if (!slot_ok(slot) || dev == NULL) {
        return "no such device";
    }
    ble_job_t j = { .kind = TANG_BLE_JOB_PAIR, .slot = slot };
    j.addr.type = dev->addr_type;
    memcpy(j.addr.a.val, dev->addr, sizeof(dev->addr));
    memcpy(j.name, dev->name, sizeof(j.name));
    j.name[sizeof(j.name) - 1] = '\0';
    return job_submit(&j);
}

const char *tang_ble_set_reconnect(int slot, bool on)
{
    if (!slot_ok(slot)) {
        return "no such slot";
    }
    const ble_job_t j = { .kind = on ? TANG_BLE_JOB_ON : TANG_BLE_JOB_OFF, .slot = slot };
    return job_submit(&j);
}

const char *tang_ble_forget(int slot)
{
    if (!slot_ok(slot)) {
        return "no such slot";
    }
    const ble_job_t j = { .kind = TANG_BLE_JOB_FORGET, .slot = slot };
    return job_submit(&j);
}

static int cmd_ble(tdsh_session_t *session, int argc, char **argv)
{
    (void)session;
    (void)argc;
    (void)argv;
    log_drain();
    if (s_start_phase == 0) {
        tdsh_printf("ble: radio off (it starts at boot when a device is paired, "
                    "or on the first Bluetooth command)\r\n");
    } else if (s_start_phase == 1) {
        tdsh_printf("ble: radio starting\r\n");
    } else if (s_enable_result != 0) {
        tdsh_printf("ble: radio failed to start (%d)\r\n", s_enable_result);
    } else {
        tdsh_printf("ble: radio up\r\n");
    }
    tang_heap_print("ble");
    if (s_start_phase == 2 && s_enable_result == 0) {
        tdsh_printf("ble: the stack took %lu bytes of heap at start (%lu free before, %lu after)\r\n",
                    (unsigned long)(s_heap_before - s_heap_after),
                    (unsigned long)s_heap_before, (unsigned long)s_heap_after);
    }
    tdsh_printf("ble: pairings %s (%s)\r\n", s_bonds_state, BLE_BONDS_PATH);
    for (int i = 0; i < HID_SLOTS; i++) {
        hid_status(&s_hid[i]);
    }
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

void tang_ble_info(tang_ble_info_t *out)
{
    memset(out, 0, sizeof(*out));
    if (s_start_phase == 0) {
        out->radio = TANG_BLE_RADIO_OFF;
    } else if (s_start_phase == 1) {
        out->radio = TANG_BLE_RADIO_STARTING;
    } else if (s_enable_result != 0) {
        out->radio = TANG_BLE_RADIO_FAILED;
        out->enable_error = s_enable_result;
    } else {
        out->radio = TANG_BLE_RADIO_UP;
        out->stack_heap = s_heap_before - s_heap_after;
    }
    out->pairings = s_bonds_state;
    out->busy = s_busy;
    out->job = s_job_kind;
    out->job_running = s_job_running;
    out->job_failed = s_job_failed;
    taskENTER_CRITICAL();
    memcpy(out->job_msg, s_job_msg, sizeof(out->job_msg));
    const TickType_t until = s_scan_until;
    const bool scanning = s_scanning;
    taskEXIT_CRITICAL();
    out->job_msg[sizeof(out->job_msg) - 1] = '\0';
    const TickType_t now = xTaskGetTickCount();
    if (scanning && (int32_t)(until - now) > 0) {
        out->scan_ms_left = (uint32_t)(until - now) * portTICK_PERIOD_MS;
    }

    for (int i = 0; i < HID_SLOTS; i++) {
        const hid_dev_t *d = &s_hid[i];
        tang_ble_slot_info_t *o = &out->slot[i];
        /* The name and address are written from the host's callbacks, and
         * the pairing by the task: copy each as a whole, as hid_status does. */
        taskENTER_CRITICAL();
        const hid_state_t state = d->state;
        memcpy(o->addr, d->addr.a.val, sizeof(o->addr));
        memcpy(o->name, d->name, sizeof(o->name));
        const tang_bond_t b = s_bonds[i];
        memcpy(o->last, s_slot_msg[i], sizeof(o->last));
        taskEXIT_CRITICAL();
        o->last[sizeof(o->last) - 1] = '\0';

        o->name[sizeof(o->name) - 1] = '\0';
        o->state = s_state_names[state];
        o->ready = state == HID_READY;
        o->active = state != HID_IDLE;
        o->boot_protocol = d->boot;
        o->reports = d->reports;
        o->paired = b.valid;
        if (b.valid) {
            memcpy(o->paired_addr, b.addr, sizeof(o->paired_addr));
            memcpy(o->paired_name, b.name, sizeof(o->paired_name));
            o->paired_name[sizeof(o->paired_name) - 1] = '\0';
        }
        o->reconnects = d->armed;
    }
}

static const tdsh_command_t s_ble_commands[] = {
    { "ble", "ble",
      "Bluetooth status: the radio, the heap, the pairings and both devices",
      cmd_ble, 0 },
    { "blescan", "blescan [seconds]",
      "Listen for Bluetooth LE devices and list them by signal strength (dBm)",
      cmd_blescan, 0 },
    { "blekbd", "blekbd pair [name] | <addr> | watch | off | on | forget",
      "Pair a Bluetooth LE keyboard as an input; it reconnects by itself after that",
      cmd_blekbd, 0 },
    { "blemouse", "blemouse pair [name] | <addr> | watch | off | on | forget",
      "Pair a Bluetooth LE mouse as the desktop's pointer; it reconnects by itself after that",
      cmd_blemouse, 0 },
};

int tang_ble_register(void)
{
    return tdsh_register_commands(s_ble_commands,
                                  sizeof(s_ble_commands) / sizeof(s_ble_commands[0]));
}
