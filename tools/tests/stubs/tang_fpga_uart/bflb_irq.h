/* Host stub: interrupt attach/enable, recorded so a test can see the RX ISR
 * was wired. SPDX-License-Identifier: MIT */
#ifndef STUB_FU_IRQ_H
#define STUB_FU_IRQ_H
#include <stdint.h>
typedef void (*host_isr_t)(int irq, void *arg);
void bflb_irq_attach(int irq, host_isr_t isr, void *arg);
void bflb_irq_enable(int irq);
uintptr_t bflb_irq_save(void);
void bflb_irq_restore(uintptr_t state);
host_isr_t host_attached_isr(void);
#endif
