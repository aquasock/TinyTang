/* Host stub: an interrupt mask the test can observe. SPDX-License-Identifier: MIT */
#ifndef STUB_BFLB_IRQ_H
#define STUB_BFLB_IRQ_H
#include <stdint.h>
uintptr_t bflb_irq_save(void);
void bflb_irq_restore(uintptr_t state);
#endif
