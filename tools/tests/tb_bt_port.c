/* SPDX-License-Identifier: MIT
 * The port's Bluetooth tables (ports/bl616/tang_bt_port.c) over fake engines:
 * the pad table's translation of tang_btpad.c for the Bluetooth window --
 * which inquiry results a pad slot is offered and as what, how a pad that is
 * turned off reads, and that a refusal's reason reaches status().message.
 * tang_ble.c and tang_btpad.c are replaced by the recorders below, so the
 * file is checked on its own.
 */
#include <stdio.h>
#include <string.h>

#include "tang_ble.h"
#include "tang_bt_ops.h"
#include "tang_btpad.h"

static int fails, checks;
static void check(bool ok, const char *what)
{
    checks++;
    if (!ok) {
        fails++;
        printf("  FAIL: %s\n", what);
    }
}

/* ---- tang_ble.c, not under test: enough to link */

void tang_ble_info(tang_ble_info_t *out) { memset(out, 0, sizeof *out); }
const char *tang_ble_scan_start(void) { return NULL; }
int tang_ble_scan_results(tang_ble_device_t *out, int max) { (void)out; (void)max; return 0; }
const char *tang_ble_pair_start(int slot, const tang_ble_device_t *dev) { (void)slot; (void)dev; return NULL; }
const char *tang_ble_set_reconnect(int slot, bool on) { (void)slot; (void)on; return NULL; }
const char *tang_ble_forget(int slot) { (void)slot; return NULL; }

/* ---- tang_btpad.c, recorded */

static tang_btpad_info_t g_info;
static tang_btpad_found_t g_found[TANG_BTPAD_FOUND_MAX];
static int g_found_n;
static bool g_scanning;
static const char *g_refuse;
static int g_pair_pad = -1, g_on_pad = -1, g_forget_pad = -1;
static bool g_on;
static uint8_t g_pair_addr[6];
static const char *g_pair_name;

void tang_btpad_info(tang_btpad_info_t *out) { *out = g_info; }
const char *tang_btpad_scan_start(void) { return g_refuse; }
int tang_btpad_scan_results(tang_btpad_found_t *out, int max)
{
    if (g_scanning) {
        return -1;
    }
    const int n = g_found_n < max ? g_found_n : max;
    memcpy(out, g_found, (size_t)n * sizeof *out);
    return n;
}
const char *tang_btpad_pair_start(int pad, const uint8_t addr[6], const char *name)
{
    g_pair_pad = pad;
    memcpy(g_pair_addr, addr, 6);
    g_pair_name = name;
    return g_refuse;
}
const char *tang_btpad_set_on(int pad, bool on)
{
    g_on_pad = pad;
    g_on = on;
    return g_refuse;
}
const char *tang_btpad_forget(int pad)
{
    g_forget_pad = pad;
    return g_refuse;
}

static tang_btpad_found_t found(const char *name, int8_t rssi, uint8_t a0, bool pad, bool periph)
{
    tang_btpad_found_t f;
    memset(&f, 0, sizeof f);
    snprintf(f.name, sizeof f.name, "%s", name);
    f.rssi = rssi;
    f.addr[0] = a0;
    f.pad = pad;
    f.peripheral = periph;
    return f;
}

