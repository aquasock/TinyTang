// SPDX-License-Identifier: MIT
// Independent shell session on the OLED, in one of three font sets: Standard
// (8x8, 12x8), Compact (6x8, 16x8) and Graphics (4x4, 24x16).  Only display
// text is reduced to the panel's printable ASCII.
#include "oled_vterm.h"
#include "tang_oled.h"
#include "tdsh_bl616.h"
#include "tdsh_terminal.h"
#include "tinydesk/td.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The emulator and input ring are allocated when the terminal first starts,
// so a board that never opens it keeps that heap for the desktop and scripts.
typedef struct {
    oled_vterm_t vt;
    uint8_t input[1024];
} oled_state_t;
static oled_state_t *s_st;
static SemaphoreHandle_t s_lock;
static TaskHandle_t s_task;
static tdsh_session_t *s_session;
static unsigned s_head, s_tail;
static td_window_t *s_window;
static uint32_t s_revision, s_drawn;
int tdsh_printf(const char *fmt, ...);

// Indexed by TANG_OLED_GRAPHICS, _STANDARD and _COMPACT.  The emulator's cells
// keep a stride of 24 at every size, and so does the core's cell store.
static const struct { int cols, rows; const char *name; } k_fonts[3] = {
    {24, 16, "graphics"}, {12, 8, "standard"}, {16, 8, "compact"},
};
static volatile int s_font_requested = TANG_OLED_STANDARD;
static int s_font_effective = TANG_OLED_STANDARD;

