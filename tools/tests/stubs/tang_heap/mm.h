/* Host stub: the SDK allocator surface tang_heap.c touches. SPDX-License-Identifier: MIT */
#ifndef STUB_MM_H
#define STUB_MM_H
#include <stddef.h>
#include <stdint.h>

#define MM_FLAG_HEAP_ANY 0x80000000u
#define CONFIG_MM_HEAP_COUNT 3

typedef struct {
    size_t total_size;
    int is_active;
} mm_heap_t;

typedef struct {
    mm_heap_t *heaps[CONFIG_MM_HEAP_COUNT];
} mm_manager_t;

extern mm_manager_t g_mem_manager;

uintptr_t mm_lock_save(void);
void mm_unlock_restore(uintptr_t flag);

void *kmalloc(size_t size, uint32_t flags);
void *krealloc(void *ptr, size_t size);
void kfree(void *ptr);
size_t kmalloc_size(void *ptr);
size_t kfree_size(int cap);
size_t kmax_free_size(int cap);
#endif
