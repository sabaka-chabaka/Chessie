#include <stdint.h>

#include <kprintf.h>
#include <multiboot.h>
#include <pmm.h>
#include <string.h>
#include <sync.h>
#include <vmm.h>

#define MAX_PAGES    (1u << 20)
#define BITMAP_WORDS (MAX_PAGES / 32)

extern char kernel_start[];
extern char kernel_end[];

static uint32_t bitmap[BITMAP_WORDS];

static uint32_t total_pages;
static uint32_t free_pages;
static uint32_t end_page;
static uint32_t hint;

static void page_free(uint32_t p)
{
    uint32_t mask = 1u << (p % 32);

    if (bitmap[p / 32] & mask) {
        bitmap[p / 32] &= ~mask;
        free_pages++;
    }
}

static void page_use(uint32_t p)
{
    uint32_t mask = 1u << (p % 32);

    if (!(bitmap[p / 32] & mask)) {
        bitmap[p / 32] |= mask;
        free_pages--;
    }
}

static void reserve_range(uint32_t start, uint32_t end)
{
    if (end <= start)
        return;

    uint32_t first = start >> PAGE_SHIFT;
    uint32_t last  = ((end - 1) >> PAGE_SHIFT) + 1;

    for (uint32_t p = first; p < last; p++)
        page_use(p);
}

void pmm_init(const multiboot_info_t *mbi)
{
    memset(bitmap, 0xFF, sizeof(bitmap));
    total_pages = 0;
    free_pages  = 0;
    end_page    = 0;
    hint        = 0;

    if (!(mbi->flags & MULTIBOOT_INFO_MMAP)) {
        kprintf("pmm: no memory map, allocator disabled\n");
        return;
    }

    uint32_t cur = mbi->mmap_addr;
    uint32_t end = mbi->mmap_addr + mbi->mmap_length;

    while (cur < end) {
        const multiboot_mmap_entry_t *e = (const multiboot_mmap_entry_t *)cur;

        if (e->type == MULTIBOOT_MEMORY_AVAILABLE && e->addr < 0x100000000ull) {
            uint64_t area_end = e->addr + e->len;
            if (area_end > 0x100000000ull)
                area_end = 0x100000000ull;

            uint32_t first = (uint32_t)((e->addr + PAGE_SIZE - 1) >> PAGE_SHIFT);
            uint32_t last  = (uint32_t)(area_end >> PAGE_SHIFT);

            if (last > (VMM_IDENTITY_LIMIT >> PAGE_SHIFT))
                last = VMM_IDENTITY_LIMIT >> PAGE_SHIFT;

            for (uint32_t p = first; p < last; p++)
                page_free(p);

            if (last > end_page)
                end_page = last;
        }

        cur += e->size + sizeof(e->size);
    }

    total_pages = free_pages;

    reserve_range(0, 0x100000);
    reserve_range((uint32_t)kernel_start, (uint32_t)kernel_end);
    reserve_range((uint32_t)mbi, (uint32_t)mbi + sizeof(*mbi));
    reserve_range(mbi->mmap_addr, mbi->mmap_addr + mbi->mmap_length);
}

static uint32_t alloc_nolock(void)
{
    uint32_t words = (end_page + 31) / 32;

    if (!words)
        return 0;

    for (uint32_t n = 0; n < words; n++) {
        uint32_t w = (hint + n) % words;

        if (bitmap[w] == 0xFFFFFFFFu)
            continue;

        uint32_t bit = (uint32_t)__builtin_ctz(~bitmap[w]);   /* первый нулевой бит */

        bitmap[w] |= 1u << bit;
        free_pages--;
        hint = w;
        return (w * 32 + bit) << PAGE_SHIFT;
    }

    return 0;
}

static void free_nolock(uint32_t addr)
{
    if (addr & (PAGE_SIZE - 1)) {
        kprintf("pmm: free of unaligned address %p\n", (void *)addr);
        return;
    }

    uint32_t p = addr >> PAGE_SHIFT;

    if (p >= end_page) {
        kprintf("pmm: free of address outside RAM %p\n", (void *)addr);
        return;
    }

    if (!(bitmap[p / 32] & (1u << (p % 32)))) {
        kprintf("pmm: double free of %p\n", (void *)addr);
        return;
    }

    page_free(p);

    if (p / 32 < hint)
        hint = p / 32;
}

uint32_t pmm_alloc_page(void)
{
    uint32_t f = irq_save();
    uint32_t p = alloc_nolock();

    irq_restore(f);
    return p;
}

void pmm_free_page(uint32_t addr)
{
    uint32_t f = irq_save();

    free_nolock(addr);
    irq_restore(f);
}

uint32_t pmm_total_pages(void) { return total_pages; }
uint32_t pmm_free_pages(void)  { return free_pages; }

uint32_t pmm_max_addr(void)
{
    if (end_page >= (1u << 20))
        return 0xFFFFF000u;
    return end_page << PAGE_SHIFT;
}