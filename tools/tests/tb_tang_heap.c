/* TinyTang — host test for the heap wrappers (ports/bl616/tang_heap.c).
 *
 * Every app on this board depends on an allocation that refuses rather than
 * freezes, and two defects already in the log came from exactly this code: a
 * refused script worker (BL6-010) and a leaked 4 KB JSON buffer (BL6-009).
 * What this asserts is the behaviour those fixed:
 *
 *  - a refused kmalloc is recorded, with its size and the task;
 *  - a successful one is not recorded, and a zero-byte one is not either;
 *  - pvPortMalloc asks for either heap, so a task stack is not confined to
 *    OCRAM, and a refusal runs the FreeRTOS hook;
 *  - krealloc(NULL, n) is an allocation, and goes to either heap;
 *  - a krealloc its own heap refuses is moved rather than failing, with the
 *    data preserved up to the smaller of the two sizes;
 *  - a realloc that cannot be satisfied anywhere is recorded once;
 *  - tang_heap_info sums only active heaps and skips heap 0.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "bflb_irq.h"
#include "mm.h"

/* ------------------------------------------------------------ the stubs -- */

mm_manager_t g_mem_manager;

/* The whole allocator is a table, so a test can decide which call refuses. */
static int fail_kmalloc;
static int fail_krealloc;
static size_t last_kmalloc_size;
static uint32_t last_kmalloc_flags;
static uintptr_t irq_depth;
static uint32_t irq_saves;
static int hook_calls;
static int suspend_calls, resume_calls;
static int trace_calls;
static const char *current_task = "tdsh_script";

/* tang_heap.c asks whether a task is running at all before naming one. */
void *volatile pxCurrentTCB = (void *)1;

uintptr_t bflb_irq_save(void) { irq_saves++; irq_depth++; return 0x5a5au; }
void bflb_irq_restore(uintptr_t state) { (void)state; irq_depth--; }

void vTaskSuspendAll(void) { suspend_calls++; }
BaseType_t xTaskResumeAll(void) { resume_calls++; return pdTRUE; }
const char *pcTaskGetName(void *task) { (void)task; return current_task; }
void traceMALLOC(void *ptr, size_t size) { (void)ptr; (void)size; trace_calls++; }

/* tang_heap.c declares this itself; defining it here makes a refusal visible. */
void vApplicationMallocFailedHook(void) { hook_calls++; }

uintptr_t mm_lock_save(void) { return 0x1u; }
void mm_unlock_restore(uintptr_t flag) { (void)flag; }

/* A heap of counted allocations: each block carries its own size so
 * kmalloc_size() can answer, which is what the move path reads. */
typedef struct {
    size_t size;
    unsigned char tag;
} block_t;

void *kmalloc(size_t size, uint32_t flags)
{
    last_kmalloc_size = size;
    last_kmalloc_flags = flags;
    if (fail_kmalloc) return NULL;
    block_t *b = malloc(sizeof(block_t) + size);
    if (!b) return NULL;
    b->size = size;
    b->tag = 0xab;
    return b + 1;
}

void *krealloc(void *ptr, size_t size)
{
    if (fail_krealloc) return NULL;
    block_t *b = (block_t *)ptr - 1;
    block_t *n = realloc(b, sizeof(block_t) + size);
    if (!n) return NULL;
    n->size = size;
    return n + 1;
}

void kfree(void *ptr)
{
    if (!ptr) return;
    free((block_t *)ptr - 1);
}

size_t kmalloc_size(void *ptr)
{
    if (!ptr) return 0;
    return ((block_t *)ptr - 1)->size;
}

size_t kfree_size(int cap) { (void)cap; return 40000; }
size_t kmax_free_size(int cap) { (void)cap; return 12000; }

/* The shell's output, collected rather than printed. */
static char out_text[4096];
static size_t out_len;
int tdsh_printf(const char *fmt, ...)
{
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsnprintf(out_text + out_len, sizeof out_text - out_len, fmt, ap);
    va_end(ap);
    if (n > 0) out_len += (size_t)n;
    return n;
}

/* The names tang_heap.c calls as the SDK's own, wired to the table above. */
void *__real_kmalloc(size_t size, uint32_t flags) { return kmalloc(size, flags); }
void *__real_krealloc(void *ptr, size_t size) { return krealloc(ptr, size); }

/* The module under test. */
#include "../../ports/bl616/tang_heap.c"

/* --------------------------------------------------------------- checks -- */

static int failures;

