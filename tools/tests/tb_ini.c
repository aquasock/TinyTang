/* SPDX-License-Identifier: MIT
 * The /tang.ini parser (ports/bl616/tang_ini.c) against the contract in
 * tang_ini.h: the socket control word it produces, which sockets it releases
 * and why, and what it only warns about.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "tang_ini.h"

static tang_ini_t ini;

static void parse_in(const char *text, size_t chunk)
{
    tang_ini_begin(&ini);
    const size_t n = strlen(text);
    for (size_t i = 0; i < n; i += chunk) {
        tang_ini_feed(&ini, text + i, n - i < chunk ? n - i : chunk);
    }
    tang_ini_end(&ini);
}

static void parse(const char *text)
{
    /* Whole, then a byte at a time: lines may span feeds. */
    parse_in(text, 1);
    const tang_ini_t bytewise = ini;
    parse_in(text, strlen(text) + 1);
    assert(bytewise.word == ini.word && bytewise.nnotes == ini.nnotes);
}

static int errors(void)
{
    int n = 0;
    for (unsigned i = 0; i < ini.nnotes; i++) {
        n += ini.notes[i].error;
    }
    return n;
}

static bool says(unsigned line, const char *part)
{
    for (unsigned i = 0; i < ini.nnotes; i++) {
        if (ini.notes[i].line == line && strstr(ini.notes[i].text, part) != NULL) {
            return true;
        }
    }
    for (unsigned i = 0; i < ini.nnotes; i++) {
        printf("  line %u: %s\n", ini.notes[i].line, ini.notes[i].text);
    }
    return false;
}

/* A file given on the command line -- docs/tang.ini -- must be the standard
 * pair with nothing to say about it. */
static void example(const char *path)
{
    FILE *f = fopen(path, "rb");
    assert(f != NULL);
    static char text[4096];
    const size_t n = fread(text, 1, sizeof(text) - 1, f);
    fclose(f);
    text[n] = '\0';
    parse(text);
    assert(ini.word == 0x2410 && ini.nnotes == 0);
}

int main(int argc, char **argv)
{
    if (argc > 1) {
        example(argv[1]);
    }
    /* The contract's own example is the standard pair, 0x2410. */
    parse("[tang]\npmod0 = oledrgb\npmod0_flip = no\npmod1 = encoder\npmod1_flip = yes\n");
    assert(ini.word == 0x2410 && ini.nnotes == 0);
    assert(ini.module[0] == TANG_PMOD_OLEDRGB && ini.module[1] == TANG_PMOD_ENCODER);
    assert(!ini.flip[0] && ini.flip[1]);

    /* No file content: both released, nothing to say. */
    parse("");
    assert(ini.word == 0 && ini.nnotes == 0);

    /* CRLF, spacing, case, comments on their own line and after a value, and
     * a last line with no newline. */
    parse("; my Tang\r\n[ TANG ]\r\n  PMOD0 =  OLEDRGB   ; the panel\r\n# encoder\r\n"
          "pmod1=Encoder\r\npmod1_flip=YES");
    assert(ini.word == 0x2410 && ini.nnotes == 0);

    /* Every personality, either socket. */
    parse("[tang]\npmod0 = i2s2\n");
    assert(ini.word == 0x0050 && ini.nnotes == 0);
    parse("[tang]\npmod1 = i2s2\npmod0 = encoder\npmod0_flip = yes\n");
    assert(ini.word == 0x1540);
    parse("[tang]\npmod0 = none\npmod1 = oledrgb\n");
    assert(ini.word == 0x0100 && ini.nnotes == 0);

    /* The PmodVGA takes both halves. */
    parse("[tang]\npmod0 = vga_j1\npmod1 = vga_j2\n");
    assert(ini.word == 0x0320 && ini.nnotes == 0);
    parse("[tang]\npmod0 = vga_j2\npmod1 = vga_j1\npmod1_flip = yes\n");
    assert(ini.word == 0x2230);
    /* A half without its partner is released; the other socket stays. */
    parse("[tang]\npmod0 = vga_j1\npmod1 = oledrgb\n");
    assert(ini.word == 0x0100 && errors() == 1 && says(2, "vga_j1 needs vga_j2 in pmod1"));
    parse("[tang]\npmod0 = vga_j1\npmod1 = vga_j1\n");
    assert(ini.word == 0 && errors() == 2);
    /* A refused partner counts as none. */
    parse("[tang]\npmod0 = vga_j1\npmod1 = vga_j3\n");
    assert(ini.word == 0 && errors() == 2 && says(3, "unknown module 'vga_j3'"));

    /* An unknown module releases its socket only. */
    parse("[tang]\npmod0 = oled\npmod1 = encoder\n");
    assert(ini.word == 0x0400 && errors() == 1 && says(2, "pmod0 released"));
    /* So does a flip that is not yes or no, even with a good module... */
    parse("[tang]\npmod0 = oledrgb\npmod0_flip = maybe\n");
    assert(ini.word == 0 && errors() == 1 && says(3, "is not yes or no"));
    /* ...and a later module line does not undo the bad flip. */
    parse("[tang]\npmod0_flip = 1\npmod0 = oledrgb\n");
    assert(ini.word == 0 && errors() == 1);
    /* A later good flip does. */
    parse("[tang]\npmod0_flip = 1\npmod0 = oledrgb\npmod0_flip = no\n");
    assert(ini.word == 0x0010 && errors() == 1 && says(4, "given again"));

    /* Given twice: the last wins, with a warning. */
    parse("[tang]\npmod1 = oledrgb\npmod1 = encoder\n");
    assert(ini.word == 0x0400 && errors() == 0 && says(3, "this line wins"));
    parse("[tang]\npmod1 = bogus\npmod1 = encoder\n");
    assert(ini.word == 0x0400 && errors() == 1);

    /* Unknown keys and sections: warned and ignored, nothing released. */
    parse("[tang]\nvideo = 720p\npmod0 = oledrgb\n[later]\npmod1 = encoder\nx = 1\n");
    assert(ini.word == 0x0010 && errors() == 0);
    assert(says(2, "unknown key video") && says(4, "section [later] is not used"));
    assert(ini.nnotes == 2);              /* the section is warned about once */
    parse("pmod0 = oledrgb\n[tang]\npmod1 = encoder\n");
    assert(ini.word == 0x0400 && says(1, "outside [tang]"));
    parse("[tang]\nthis is not a setting\npmod0 = oledrgb\n[broken\npmod1 = encoder\n");
    assert(ini.word == 0x0010 && says(2, "not a key = value") && says(4, "without ']'"));

    /* A flip with no module has nothing to flip. */
    parse("[tang]\npmod1_flip = yes\n");
    assert(ini.word == 0 && errors() == 0 && says(2, "no module to apply to"));

    /* An overlong line is ignored and the next one still parses. */
    char longline[400];
    snprintf(longline, sizeof(longline), "[tang]\n; %0300d\npmod0 = encoder\n", 0);
    parse(longline);
    assert(ini.word == 0x0040 && says(2, "longer than 127"));

    /* Past the note table, problems are counted, not lost. */
    parse("[tang]\na=1\nb=1\nc=1\nd=1\ne=1\nf=1\ng=1\nh=1\ni=1\nj=1\npmod0 = i2s2\n");
    assert(ini.nnotes == TANG_INI_NOTES && ini.more_notes == 2 && ini.word == 0x0050);

    assert(strcmp(tang_ini_module_name(TANG_PMOD_I2S2), "i2s2") == 0);
    assert(strcmp(tang_ini_module_name(9), "?") == 0);

    printf("tb_ini: PASS\n");
    return 0;
}
