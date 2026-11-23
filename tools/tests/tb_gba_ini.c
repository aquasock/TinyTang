/* SPDX-License-Identifier: MIT
 *
 * The /tinygba.ini parser and writer on the host, as tools/tests/tb_snes_ini.c
 * does for /tinysnes.ini.  tang_gba_ini.c reads its file through FatFS, so this
 * supplies the calls it makes and drives the real load and save paths.
 *
 * What matters most: the defaults must be the mapping the core has always had
 * fixed in fabric and holds out of reset -- X with A, Y with B, ZL with L, ZR
 * with R -- so an absent file changes nothing; `l` must not match `left`; and
 * the bytes frame 0x1A carries must be the order iosys_bl616.v shifts in,
 * source 13's nibble first, with anything that is not a key sent as none.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- the FatFS calls, over a string: the types are in the stub ff.h ---- */
#include "ff.h"

static bool s_open_fails;
static bool s_read_fails;
static const char *s_text;

FRESULT f_open(FIL *fp, const char *path, uint8_t mode)
{
    (void)path;
    (void)mode;
    if (s_open_fails || s_text == NULL) {
        return FR_NO_FILE;
    }
    fp->fd = 1;
    return FR_OK;
}
static size_t s_at;

FRESULT f_read(FIL *fp, void *buff, UINT btr, UINT *br)
{
    (void)fp;
    if (s_read_fails) {
        return FR_DISK_ERR;
    }
    const size_t left = strlen(s_text) - s_at;
    const size_t take = btr < left ? btr : left;
    memcpy(buff, s_text + s_at, take);
    s_at += take;
    *br = (UINT)take;
    return FR_OK;
}

/* A one-file card: enough for the save path's temporary, backup and rename,
 * which is where the interesting behaviour is. */
static char s_card[4096];
static bool s_card_used;
static bool s_write_fails;

FRESULT f_write(FIL *fp, const void *buff, UINT btw, UINT *bw)
{
    (void)fp;
    if (s_write_fails) {
        *bw = 0;
        return FR_DISK_ERR;
    }
    const size_t at = strlen(s_card);
    if (at + btw >= sizeof s_card) {
        *bw = 0;
        return FR_DISK_ERR;
    }
    memcpy(s_card + at, buff, btw);
    s_card[at + btw] = '\0';
    *bw = btw;
    return FR_OK;
}

FRESULT f_unlink(const char *path)
{
    (void)path;
    if (strstr(path, ".bak") != NULL) {
        /* The backup is discarded, which does not touch the file in place. */
        return FR_OK;
    }
    s_card[0] = '\0';
    s_card_used = false;
    return FR_OK;
}

FRESULT f_rename(const char *from, const char *to)
{
    (void)from;
    (void)to;
    /* The card holds one file, so both renames -- old aside and new in place --
     * leave the same bytes under the name the loader reads. */
    s_card_used = s_card[0] != '\0';
    return s_card_used ? FR_OK : FR_NO_FILE;
}

FRESULT f_close(FIL *fp)
{
    (void)fp;
    return FR_OK;
}

#include "tang_gba_ini.h"

static int s_checks;
static int s_failed;

static void check(int ok, const char *what)
{
    s_checks++;
    if (!ok) {
        s_failed++;
        printf("FAIL  %s\n", what);
    } else {
        printf("ok    %s\n", what);
    }
}

static void load(tang_gba_settings_t *s, const char *text)
{
    s_text = text;
    s_at = 0;
    s_open_fails = false;
    s_read_fails = false;
    (void)tang_gba_settings_load(s, "/tinygba.ini");
}

