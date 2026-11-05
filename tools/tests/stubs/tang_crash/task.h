/* Host stub: SPDX-License-Identifier: MIT */
#ifndef STUB_CR_TASK_H
#define STUB_CR_TASK_H
#include "FreeRTOS.h"
void vTaskDelay(TickType_t ticks);
TickType_t xTaskGetTickCount(void);
TickType_t xTaskGetTickCountFromISR(void);
TaskHandle_t xTaskCreate(void (*fn)(void *), const char *name, uint32_t stack,
                         void *arg, unsigned prio, TaskHandle_t *out);
const char *pcTaskGetName(TaskHandle_t task);
UBaseType_t uxTaskGetNumberOfTasks(void);
UBaseType_t uxTaskGetSystemState(TaskStatus_t *status, UBaseType_t room, uint32_t *total);
#endif
