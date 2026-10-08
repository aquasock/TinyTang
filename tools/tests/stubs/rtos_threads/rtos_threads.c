// SPDX-License-Identifier: MIT
// POSIX-thread implementation of the stand-ins declared in this directory.
#define _GNU_SOURCE
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "bflb_mtimer.h"

#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

struct stub_task {
    pthread_t thread;
    void (*fn)(void *);
    void *arg;
};

struct stub_sem {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int count;
};

static pthread_mutex_t critical = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
static _Thread_local void *tls_pointer;
static _Thread_local struct stub_task *self;
int stub_tasks_created;
int stub_task_create_budget = -1;

void stub_critical_enter(void) { pthread_mutex_lock(&critical); }
void stub_critical_exit(void) { pthread_mutex_unlock(&critical); }
void stub_yield(void) { sched_yield(); }

uint64_t bflb_mtimer_get_time_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u;
}
uint64_t bflb_mtimer_get_time_ms(void) { return bflb_mtimer_get_time_us() / 1000u; }
TickType_t xTaskGetTickCount(void) { return (TickType_t)bflb_mtimer_get_time_ms(); }

void vTaskDelay(TickType_t ticks)
{
    if (ticks == portMAX_DELAY) {
        for (;;) pause();
    }
    struct timespec ts = {(time_t)(ticks / 1000u), (long)(ticks % 1000u) * 1000000L};
    nanosleep(&ts, NULL);
}

static void *trampoline(void *param)
{
    self = param;
    self->fn(self->arg);
    return NULL;
}

BaseType_t xTaskCreate(void (*fn)(void *), const char *name, uint32_t stack_words,
                       void *arg, UBaseType_t priority, TaskHandle_t *handle)
{
    (void)name; (void)stack_words; (void)priority;
    if (stub_task_create_budget == 0) return pdFAIL;
    if (stub_task_create_budget > 0) stub_task_create_budget--;
    struct stub_task *t = calloc(1, sizeof(*t));
    if (!t) return pdFAIL;
    t->fn = fn;
    t->arg = arg;
    if (pthread_create(&t->thread, NULL, trampoline, t) != 0) {
        free(t);
        return pdFAIL;
    }
    pthread_detach(t->thread);
    __atomic_add_fetch(&stub_tasks_created, 1, __ATOMIC_SEQ_CST);
    if (handle) *handle = t;
    return pdPASS;
}

void vTaskDelete(TaskHandle_t task)
{
    if (!task || task == self) pthread_exit(NULL);
    pthread_cancel(task->thread);
}

UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t task) { (void)task; return 1024; }

void *pvTaskGetThreadLocalStoragePointer(TaskHandle_t task, BaseType_t index)
{
    (void)task; (void)index;
    return tls_pointer;
}

void vTaskSetThreadLocalStoragePointer(TaskHandle_t task, BaseType_t index, void *value)
{
    (void)task; (void)index;
    tls_pointer = value;
}

static struct stub_sem *sem_new(int count)
{
    struct stub_sem *s = calloc(1, sizeof(*s));
    if (!s) return NULL;
    pthread_mutex_init(&s->mutex, NULL);
    pthread_cond_init(&s->cond, NULL);
    s->count = count;
    return s;
}
SemaphoreHandle_t xSemaphoreCreateMutex(void) { return sem_new(1); }
SemaphoreHandle_t xSemaphoreCreateBinary(void) { return sem_new(0); }

BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t ticks)
{
    pthread_mutex_lock(&s->mutex);
    if (ticks != portMAX_DELAY) {
        struct timespec until;
        clock_gettime(CLOCK_REALTIME, &until);
        until.tv_sec += (time_t)(ticks / 1000u);
        until.tv_nsec += (long)(ticks % 1000u) * 1000000L;
        if (until.tv_nsec >= 1000000000L) { until.tv_sec++; until.tv_nsec -= 1000000000L; }
        while (s->count == 0) {
            if (pthread_cond_timedwait(&s->cond, &s->mutex, &until) != 0) break;
        }
    } else {
        while (s->count == 0) pthread_cond_wait(&s->cond, &s->mutex);
    }
    BaseType_t got = s->count > 0 ? pdTRUE : pdFALSE;
    if (got) s->count--;
    pthread_mutex_unlock(&s->mutex);
    return got;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t s)
{
    pthread_mutex_lock(&s->mutex);
    s->count = 1;
    pthread_cond_signal(&s->cond);
    pthread_mutex_unlock(&s->mutex);
    return pdTRUE;
}

void vSemaphoreDelete(SemaphoreHandle_t s)
{
    pthread_cond_destroy(&s->cond);
    pthread_mutex_destroy(&s->mutex);
    free(s);
}
