/* Host stub: the task surface tang_heap.c touches. SPDX-License-Identifier: MIT */
#ifndef STUB_HEAP_TASK_H
#define STUB_HEAP_TASK_H
#include "FreeRTOS.h"
void vTaskSuspendAll(void);
BaseType_t xTaskResumeAll(void);
const char *pcTaskGetName(void *task);
void traceMALLOC(void *ptr, size_t size);
#endif
