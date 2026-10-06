// TinyTang — heap allocation that fails instead of freezing.  See tang_heap.h.
//
// SPDX-License-Identifier: MIT

#include "tang_heap.h"

#include <stdbool.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "bflb_irq.h"
#include "mem.h"
#include "tlsf.h"

int tdsh_printf(const char *fmt, ...);

extern void *volatile pxCurrentTCB;

static volatile uint32_t s_failures;
static size_t s_last_size;
static uintptr_t s_last_caller;
static char s_last_task[16];

/* Called with interrupts off, from inside the allocator's lock. */
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

static void count_out(struct mem_heap_s *heap, void *ptr)
{
    heap->free_bytes -= tlsf_block_size(ptr);
    heap->free_bytes -= tlsf_alloc_overhead();
}

/* The SDK's functions, renamed by the linker (CMakeLists.txt).  They differ
 * from the originals only in what happens on failure; bflb_realloc also counts
 * the new block's size rather than the old pointer's, which the original reads
 * after the block may have moved. */

void *__wrap_bflb_malloc(struct mem_heap_s *heap, size_t nbytes)
{
    const uintptr_t flag = bflb_irq_save();
    void *ret = tlsf_memalign(heap->priv, 32, nbytes);
    if (ret) {
        count_out(heap, ret);
    } else {
        note_failure(nbytes, __builtin_return_address(0));
    }
    bflb_irq_restore(flag);
    return ret;
}

void *__wrap_bflb_malloc_align(struct mem_heap_s *heap, size_t align, size_t size)
{
    const uintptr_t flag = bflb_irq_save();
    void *ret = tlsf_memalign(heap->priv, align, size);
    if (ret) {
        count_out(heap, ret);
    } else {
        note_failure(size, __builtin_return_address(0));
    }
    bflb_irq_restore(flag);
    return ret;
}

void *__wrap_bflb_calloc(struct mem_heap_s *heap, size_t count, size_t size)
{
    if (count == 0 || size == 0 || count > SIZE_MAX / size) {
        return NULL;
    }
    const size_t total = count * size;
    const uintptr_t flag = bflb_irq_save();
    void *ptr = tlsf_memalign(heap->priv, 32, total);
    if (ptr) {
        count_out(heap, ptr);
        memset(ptr, 0, total);
    } else {
        note_failure(total, __builtin_return_address(0));
    }
    bflb_irq_restore(flag);
    return ptr;
}

void *__wrap_bflb_realloc(struct mem_heap_s *heap, void *ptr, size_t nbytes)
{
    const uintptr_t flag = bflb_irq_save();
    const size_t before = ptr ? tlsf_block_size(ptr) : 0;
    void *ret = tlsf_realloc(heap->priv, ptr, nbytes);
    if (ret) {
        heap->free_bytes += before;
        heap->free_bytes -= tlsf_block_size(ret);
        if (!ptr) {
            heap->free_bytes -= tlsf_alloc_overhead();
        }
    } else if (nbytes != 0) {
        /* The old block is untouched, as realloc promises. */
        note_failure(nbytes, __builtin_return_address(0));
    } else if (ptr) {
        heap->free_bytes += before + tlsf_alloc_overhead();   /* realloc(p, 0) freed it */
    }
    bflb_irq_restore(flag);
    return ret;
}

/* ------------------------------------------------------------ reporting */

static void largest_walker(void *ptr, size_t size, int used, void *user)
{
    (void)ptr;
    size_t *largest = user;
    if (!used && size > *largest) {
        *largest = size;
    }
}

void tang_heap_info(tang_heap_info_t *out)
{
    memset(out, 0, sizeof(*out));
    struct mem_heap_s *heap = KMEM_HEAP;
    /* A few hundred blocks at most; walked under the allocator's own lock so
     * the picture is consistent. */
    const uintptr_t flag = bflb_irq_save();
    out->total = heap->heapsize;
    out->free = heap->free_bytes;
    tlsf_walk_pool(tlsf_get_pool(heap->priv), largest_walker, &out->largest);
    out->failures = s_failures;
    out->last_size = s_last_size;
    out->last_caller = s_last_caller;
    memcpy(out->last_task, s_last_task, sizeof(out->last_task));
    bflb_irq_restore(flag);
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
