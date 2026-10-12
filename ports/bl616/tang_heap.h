// TinyTang — heap allocation that fails instead of freezing.
//
// SDK 2.3.36's allocator returns NULL on allocation failure, so the old SDK's
// TLSF replacements -- which existed only to prevent a silent spin (BL6-008) --
// are gone.  The linker now wraps kmalloc and krealloc to record the refused
// request, task and caller, and krealloc also to move a block to the other
// heap when its own heap cannot grow it (BL6-010); allocation, locking and
// heap accounting stay in the SDK.
//
// SPDX-License-Identifier: MIT

#ifndef TANG_HEAP_H
#define TANG_HEAP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    size_t   total;          /* the heap's size */
    size_t   free;           /* free bytes, as the allocator counts them */
    size_t   largest;        /* the largest single free block */
    uint32_t failures;       /* allocations refused since boot */
    size_t   last_size;      /* the last refused request */
    uintptr_t last_caller;   /* and where it came from */
    char     last_task[16];
} tang_heap_info_t;

/* A snapshot across the SDK's active heaps, with the largest free block. */
void tang_heap_info(tang_heap_info_t *out);

/* Print the snapshot as one or two lines prefixed with `who`. */
void tang_heap_print(const char *who);

#ifdef __cplusplus
}
#endif

#endif /* TANG_HEAP_H */
