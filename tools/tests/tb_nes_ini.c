/* SPDX-License-Identifier: MIT
 *
 * The /nes.ini parser on the host, as tools/tests/test_ini.sh does for
 * /tang.ini.  tang_nes_ini.c reads its file through FatFS, so this supplies
 * the four calls it makes and drives the real load path -- including the chunk
 * boundary, which is the part a buffer-only test would not reach.
 *
 * The case that matters most is the first: several `key = value` pairs on one
 * line.  The parser once took only the first per line while the file shipped
 * with four, which silently dropped all sixteen input keys and cost a cycle.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- the FatFS calls, over a string: the types are in tools/tests/ff.h ---- */
#include "ff.h"

static bool s_open_fails;
static bool s_read_fails;
static const char *s_text;

FRESULT f_open(FIL *fp, const TCHAR *path, BYTE mode)
{
    (void)path;
    (void)mode;
    if (s_open_fails) {
        return 1;
    }
    fp->text = s_text;
    fp->at = 0;
    return FR_OK;
}

FRESULT f_read(FIL *fp, void *buff, UINT btr, UINT *br)
{
    if (s_read_fails) {
        return 1;
    }
    size_t left = strlen(fp->text) - fp->at;
    size_t take = btr < left ? btr : left;
    memcpy(buff, fp->text + fp->at, take);
    fp->at += take;
    *br = (UINT)take;
    return FR_OK;
}

FRESULT f_close(FIL *fp)
{
    (void)fp;
    return FR_OK;
}

#include "tang_nes_ini.h"

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

/* Load `text` and hand back the result. */
static void load(tang_nes_settings_t *s, const char *text)
{
    s_text = text;
    s_open_fails = false;
    s_read_fails = false;
    (void)tang_nes_load(s, "/nes.ini");
}

int main(int argc, char **argv)
{
    static tang_nes_settings_t s;

    /* The defaults are what the firmware did before this file existed. */
    tang_nes_defaults(&s);
    check(s.p1[TANG_NES_A] == NES_CTL_B, "default: NES A answers to B");
    check(s.p1[TANG_NES_B] == NES_CTL_Y, "default: NES B answers to Y");
    check(s.aspect == TANG_ASPECT_8_7, "default: aspect is 8:7");
    check(s.volume == 75, "default: volume is 75");

    /* The regression: four keys on one line, as the shipped file has them. */
    load(&s, "[input]\n"
             "p1_a = b            p1_b = y           p1_select = select   p1_start = start\n"
             "p1_up = up          p1_down = down     p1_left = left       p1_right = right\n");
    check(s.keys == 8, "four keys on a line: all eight counted");
    check(s.p1[TANG_NES_SELECT] == NES_CTL_SELECT, "four keys on a line: select took");
    check(s.p1[TANG_NES_RIGHT] == NES_CTL_RIGHT, "four keys on a line: the last one took");

    /* One per line, which is what a hand edit usually looks like. */
    load(&s, "[input]\np1_a = y\np2_a = b\n");
    check(s.keys == 2, "one key per line: both counted");
    check(s.p1[TANG_NES_A] == NES_CTL_Y, "one key per line: p1 took");
    check(s.p2[TANG_NES_A] == NES_CTL_B, "one key per line: p2 is separate");

    /* A value the parser cannot place must leave the default, not half-apply. */
    load(&s, "[input]\np1_a = zzz\n");
    check(s.keys == 0, "a bad value counts nothing");
    check(s.p1[TANG_NES_A] == NES_CTL_B, "a bad value leaves the default");

    /* An unknown key and an unknown section are ignored, not refused. */
    load(&s, "[bogus]\nwhatever = 1\n[input]\np1_zz = b\np1_a = y\n");
    check(s.keys == 1, "unknown key and section: only the real one counts");
    check(s.p1[TANG_NES_A] == NES_CTL_Y, "unknown key and section: still parsed the rest");

    /* Comments, blanks, CRLF and a comment after a value. */
    load(&s, "# a note\r\n\r\n[input]\r\np1_a = y   # the button to use\r\n");
    check(s.keys == 1, "comment after a value: the key still took");
    check(s.p1[TANG_NES_A] == NES_CTL_Y, "comment after a value: the value is right");

    /* The rest of the file. */
    load(&s, "[video]\naspect = 1:1\nscale = 2\noverscan = show\n"
             "[audio]\nvolume = 40\nmute = yes\nchannels = mono\n");
    check(s.keys == 6, "video and audio counted");
    check(s.aspect == TANG_ASPECT_1_1, "aspect 1:1");
    check(s.scale == 2, "scale 2");
    check(s.overscan == 0, "overscan show");
    check(s.volume == 40, "volume 40");
    check(s.mute == 1, "mute yes");
    check(s.channels == TANG_CHANNELS_MONO, "channels mono");

    /* Out-of-range values are clamped, not taken. */
    load(&s, "[audio]\nvolume = 900\n[video]\nscale = 0\n");
    check(s.volume == 100, "volume clamps at 100");
    check(s.scale == 1, "scale clamps at 1");

    /* No file at all: the defaults, and no error. */
    s_text = "";
    s_open_fails = true;
    check(!tang_nes_load(&s, "/nes.ini"), "an absent file reports false");
    check(s.p1[TANG_NES_A] == NES_CTL_B, "an absent file leaves the defaults");
    check(!s.found, "an absent file is not recorded as found");
    s_open_fails = false;

    /* A read that fails part way leaves what was parsed, and claims nothing. */
    s_text = "[input]\np1_a = y\n";
    s_read_fails = true;
    (void)tang_nes_load(&s, "/nes.ini");
    check(s.p1[TANG_NES_A] == NES_CTL_B, "a failed read leaves the defaults");

    /* The shipped file, if the caller named one. */
    if (argc > 1) {
        s_open_fails = false;
        s_read_fails = false;
        s_text = NULL;
        /* Read it for real: the stub above works from a string, so load the
         * file into one first. */
        FILE *f = fopen(argv[1], "rb");
        if (f == NULL) {
            printf("FAIL  cannot open %s\n", argv[1]);
            s_failed++;
        } else {
            static char text[8192];
            size_t got = fread(text, 1, sizeof text - 1, f);
            text[got] = '\0';
            (void)fclose(f);
            load(&s, text);
            printf("ok    %s parses: %u keys understood\n", argv[1],
                   (unsigned)s.keys);
            s_checks++;
            if (s.keys == 0) {
                s_failed++;
            }
        }
    }

    printf("%d checks, %d failed\n", s_checks, s_failed);
    return s_failed != 0;
}
