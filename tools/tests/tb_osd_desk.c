/*
 * Host test for the desktop layer's firmware side.
 *
 * tang_osd_desk.c compiles inside the BL616 firmware, which means the only
 * thing a build can tell you is that it links.  Everything that can actually be
 * wrong -- the palette, the row-span diff, the frame framing, and the answer
 * the layer gives the desktop when it asks how big it is -- only shows up when
 * bytes go through it.  So they are checked here, on the host, against the real
 * source: this file includes tang_osd_desk.c rather than a copy of it.
 *
 *   tools/tests/test_osd_desk.sh
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* The real driver, not a copy. */
#include "tang_osd_desk.c"

/* ------------------------------------------------------- the stand-ins */

/* A frame recorder.  Everything the driver sends lands here, so the test can
 * look at exactly what would have gone down the UART. */
#define MAX_FRAMES  256
#define MAX_PAYLOAD 512

static uint8_t g_type[MAX_FRAMES];
static uint8_t g_payload[MAX_FRAMES][MAX_PAYLOAD];
static size_t  g_len[MAX_FRAMES];
static int     g_frames;

static void frames_clear(void)
{
    g_frames = 0;
}

int tang_fpga_link_open(void) { return 0; }
void tang_fpga_lock(void) { }
void tang_fpga_unlock(void) { }

int tang_fpga_frame(uint8_t type, const uint8_t *payload, size_t length)
{
    if (g_frames >= MAX_FRAMES || length > MAX_PAYLOAD) {
        return -1;
    }
    g_type[g_frames] = type;
    g_len[g_frames] = length;
    if (length != 0) {
        memcpy(g_payload[g_frames], payload, length);
    }
    g_frames++;
    return 0;
}

/* Records what the driver tells the OSD module, so the test can check the
 * overlay is left in the right state. */
static int  g_osd_set_calls;
static bool g_osd_last;

int tang_osd_set(bool on)
{
    g_osd_set_calls++;
    g_osd_last = on;
    return 0;
}

bool tang_osd_shown(void)
{
    return g_osd_last;
}

/* The locked forms the driver uses for the re-arm and the F12 / L switch
 * (tang_osd.c): one atomic read-and-send on the board, the same here. */
int tang_osd_reassert(void)
{
    return tang_osd_set(g_osd_last);
}

int tang_osd_toggle(void)
{
    return tang_osd_set(!g_osd_last);
}

/* The keyboard link: no keyboard here, so an empty report, never fresh. */
bool tang_fpga_keyboard(uint8_t out[8])
{
    for (int i = 0; i < 8; i++) {
        out[i] = 0;
    }
    return false;
}

/* The Bluetooth keyboard: a report the test sets, live while "connected". */
static uint8_t g_ble_rep[8];
static bool    g_ble_live;

bool tang_ble_keyboard(uint8_t out[8])
{
    for (int i = 0; i < 8; i++) {
        out[i] = g_ble_live ? g_ble_rep[i] : 0;
    }
    return g_ble_live;
}

/* The Bluetooth mouse: what the next poll takes, live while "connected". */
static tang_mouse_t g_mouse;
static bool         g_mouse_live;

bool tang_ble_mouse(tang_mouse_t *out)
{
    if (g_mouse_live) {
        *out = g_mouse;
    } else {
        memset(out, 0, sizeof(*out));
    }
    g_mouse.dx = g_mouse.dy = g_mouse.wheel = 0;
    return g_mouse_live;
}

static void ble_report(uint8_t mods, uint8_t key)
{
    memset(g_ble_rep, 0, sizeof(g_ble_rep));
    g_ble_rep[0] = mods;
    g_ble_rep[2] = key;
}

/* Everything the desktop would read, as a string. */
static void read_typed(char *buf, size_t cap)
{
    size_t n = 0;
    int b;
    while ((b = tang_osd_desk_read_byte()) >= 0) {
        if (n + 1 < cap) {
            buf[n++] = (char)b;
        }
    }
    buf[n] = '\0';
}

