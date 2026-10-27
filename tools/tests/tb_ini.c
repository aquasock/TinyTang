/* SPDX-License-Identifier: MIT
 * The /tinytang.ini parser (ports/bl616/tang_ini.c) against the contract in
 * tang_ini.h: the layout it settles on, when it releases the sockets and why,
 * what it only warns about, and the word each core is sent for a layout.
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
    assert(bytewise.layout == ini.layout && bytewise.nnotes == ini.nnotes);
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

/* A file given on the command line -- docs/tinytang.ini -- must declare the
 * i2s2_oled layout with nothing to say about it. */
static void example(const char *path)
{
    FILE *f = fopen(path, "rb");
    assert(f != NULL);
    static char text[4096];
    const size_t n = fread(text, 1, sizeof(text) - 1, f);
    fclose(f);
    text[n] = '\0';
    parse(text);
    assert(ini.layout == TANG_LAYOUT_I2S2_OLED && ini.nnotes == 0);
}

int main(int argc, char **argv)
{
    if (argc > 1) {
        example(argv[1]);
    }

    /* The two layouts. */
    parse("[tang]\nlayout = i2s2_oled\n");
    assert(ini.layout == TANG_LAYOUT_I2S2_OLED && ini.nnotes == 0);
    parse("[tang]\nlayout = vga\n");
    assert(ini.layout == TANG_LAYOUT_VGA && ini.nnotes == 0);

    /* The word each core is sent.  VGA right side up is J2 in PMOD0 and J1 in
     * PMOD1, 0x0230, on both cores (PMOD-006). */
    assert(tang_ini_word(TANG_LAYOUT_VGA, TANG_INI_CORE_PHOSPHOR) == 0x0230);
    assert(tang_ini_word(TANG_LAYOUT_VGA, TANG_INI_CORE_DESKTOP) == 0x0230);
    /* OLED in PMOD0 and I2S2 in PMOD1 on Phosphor; the desktop core has no
     * I2S2 and refuses a word naming it, so it gets the OLED alone. */
    assert(tang_ini_word(TANG_LAYOUT_I2S2_OLED, TANG_INI_CORE_PHOSPHOR) == 0x0510);
    assert(tang_ini_word(TANG_LAYOUT_I2S2_OLED, TANG_INI_CORE_DESKTOP) == 0x0010);
    /* Released, or a core without the sockets: nothing. */
    assert(tang_ini_word(TANG_LAYOUT_NONE, TANG_INI_CORE_PHOSPHOR) == 0);
    assert(tang_ini_word(TANG_LAYOUT_NONE, TANG_INI_CORE_DESKTOP) == 0);
    assert(tang_ini_word(TANG_LAYOUT_VGA, 0x4e) == 0);
    assert(tang_ini_word(7, TANG_INI_CORE_DESKTOP) == 0);
    /* No layout ever sets a flip bit. */
    for (uint8_t l = 0; l < 3; l++) {
        assert((tang_ini_word(l, TANG_INI_CORE_PHOSPHOR) & 0x3000u) == 0);
        assert((tang_ini_word(l, TANG_INI_CORE_DESKTOP) & 0x3000u) == 0);
    }

    /* No file content, or no layout line: released, and said so. */
    parse("");
    assert(ini.layout == TANG_LAYOUT_NONE && errors() == 0 && says(0, "no layout line"));
    parse("[tang]\n; nothing seated\n");
    assert(ini.layout == TANG_LAYOUT_NONE && errors() == 0 && says(0, "no layout line"));

    /* CRLF, spacing, case, comments on their own line and after a value, and
     * a last line with no newline. */
    parse("; my Tang\r\n[ TANG ]\r\n# audio\r\n  LAYOUT =  I2S2_Oled   ; beside HDMI");
    assert(ini.layout == TANG_LAYOUT_I2S2_OLED && ini.nnotes == 0);

    /* An unknown layout releases both, naming the two that exist. */
    parse("[tang]\nlayout = audio\n");
    assert(ini.layout == TANG_LAYOUT_NONE && errors() == 2);
    assert(says(2, "unknown layout 'audio'; use i2s2_oled, vga or lora"));
    assert(says(0, "both sockets released"));
    /* "none" is not a layout: leave the line out instead. */
    parse("[tang]\nlayout = none\n");
    assert(ini.layout == TANG_LAYOUT_NONE && says(2, "unknown layout 'none'"));

    /* The old per-socket keys are refused, and release both even beside a good
     * layout, since the file was written for the old contract. */
    parse("[tang]\npmod0 = oledrgb\npmod1 = i2s2\n");
    assert(ini.layout == TANG_LAYOUT_NONE && errors() == 3);
    assert(says(2, "pmod0 is no longer read; write layout = i2s2_oled or vga"));
    assert(says(3, "pmod1 is no longer read"));
    parse("[tang]\nlayout = vga\npmod1_flip = yes\n");
    assert(ini.layout == TANG_LAYOUT_NONE && says(3, "pmod1_flip is no longer read"));
    parse("[tang]\nPMOD0_FLIP = no\nlayout = vga\n");
    assert(ini.layout == TANG_LAYOUT_NONE && says(2, "PMOD0_FLIP is no longer read"));

    /* Given twice: the last wins, with a warning; a later good line undoes an
     * earlier bad one. */
    parse("[tang]\nlayout = vga\nlayout = i2s2_oled\n");
    assert(ini.layout == TANG_LAYOUT_I2S2_OLED && errors() == 0 && says(3, "this line wins"));
    parse("[tang]\nlayout = bogus\nlayout = vga\n");
    assert(ini.layout == TANG_LAYOUT_VGA && errors() == 1);
    parse("[tang]\nlayout = vga\nlayout = bogus\n");
    assert(ini.layout == TANG_LAYOUT_NONE && errors() == 2);

    /* Unknown keys and sections: warned and ignored, nothing released. */
    parse("[tang]\nvideo = 720p\nlayout = vga\n[later]\nlayout = i2s2_oled\nx = 1\n");
    assert(ini.layout == TANG_LAYOUT_VGA && errors() == 0);
    assert(says(2, "unknown key video") && says(4, "section [later] is not used"));
    assert(ini.nnotes == 2);              /* the section is warned about once */
    parse("layout = vga\n[tang]\nlayout = i2s2_oled\n");
    assert(ini.layout == TANG_LAYOUT_I2S2_OLED && says(1, "outside [tang]"));
    parse("[tang]\nthis is not a setting\nlayout = vga\n[broken\nlayout = i2s2_oled\n");
    assert(ini.layout == TANG_LAYOUT_VGA && says(2, "not a key = value") && says(4, "without ']'"));

    /* An overlong line is ignored and the next one still parses. */
    char longline[400];
    snprintf(longline, sizeof(longline), "[tang]\n; %0300d\nlayout = vga\n", 0);
    parse(longline);
    assert(ini.layout == TANG_LAYOUT_VGA && says(2, "longer than 127"));

    /* Past the note table, problems are counted, not lost. */
    parse("[tang]\na=1\nb=1\nc=1\nd=1\ne=1\nf=1\ng=1\nh=1\ni=1\nj=1\nlayout = vga\n");
    assert(ini.nnotes == TANG_INI_NOTES && ini.more_notes == 2 && ini.layout == TANG_LAYOUT_VGA);

    assert(strcmp(tang_ini_layout_name(TANG_LAYOUT_I2S2_OLED), "i2s2_oled") == 0);
    assert(strcmp(tang_ini_layout_name(TANG_LAYOUT_VGA), "vga") == 0);
    assert(strcmp(tang_ini_layout_name(TANG_LAYOUT_LORA), "lora") == 0);
    assert(strcmp(tang_ini_layout_name(4), "?") == 0);
    assert(strcmp(tang_ini_module_name(TANG_PMOD_I2S2), "i2s2") == 0);
    assert(strcmp(tang_ini_module_name(TANG_PMOD_LORA), "lora") == 0);
    assert(strcmp(tang_ini_module_name(9), "?") == 0);

    /* The LoRa layout: personality 8 on PMOD0, PMOD1 released, on the desktop
     * core that carries ABI 1.6's UART. */
    parse("[tang]\nlayout = lora\n");
    assert(ini.layout == TANG_LAYOUT_LORA && !ini.layout_bad);
    assert(tang_ini_word(TANG_LAYOUT_LORA, TANG_INI_CORE_DESKTOP) == (8u << 4));
    assert(tang_ini_word(TANG_LAYOUT_LORA, TANG_INI_CORE_PHOSPHOR) == (8u << 4));

    printf("tb_ini: PASS\n");
    return 0;
}
