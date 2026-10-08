#ifndef STUB_RTOS_THREADS_TASK_H
#define STUB_RTOS_THREADS_TASK_H
#include "FreeRTOS.h"

BaseType_t xTaskCreate(void (*fn)(void *), const char *name, uint32_t stack_words,
                       void *arg, UBaseType_t priority, TaskHandle_t *handle);
void vTaskDelete(TaskHandle_t task);
void vTaskDelay(TickType_t ticks);
TickType_t xTaskGetTickCount(void);
UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t task);
void *pvTaskGetThreadLocalStoragePointer(TaskHandle_t task, BaseType_t index);
void vTaskSetThreadLocalStoragePointer(TaskHandle_t task, BaseType_t index, void *value);

/* Test controls: tasks created so far, and the number of xTaskCreate calls
 * still allowed to succeed (negative = unlimited). */
extern int stub_tasks_created;
extern int stub_task_create_budget;
#endif
