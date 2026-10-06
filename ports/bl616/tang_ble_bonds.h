// TinyTang — Bluetooth pairings kept on the SD card.
//
// The SDK keeps pairing keys in flash only through easyflash
// (CONFIG_BT_SETTINGS), which needs a key-value partition this board's flash
// layout does not have (FLS-002).  So the keys are kept here instead: after a
// pairing, tang_ble.c copies the host's key entry for that device into a
// record, the records are written to /sd/ble/bonds.bin, and at boot they are
// read back and put into the host's key table before the devices reconnect.
//
// This file is only the format -- encode and decode, with nothing from the
// Bluetooth stack in it -- so it can be checked on the host.  The layout is
// explicit little-endian bytes, not a struct copy, so a change of compiler or
// packing cannot silently change the file:
//
//     0   "TTBL"       magic
//     4   version      1
//     5   slots        TANG_BOND_SLOTS
//     6   reserved     0, 0
//     8   slot records, TANG_BOND_RECORD_LEN bytes each
//     ..  CRC-32 (IEEE 802.3, as zlib) of every byte before it
//
// The keys are stored as they are, unencrypted: anyone holding the card could
// present itself to these devices as the board.
//
// SPDX-License-Identifier: MIT

#ifndef TANG_BLE_BONDS_H
#define TANG_BLE_BONDS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TANG_BOND_SLOTS      2      /* 0 keyboard, 1 mouse: tang_ble.c's slots */
#define TANG_BOND_NAME_LEN   24
#define TANG_BOND_RECORD_LEN 80
#define TANG_BOND_FILE_LEN   (8 + TANG_BOND_SLOTS * TANG_BOND_RECORD_LEN + 4)
#define TANG_BOND_VERSION    1

/* One paired device: its address and the parts of the host's struct bt_keys
 * a central needs to encrypt to it again. */
typedef struct {
    bool     valid;
    uint8_t  addr_type;                 /* bt_addr_le_t.type */
    uint8_t  addr[6];                   /* bt_addr_t.val, least significant first */
    char     name[TANG_BOND_NAME_LEN];  /* NUL-terminated; empty if unknown */
    uint8_t  enc_size;
    uint8_t  flags;                     /* BT_KEYS_SC, BT_KEYS_AUTHENTICATED */
    uint16_t keys;                      /* BT_KEYS_LTK, _LTK_P256, _IRK */
    uint8_t  ltk_rand[8];
    uint8_t  ltk_ediv[2];
    uint8_t  ltk_val[16];
    uint8_t  irk_val[16];
} tang_bond_t;

/* Write the file image for `b` into `out`, which must hold
 * TANG_BOND_FILE_LEN bytes.  Returns the length written. */
size_t tang_bonds_encode(const tang_bond_t b[TANG_BOND_SLOTS], uint8_t *out);

/* Read a file image into `b`.  Returns 0, or -1 if the length, magic,
 * version, slot count or CRC is wrong -- in which case every slot is left
 * invalid, so a damaged file means "no pairings", never half of one. */
int tang_bonds_decode(const uint8_t *in, size_t len, tang_bond_t b[TANG_BOND_SLOTS]);

/* CRC-32 (IEEE 802.3, reflected, init and final XOR 0xFFFFFFFF). */
uint32_t tang_bonds_crc32(const uint8_t *p, size_t len);

#endif /* TANG_BLE_BONDS_H */
