/* SPDX-License-Identifier: MIT
 *
 * The /tinysnes.ini parser on the host, as tools/tests/tb_nes_ini.c does for
 * /tinynes.ini.  tang_snes_ini.c reads its file through FatFS, so this supplies
 * the three calls it makes and drives the real load path.
 *
 * The two cases that matter most: the defaults must be the identity map, so an
 * absent file changes nothing and every build before this one behaves the same;
 * and `r` must not match `rt`, which a prefix compare would do and which would
 * quietly bind the right shoulder to the wrong button.
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

#include "tang_snes_ini.h"

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

static void load(tang_snes_settings_t *s, const char *text)
{
    s_text = text;
    s_at = 0;
    s_open_fails = false;
    s_read_fails = false;
    (void)tang_snes_settings_load(s, "/tinysnes.ini");
}

int main(void)
{
    static tang_snes_settings_t s;

    /* The defaults are the wiring every build before this file had: output
     * button i takes source i. */
    tang_snes_defaults(&s);
    check(s.p1[TANG_SNES_A] == TANG_SNES_A, "default: A answers to A");
    check(s.p1[TANG_SNES_B] == TANG_SNES_B, "default: B answers to B");
    check(s.p1[TANG_SNES_RT] == TANG_SNES_RT, "default: RT answers to RT");
    for (int i = 0; i < TANG_SNES_BUTTONS; i++) {
        if (s.p1[i] != i || s.p2[i] != i) {
            check(0, "default: identity for every button");
            break;
        }
    }

    /* An absent file is the defaults, not an error. */
    s_text = NULL;
    (void)tang_snes_settings_load(&s, "/tinysnes.ini");
    check(!s.found, "absent file: not found");
    check(s.p1[TANG_SNES_A] == TANG_SNES_A, "absent file: A still A");

    /* Several pairs on one line, which is what the shipped files look like and
     * the shape that once cost a cycle when the parser took only the first. */
    load(&s, "[input]\np1_a = b  p1_b = a  p1_x = y  p1_y = x\n");
    check(s.found, "loaded: found");
    check(s.keys == 4, "loaded: four keys understood");
    check(s.p1[TANG_SNES_A] == TANG_SNES_B, "swap: A answers to B");
    check(s.p1[TANG_SNES_B] == TANG_SNES_A, "swap: B answers to A");
    check(s.p1[TANG_SNES_X] == TANG_SNES_Y, "swap: X answers to Y");
    check(s.p1[TANG_SNES_Y] == TANG_SNES_X, "swap: Y answers to X");
    check(s.p1[TANG_SNES_START] == TANG_SNES_START, "swap: START untouched");

    /* `r` must not match `rt`, and `rt` must not match `r`. */
    load(&s, "[input]\np1_r = rt\np1_rt = r\n");
    check(s.p1[TANG_SNES_R] == TANG_SNES_RT, "r takes rt");
    check(s.p1[TANG_SNES_RT] == TANG_SNES_R, "rt takes r");

    /* `none` removes a binding rather than moving it. */
    load(&s, "[input]\np1_select = none\n");
    check(s.p1[TANG_SNES_SELECT] == TANG_SNES_SRC_NONE, "select: unbound");
    check(strcmp(tang_snes_source_name(s.p1[TANG_SNES_SELECT]), "none") == 0,
          "select: named none");

    /* Player 2 is a separate array. */
    load(&s, "[input]\np2_up = dn\n");
    check(s.p2[TANG_SNES_UP] == TANG_SNES_DN, "p2: UP answers to DN");
    check(s.p1[TANG_SNES_UP] == TANG_SNES_UP, "p2: player 1 untouched");

    /* A misspelt key or value is ignored and not counted, so it shows in the
     * key count rather than as a button that silently does nothing. */
    load(&s, "[input]\np1_a = nosuchbutton\np1_nosuch = b\n");
    check(s.keys == 0, "unknown names: not counted");
    check(s.p1[TANG_SNES_A] == TANG_SNES_A, "unknown names: A keeps default");

    /* Comments and an unknown section and key are skipped.  [video] is a known
     * section now, so this file has three understood keys, not two. */
    load(&s, "# a comment\n; another\n[input]\np1_a = b\n"
             "[video]\naspect = 4:3\n[tinysnes]\nversion = 1\n");
    check(s.keys == 3, "comments and unknown sections skipped");
    check(s.p1[TANG_SNES_A] == TANG_SNES_B, "still read the known key");

    /* The frame word: button 11 in the top nibble, identity packing. */
    tang_snes_defaults(&s);
    check(tang_snes_map_word(s.p1) == 0xba9876543210ull,
          "map word: identity packs as ba9876543210");

    uint8_t swapped[TANG_SNES_BUTTONS];
    memcpy(swapped, s.p1, sizeof swapped);
    swapped[TANG_SNES_A] = TANG_SNES_B;
    swapped[TANG_SNES_B] = TANG_SNES_A;
    /* A is button 3 and B is button 11, so a swap moves nibbles 3 and 11 and
     * leaves the other ten alone. */
    check(tang_snes_map_word(swapped) == 0x3a987654b210ull,
          "map word: A/B swap moves nibbles 3 and 11 only");

    uint8_t all_none[TANG_SNES_BUTTONS];
    for (int i = 0; i < TANG_SNES_BUTTONS; i++) {
        all_none[i] = TANG_SNES_SRC_NONE;
    }
    check(tang_snes_map_word(all_none) == 0xccccccccccccull,
          "map word: all unbound is all 12s");

    /* Names, and the out-of-range answer. */
    check(strcmp(tang_snes_button_name(TANG_SNES_B), "b") == 0, "name: B is b");
    check(strcmp(tang_snes_button_name(99), "?") == 0, "name: out of range is ?");
    check(strcmp(tang_snes_source_name(TANG_SNES_SRC_NONE), "none") == 0,
          "name: none");

    /* A round trip: what Save writes is what the next launch reads. */
    tang_snes_settings_t written;
    tang_snes_defaults(&written);
    written.p1[TANG_SNES_A] = TANG_SNES_B;
    written.p1[TANG_SNES_SELECT] = TANG_SNES_SRC_NONE;
    written.p2[TANG_SNES_UP] = TANG_SNES_DN;
    written.audio.volume = 65;
    written.audio.mute = 1;
    s_card[0] = '\0';
    s_card_used = false;
    s_write_fails = false;
    char why[96];
    check(tang_snes_settings_save(&written, "/tinysnes.ini", why, sizeof why) == 0,
          "save: wrote the file");

    static tang_snes_settings_t reread;
    load(&reread, s_card);
    check(reread.p1[TANG_SNES_A] == TANG_SNES_B, "round trip: A still B");
    check(reread.p1[TANG_SNES_SELECT] == TANG_SNES_SRC_NONE,
          "round trip: SELECT still unbound");
    check(reread.p2[TANG_SNES_UP] == TANG_SNES_DN, "round trip: p2 UP still DN");
    check(reread.keys == 2 * TANG_SNES_BUTTONS + 5,
          "round trip: every button, the version, the video and audio keys");
    check(reread.aspect == written.aspect, "round trip: aspect kept");
    check(reread.overscan == written.overscan, "round trip: overscan kept");
    check(reread.audio.volume == 65, "round trip: volume kept");
    check(reread.audio.mute == 1, "round trip: mute kept");

    /* The video keys, and a shape this build does not know. */
    load(&s, "[video]\naspect = 8:7\noverscan = crop\n");
    check(s.aspect == TANG_SNES_ASPECT_8_7, "video: 8:7 read");
    check(s.overscan == 0, "video: crop read");
    load(&s, "[video]\naspect = 16:9\n");
    check(s.aspect == TANG_SNES_ASPECT_4_3, "video: unknown shape keeps default");
    check(s.keys == 0, "video: unknown shape not counted");

    /* The audio keys.  They go through the same parser /tinynes.ini and
     * /tinygbc.ini use, so what is checked here is that this file reaches it
     * and that the defaults are a pass-through. */
    tang_snes_defaults(&s);
    check(s.audio.volume == 100, "audio: default is full volume");
    check(s.audio.mute == 0, "audio: default is not muted");
    check(tang_audio_gain(s.audio.volume, s.audio.mute) == TANG_VOLUME_UNITY,
          "audio: the default gain is exactly unity");
    load(&s, "[audio]\nvolume = 40\nmute = yes\n");
    check(s.audio.volume == 40, "audio: volume read");
    check(s.audio.mute == 1, "audio: mute read");
    check(s.keys == 2, "audio: both keys counted");
    load(&s, "[audio]\nvolume = 250\n");
    check(s.audio.volume == 100, "audio: a volume past the top is clamped");
    load(&s, "[audio]\nvolume = 0\n");
    check(tang_audio_gain(s.audio.volume, 0) == 0, "audio: zero is silence");

    /* The curve.  These are the numbers a player hears, so they are checked
     * rather than derived again in the app: half travel is a quarter of the
     * amplitude, which is -12 dB, and 100 is exactly unity so the default
     * leaves every sample as it was. */
    check(tang_audio_gain(100, 0) == 256u, "gain: 100% is unity");
    check(tang_audio_gain(50, 0) == 64u, "gain: 50% is a quarter");
    check(tang_audio_gain(5, 0) == 1u, "gain: 5% is about -48 dB");
    check(tang_audio_gain(100, 1) == 0u, "gain: mute is silence at full volume");
    check(tang_audio_gain(0, 0) == 0u, "gain: zero is silence");

    /* A card that stops taking the file says so rather than reporting a save. */
    s_card[0] = '\0';
    s_write_fails = true;
    check(tang_snes_settings_save(&written, "/tinysnes.ini", why, sizeof why) != 0,
          "save: a failing card is reported");

    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed != 0;
}
