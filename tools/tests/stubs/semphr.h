/* Host stub: the FreeRTOS mutex and task-notification calls the desk layer
 * uses, single-threaded: a mutex is always free, a notification never waits.
 * SPDX-License-Identifier: MIT */
#ifndef STUB_SEMPHR_H
#define STUB_SEMPHR_H
#include "FreeRTOS.h"
typedef void *SemaphoreHandle_t;
static inline SemaphoreHandle_t xSemaphoreCreateMutex(void) { return (SemaphoreHandle_t)1; }
static inline BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t t) { (void)s; (void)t; return pdTRUE; }
static inline BaseType_t xSemaphoreGive(SemaphoreHandle_t s) { (void)s; return pdTRUE; }
#endif
