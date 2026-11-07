// TinyTang — td_bt_ops_t over tang_ble.c's engine.
//
// SPDX-License-Identifier: MIT
//
// Translation only.  Every function here forwards to the tang_ble_* call that
// already does the work and converts the types on the way through, so the
// Bluetooth window can be written against tang_bt_ops.h and TinyDesk can adopt
// that interface later without anything being renamed.  No decision is made
// here: tang_ble.c owns the radio, the jobs and the slots, and this file is the
// shape in front of them.
//
// One thing is translated rather than forwarded, and it is the contract
// td_net_ops_t states: a request returns bool, and *why* it was refused appears
// in status().message.  tang_ble.c returns the reason as a string instead, so
// the refusal is kept here and reported through status() -- which is what lets
// a caller that only knows td_bt_ops_t show the user a reason.

#include <stdio.h>
#include <string.h>

#include "tang_ble.h"
#include "tang_bt_ops.h"

/* tang_ble_info() fills about 500 bytes; it is not a stack allocation on a port
 * whose script workers run in 16 KB, and the window already keeps its own copy
 * for the same reason. */
static tang_ble_info_t s_info;

/* Why the last request was refused, or empty.  Cleared by a request that was
 * accepted, so status().message always describes the last thing that happened. */
static char s_refusal[TANG_BT_MESSAGE_LEN];

/* Record a refusal.  tang_ble_* returns NULL when the job was handed over, and
 * a reason otherwise. */
static bool refused(const char *why)
{
    if (why == NULL) {
        s_refusal[0] = '\0';
        return false;
    }
    snprintf(s_refusal, sizeof s_refusal, "%s", why);
    return true;
}

/* td_bt_kind_t mirrors tang_ble_kind_t, but the two are separate types on
 * purpose -- the interface must not break when the engine's enum changes, so
 * the mapping is spelled out rather than assigned. */
static td_bt_kind_t bt_kind(tang_ble_kind_t k)
{
    switch (k) {
    case TANG_BLE_KIND_HID:      return TD_BT_KIND_HID;
    case TANG_BLE_KIND_KEYBOARD: return TD_BT_KIND_KEYBOARD;
    case TANG_BLE_KIND_MOUSE:    return TD_BT_KIND_MOUSE;
    case TANG_BLE_KIND_OTHER:    break;
    }
    return TD_BT_KIND_OTHER;
}

static tang_ble_kind_t ble_kind(td_bt_kind_t k)
{
    switch (k) {
    case TD_BT_KIND_KEYBOARD: return TANG_BLE_KIND_KEYBOARD;
    case TD_BT_KIND_MOUSE:    return TANG_BLE_KIND_MOUSE;
    case TD_BT_KIND_HID:      return TANG_BLE_KIND_HID;
    case TD_BT_KIND_OTHER:    break;
    }
    return TANG_BLE_KIND_OTHER;
}

static td_bt_radio_t bt_radio(tang_ble_radio_t r)
{
    switch (r) {
    case TANG_BLE_RADIO_STARTING: return TD_BT_RADIO_STARTING;
    case TANG_BLE_RADIO_UP:       return TD_BT_RADIO_UP;
    case TANG_BLE_RADIO_FAILED:   return TD_BT_RADIO_FAILED;
    case TANG_BLE_RADIO_OFF:      break;
    }
    return TD_BT_RADIO_OFF;
}

static void bt_status(td_bt_status_t *out)
{
    if (out == NULL) {
        return;
    }
    tang_ble_info(&s_info);

    memset(out, 0, sizeof *out);
    out->radio = bt_radio(s_info.radio);
    out->radio_up = s_info.radio == TANG_BLE_RADIO_UP;
    out->enable_error = s_info.enable_error;
    out->stack_heap = s_info.stack_heap;
    out->pairings = s_info.pairings;
    out->busy = s_info.busy;
    out->job_failed = s_info.job_failed;
    out->scan_ms_left = s_info.scan_ms_left;
    snprintf(out->message, sizeof out->message, "%s",
             s_refusal[0] != '\0' ? s_refusal : s_info.job_msg);
    /* One LE stack on this board and no room for both (BLE-002). */
    out->wifi_exclusive = true;

    for (int i = 0; i < TANG_BOND_SLOTS && i < TANG_BT_SLOTS; i++) {
        const tang_ble_slot_info_t *s = &s_info.slot[i];
        td_bt_slot_t *d = &out->slot[i];
        d->state = s->state;
        d->active = s->active;
        d->ready = s->ready;
        d->connected = s->ready;
        memcpy(d->addr, s->addr, sizeof d->addr);
        snprintf(d->name, sizeof d->name, "%s", s->name);
        d->boot_protocol = s->boot_protocol;
        d->reports = s->reports;
        d->paired = s->paired;
        memcpy(d->paired_addr, s->paired_addr, sizeof d->paired_addr);
        snprintf(d->paired_name, sizeof d->paired_name, "%s", s->paired_name);
        d->reconnects = s->reconnects;
        snprintf(d->last, sizeof d->last, "%s", s->last);
        if (s->paired) {
            out->paired++;
        }
        if (s->ready) {
            out->connected++;
        }
    }
}

