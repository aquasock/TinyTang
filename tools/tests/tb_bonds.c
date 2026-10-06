/*
 * Host test for the Bluetooth pairing file format (tang_ble_bonds.c).
 *
 * The file is the only thing that carries a pairing across a reset, and a
 * wrong byte in it fails silently on the board: the keyboard simply does not
 * come back.  So the format is checked here -- the round trip, the CRC
 * against the standard check value, and that a damaged file yields no
 * pairings rather than a corrupt one.
 *
 *   tools/tests/test_bonds.sh
 */
#include <stdio.h>
#include <string.h>

#include "tang_ble_bonds.h"

static int fails;
static int checks;

static void check(int ok, const char *what)
{
    checks++;
    if (!ok) {
        fails++;
        printf("  FAIL: %s\n", what);
    }
}

int main(void)
{
    /* CRC-32 check value from the IEEE / zlib definition. */
    check(tang_bonds_crc32((const uint8_t *)"123456789", 9) == 0xCBF43926u,
          "CRC-32 of \"123456789\" is not 0xCBF43926");

    tang_bond_t in[TANG_BOND_SLOTS];
    memset(in, 0, sizeof(in));
    in[0].valid = true;
    in[0].addr_type = 1;
    const uint8_t a0[6] = { 0xD8, 0xD9, 0x81, 0xA7, 0x88, 0xDB };
    memcpy(in[0].addr, a0, 6);
    strcpy(in[0].name, "Logi K950");
    in[0].enc_size = 16;
    in[0].flags = 0x10;
    in[0].keys = 0x0024;
    for (int i = 0; i < 8; i++) in[0].ltk_rand[i] = (uint8_t)(0x10 + i);
    in[0].ltk_ediv[0] = 0xAB;
    in[0].ltk_ediv[1] = 0xCD;
    for (int i = 0; i < 16; i++) in[0].ltk_val[i] = (uint8_t)(0x80 + i);
    for (int i = 0; i < 16; i++) in[0].irk_val[i] = (uint8_t)(0xC0 + i);
    /* Slot 1 empty. */

    uint8_t file[TANG_BOND_FILE_LEN];
    const size_t n = tang_bonds_encode(in, file);
    check(n == TANG_BOND_FILE_LEN, "encode length");
    check(memcmp(file, "TTBL", 4) == 0 && file[4] == 1 && file[5] == 2, "header");

    tang_bond_t out[TANG_BOND_SLOTS];
    check(tang_bonds_decode(file, n, out) == 0, "a good file did not decode");
    check(out[0].valid && !out[1].valid, "valid flags");
    check(out[0].addr_type == 1 && memcmp(out[0].addr, a0, 6) == 0, "address");
    check(strcmp(out[0].name, "Logi K950") == 0, "name");
    check(out[0].enc_size == 16 && out[0].flags == 0x10 && out[0].keys == 0x0024,
          "size, flags and key types");
    check(memcmp(out[0].ltk_rand, in[0].ltk_rand, 8) == 0 &&
          memcmp(out[0].ltk_ediv, in[0].ltk_ediv, 2) == 0 &&
          memcmp(out[0].ltk_val, in[0].ltk_val, 16) == 0 &&
          memcmp(out[0].irk_val, in[0].irk_val, 16) == 0, "keys");

    /* Little-endian key types at a fixed offset: record 0 starts at 8, and
     * keys follows valid, type, address, name, size and flags. */
    check(file[8 + 34] == 0x24 && file[8 + 35] == 0x00, "key types not little-endian at 42");

    /* A full-length name is cut to fit with its terminator. */
    memset(in[1].name, 'x', sizeof(in[1].name));
    in[1].valid = true;
    tang_bonds_encode(in, file);
    tang_bonds_decode(file, n, out);
    check(strlen(out[1].name) == TANG_BOND_NAME_LEN - 1, "a long name was not cut to fit");

    /* Damage: any flipped byte, a short file, a wrong version -> nothing. */
    tang_bonds_encode(in, file);
    file[50] ^= 0x01;
    check(tang_bonds_decode(file, n, out) == -1, "a flipped byte was accepted");
    check(!out[0].valid && !out[1].valid, "a rejected file left a slot valid");
    tang_bonds_encode(in, file);
    check(tang_bonds_decode(file, n - 1, out) == -1, "a short file was accepted");
    file[4] = 2;
    check(tang_bonds_decode(file, n, out) == -1, "a newer version was accepted");

    printf("tb_bonds: %d checks, %d failures\n", checks, fails);
    return fails == 0 ? 0 : 1;
}
