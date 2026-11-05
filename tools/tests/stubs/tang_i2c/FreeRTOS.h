/* Host stub: the FreeRTOS surface tang_i2c.cpp touches. SPDX-License-Identifier: MIT */
#ifndef STUB_I2C_FREERTOS_H
#define STUB_I2C_FREERTOS_H
#include <stdint.h>
#include <stddef.h>
typedef int BaseType_t;
typedef unsigned int UBaseType_t;
typedef uint32_t TickType_t;
#define pdTRUE  1
#define pdFALSE 0
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define portMAX_DELAY ((TickType_t)0xffffffffu)
#define portTICK_PERIOD_MS 1u
TickType_t xTaskGetTickCount(void);
#endif