/* The pad the poll sees. */
static uint16_t g_joy;

void tang_fpga_joypad(uint16_t *joy1, uint16_t *joy2)
{
    *joy1 = g_joy;
    *joy2 = 0;
}

void vTaskDelay(TickType_t ticks) { (void)ticks; }
/* Milliseconds, since the stub's tick rate is 1000 Hz. */
static TickType_t g_tick;
TickType_t xTaskGetTickCount(void) { return g_tick; }

/* Report failure so no task starts: the test drives the diff itself. */
BaseType_t xTaskCreate(void (*fn)(void *), const char *name, uint32_t stack_words,
                       void *arg, unsigned priority, void *handle)
{
    (void)fn; (void)name; (void)stack_words; (void)arg; (void)priority; (void)handle;
    return pdFAIL;
}

int tdsh_printf(const char *fmt, ...) { (void)fmt; return 0; }

/* ------------------------------------------------------------- the checks */

static int fails;
static int checks;

static void check(bool ok, const char *what)
{
    checks++;
    if (!ok) {
        fails++;
        printf("  FAIL: %s\n", what);
    }
}

static size_t feed(const char *s)
{
    tang_osd_desk_feed(s, strlen(s));
    return strlen(s);
}

int main(void)
{
    printf("osd desk: %dx%d grid\n", TANG_DESK_COLS, TANG_DESK_ROWS);

    /* ---- enabling paints the whole grid, because nothing matches yet ---- */
    frames_clear();
    check(tang_osd_desk_set(true) == 0, "set(true) failed");
    check(tang_osd_desk_enabled(), "layer not enabled after set(true)");
    check(g_frames == TANG_DESK_ROWS * 2 + 1,
          "first paint should be the enable, then one cursor+run per row");
    check(g_type[0] == 0x15 && g_len[0] == 1 && g_payload[0][0] == 1,
          "the enable frame is not the first thing sent");
    check(g_type[1] == 0x13 && g_len[1] == 2, "first frame is not a cursor move");
    check(g_type[2] == 0x14 && g_len[2] == TANG_DESK_COLS * 5,
          "second frame is not a full-width cell run");
    check(g_payload[2][0] == ' ', "a cleared cell is not a space");
    if (g_frames != TANG_DESK_ROWS * 2 + 1) {
        printf("        (first paint: %d frames)\n", g_frames);
    }

    /* ---- a known paint: two coloured letters on row 0 -------------------
     * 38;5;196 is the palette's pure red, 48;5;21 its pure blue.  In 15-bit
     * BGR5 red is 0x001F and blue is 0x7C00, so the four colour bytes below
     * are derived from the palette by hand and not from the code under test. */
    frames_clear();
    feed("\x1b[2J\x1b[H");
    feed("\x1b[38;5;196m\x1b[48;5;21m");
    feed("AB");
    flush();

    check(g_frames == 2, "two changed cells should be one cursor and one run");
    if (g_frames == 2) {
        check(g_type[0] == 0x13 && g_payload[0][0] == 0 && g_payload[0][1] == 0,
              "the run does not start at (0,0)");
        check(g_type[1] == 0x14 && g_len[1] == 10, "the run is not two cells");
        check(g_payload[1][0] == 'A' && g_payload[1][5] == 'B',
              "the glyphs did not come through");
        const uint8_t *c = g_payload[1];
        check(c[1] == 0x00 && c[2] == 0x1F, "foreground 196 is not BGR5 red");
        check(c[3] == 0x7C && c[4] == 0x00, "background 21 is not BGR5 blue");
        check(c[6] == 0x00 && c[7] == 0x1F, "the second cell lost its colour");
    }

    /* ---- nothing changed, so nothing is sent ---- */
    frames_clear();
    flush();
    check(g_frames == 0, "an unchanged screen sent frames");

    /* ---- the size answer -------------------------------------------------
     * The desktop asks by moving the cursor past the corner and reading it
     * back, so the answer is the layer's own grid.  45 rows by 80 columns. */
    {
        char got[32];
        int n = 0, b;
        feed("\x1b[999;999H\x1b[6n");
        while ((b = tang_osd_desk_read_byte()) >= 0 && n < (int)sizeof(got) - 1) {
            got[n++] = (char)b;
        }
        got[n] = '\0';
        check(strcmp(got, "\x1b[45;80R") == 0, "size answer is not ESC[45;80R");
        if (strcmp(got, "\x1b[45;80R") != 0) {
            printf("        (got \"%s\")\n", got);
        }
    }

    /* ---- box-drawing glyphs become the ASCII the core's font has ---------
     * The desktop draws its window frames with these, and the core's font is
     * ASCII only.  Masking a 16-bit code point to seven bits would turn the
     * double-frame corner into 'T' and a frame into prose. */
    frames_clear();
    feed("\x1b[10;1H" "\xe2\x95\x94");   /* U+2554, double top-left corner */
    feed("\xe2\x98\x83");                 /* U+2603, no ASCII form */
    flush();
    check(g_frames == 2, "two adjacent glyphs should be one run");
    if (g_frames == 2) {
        check(g_payload[1][0] == '+', "a double corner did not become '+'");
        check(g_payload[1][5] == '?', "an unmappable glyph did not become '?'");
    }

    /* ---- one changed cell is a span of one, not a whole row ---- */
    frames_clear();
    feed("\x1b[3;7Hx");
    flush();
    check(g_frames == 2, "one changed cell should still be one cursor and one run");
    if (g_frames == 2) {
        check(g_payload[0][0] == 6 && g_payload[0][1] == 2,
              "the span did not start at the changed cell");
        check(g_len[1] == 5, "a one-cell run is not five bytes");
        check(g_payload[1][0] == 'x', "the changed glyph is wrong");
    }

    /* ---- a write during a send is not lost -------------------------------
     * The shadow is updated at copy time, so a change arriving after the row
     * was copied must still be found by the next poll. */
    frames_clear();
    feed("\x1b[5;1H" "one");
    flush();
    const int first_pass = g_frames;
    check(first_pass == 2, "the row did not go out in one pass");
    frames_clear();
    feed("\x1b[5;4H" "two");          /* lands after the previous send */
    flush();
    check(g_frames == 2, "the later change was lost");

    /* ---- the pointer is drawn, because nothing else here draws it -------
     * tinydesk never draws a mouse pointer: a real terminal does.  Over the OSD
     * there is no terminal, so without this the pointer would be invisible and
     * the controller unusable.  The cell under it is shown reversed, and the
     * pad starts at the middle of an 80x45 grid: cell (40,22).  The pointer
     * exists only while the desktop runs, which turns it on (cycle 15); this
     * section is the desktop's view, and console mode is checked at the end. */
    tang_osd_desk_set_pointer(true);
    frames_clear();
    feed("\x1b[23;41H");                          /* 1-based row 23, column 41 */
    feed("\x1b[38;5;196m\x1b[48;5;21m" "M");      /* (40,22) red on blue */
    feed("\x1b[38;5;46m\x1b[48;5;16m" "N");       /* (41,22) green on black */
    flush();
    check(g_frames == 2, "the marked cells did not go out");
    if (g_frames == 2) {
        check(g_payload[0][0] == 40 && g_payload[0][1] == 22,
              "the run is not at the pointer");
        check(g_len[1] == 10, "the run is not two cells");
        /* Under the pointer the colours are swapped: 0x7C00 (blue) as the
         * foreground, 0x001F (red) as the background. */
        check(g_payload[1][1] == 0x7C && g_payload[1][2] == 0x00,
              "the pointer cell is not reversed");
        check(g_payload[1][3] == 0x00 && g_payload[1][4] == 0x1F,
              "the pointer cell kept its background");
        /* Its neighbour is untouched: 0x03E0 (green) on 0x0000 (black). */
        check(g_payload[1][6] == 0x03 && g_payload[1][7] == 0xE0,
              "a cell away from the pointer was reversed too");
        check(g_payload[1][8] == 0x00 && g_payload[1][9] == 0x00,
              "a cell away from the pointer lost its background");
    }

    /* ---- moving off a cell puts the real one back ------------------------
     * The shadow keeps what was actually sent, pointer and all, so when the
     * pointer leaves, the shadow disagrees with the emulator and the next poll
     * re-sends the truth.  Nothing has to remember where the pointer was, and
     * no cell is left showing an inverted square forever. */
    frames_clear();
    {
        char seq[96];
        bool toggle = false;
        (void)tang_pad_step(&s_pad, TANG_PAD_RIGHT, seq, sizeof(seq), &toggle);
    }
    flush();
    check(g_frames == 2, "moving the pointer should touch one row");
    if (g_frames == 2) {
        check(g_payload[0][0] == 40 && g_payload[0][1] == 22,
              "the run does not cover the old and new pointer cells");
        check(g_len[1] == 10, "the run is not two cells");
        /* Cell 40 is back to red-on-blue; cell 41 is now the reversed one. */
        check(g_payload[1][1] == 0x00 && g_payload[1][2] == 0x1F,
              "the cell the pointer left was not restored");
        check(g_payload[1][3] == 0x7C && g_payload[1][4] == 0x00,
              "the restored cell kept the pointer's colours");
        check(g_payload[1][6] == 0x00 && g_payload[1][7] == 0x00,
              "the new pointer cell is not reversed");
        check(g_payload[1][8] == 0x03 && g_payload[1][9] == 0xE0,
              "the new pointer cell kept its background");
    }

    /* ---- the pad belongs to the game while the desktop is hidden ---------
     * L has to be read then -- that is exactly when it is wanted -- but a
     * pointer nobody can see should not wander off while somebody is playing.
     * Coming back to find it somewhere else reads as a fault. */
    g_joy = TANG_PAD_RIGHT;
    g_osd_last = true;
    while (tang_osd_desk_read_byte() >= 0) { }
    desk_poll();
    check(tang_osd_desk_read_byte() >= 0, "a visible pointer did not move");

    g_osd_last = false;
    while (tang_osd_desk_read_byte() >= 0) { }
    desk_poll();
    check(tang_osd_desk_read_byte() < 0, "a hidden pointer moved anyway");

    g_osd_set_calls = 0;
    g_joy = TANG_PAD_LB;
    desk_poll();
    check(g_osd_set_calls == 1, "L did not switch while the desktop was hidden");

    /* ---- a reprogrammed core takes the layer with it ---------------------
     * tangload wipes the FPGA: the enable goes back to 0 and the cell store
     * goes blank, while the shadow still matches the emulator -- so the diff
     * would send nothing and the desktop would stay gone for good.  That is
     * what launching a cartridge from the desktop does. */
    g_joy = 0;
    g_osd_last = true;
    frames_clear();
    tang_osd_desk_core_reloaded();
    desk_poll();
    check(g_frames == 1 && g_type[0] == 0x15 && g_payload[0][0] == 1,
          "a reprogrammed core did not re-enable the layer");
    check(g_osd_last, "a reprogrammed core did not re-assert a shown overlay");
    frames_clear();
    flush();
    check(g_frames == TANG_DESK_ROWS * 2,
          "the layer was not repainted after a reload");

    /* ---- stopping the layer stops the overlay too ------------------------
     * Disabling it while the overlay stays asserted falls through the
     * compositor to the legacy page: a black screen carrying the core's logo,
     * which is the symptom PROT-005 describes.  Stopping means the game comes
     * back. */
    g_osd_set_calls = 0;
    (void)tang_osd_desk_set(false);
    check(g_osd_set_calls == 1, "stopping the layer did not touch the overlay");
    check(!g_osd_last, "stopping the layer left the overlay asserted");
    check(!tang_osd_desk_enabled(), "the layer still reports itself enabled");

    /* ---- the re-arm restores the overlay, it does not force it ----------
     * With the game on screen the overlay is off, and a reload must leave it
     * off: forcing it on put TinyDesk back over a running game (cycle 14). */
    g_osd_last = false;
    frames_clear();
    tang_osd_desk_core_reloaded();
    desk_poll();
    check(!g_osd_last, "a reprogrammed core put a hidden overlay back on");

    /* ---- console mode has no pointer ------------------------------------
     * Off, the pointer is not drawn and the pad sends nothing to the shell:
     * its mouse reports would arrive at the prompt as text (cycle 15).  The
     * layer is turned back on first: with it off the desktop reads nothing,
     * and this check would pass whatever the pad did. */
    (void)tang_osd_desk_set(true);
    tang_osd_desk_set_pointer(false);
    g_osd_last = true;
    g_joy = TANG_PAD_RIGHT;
    while (tang_osd_desk_read_byte() >= 0) { }
    desk_poll();
    check(tang_osd_desk_read_byte() < 0, "the pad sent mouse reports in console mode");
    g_joy = 0;
    while (tang_osd_desk_read_byte() >= 0) { }

    /* ---- the Bluetooth keyboard types like the wired one ----------------- */
    {
        char got[64];

        g_tick = 1000;
        g_ble_live = true;
        ble_report(0, 0x04);                     /* a */
        desk_poll();
        read_typed(got, sizeof(got));
        check(strcmp(got, "a") == 0, "a Bluetooth key did not type");

        /* A BLE keyboard sends nothing while a key is held, so repeat must
         * outlive the wired link's 300 ms heartbeat timeout while connected. */
        g_tick = 1600;
        desk_poll();
        read_typed(got, sizeof(got));
        check(strcmp(got, "a") == 0, "a held Bluetooth key did not repeat");
        g_tick = 1700;
        desk_poll();
        read_typed(got, sizeof(got));
        check(strcmp(got, "a") == 0, "a held Bluetooth key stopped repeating");

        /* The link drops with the key held: released, never repeated. */
        g_ble_live = false;
        g_tick = 1800;
        desk_poll();
        g_tick = 2500;
        desk_poll();
        read_typed(got, sizeof(got));
        check(got[0] == '\0', "a key held when the link dropped kept repeating");

        /* Console mode: left-alt with an arrow types nothing and moves nothing,
         * as the core's pointer mode does for the wired keyboard. */
        g_ble_live = true;
        ble_report(TANG_KEY_LALT, 0x4F);         /* left-alt + Right */
        g_tick = 2600;
        desk_poll();
        read_typed(got, sizeof(got));
        check(got[0] == '\0', "left-alt + Right reached the shell in console mode");
        ble_report(0, 0);
        g_tick = 2700;
        desk_poll();

        /* The desktop: left-alt with an arrow moves the pointer, and does not
         * also type an arrow. */
        tang_osd_desk_set_pointer(true);
        ble_report(TANG_KEY_LALT, 0x4F);
        g_tick = 2800;
        desk_poll();
        read_typed(got, sizeof(got));
        check(strncmp(got, "\x1b[<", 3) == 0, "left-alt + Right did not move the pointer");
        check(strstr(got, "\x1b[C") == NULL, "left-alt + Right also typed an arrow");

        /* Without left-alt the same key is only an arrow. */
        ble_report(0, 0);
        g_tick = 2900;
        desk_poll();
        read_typed(got, sizeof(got));
        ble_report(0, 0x4F);
        g_tick = 3000;
        desk_poll();
        read_typed(got, sizeof(got));
        check(strcmp(got, "\x1b[C") == 0, "Right without left-alt is not an arrow");
        tang_osd_desk_set_pointer(false);

        /* F12 on the Bluetooth keyboard switches the screen, once per press. */
        ble_report(0, 0);
        g_tick = 3100;
        desk_poll();
        g_osd_set_calls = 0;
        ble_report(0, TANG_KEY_USAGE_TOGGLE);
        g_tick = 3200;
        desk_poll();
        g_tick = 3300;
        desk_poll();
        check(g_osd_set_calls == 1, "Bluetooth F12 did not switch exactly once");
        check(!g_osd_last, "Bluetooth F12 did not hide TinyDesk");

        /* With the core on screen the keys are the game's: absorbed. */
        ble_report(0, 0x05);                     /* b */
        g_tick = 3400;
        desk_poll();
        read_typed(got, sizeof(got));
        check(got[0] == '\0', "a Bluetooth key typed while the core had the screen");
        g_ble_live = false;
    }

    /* ---- the Bluetooth mouse drives the desktop's pointer ---------------- */
    {
        char got[256];

        g_osd_last = true;
        tang_osd_desk_set_pointer(true);
        g_mouse_live = true;
        while (tang_osd_desk_read_byte() >= 0) { }

        const int x0 = s_pad.x;
        g_mouse.dx = 32;                         /* two cells right */
        desk_poll();
        read_typed(got, sizeof(got));
        check(strncmp(got, "\x1b[<35;", 6) == 0, "the mouse did not move the pointer");
        check(s_pad.x == x0 + 2, "32 counts did not move two cells");

        g_mouse.buttons = TANG_MOUSE_LEFT;
        desk_poll();
        read_typed(got, sizeof(got));
        check(strncmp(got, "\x1b[<0;", 5) == 0 && got[strlen(got) - 1] == 'M',
              "the mouse's left button did not press");

        /* The link drops with the button held: released, not left down. */
        g_mouse_live = false;
        desk_poll();
        read_typed(got, sizeof(got));
        check(strncmp(got, "\x1b[<0;", 5) == 0 && got[strlen(got) - 1] == 'm',
              "a button held when the mouse dropped was not released");
        g_mouse.buttons = 0;

        /* Console mode: the mouse moves nothing and sends nothing. */
        g_mouse_live = true;
        tang_osd_desk_set_pointer(false);
        const int x1 = s_pad.x;
        g_mouse.dx = 64;
        g_mouse.wheel = 1;
        desk_poll();
        read_typed(got, sizeof(got));
        check(got[0] == '\0', "the mouse sent reports in console mode");
        check(s_pad.x == x1, "the mouse moved the pointer in console mode");

        /* Movement made in console mode is not saved up for the desktop. */
        tang_osd_desk_set_pointer(true);
        desk_poll();
        read_typed(got, sizeof(got));
        check(got[0] == '\0', "console-mode movement came out in the desktop");

        /* The wheel. */
        g_mouse.wheel = -1;
        desk_poll();
        read_typed(got, sizeof(got));
        check(strncmp(got, "\x1b[<65;", 6) == 0, "the wheel did not scroll down");
        tang_osd_desk_set_pointer(false);
        g_mouse_live = false;
    }

    /* ---- a push that does not fit is dropped whole ----------------------- */
    {
        char got[DESK_IN_MAX + 8];
        char big[DESK_IN_MAX];
        while (tang_osd_desk_read_byte() >= 0) { }
        memset(big, 'x', sizeof(big));
        desk_in_push(big, DESK_IN_MAX - 10);
        desk_in_push("\x1b[<0;1;1M", 11);       /* one byte too many */
        read_typed(got, sizeof(got));
        check(strlen(got) == DESK_IN_MAX - 10, "a push that did not fit was cut short");
    }

    printf("osd desk: %s (%d checks)\n", fails == 0 ? "PASS" : "FAIL", checks);
    return fails == 0 ? 0 : 1;
}
