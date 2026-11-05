/* Host stub: vTaskDelay advances the test's own clock. SPDX-License-Identifier: MIT */
#ifndef STUB_FU_TASK_H
#define STUB_FU_TASK_H
#include "FreeRTOS.h"
void vTaskDelay(TickType_t ticks);
void taskYIELD(void);
#endif