int main(void)
{
    const tang_bt_pad_ops_t *ops = tang_bt_pad_ops();
    check(ops != NULL, "no pad table");
    tang_bt_pad_status_t st;

    /* The inquiry: -1 while it runs; then peripherals only, a gamepad as HID,
     * every one marked Classic, in the engine's order. */
    g_scanning = true;
    check(ops->scan_results(NULL, 0) == -1, "a running inquiry did not answer -1");
    g_scanning = false;
    g_found[0] = found("Cube 2", -50, 1, false, false);          /* a speaker */
    g_found[1] = found("Pro Controller", -60, 2, true, true);
    g_found[2] = found("A Classic keyboard", -70, 3, false, true);
    g_found_n = 3;
    td_bt_device_t d[TANG_BT_MAX_DEVICES];
    const int n = ops->scan_results(d, TANG_BT_MAX_DEVICES);
    check(n == 2, "a non-peripheral was offered to a pad slot, or a peripheral was dropped");
    check(n == 2 && d[0].addr[0] == 2 && d[0].kind == TD_BT_KIND_HID && d[0].classic &&
          strcmp(d[0].name, "Pro Controller") == 0, "the gamepad is not first as a Classic HID device");
    check(n == 2 && d[1].addr[0] == 3 && d[1].kind == TD_BT_KIND_OTHER && d[1].classic,
          "another peripheral is not offered as a Classic other");
    check(ops->scan_results(NULL, 0) == 2, "the count without a buffer is not the filtered count");
    check(ops->scan_results(d, 1) == 1 && d[0].addr[0] == 2, "max was not honoured");

    /* The requests reach tang_btpad.c with the pad index and the device. */
    check(ops->pair(1, &d[0]) && g_pair_pad == 1 && g_pair_addr[0] == 2 &&
          g_pair_name != NULL && strcmp(g_pair_name, "Pro Controller") == 0,
          "pair did not pass the pad, address and name");
    check(ops->set_reconnect(0, false) && g_on_pad == 0 && !g_on, "off did not reach the pad");
    check(ops->set_reconnect(1, true) && g_on_pad == 1 && g_on, "on did not reach the pad");
    check(ops->forget(1) && g_forget_pad == 1, "forget did not reach the pad");

    /* A refusal returns false and its reason is status().message, over the
     * job's own message, until a request is taken again. */
    g_info.available = true;
    snprintf(g_info.job_msg, sizeof g_info.job_msg, "pad 1 is paired and reporting");
    g_refuse = "Bluetooth is busy; try again when it has finished";
    check(!ops->scan(), "a refused inquiry returned true");
    ops->status(&st);
    check(strcmp(st.message, g_refuse) == 0, "the refusal is not in status().message");
    g_refuse = NULL;
    check(ops->scan(), "an accepted inquiry returned false");
    ops->status(&st);
    check(strcmp(st.message, "pad 1 is paired and reporting") == 0,
          "an old refusal hid the job's message");

    /* The slots: saved, reporting, and off. */
    g_info.job_running = true;
    g_info.scan_ms_left = 4000;
    g_info.pairings = "loaded from the card";
    tang_btpad_slot_info_t *p = &g_info.pad[0];
    p->state = "ready";
    p->saved = p->linked = p->ready = true;
    p->addr[0] = 0x7f;
    snprintf(p->name, sizeof p->name, "Pro Controller");
    p->reports = 99;
    snprintf(p->last, sizeof p->last, "the pad connected");
    tang_btpad_slot_info_t *q = &g_info.pad[1];
    q->state = "off";
    q->saved = q->off = true;
    q->addr[0] = 0x91;
    snprintf(q->name, sizeof q->name, "Nintendo RVL-CNT-01-UC");
    ops->status(&st);
    check(st.available && st.busy && st.scan_ms_left == 4000 &&
          strcmp(st.pairings, "loaded from the card") == 0, "the engine's state was not carried");
    check(st.slot[0].paired && st.slot[0].ready && st.slot[0].active && st.slot[0].reconnects &&
          st.slot[0].paired_addr[0] == 0x7f && st.slot[0].reports == 99 &&
          strcmp(st.slot[0].paired_name, "Pro Controller") == 0 &&
          strcmp(st.slot[0].last, "the pad connected") == 0, "a reporting pad is not shown so");
    check(st.slot[1].paired && !st.slot[1].reconnects && !st.slot[1].active &&
          strcmp(st.slot[1].state, "off") == 0, "a pad that is off reads as reconnecting");
    g_info.available = false;
    ops->status(&st);
    check(!st.available, "the LE-only build's missing pads were not passed on");

    printf("bt port: %s (%d checks)\n", fails ? "FAIL" : "PASS", checks);
    return fails ? 1 : 0;
}
