// TinyTang — heap allocation that fails instead of freezing.
//
// The SDK's allocator (bflb_malloc and its siblings over TLSF) never returns
// NULL: a failed allocation prints to a UART this board does not expose, then
// spins with interrupts off, and only the watchdog ends it (BL6-008).  With the
// Bluetooth stack and two devices connected the heap is small and fragmented
// enough for that to happen -- `tdsh run` asking for a large task stack froze
// the board.  The linker renames those functions (--wrap) to the versions in
// tang_heap.c, which are the same TLSF calls under the same lock but return
// NULL on failure and note it, so the caller can fail cleanly.
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

/* A snapshot, including a walk of the heap for its largest free block. */
void tang_heap_info(tang_heap_info_t *out);

/* Print the snapshot as one or two lines prefixed with `who`. */
void tang_heap_print(const char *who);

#ifdef __cplusplus
}
#endif

#endif /* TANG_HEAP_H */