int main(void)
{
    static tang_gba_settings_t s;

    /* The defaults: the old fixed mapping, which the core's PAD_MAP_DEFAULT
     * holds too.  0x98765432321010 is that word -- iosys_bl616.v's literal --
     * so the two cannot drift without this failing. */
    tang_gba_defaults(&s);
    check(s.map[TANG_GBA_SRC_X] == TANG_GBA_KEY_A, "default: X presses A");
    check(s.map[TANG_GBA_SRC_Y] == TANG_GBA_KEY_B, "default: Y presses B");
    check(s.map[TANG_GBA_SRC_ZL] == TANG_GBA_KEY_L, "default: ZL presses L");
    check(s.map[TANG_GBA_SRC_ZR] == TANG_GBA_KEY_R, "default: ZR presses R");
    check(s.map[TANG_GBA_SRC_PLUS] == TANG_GBA_KEY_START, "default: + is START");
    check(s.map[TANG_GBA_SRC_MINUS] == TANG_GBA_KEY_SELECT, "default: - is SELECT");
    check(tang_gba_map_word(s.map) == 0x98765432321010ull,
          "map word: the default is the core's PAD_MAP_DEFAULT");

    /* An absent file is the defaults, not an error. */
    s_text = NULL;
    (void)tang_gba_settings_load(&s, "/tinygba.ini");
    check(!s.found, "absent file: not found");
    check(s.map[TANG_GBA_SRC_A] == TANG_GBA_KEY_A, "absent file: A still A");

    /* Several pairs on one line; a swap, and a button removed. */
    load(&s, "[input]\na = b  b = a  zl = none\n");
    check(s.found, "loaded: found");
    check(s.keys == 3, "loaded: three keys understood");
    check(s.map[TANG_GBA_SRC_A] == TANG_GBA_KEY_B, "swap: A presses B");
    check(s.map[TANG_GBA_SRC_B] == TANG_GBA_KEY_A, "swap: B presses A");
    check(s.map[TANG_GBA_SRC_ZL] == TANG_GBA_KEY_NONE, "ZL presses nothing");
    check(s.map[TANG_GBA_SRC_X] == TANG_GBA_KEY_A, "X untouched");

    /* `l` must not match `left`, as key or as value. */
    load(&s, "[input]\nl = left\nleft = l\n");
    check(s.map[TANG_GBA_SRC_L] == TANG_GBA_KEY_LEFT, "l presses LEFT");
    check(s.map[TANG_GBA_SRC_LEFT] == TANG_GBA_KEY_L, "left presses L");

    /* plus and minus are spelt out. */
    load(&s, "[input]\nplus = select\nminus = start\n");
    check(s.map[TANG_GBA_SRC_PLUS] == TANG_GBA_KEY_SELECT, "plus presses SELECT");
    check(s.map[TANG_GBA_SRC_MINUS] == TANG_GBA_KEY_START, "minus presses START");

    /* A misspelt key or value is ignored and not counted. */
    load(&s, "[input]\na = nosuchkey\nnosuch = b\n");
    check(s.keys == 0, "unknown names: not counted");
    check(s.map[TANG_GBA_SRC_A] == TANG_GBA_KEY_A, "unknown names: A keeps default");

    /* Comments, the version and an unknown section. */
    load(&s, "# a comment\n; another\n[input]\na = b\n"
             "[video]\naspect = 4:3\n[tinygba]\nversion = 1\n");
    check(s.keys == 2, "comments and unknown sections skipped");

    /* The frame's bytes: source 13's nibble first. */
    tang_gba_defaults(&s);
    uint8_t bytes[7];
    tang_gba_map_bytes(s.map, bytes);
    check(bytes[0] == 0x98 && bytes[1] == 0x76 && bytes[2] == 0x54 &&
          bytes[3] == 0x32 && bytes[4] == 0x32 && bytes[5] == 0x10 && bytes[6] == 0x10,
          "map bytes: 98 76 54 32 32 10 10 for the default");
    uint8_t m[TANG_GBA_SOURCES];
    tang_gba_default_map(m);
    m[TANG_GBA_SRC_A] = TANG_GBA_KEY_NONE;
    m[TANG_GBA_SRC_RIGHT] = 200;            /* corrupt: must be sent as none */
    check(tang_gba_map_word(m) == 0xf876543232101full,
          "map word: none and a corrupt entry both send f");

    /* Names, and the out-of-range answer. */
    check(strcmp(tang_gba_source_name(TANG_GBA_SRC_ZL), "ZL") == 0, "name: ZL");
    check(strcmp(tang_gba_source_name(TANG_GBA_SRC_PLUS), "+") == 0, "name: +");
    check(strcmp(tang_gba_key_name(TANG_GBA_KEY_NONE), "none") == 0, "name: none");
    check(strcmp(tang_gba_key_name(99), "?") == 0, "name: out of range is ?");

    /* A round trip: what Save writes is what the next launch reads. */
    tang_gba_settings_t written;
    tang_gba_defaults(&written);
    written.map[TANG_GBA_SRC_A] = TANG_GBA_KEY_B;
    written.map[TANG_GBA_SRC_B] = TANG_GBA_KEY_A;
    written.map[TANG_GBA_SRC_ZL] = TANG_GBA_KEY_NONE;
    s_card[0] = '\0';
    s_card_used = false;
    s_write_fails = false;
    char why[96];
    check(tang_gba_settings_save(&written, "/tinygba.ini", why, sizeof why) == 0,
          "save: wrote the file");
    static tang_gba_settings_t reread;
    load(&reread, s_card);
    check(memcmp(reread.map, written.map, TANG_GBA_SOURCES) == 0,
          "round trip: the whole map kept");
    check(reread.keys == TANG_GBA_SOURCES + 1, "round trip: every button and the version");

    /* A card that stops taking the file says so rather than reporting a save. */
    s_card[0] = '\0';
    s_write_fails = true;
    check(tang_gba_settings_save(&written, "/tinygba.ini", why, sizeof why) != 0,
          "save: a failing card is reported");

    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed != 0;
}
