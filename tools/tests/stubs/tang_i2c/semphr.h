/* Host stub: a recursive mutex that counts nesting. SPDX-License-Identifier: MIT */
#ifndef STUB_I2C_SEMPHR_H
#define STUB_I2C_SEMPHR_H
#include "FreeRTOS.h"
typedef void *SemaphoreHandle_t;
SemaphoreHandle_t xSemaphoreCreateRecursiveMutex(void);
BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t s, TickType_t wait);
BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t s);
#endif
