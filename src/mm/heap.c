#include <stddef.h>
#include <stdint.h>

#include <heap.h>
#include <kprintf.h>
#include <pmm.h>
#include <string.h>
#include <sync.h>
#include <vmm.h>

#define MAGIC_USED 0xA110C8EDu
#define MAGIC_FREE 0xF4EEB10Cu

struct block {
    uint32_t      magic;
    uint32_t      size;
    struct block *next;
    struct block *prev;
};

#define HDR       ((uint32_t)sizeof(struct block))
#define MIN_SPLIT 16u

_Static_assert(sizeof(struct block) == 16, "header must be 16 bytes to keep payload aligned");

static struct block *head;
static struct block *tail;
static uint32_t      heap_end = KHEAP_START;

static void heap_panic(const char *msg, void *at)
{
    kprintf("\nHEAP CORRUPTION: %s at %p\n", msg, at);
    for (;;)
        __asm__ volatile ("cli; hlt");
}

static int heap_grow(uint32_t bytes)
{
    uint32_t pages = (bytes + PAGE_SIZE - 1) / PAGE_SIZE;
    uint32_t got = 0;

    while (got < pages && heap_end + PAGE_SIZE <= KHEAP_MAX) {
        uint32_t phys = pmm_alloc_page();

        if (!phys)
            break;
        if (vmm_map_page(heap_end, phys, VMM_WRITE) != 0) {
            pmm_free_page(phys);
            break;
        }
        heap_end += PAGE_SIZE;
        got++;
    }

    if (!got)
        return -1;

    uint32_t added = got * PAGE_SIZE;

    if (tail && tail->magic == MAGIC_FREE) {
        tail->size += added;
    } else {
        struct block *b = (struct block *)(heap_end - added);

        b->magic = MAGIC_FREE;
        b->size  = added - HDR;
        b->next  = 0;
        b->prev  = tail;
        if (tail)
            tail->next = b;
        else
            head = b;
        tail = b;
    }

    return got < pages ? -1 : 0;
}

void heap_init(void)
{
    head = 0;
    tail = 0;
    heap_end = KHEAP_START;

    if (heap_grow(PAGE_SIZE) != 0)
        kprintf("heap: cannot map initial page\n");
}

static struct block *find_fit(uint32_t size)
{
    for (struct block *b = head; b; b = b->next) {
        if (b->magic == MAGIC_FREE) {
            if (b->size >= size)
                return b;
        } else if (b->magic != MAGIC_USED) {
            heap_panic("bad block magic", b);
        }
    }
    return 0;
}

static void *kmalloc_nolock(size_t size)
{
    if (size == 0 || size > KHEAP_MAX - KHEAP_START)
        return 0;

    uint32_t need = ((uint32_t)size + 15u) & ~15u;
    struct block *b = find_fit(need);

    if (!b) {
        if (heap_grow(need + HDR) != 0)
            return 0;
        b = find_fit(need);
        if (!b)
            return 0;
    }

    if (b->size >= need + HDR + MIN_SPLIT) {
        struct block *n = (struct block *)((uint8_t *)b + HDR + need);

        n->magic = MAGIC_FREE;
        n->size  = b->size - need - HDR;
        n->next  = b->next;
        n->prev  = b;
        if (b->next)
            b->next->prev = n;
        else
            tail = n;
        b->next = n;
        b->size = need;
    }

    b->magic = MAGIC_USED;
    return (uint8_t *)b + HDR;
}

static void kfree_nolock(void *ptr)
{
    if (!ptr)
        return;

    uint32_t a = (uint32_t)ptr;

    if (a < KHEAP_START + HDR || a >= heap_end || (a & 15)) {
        kprintf("kfree: bad pointer %p\n", ptr);
        return;
    }

    struct block *b = (struct block *)(a - HDR);

    if (b->magic == MAGIC_FREE) {
        kprintf("kfree: double free of %p\n", ptr);
        return;
    }
    if (b->magic != MAGIC_USED) {
        kprintf("kfree: %p is not a heap block\n", ptr);
        return;
    }

    b->magic = MAGIC_FREE;

    if (b->next && b->next->magic == MAGIC_FREE) {
        struct block *n = b->next;

        b->size += HDR + n->size;
        b->next = n->next;
        if (n->next)
            n->next->prev = b;
        else
            tail = b;
        n->magic = 0;
    }

    if (b->prev && b->prev->magic == MAGIC_FREE) {
        struct block *p = b->prev;

        p->size += HDR + b->size;
        p->next = b->next;
        if (b->next)
            b->next->prev = p;
        else
            tail = p;
        b->magic = 0;
    }
}

static void stats_nolock(heap_stats_t *s)
{
    memset(s, 0, sizeof(*s));
    s->mapped = heap_end - KHEAP_START;

    for (struct block *b = head; b; b = b->next) {
        s->blocks++;

        if (b->magic == MAGIC_USED) {
            s->used += b->size;
        } else if (b->magic == MAGIC_FREE) {
            s->free += b->size;
        } else {
            s->errors++;
            break;
        }

        if (b->next) {
            if ((uint8_t *)b + HDR + b->size != (uint8_t *)b->next)
                s->errors++;
            if (b->next->prev != b)
                s->errors++;
        }
    }
}

void *kmalloc(size_t size)
{
    uint32_t f = irq_save();
    void *p = kmalloc_nolock(size);

    irq_restore(f);
    return p;
}

void *kcalloc(size_t n, size_t size)
{
    if (n && size > 0xFFFFFFFFu / n)
        return 0;

    void *p = kmalloc(n * size);

    if (p)
        memset(p, 0, n * size);
    return p;
}

void kfree(void *ptr)
{
    uint32_t f = irq_save();

    kfree_nolock(ptr);
    irq_restore(f);
}

void heap_stats(heap_stats_t *s)
{
    uint32_t f = irq_save();

    stats_nolock(s);
    irq_restore(f);
}