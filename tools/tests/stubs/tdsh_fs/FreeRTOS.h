/* Host stub: the FreeRTOS surface tdsh_fs_bl616.c touches. SPDX-License-Identifier: MIT */
#ifndef STUB_FS_FREERTOS_H
#define STUB_FS_FREERTOS_H
#include <stdint.h>
#include <stddef.h>
typedef int BaseType_t;
typedef uint32_t TickType_t;
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
void host_enter_critical(void);
void host_exit_critical(void);
int host_critical_depth(void);
#define taskENTER_CRITICAL() host_enter_critical()
#define taskEXIT_CRITICAL()  host_exit_critical()
#endif
