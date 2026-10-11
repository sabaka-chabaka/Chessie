#include <stdint.h>

#include <kprintf.h>
#include <pmm.h>
#include <string.h>
#include <vmm.h>

#define ENTRIES 1024

static uint32_t *pd;

static inline void invlpg(uint32_t virt)
{
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}

static int map(uint32_t virt, uint32_t phys, uint32_t flags)
{
    uint32_t di = virt >> 22;
    uint32_t ti = (virt >> 12) & 0x3FF;

    if (!(pd[di] & VMM_PRESENT)) {
        uint32_t t = pmm_alloc_page();

        if (!t)
            return -1;
        memset((void *)t, 0, PAGE_SIZE);
        pd[di] = t | VMM_PRESENT | VMM_WRITE;
    }

    uint32_t *pt = (uint32_t *)(pd[di] & 0xFFFFF000);
    pt[ti] = (phys & 0xFFFFF000) | (flags & 0xFFF) | VMM_PRESENT;
    return 0;
}

void vmm_init(void)
{
    uint32_t limit = pmm_max_addr();

    if (limit > VMM_IDENTITY_LIMIT)
        limit = VMM_IDENTITY_LIMIT;

    pd = (uint32_t *)pmm_alloc_page();
    if (!pd) {
        kprintf("vmm: no memory for page directory\n");
        return;
    }
    memset(pd, 0, PAGE_SIZE);

    for (uint32_t a = PAGE_SIZE; a < limit; a += PAGE_SIZE) {
        if (map(a, a, VMM_WRITE) != 0) {
            kprintf("vmm: out of memory while building tables\n");
            return;
        }
    }

    uint32_t cr0;

    __asm__ volatile ("mov %0, %%cr3" : : "r"((uint32_t)pd) : "memory");
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000u;
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0) : "memory");
}

int vmm_map_page(uint32_t virt, uint32_t phys, uint32_t flags)
{
    if (!pd || (virt & 0xFFF) || (phys & 0xFFF))
        return -1;

    if (map(virt, phys, flags) != 0)
        return -1;

    invlpg(virt);
    return 0;
}

void vmm_unmap_page(uint32_t virt)
{
    uint32_t di = virt >> 22;
    uint32_t ti = (virt >> 12) & 0x3FF;

    if (!pd || !(pd[di] & VMM_PRESENT))
        return;

    uint32_t *pt = (uint32_t *)(pd[di] & 0xFFFFF000);

    pt[ti] = 0;
    invlpg(virt);
}

uint32_t vmm_get_phys(uint32_t virt)
{
    uint32_t di = virt >> 22;
    uint32_t ti = (virt >> 12) & 0x3FF;

    if (!pd || !(pd[di] & VMM_PRESENT))
        return 0;

    uint32_t *pt = (uint32_t *)(pd[di] & 0xFFFFF000);

    if (!(pt[ti] & VMM_PRESENT))
        return 0;
    return (pt[ti] & 0xFFFFF000) | (virt & 0xFFF);
}