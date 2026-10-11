// TinyTang — heap allocation that fails instead of freezing.  See tang_heap.h.
//
// SPDX-License-Identifier: MIT

#include "tang_heap.h"

#include <stdbool.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "bflb_irq.h"
#include "mm.h"

int tdsh_printf(const char *fmt, ...);

extern void *volatile pxCurrentTCB;

static volatile uint32_t s_failures;
static size_t s_last_size;
static uintptr_t s_last_caller;
static char s_last_task[16];

/* Called with interrupts off, after the SDK has refused an allocation. */
static void note_failure(size_t size, void *caller)
{
    s_failures++;
    s_last_size = size;
    s_last_caller = (uintptr_t)caller;
    const char *name = pxCurrentTCB ? pcTaskGetName(NULL) : "(boot)";
    size_t i = 0;
    for (; name && name[i] && i + 1 < sizeof(s_last_task); i++) {
        s_last_task[i] = name[i];
    }
    s_last_task[i] = '\0';
}

/* The SDK's allocator (components/mm) already returns NULL when it cannot
 * satisfy a request -- it does not spin with interrupts off the way the old
 * SDK's bflb_malloc did (BL6-008) -- so nothing needs replacing.  The linker
 * wraps kmalloc and krealloc only to record the request that was refused; the
 * allocation, its locking and the heap accounting all stay in the SDK. */

void *__real_kmalloc(size_t size, uint32_t flags);
void *__real_krealloc(void *ptr, size_t size);

void *__wrap_kmalloc(size_t size, uint32_t flags)
{
    void *ret = __real_kmalloc(size, flags);
    if (!ret && size != 0) {
        const uintptr_t irq = bflb_irq_save();
        note_failure(size, __builtin_return_address(0));
        bflb_irq_restore(irq);
    }
    return ret;
}

void *__wrap_krealloc(void *ptr, size_t size)
{
    /* A NULL pointer is an allocation, not a resize; handle it here so a
     * refused realloc(NULL, n) is recorded once rather than not at all. */
    if (!ptr) {
        void *ret = __real_kmalloc(size, MM_FLAG_HEAP_ANY);
        if (!ret && size != 0) {
            const uintptr_t irq = bflb_irq_save();
            note_failure(size, __builtin_return_address(0));
            bflb_irq_restore(irq);
        }
        return ret;
    }
    void *ret = __real_krealloc(ptr, size);
    if (!ret && size != 0) {
        const uintptr_t irq = bflb_irq_save();
        note_failure(size, __builtin_return_address(0));
        bflb_irq_restore(irq);
    }
    return ret;
}

/* ------------------------------------------------------------ reporting */

void tang_heap_info(tang_heap_info_t *out)
{
    memset(out, 0, sizeof(*out));

    /* The heap size, summed over the SDK's active heaps.  Held under the mm
     * lock so the list does not change under us. */
    const uintptr_t flag = mm_lock_save();
    for (uint32_t i = 1; i < CONFIG_MM_HEAP_COUNT; i++) {
        const mm_heap_t *heap = g_mem_manager.heaps[i];
        if (heap && heap->is_active) {
            out->total += heap->total_size;
        }
    }
    mm_unlock_restore(flag);

    /* kfree_size() and kmax_free_size() take the mm lock themselves, so they
     * are called outside the section above rather than nested inside it. */
    out->free = kfree_size(0);
    out->largest = kmax_free_size(0);
    out->failures = s_failures;
    out->last_size = s_last_size;
    out->last_caller = s_last_caller;
    memcpy(out->last_task, s_last_task, sizeof(out->last_task));
}

void tang_heap_print(const char *who)
{
    tang_heap_info_t h;
    tang_heap_info(&h);
    tdsh_printf("%s: heap %lu bytes free of %lu, largest free block %lu\r\n", who,
                (unsigned long)h.free, (unsigned long)h.total, (unsigned long)h.largest);
    if (h.failures) {
        tdsh_printf("%s: %lu allocation(s) refused since boot; the last was %lu bytes "
                    "for task %s from 0x%08lx\r\n", who, (unsigned long)h.failures,
                    (unsigned long)h.last_size, h.last_task, (unsigned long)h.last_caller);
    }
}
