// SPDX-License-Identifier: MIT
// Independent shell session; only display text is reduced to the 4x4 alphabet.
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

static oled_vterm_t s_vt;
static SemaphoreHandle_t s_lock;
static TaskHandle_t s_task;
static tdsh_session_t *s_session;
static uint8_t s_input[1024];
static unsigned s_head, s_tail;
static td_window_t *s_window;
static uint32_t s_revision, s_drawn;
int tdsh_printf(const char *fmt, ...);

static bool enqueue(const void *data, size_t length)
{
    if (length > sizeof(s_input)-1) return false;
    taskENTER_CRITICAL();
    unsigned available = (s_tail-s_head-1) & (sizeof(s_input)-1);
    if (length > available) { taskEXIT_CRITICAL(); return false; }
    const uint8_t *p=data;
    for (size_t i=0;i<length;i++) {
        s_input[s_head]=p[i]; s_head=(s_head+1)&(sizeof(s_input)-1);
    }
    taskEXIT_CRITICAL();
    return true;
}
static int route_read(void *context)
{
    (void)context;
    int result=-1;
    taskENTER_CRITICAL();
    if (s_tail!=s_head) {
        result=s_input[s_tail]; s_tail=(s_tail+1)&(sizeof(s_input)-1);
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
        oled_vterm_write(&s_vt,p,n); p+=n; remaining-=(size_t)n;
        ++s_revision;
        xSemaphoreGive(s_lock);
        if (remaining) xSemaphoreTake(s_lock,portMAX_DELAY);
    }
    if (!length) xSemaphoreGive(s_lock);
    return (int)length;
}
static tdsh_bl616_route_t s_route={NULL,route_read,route_write,24};
static int io_read(void *context,uint8_t *out)
{
    int c;
    while ((c=route_read(context))<0) vTaskDelay(pdMS_TO_TICKS(5));
    *out=(uint8_t)c;return 0;
}
static int io_write(void *context,const void *data,size_t length)
{ return route_write(context,data,length); }
static int columns(void *context) { (void)context; return 24; }
static void shell_task(void *arg)
{
    (void)arg;
    tdsh_bl616_route_set(&s_route);
    const tdsh_terminal_io_t io={.read_byte=io_read,.write_bytes=io_write,.columns=columns};
    char line[512],prompt[TDSH_MAX_PATH+32];
    route_write(NULL,"OLED Terminal 24x16\r\n",21);
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
static int start_claimed(void)
{
    if (!s_lock) s_lock=xSemaphoreCreateMutex();
    if (!s_lock) return -ENOMEM;
    s_session=calloc(1,sizeof(*s_session));
    if (!s_session) return -ENOMEM;
    int rc=tdsh_session_init(s_session,"root",true);
    if (rc) { free(s_session);s_session=NULL;return rc; }
    s_session->terminal_caps=TDSH_TERM_CAP_ANSI|TDSH_TERM_CAP_COLOR|TDSH_TERM_CAP_FULLSCREEN;
    xSemaphoreTake(s_lock,portMAX_DELAY);
    oled_vterm_init(&s_vt,24,16);s_vt.reply=reply;s_revision++;
    xSemaphoreGive(s_lock);
    TaskHandle_t task=NULL;
    if (xTaskCreate(shell_task,"oledterm",4096,NULL,3,&task)!=pdPASS) {
        free(s_session);s_session=NULL;return -ENOMEM;
    }
    taskENTER_CRITICAL();
    s_task=task;
    taskEXIT_CRITICAL();
    return 0;
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
        td_vcell_t c=s_vt.cells[i];
        cells[i]=(uint16_t)((colour(c.bg)<<12)|(colour(c.fg)<<8)|c.ch);
    }
    *cursor=(s_vt.cursor_visible ? 0x10000:0)|((uint32_t)s_vt.cy<<8)|(uint32_t)s_vt.cx;
    xSemaphoreGive(s_lock);
    return true;
}
static void draw(td_window_t *window,int w,int h)
{
    (void)window;
    td_fill(td_rect(0,0,w,h),' ',7,0);
    if (!s_task) { td_text(0,0,"Unable to start shell",9,0,0);return; }
    xSemaphoreTake(s_lock,portMAX_DELAY);
    for (int y=0;y<16 && y<h;y++) for(int x=0;x<24 && x<w;x++) {
        td_vcell_t c=s_vt.cells[y*24+x];
        if(s_vt.cursor_visible && x==s_vt.cx && y==s_vt.cy) {
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
        tdsh_printf("OLED terminal: %s, 24x16, input only in focused window\r\n",s_task ? "running":"stopped");return 0;
    }
    if(argc==2 && !strcmp(argv[1],"start")) return tang_oled_start() ? 1:0;
    if(argc==3 && !strcmp(argv[1],"run")) {
        size_t n=strlen(argv[2]);
        if(n>510 || tang_oled_start()) return 1;
        char buf[513];buf[0]=21;memcpy(buf+1,argv[2],n);buf[n+1]='\r';
        return enqueue(buf,n+2) ? 0:1;
    }
    tdsh_printf("usage: oledterm start|status|run <quoted command>\r\n");return 1;
}
int tang_oled_register(void)
{
    static const tdsh_command_t cmd={"oledterm","oledterm start|status|run <command>","Independent 24x16 OLED shell",command,0};
    return tdsh_register_command(&cmd);
}
