/* Host stub: the FreeRTOS surface tang_fpga_uart.c touches. SPDX-License-Identifier: MIT */
#ifndef STUB_FU_FREERTOS_H
#define STUB_FU_FREERTOS_H
#include <stdint.h>
#include <stddef.h>
typedef int BaseType_t;
typedef unsigned int UBaseType_t;
typedef uint32_t TickType_t;
typedef void *TaskHandle_t;
#define pdTRUE  1
#define pdFALSE 0
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define portMAX_DELAY ((TickType_t)0xffffffffu)
#define portTICK_PERIOD_MS 1u
TickType_t xTaskGetTickCount(void);
TaskHandle_t xTaskGetCurrentTaskHandle(void);
/* The real ones disable interrupts; here they only count nesting, so a test
 * can see that a section was left balanced. */
void host_enter_critical(void);
void host_exit_critical(void);
int host_critical_depth(void);
#define taskENTER_CRITICAL() host_enter_critical()
#define taskEXIT_CRITICAL()  host_exit_critical()
#endif
