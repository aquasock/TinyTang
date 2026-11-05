/* Host stub: the FreeRTOS surface tang_crash.c touches. SPDX-License-Identifier: MIT */
#ifndef STUB_CR_FREERTOS_H
#define STUB_CR_FREERTOS_H
#include <stdint.h>
#include <stddef.h>
typedef int BaseType_t;
typedef unsigned int UBaseType_t;
typedef uint32_t TickType_t;
typedef void *TaskHandle_t;
typedef uint32_t StackType_t;
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define pdTRUE 1
#define configMAX_PRIORITIES 32
typedef struct {
    const char *pcTaskName;
    UBaseType_t uxCurrentPriority;
    UBaseType_t usStackHighWaterMark;
} TaskStatus_t;
void host_enter_critical(void);
void host_exit_critical(void);
int host_critical_depth(void);
#define taskENTER_CRITICAL() host_enter_critical()
#define taskEXIT_CRITICAL()  host_exit_critical()
#endif
