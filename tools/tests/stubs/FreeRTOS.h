/* Minimal FreeRTOS stand-ins so the layer driver can be exercised on a host.
 *
 * The point is to run the real tang_osd_desk.c, not a copy of it, so these
 * exist only to satisfy its includes.  xTaskCreate deliberately reports failure
 * so no task is started and the test drives the diff itself. */
#ifndef STUB_FREERTOS_H
#define STUB_FREERTOS_H

#include <stdint.h>

typedef uint32_t TickType_t;
typedef long     BaseType_t;

#define pdPASS 1
#define pdFAIL 0
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))

#define taskENTER_CRITICAL() do { } while (0)
#define taskEXIT_CRITICAL()  do { } while (0)

void      vTaskDelay(TickType_t ticks);
BaseType_t xTaskCreate(void (*fn)(void *), const char *name,
                       uint32_t stack_words, void *arg,
                       unsigned priority, void *handle);

#endif /* STUB_FREERTOS_H */
