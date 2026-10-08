// SPDX-License-Identifier: MIT
#include "../tang_oled.h"
#include "fpga_debug.h"
extern "C" {
#include "FreeRTOS.h"
#include "semphr.h"
}
#include <cstring>

// Called only by osddesk. Static buffers keep its 4 KB stack bounded.
static SemaphoreHandle_t lock;
static bool paused,probed,available,full=true;
static unsigned probe_tick;
static uint16_t cells[384],shadow[384];
static uint32_t words[64],last_cursor=0xffffffff;
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
extern "C" void tang_oled_core_replacing(void)
{
    if(!acquire()) return;
    paused=true;probed=false;available=false;full=true;last_cursor=0xffffffff;
    xSemaphoreGive(lock);
}
extern "C" void tang_oled_core_loaded(void)
{
    if(!acquire()) return;
    paused=false;probed=false;available=false;probe_tick=0;full=true;last_cursor=0xffffffff;
    xSemaphoreGive(lock);
}
extern "C" void tang_oled_poll(void)
{
    uint32_t cursor;
    if(!tang_oled_snapshot(cells,&cursor) || !acquire()) return;
    if(paused) {xSemaphoreGive(lock);return;}
    if(!probed || ++probe_tick>=25) {
        uint32_t id=0,abi=0,sockets=0;
        bool ok=read(0,id) && id==0x00544453 && read(4,abi) && abi>=0x00010001 && read(0xc0,sockets);
        bool selected=ok && (((sockets>>4)&15)==1 || ((sockets>>8)&15)==1);
        if(selected && !available) full=true;
        available=selected;probed=true;probe_tick=0;
    }
    if(available) {
        for(unsigned first=0;first<384;first+=64) {
            unsigned count=384-first<64 ? 384-first:64;
            bool dirty=full;
            for(unsigned i=0;i<count;i++) {
                words[i]=cells[first+i];dirty|=cells[first+i]!=shadow[first+i];
            }
            if(!dirty) continue;
            fpga_debug_result result={};
            if(!fpga_debug_write_block(0x200+first*4,words,count,&result) || result.status) {
                available=false;probed=false;full=true;break;
            }
            std::memcpy(shadow+first,cells+first,count*sizeof(cells[0]));
        }
        if(available && (full || cursor!=last_cursor)) {
            fpga_debug_result result={};
            if(fpga_debug_request(FPGA_EXT_WRITE32,0x114,cursor,&result) && !result.status) last_cursor=cursor;
            else {available=false;probed=false;full=true;}
        }
        if(available) full=false;
    }
    xSemaphoreGive(lock);
}