#define CHECK(x)                                                              \
    do {                                                                      \
        if (!(x)) {                                                           \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x);                \
            failures++;                                                       \
        }                                                                     \
    } while (0)

#define CHECK_EQ(a, b)                                                        \
    do {                                                                      \
        long _a = (long)(a), _b = (long)(b);                                   \
        if (_a != _b) {                                                       \
            printf("FAIL %s:%d: %s == %ld, expected %ld\n", __FILE__, __LINE__, \
                   #a, _a, _b);                                               \
            failures++;                                                       \
        }                                                                     \
    } while (0)

static void reset_world(void)
{
    /* The module's own counters are statics in the included file, so they are
     * reset here rather than assumed to start clean. */
    s_failures = 0;
    s_last_size = 0;
    s_last_caller = 0;
    s_last_task[0] = '\0';
    fail_kmalloc = fail_krealloc = 0;
    last_kmalloc_size = 0;
    last_kmalloc_flags = 0;
    irq_depth = 0;
    irq_saves = 0;
    hook_calls = 0;
    suspend_calls = resume_calls = 0;
    trace_calls = 0;
    current_task = "tdsh_script";
    pxCurrentTCB = (void *)1;
    out_len = 0;
    out_text[0] = '\0';
}

int main(void)
{
    /* A refused kmalloc is recorded with its size and the asking task, and
     * interrupts are back on afterwards -- the note happens with them off. */
    reset_world();
    fail_kmalloc = 1;
    CHECK(__wrap_kmalloc(1500, 0) == NULL);
    CHECK_EQ(s_failures, 1);
    CHECK_EQ(s_last_size, 1500);
    CHECK_EQ(strcmp(s_last_task, "tdsh_script"), 0);
    CHECK(irq_saves >= 1);
    CHECK_EQ(irq_depth, 0);          /* restored, not left masked */
    CHECK_EQ(hook_calls, 0);         /* kmalloc does not run the FreeRTOS hook */

    /* A successful allocation records nothing. */
    reset_world();
    {
        void *p = __wrap_kmalloc(64, 0);
        CHECK(p != NULL);
        CHECK_EQ(s_failures, 0);
        CHECK_EQ(irq_saves, 0);
        kfree(p);
    }

    /* A zero-byte refusal is not a failure: asking for nothing is not a
     * shortage, and counting it would make a real one harder to see. */
    reset_world();
    fail_kmalloc = 1;
    CHECK(__wrap_kmalloc(0, 0) == NULL);
    CHECK_EQ(s_failures, 0);

    /* pvPortMalloc asks for either heap, so a task stack is not confined to
     * OCRAM however much WRAM is free (BL6-010). */
    reset_world();
    {
        void *stack = __wrap_pvPortMalloc(2048);
        CHECK(stack != NULL);
        CHECK_EQ(s_failures, 0);
        CHECK((last_kmalloc_flags & 0x80000000u) != 0);   /* MM_FLAG_HEAP_ANY */
        CHECK_EQ(suspend_calls, 1);
        CHECK_EQ(resume_calls, 1);
        CHECK_EQ(trace_calls, 1);
        kfree(stack);
    }

    /* A refused task stack runs the hook as well as recording, because
     * FreeRTOS's own contract is that the hook is how the caller learns. */
    reset_world();
    fail_kmalloc = 1;
    CHECK(__wrap_pvPortMalloc(4096) == NULL);
    CHECK_EQ(hook_calls, 1);
    CHECK_EQ(s_failures, 1);
    CHECK_EQ(s_last_size, 4096);
    CHECK_EQ(resume_calls, 1);       /* the scheduler is resumed on the way out */

    /* krealloc(NULL, n) is an allocation, and takes either heap. */
    reset_world();
    {
        void *p = __wrap_krealloc(NULL, 100);
        CHECK(p != NULL);
        CHECK((last_kmalloc_flags & 0x80000000u) != 0);
        CHECK_EQ(s_failures, 0);
        kfree(p);
    }

    /* The move path: the block's own heap cannot grow it, so it is moved to
     * whichever heap has room and the bytes are carried across, up to the
     * smaller of the two sizes. */
    reset_world();
    {
        unsigned char *p = __wrap_kmalloc(32, 0);
        CHECK(p != NULL);
        for (int i = 0; i < 32; i++) p[i] = (unsigned char)(i + 1);
        fail_krealloc = 1;                    /* its own heap refuses */
        unsigned char *q = __wrap_krealloc(p, 16);   /* shrink: all 16 kept */
        CHECK(q != NULL);
        CHECK_EQ(s_failures, 0);
        CHECK(q != p);
        for (int i = 0; i < 16; i++) CHECK_EQ(q[i], i + 1);
        CHECK_EQ(kmalloc_size(q), 16);
        kfree(q);

        /* Growing: the copy is bounded by the old size, so a heap that hands
         * back junk past the old length cannot corrupt the result. */
        reset_world();
        unsigned char *r = __wrap_kmalloc(8, 0);
        for (int i = 0; i < 8; i++) r[i] = (unsigned char)(0xa0 + i);
        fail_krealloc = 1;
        unsigned char *s = __wrap_krealloc(r, 64);
        CHECK(s != NULL);
        CHECK_EQ(kmalloc_size(s), 64);
        for (int i = 0; i < 8; i++) CHECK_EQ(s[i], 0xa0 + i);
        kfree(s);
    }

    /* Before the scheduler starts there is no task to name, and the record
     * says so rather than naming whatever the pointer happened to hold. */
    reset_world();
    fail_kmalloc = 1;
    pxCurrentTCB = NULL;
    CHECK(__wrap_kmalloc(99, 0) == NULL);
    CHECK_EQ(strcmp(s_last_task, "(boot)"), 0);
    CHECK_EQ(s_failures, 1);

    /* A realloc that cannot be satisfied anywhere is recorded once, not twice
     * for the attempt and the fallback. */
    reset_world();
    {
        unsigned char *p = __wrap_kmalloc(16, 0);
        fail_krealloc = 1;
        fail_kmalloc = 1;
        CHECK(__wrap_krealloc(p, 128) == NULL);
        CHECK_EQ(s_failures, 1);
        CHECK_EQ(s_last_size, 128);
        kfree(p);
    }

    /* ---------------------------------------------------------- reporting --
     * Only active heaps are summed, and heap 0 is skipped: the SDK keeps a
     * sentinel there that is not a heap. */
    reset_world();
    {
        static mm_heap_t h0 = {0xdeadbeefu, 1};
        static mm_heap_t h1 = {327680, 1};
        static mm_heap_t h2 = {32768, 1};
        static mm_heap_t h3 = {999, 0};      /* inactive: not counted */
        g_mem_manager.heaps[0] = &h0;
        g_mem_manager.heaps[1] = &h1;
        g_mem_manager.heaps[2] = &h2;
        (void)h3;

        tang_heap_info_t info;
        tang_heap_info(&info);
        CHECK_EQ(info.total, 327680 + 32768);
        CHECK_EQ(info.free, 40000);
        CHECK_EQ(info.largest, 12000);
        CHECK_EQ(info.failures, 0);

        /* A recorded refusal shows up in the snapshot. */
        fail_kmalloc = 1;
        current_task = "phosphor";
        CHECK(__wrap_kmalloc(700, 0) == NULL);
        tang_heap_info(&info);
        CHECK_EQ(info.failures, 1);
        CHECK_EQ(info.last_size, 700);
        CHECK_EQ(strcmp(info.last_task, "phosphor"), 0);

        /* An inactive heap is skipped even when it has a size. */
        h1.is_active = 0;
        tang_heap_info(&info);
        CHECK_EQ(info.total, 32768);
        h1.is_active = 1;

        /* A missing heap slot is skipped rather than dereferenced. */
        g_mem_manager.heaps[2] = NULL;
        tang_heap_info(&info);
        CHECK_EQ(info.total, 327680);
        g_mem_manager.heaps[2] = &h2;
    }

    /* The printed line names the free bytes, the total and the largest block,
     * and a refusal adds a second line naming the size and the task. */
    reset_world();
    {
        tang_heap_print("test");
        CHECK(strstr(out_text, "test: heap 40000 bytes free of") != NULL);
        CHECK(strstr(out_text, "largest free block 12000") != NULL);
        CHECK(strstr(out_text, "refused") == NULL);

        out_len = 0;
        out_text[0] = '\0';
        fail_kmalloc = 1;
        current_task = "tdsh_script";
        CHECK(__wrap_kmalloc(1234, 0) == NULL);
        tang_heap_print("test");
        CHECK(strstr(out_text, "1 allocation(s) refused") != NULL);
        CHECK(strstr(out_text, "1234 bytes") != NULL);
        CHECK(strstr(out_text, "tdsh_script") != NULL);
    }

    if (failures) {
        printf("tb_tang_heap: %d failure(s)\n", failures);
        return 1;
    }
    printf("tb_tang_heap: all checks passed\n");
    return 0;
}
