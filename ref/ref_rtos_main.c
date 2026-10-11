// Diagnostic main: the SDK's device CDC example, but with FreeRTOS running.
// Answers one question -- does enabling FreeRTOS by itself stop the USB
// device from enumerating -- without any of our shell code in the way.

#include "FreeRTOS.h"
#include "task.h"
#include "board.h"

extern void cdc_acm_init(uint8_t busid, uintptr_t reg_base);
extern void cdc_acm_data_send_with_dtr_test(void);

static void tick_task(void *arg)
{
    (void)arg;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

int main(void)
{
    board_init();

    cdc_acm_init(0, 0);

    xTaskCreate(tick_task, "tick", 512, NULL, 1, NULL);

    vTaskStartScheduler();

    while (1) {
    }
}
