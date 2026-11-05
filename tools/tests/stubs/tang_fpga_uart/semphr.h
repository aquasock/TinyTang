/* Host stub: a counting mutex the test can hold to make the link busy. SPDX-License-Identifier: MIT */
#ifndef STUB_FU_SEMPHR_H
#define STUB_FU_SEMPHR_H
#include "FreeRTOS.h"
typedef void *SemaphoreHandle_t;
SemaphoreHandle_t xSemaphoreCreateMutex(void);
BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t wait);
BaseType_t xSemaphoreGive(SemaphoreHandle_t s);
#endif