static bool bt_scan(void)
{
    return !refused(tang_ble_scan_start());
}

static int bt_scan_results(td_bt_device_t *out, int max)
{
    /* The interface promises -1 while a scan runs, which is how a caller that
     * only knows this table can tell when to collect.  The engine reports that
     * as a job flag instead, so the translation happens here. */
    tang_ble_info(&s_info);
    if (s_info.job == TANG_BLE_JOB_SCAN && s_info.job_running) {
        return -1;
    }

    tang_ble_device_t found[TANG_BLE_MAX_DEVICES];
    const int n = tang_ble_scan_results(found, TANG_BLE_MAX_DEVICES);
    if (out == NULL || max <= 0) {
        return n;
    }
    const int take = n < max ? n : max;
    for (int i = 0; i < take; i++) {
        memset(&out[i], 0, sizeof out[i]);
        memcpy(out[i].addr, found[i].addr, sizeof out[i].addr);
        out[i].addr_type = found[i].addr_type;
        out[i].rssi = found[i].rssi;
        out[i].kind = bt_kind(found[i].kind);
        snprintf(out[i].name, sizeof out[i].name, "%s", found[i].name);
    }
    return take;
}

static bool bt_pair(int slot, const td_bt_device_t *dev)
{
    if (dev == NULL) {
        return false;
    }
    tang_ble_device_t d;
    memset(&d, 0, sizeof d);
    memcpy(d.addr, dev->addr, sizeof d.addr);
    d.addr_type = dev->addr_type;
    d.rssi = dev->rssi;
    d.kind = ble_kind(dev->kind);
    snprintf(d.name, sizeof d.name, "%s", dev->name);
    return !refused(tang_ble_pair_start(slot, &d));
}

static bool bt_disconnect(int slot)
{
    /* "Let the slot's device go and stop reconnecting": the pairing is kept,
     * which is the difference between this and forget(). */
    return !refused(tang_ble_set_reconnect(slot, false));
}

static int bt_paired(td_bt_paired_t *out, int max)
{
    if (out == NULL || max <= 0) {
        return 0;
    }
    tang_ble_info(&s_info);
    int n = 0;
    for (int i = 0; i < TANG_BOND_SLOTS && n < max; i++) {
        if (!s_info.slot[i].paired) {
            continue;
        }
        memset(&out[n], 0, sizeof out[n]);
        out[n].slot = i;
        memcpy(out[n].addr, s_info.slot[i].paired_addr, sizeof out[n].addr);
        snprintf(out[n].name, sizeof out[n].name, "%s", s_info.slot[i].paired_name);
        out[n].connected = s_info.slot[i].ready;
        out[n].reconnects = s_info.slot[i].reconnects;
        n++;
    }
    return n;
}

static bool bt_forget(int slot)
{
    return !refused(tang_ble_forget(slot));
}

static bool bt_set_reconnect(int slot, bool on)
{
    return !refused(tang_ble_set_reconnect(slot, on));
}

static const td_bt_ops_t s_ops = {
    .status = bt_status,
    .scan = bt_scan,
    .scan_results = bt_scan_results,
    .pair = bt_pair,
    .disconnect = bt_disconnect,
    .paired = bt_paired,
    .forget = bt_forget,
    .set_reconnect = bt_set_reconnect,
};

const td_bt_ops_t *td_bt_port_ops(void)
{
    return &s_ops;
}
