// SPDX-License-Identifier: MIT
#include "../tang_oled.h"
#include "fpga_debug.h"
extern "C" {
#include "FreeRTOS.h"
#include "semphr.h"
#include "tang_fpga_link.h"
}
#include <cstdlib>
#include <cstring>

// Called only by osddesk. Its buffers are allocated on the first poll after
// the terminal starts, off osddesk's 4 KB stack and out of the heap only
// when the OLED is used.
//
// The loaded core is identified once per load with the legacy ID command every
// core answers, as pmod_sockets.cpp does; only the desktop core (0x54) is then
// sent extended frames.  Any other core gets nothing more until the next load,
// so a game core is never sent frames it does not speak and osddesk, which
// also watches for F12, never waits out a timeout on it.
//
// The terminal's font set goes to register 0x118 on a desktop core at ABI 1.2
// or later, before the cells, whenever it changes or the panel is repainted.
// An older desktop core shows only the 4x4 Graphics set, so the terminal is
// sized for that instead.
namespace {
constexpr uint8_t DESKTOP_ID=0x54;
constexpr uint32_t FONT_ABI=0x00010002;   // desktop ABI with register 0x118
constexpr unsigned RECHECK_POLLS=25;    // ~1 s at osddesk's 40 ms
constexpr unsigned ID_ATTEMPTS=3;
constexpr uint32_t ID_TIMEOUT_MS=100;
enum core_kind {UNKNOWN,DESKTOP,OTHER};
}
static SemaphoreHandle_t lock;
static bool paused,available,full=true;
static core_kind kind=UNKNOWN;
static unsigned probe_tick,id_attempts;
struct link_buffers {
    uint16_t cells[384],shadow[384];
    uint32_t words[64];
};
static link_buffers *bufs;
static uint32_t last_cursor=0xffffffff;
static uint32_t core_abi;
static int font_sent=-1;
static bool read(uint32_t address,uint32_t &value)
{
    fpga_debug_result r={};
    if(!fpga_debug_request(FPGA_EXT_READ32,address,0,&r) || r.status) return false;
    value=r.data;return true;
}
static bool acquire()
{
    // Poll task creates this before any foreground core replacement uses it.
    taskENTER_CRITICAL();
    bool ready=lock!=nullptr;
    taskEXIT_CRITICAL();
    if(!ready) {
        SemaphoreHandle_t candidate=xSemaphoreCreateMutex();
        if(!candidate) return false;
        taskENTER_CRITICAL();
        if(!lock) {lock=candidate;candidate=nullptr;}
        taskEXIT_CRITICAL();
        if(candidate) vSemaphoreDelete(candidate);
    }
    return xSemaphoreTake(lock,portMAX_DELAY)==pdTRUE;
}
static void forget_core()
{
    kind=UNKNOWN;id_attempts=0;probe_tick=0;available=false;full=true;last_cursor=0xffffffff;
    core_abi=0;font_sent=-1;
}
extern "C" void tang_oled_core_replacing(void)
{
    if(!acquire()) return;
    paused=true;forget_core();
    xSemaphoreGive(lock);
}
extern "C" void tang_oled_core_loaded(void)
{
    if(!acquire()) return;
    paused=false;forget_core();
    xSemaphoreGive(lock);
}
// Decide whether the panel can be drawn on; true when it is selected.
static bool probe()
{
    if(kind==UNKNOWN) {
        if(id_attempts>=ID_ATTEMPTS) return false;       // until the next load
        if(id_attempts && ++probe_tick<RECHECK_POLLS) return false;
        probe_tick=0;id_attempts++;
        uint8_t id=0;
        if(!tang_fpga_core_id(&id,ID_TIMEOUT_MS)) return false;
        kind=id==DESKTOP_ID ? DESKTOP:OTHER;
        probe_tick=RECHECK_POLLS;
    }
    if(kind!=DESKTOP) return false;
    if(++probe_tick<RECHECK_POLLS) return available;
    probe_tick=0;
    uint32_t id=0,abi=0,sockets=0;
    bool ok=read(0,id) && id==0x00544453 && read(4,abi) && abi>=0x00010001 && read(0xc0,sockets);
    if(ok) core_abi=abi;
    bool selected=ok && (((sockets>>4)&15)==1 || ((sockets>>8)&15)==1);
    if(selected && !available) full=true;
    return selected;
}
extern "C" void tang_oled_poll(void)
{
    if(!bufs) {
        if(!tang_oled_running()) return;
        bufs=static_cast<link_buffers*>(std::calloc(1,sizeof(link_buffers)));
        if(!bufs) return;
    }
    uint16_t *cells=bufs->cells,*shadow=bufs->shadow;
    uint32_t *words=bufs->words;
    uint32_t cursor;
    if(!tang_oled_snapshot(cells,&cursor) || !acquire()) return;
    if(paused) {xSemaphoreGive(lock);return;}
    available=probe();
    if(available) {
        const bool fonts=core_abi>=FONT_ABI;
        const int font=fonts ? tang_oled_font_requested():TANG_OLED_GRAPHICS;
        // A new grid redraws every cell; this poll's snapshot is the old one,
        // so the next poll sends the new text.
        if(tang_oled_font_effective(font)) full=true;
        if(fonts && (full || font!=font_sent)) {
            fpga_debug_result result={};
            if(fpga_debug_request(FPGA_EXT_WRITE32,0x118,(uint32_t)font,&result) && !result.status) font_sent=font;
            else available=false;
        }
        for(unsigned first=0;available && first<384;first+=64) {
            unsigned count=384-first<64 ? 384-first:64;
            bool dirty=full;
            for(unsigned i=0;i<count;i++) {
                words[i]=cells[first+i];dirty|=cells[first+i]!=shadow[first+i];
            }
            if(!dirty) continue;
            fpga_debug_result result={};
            if(!fpga_debug_write_block(0x200+first*4,words,count,&result) || result.status) {
                available=false;break;
            }
            std::memcpy(shadow+first,cells+first,count*sizeof(cells[0]));
        }
        if(available && (full || cursor!=last_cursor)) {
            fpga_debug_result result={};
            if(fpga_debug_request(FPGA_EXT_WRITE32,0x114,cursor,&result) && !result.status) last_cursor=cursor;
            else available=false;
        }
        if(available) full=false;
        // A refused write: repaint everything once the next check selects it.
        else {full=true;last_cursor=0xffffffff;font_sent=-1;probe_tick=0;}
    }
    xSemaphoreGive(lock);
}
