#include <stddef.h>
#include <stdint.h>

#include <heap.h>
#include <kprintf.h>
#include <pmm.h>
#include <string.h>
#include <vmm.h>

#define PDE_SPAN 0x400000u

static uint32_t *kpd;

static inline void invlpg(uint32_t virt)
{
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}

static inline int in_user_region(uint32_t virt)
{
    return virt >= VMM_USER_BASE && virt < VMM_USER_END;
}

static uint32_t *table_for(uint32_t *pd, uint32_t di, int create, uint32_t pde_flags)
{
    if (!(pd[di] & VMM_PRESENT)) {
        if (!create)
            return NULL;

        uint32_t t = pmm_alloc_page();

        if (!t)
            return NULL;
        memset((void *)t, 0, PAGE_SIZE);        /* без обнуления в таблице мусор */
        pd[di] = t | VMM_PRESENT | VMM_WRITE | (pde_flags & VMM_USER);
    }
    return (uint32_t *)(pd[di] & 0xFFFFF000);
}

static int map_raw(uint32_t *pd, uint32_t virt, uint32_t phys, uint32_t flags)
{
    uint32_t di = virt >> 22;
    uint32_t ti = (virt >> 12) & 0x3FF;
    uint32_t *pt = table_for(pd, di, 1, in_user_region(virt) ? VMM_USER : 0);

    if (!pt)
        return -1;

    pt[ti] = (phys & 0xFFFFF000) | (flags & (VMM_WRITE | VMM_USER)) | VMM_PRESENT;
    return 0;
}

void vmm_init(void)
{
    uint32_t limit = pmm_max_addr();

    if (limit > VMM_IDENTITY_LIMIT)
        limit = VMM_IDENTITY_LIMIT;

    kpd = (uint32_t *)pmm_alloc_page();
    if (!kpd) {
        kprintf("vmm: no memory for page directory\n");
        return;
    }
    memset(kpd, 0, PAGE_SIZE);

    for (uint32_t a = PAGE_SIZE; a < limit; a += PAGE_SIZE) {
        if (map_raw(kpd, a, a, VMM_WRITE) != 0) {
            kprintf("vmm: out of memory while building tables\n");
            return;
        }
    }

    for (uint32_t a = KHEAP_START; a < KHEAP_MAX; a += PDE_SPAN) {
        if (!table_for(kpd, a >> 22, 1, 0)) {
            kprintf("vmm: out of memory for heap tables\n");
            return;
        }
    }

    uint32_t cr0;

    __asm__ volatile ("mov %0, %%cr3" : : "r"((uint32_t)kpd) : "memory");
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000u;                                 /* бит PG */
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0) : "memory");
}

uint32_t *vmm_kernel_pd(void)
{
    return kpd;
}

int vmm_map_in(uint32_t *pd, uint32_t virt, uint32_t phys, uint32_t flags)
{
    if (!pd || ((virt | phys) & 0xFFF))
        return -1;

    if ((flags & VMM_USER) && !in_user_region(virt))
        return -1;

    if (map_raw(pd, virt, phys, flags) != 0)
        return -1;

    invlpg(virt);
    return 0;
}

uint32_t vmm_unmap_in(uint32_t *pd, uint32_t virt)
{
    uint32_t di = virt >> 22;
    uint32_t ti = (virt >> 12) & 0x3FF;
    uint32_t *pt;

    if (!pd || !(pt = table_for(pd, di, 0, 0)) || !(pt[ti] & VMM_PRESENT))
        return 0;

    uint32_t phys = pt[ti] & 0xFFFFF000;

    pt[ti] = 0;
    invlpg(virt);
    return phys;
}

uint32_t vmm_get_phys_in(uint32_t *pd, uint32_t virt)
{
    uint32_t di = virt >> 22;
    uint32_t ti = (virt >> 12) & 0x3FF;
    uint32_t *pt;

    if (!pd || !(pt = table_for(pd, di, 0, 0)) || !(pt[ti] & VMM_PRESENT))
        return 0;

    return (pt[ti] & 0xFFFFF000) | (virt & 0xFFF);
}

int vmm_user_ok(uint32_t *pd, uint32_t ptr, uint32_t len, int write)
{
    if (len == 0)
        return 1;

    uint32_t end = ptr + len;

    if (end < ptr)
        return 0;
    if (ptr < VMM_USER_BASE || end > VMM_USER_END)
        return 0;

    for (uint32_t a = ptr & ~0xFFFu; a < end; a += PAGE_SIZE) {
        uint32_t di = a >> 22;

        if (!(pd[di] & VMM_PRESENT) || !(pd[di] & VMM_USER))
            return 0;

        uint32_t e = ((uint32_t *)(pd[di] & 0xFFFFF000))[(a >> 12) & 0x3FF];

        if (!(e & VMM_PRESENT) || !(e & VMM_USER))
            return 0;
        if (write && !(e & VMM_WRITE))
            return 0;
    }
    return 1;
}

int vmm_map_page(uint32_t virt, uint32_t phys, uint32_t flags)
{
    return vmm_map_in(kpd, virt, phys, flags);
}

void vmm_unmap_page(uint32_t virt)
{
    vmm_unmap_in(kpd, virt);
}

uint32_t vmm_get_phys(uint32_t virt)
{
    return vmm_get_phys_in(kpd, virt);
}