static bool enqueue(const void *data, size_t length)
{
    if (!s_st || length > sizeof(s_st->input)-1) return false;
    taskENTER_CRITICAL();
    unsigned available = (s_tail-s_head-1) & (sizeof(s_st->input)-1);
    if (length > available) { taskEXIT_CRITICAL(); return false; }
    const uint8_t *p=data;
    for (size_t i=0;i<length;i++) {
        s_st->input[s_head]=p[i]; s_head=(s_head+1)&(sizeof(s_st->input)-1);
    }
    taskEXIT_CRITICAL();
    return true;
}
static int route_read(void *context)
{
    (void)context;
    int result=-1;
    if (!s_st) return -1;
    taskENTER_CRITICAL();
    if (s_tail!=s_head) {
        result=s_st->input[s_tail]; s_tail=(s_tail+1)&(sizeof(s_st->input)-1);
    }
    taskEXIT_CRITICAL();
    return result;
}
static void reply(void *context, const char *data, int length)
{
    (void)context;
    if (length>0) (void)enqueue(data,(size_t)length);
}
static int route_write(void *context,const void *data,size_t length)
{
    (void)context;
    xSemaphoreTake(s_lock,portMAX_DELAY);
    // Bound lock duration even when a program writes a large block.
    const uint8_t *p=data;
    size_t remaining=length;
    while (remaining) {
        int n=remaining>256 ? 256:(int)remaining;
        oled_vterm_write(&s_st->vt,p,n); p+=n; remaining-=(size_t)n;
        ++s_revision;
        xSemaphoreGive(s_lock);
        if (remaining) xSemaphoreTake(s_lock,portMAX_DELAY);
    }
    if (!length) xSemaphoreGive(s_lock);
    return (int)length;
}
static tdsh_bl616_route_t s_route={NULL,route_read,route_write,12};
static int io_read(void *context,uint8_t *out)
{
    int c;
    while ((c=route_read(context))<0) vTaskDelay(pdMS_TO_TICKS(5));
    *out=(uint8_t)c;return 0;
}
static int io_write(void *context,const void *data,size_t length)
{ return route_write(context,data,length); }
static int columns(void *context) { (void)context; return k_fonts[s_font_effective].cols; }
static void shell_task(void *arg)
{
    (void)arg;
    tdsh_bl616_route_set(&s_route);
    const tdsh_terminal_io_t io={.read_byte=io_read,.write_bytes=io_write,.columns=columns};
    char line[512],prompt[TDSH_MAX_PATH+32];
    char banner[24];
    int n=snprintf(banner,sizeof(banner),"OLED %dx%d\r\n",
                   k_fonts[s_font_effective].cols,k_fonts[s_font_effective].rows);
    route_write(NULL,banner,(size_t)n);
    for (;;) {
        snprintf(prompt,sizeof(prompt),"\033[1;32moled\033[0m:%s# ",s_session->cwd);
        int rc=tdsh_terminal_readline(s_session,&io,prompt,line,sizeof(line));
        if (rc>=0 && line[0]) {
            s_session->last_status=tdsh_execute_line(s_session,line);
            if (s_session->logout_requested) {
                s_session->logout_requested=false;
                route_write(NULL,"Session remains open.\r\n",23);
            }
        } else if (rc!=-EINTR) vTaskDelay(pdMS_TO_TICKS(20));
    }
}
static bool s_starting;
// A failed start keeps nothing but the mutex, so it can be retried.
static int start_failed(int rc)
{
    free(s_session);s_session=NULL;
    free(s_st);s_st=NULL;
    return rc;
}
static int start_claimed(void)
{
    if (!s_lock) s_lock=xSemaphoreCreateMutex();
    if (!s_lock) return -ENOMEM;
    s_st=calloc(1,sizeof(*s_st));
    s_session=calloc(1,sizeof(*s_session));
    if (!s_st || !s_session) return start_failed(-ENOMEM);
    int rc=tdsh_session_init(s_session,"root",true);
    if (rc) return start_failed(rc);
    s_session->terminal_caps=TDSH_TERM_CAP_ANSI|TDSH_TERM_CAP_COLOR|TDSH_TERM_CAP_FULLSCREEN;
    xSemaphoreTake(s_lock,portMAX_DELAY);
    oled_vterm_init(&s_st->vt,k_fonts[s_font_effective].cols,k_fonts[s_font_effective].rows);
    s_st->vt.reply=reply;s_revision++;
    s_route.columns=k_fonts[s_font_effective].cols;
    xSemaphoreGive(s_lock);
    TaskHandle_t task=NULL;
    if (xTaskCreate(shell_task,"oledterm",4096,NULL,3,&task)!=pdPASS) return start_failed(-ENOMEM);
    taskENTER_CRITICAL();
    s_task=task;
    taskEXIT_CRITICAL();
    return 0;
}
int tang_oled_font_requested(void) { return s_font_requested; }
bool tang_oled_font_effective(int font)
{
    if (font<0 || font>2 || !s_lock) return false;
    xSemaphoreTake(s_lock,portMAX_DELAY);
    bool changed=font!=s_font_effective;
    if (changed) {
        s_font_effective=font;
        s_route.columns=k_fonts[font].cols;
        // Resizing keeps the text and the cursor's line, and blanks the
        // cells the new grid no longer shows.
        if (s_st) { oled_vterm_resize(&s_st->vt,k_fonts[font].cols,k_fonts[font].rows);++s_revision; }
    }
    xSemaphoreGive(s_lock);
    return changed;
}
bool tang_oled_running(void)
{
    taskENTER_CRITICAL();
    const bool running=s_task!=NULL;
    taskEXIT_CRITICAL();
    return running;
}
int tang_oled_start(void)
{
    // Called by the UI and by any shell's oledterm, never by the polling
    // task. One caller starts the session; a concurrent one is told busy.
    taskENTER_CRITICAL();
    const bool running=s_task!=NULL, claimed=!running && !s_starting;
    if (claimed) s_starting=true;
    taskEXIT_CRITICAL();
    if (running) return 0;
    if (!claimed) return -EBUSY;
    int rc=start_claimed();
    s_starting=false;
    return rc;
}
// Quantize xterm colours to the OLED's 16-colour palette.
static uint8_t colour(uint8_t c)
{
    if (c<16) return c;
    if (c>=232) return c<238 ? 0:c<246 ? 8:c<252 ? 7:15;
    unsigned n=c-16,r=n/36,g=(n/6)%6,b=n%6;
    return (r>=3 ? 1:0)|(g>=3 ? 2:0)|(b>=3 ? 4:0)|((r>=4||g>=4||b>=4) ? 8:0);
}
bool tang_oled_snapshot(uint16_t cells[384],uint32_t *cursor)
{
    if (!s_task) return false;
    xSemaphoreTake(s_lock,portMAX_DELAY);
    for (int i=0;i<384;i++) {
        td_vcell_t c=s_st->vt.cells[i];
        cells[i]=(uint16_t)((colour(c.bg)<<12)|(colour(c.fg)<<8)|c.ch);
    }
    *cursor=(s_st->vt.cursor_visible ? 0x10000:0)|((uint32_t)s_st->vt.cy<<8)|(uint32_t)s_st->vt.cx;
    xSemaphoreGive(s_lock);
    return true;
}
static void draw(td_window_t *window,int w,int h)
{
    (void)window;
    td_fill(td_rect(0,0,w,h),' ',7,0);
    if (!s_task) { td_text(0,0,"Unable to start shell",9,0,0);return; }
    xSemaphoreTake(s_lock,portMAX_DELAY);
    for (int y=0;y<s_st->vt.rows && y<h;y++) for(int x=0;x<s_st->vt.cols && x<w;x++) {
        td_vcell_t c=s_st->vt.cells[y*24+x];
        if(s_st->vt.cursor_visible && x==s_st->vt.cx && y==s_st->vt.cy) {
            uint8_t tmp=c.fg;c.fg=c.bg;c.bg=tmp;
        }
        td_putc(x,y,c.ch==127 ? '?':c.ch,c.fg,c.bg,0);
    }
    s_drawn=s_revision;
    xSemaphoreGive(s_lock);
}
static void tick(td_window_t *window)
{
    if (!s_lock) return;
    xSemaphoreTake(s_lock,portMAX_DELAY);
    bool dirty=s_revision!=s_drawn;
    xSemaphoreGive(s_lock);
    if (dirty) td_win_invalidate(window);
}
static int encode(const td_event_t *event,char out[24])
{
    uint32_t k=event->key;
    uint8_t m=event->mods;
    if((m&TD_MOD_CTRL) && k>='A' && k<='Z') k+=32;
    if((m&TD_MOD_CTRL) && k>='a' && k<='z') {out[0]=(char)(k-'a'+1);return 1;}
    const char *seq=NULL;
    switch(k) {
        case TD_KEY_ENTER:seq="\r";break;
        case TD_KEY_TAB:seq=(m&TD_MOD_SHIFT)?"\033[Z":"\t";break;
        case TD_KEY_BACKSPACE:seq="\177";break;
        case TD_KEY_ESC:seq="\033";break;
        case TD_KEY_DELETE:seq="\033[3~";break;
        case TD_KEY_INSERT:seq="\033[2~";break;
        case TD_KEY_PGUP:seq="\033[5~";break;
        case TD_KEY_PGDN:seq="\033[6~";break;
        default:break;
    }
    const uint32_t keys[]={TD_KEY_UP,TD_KEY_DOWN,TD_KEY_RIGHT,TD_KEY_LEFT,TD_KEY_HOME,TD_KEY_END};
    for(int i=0;i<6;i++) if(k==keys[i]) {
        int mod=1+((m&TD_MOD_SHIFT)?1:0)+((m&TD_MOD_ALT)?2:0)+((m&TD_MOD_CTRL)?4:0);
        return mod==1 ? snprintf(out,24,"\033[%c","ABCDHF"[i]):snprintf(out,24,"\033[1;%d%c",mod,"ABCDHF"[i]);
    }
    int n=0;
    if(m&TD_MOD_ALT) out[n++]='\033';
    if(seq) {size_t len=strlen(seq);memcpy(out+n,seq,len);return n+(int)len;}
    if(k>=32 && k<TD_KEY_BASE) return n+td_utf8_encode(k,(uint8_t*)out+n);
    return 0;
}
static bool event(td_window_t *window,const td_event_t *ev)
{
    if(window!=td_win_focused()) return false;
    if(ev->type==TD_EV_PASTE) {
        int n=0;const char *p=td_paste_text(&n);
        return p && n>0 && enqueue(p,(size_t)n);
    }
    if(ev->type!=TD_EV_KEY) return false;
    char bytes[24];int n=encode(ev,bytes);
    return n>0 && enqueue(bytes,(size_t)n);
}
static void close_window(td_window_t *window) {(void)window;s_window=NULL;}
static void launch(void)
{
    if(td_win_is_open(s_window)) {td_win_focus(s_window);return;}
    (void)tang_oled_start();
    td_window_desc_t d={.title="OLED Terminal",.rect={-1,-1,26,18},
        .flags=TD_WIN_MOVABLE|TD_WIN_CLOSABLE|TD_WIN_RAW_KEYS,
        .on_draw=draw,.on_event=event,.on_tick=tick,.tick_ms=40,.on_close=close_window};
    s_window=td_win_create(&d);
}
void td_oled_terminal_register(void)
{
    s_window=NULL;
    static const td_app_t app={"OLED Terminal",launch,"O_"};
    td_app_register(&app);
}
static int command(tdsh_session_t *session,int argc,char **argv)
{
    (void)session;
    if(argc==2 && !strcmp(argv[1],"status")) {
        const int f=s_font_effective;
        tdsh_printf("OLED terminal: %s, %s %dx%d%s, input only in focused window\r\n",
                    s_task ? "running":"stopped",k_fonts[f].name,k_fonts[f].cols,k_fonts[f].rows,
                    f!=s_font_requested ? " (the loaded core shows Graphics only)":"");
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"font")) {
        tdsh_printf("%s\r\n",k_fonts[s_font_requested].name);return 0;
    }
    if(argc==3 && !strcmp(argv[1],"font")) {
        for(int f=0;f<3;f++) if(!strcmp(argv[2],k_fonts[f].name)) {
            // Takes effect now; the panel follows within one link poll, and a
            // core before desktop ABI 1.2 holds the terminal at Graphics.
            s_font_requested=f;
            if(s_lock) (void)tang_oled_font_effective(f);
            else s_font_effective=f;
            return 0;
        }
        tdsh_printf("oledterm: font is standard, compact or graphics\r\n");return 1;
    }
    if(argc==2 && !strcmp(argv[1],"start")) return tang_oled_start() ? 1:0;
    if(argc==3 && !strcmp(argv[1],"run")) {
        size_t n=strlen(argv[2]);
        if(n>510 || tang_oled_start()) return 1;
        char buf[513];buf[0]=21;memcpy(buf+1,argv[2],n);buf[n+1]='\r';
        return enqueue(buf,n+2) ? 0:1;
    }
    tdsh_printf("usage: oledterm start|status|font [standard|compact|graphics]|run <quoted command>\r\n");return 1;
}
int tang_oled_register(void)
{
    static const tdsh_command_t cmd={"oledterm","oledterm start|status|font [name]|run <command>","Independent OLED shell: standard 12x8, compact 16x8, graphics 24x16",command,0};
    return tdsh_register_command(&cmd);
}
