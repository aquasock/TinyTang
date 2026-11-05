/* Host stub: the reset recorder and the software reset. SPDX-License-Identifier: MIT */
#ifndef STUB_CR_GLB_H
#define STUB_CR_GLB_H
#include <stdint.h>
typedef struct {
    uint32_t reset_recorder_ana_powb;
    uint32_t reset_recorder_ext_rst_n;
    uint32_t reset_recorder_pds_reset;
    uint32_t reset_recorder_wdt_rst_n;
    uint32_t reset_recorder_cpu_porst_n;
    uint32_t reset_recorder_sys_reset_n;
    uint32_t reset_recorder_cpu_sys_rstreq_n;
} GLB_RESET_RECORD_Type;
typedef GLB_RESET_RECORD_Type bl616_glb_reset_record;
int GLB_Get_Reset_Reason(GLB_RESET_RECORD_Type *r);
void GLB_SW_POR_Reset(void);
#endif
