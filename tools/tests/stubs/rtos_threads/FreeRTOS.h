/* FreeRTOS stand-ins backed by POSIX threads, for host tests that must run
 * firmware code on more than one task at once: each task is a thread, its
 * thread-local storage pointer is a C11 thread_local, and a critical section
 * is one process-wide recursive mutex.  Only what the tested files use. */
#ifndef STUB_RTOS_THREADS_FREERTOS_H
#define STUB_RTOS_THREADS_FREERTOS_H

#include <stddef.h>
#include <stdint.h>

typedef uint32_t TickType_t;
typedef long BaseType_t;
typedef unsigned long UBaseType_t;
typedef uint32_t StackType_t;
typedef struct stub_task *TaskHandle_t;
typedef struct stub_sem *SemaphoreHandle_t;

#define pdPASS 1
#define pdFAIL 0
#define pdTRUE 1
#define pdFALSE 0
#define portMAX_DELAY 0xffffffffu
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define configTICK_RATE_HZ 1000
#define configMAX_PRIORITIES 8
#define tskIDLE_PRIORITY 0

void stub_critical_enter(void);
void stub_critical_exit(void);
#define taskENTER_CRITICAL() stub_critical_enter()
#define taskEXIT_CRITICAL() stub_critical_exit()
#define taskYIELD() stub_yield()
void stub_yield(void);

#endif
