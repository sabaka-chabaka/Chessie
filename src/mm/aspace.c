#include <stddef.h>
#include <stdint.h>

#include <aspace.h>
#include <heap.h>
#include <pmm.h>
#include <string.h>
#include <sync.h>
#include <vmm.h>

#define USER_PDE_FIRST (VMM_USER_BASE >> 22)
#define USER_PDE_LAST  (VMM_USER_END >> 22)

static aspace_t  kernel_as;
static aspace_t *cur;

static void ensure(void)
{
    if (!kernel_as.pd) {
        kernel_as.pd = vmm_kernel_pd();
        cur = &kernel_as;
    }
}

aspace_t *aspace_kernel(void)
{
    ensure();
    return &kernel_as;
}

aspace_t *aspace_current(void)
{
    ensure();
    return cur;
}

aspace_t *aspace_create(void)
{
    ensure();

    aspace_t *as = kmalloc(sizeof(*as));

    if (!as)
        return NULL;

    uint32_t *pd = (uint32_t *)pmm_alloc_page();

    if (!pd) {
        kfree(as);
        return NULL;
    }

    memset(pd, 0, PAGE_SIZE);

    for (uint32_t i = 0; i < 1024; i++) {
        if (i < USER_PDE_FIRST || i >= USER_PDE_LAST)
            pd[i] = kernel_as.pd[i];
    }

    as->pd = pd;
    return as;
}

void aspace_destroy(aspace_t *as)
{
    ensure();

    if (!as || as == &kernel_as)
        return;

    if (as == cur)
        aspace_switch(&kernel_as);

    for (uint32_t i = USER_PDE_FIRST; i < USER_PDE_LAST; i++) {
        if (!(as->pd[i] & VMM_PRESENT))
            continue;

        uint32_t *pt = (uint32_t *)(as->pd[i] & 0xFFFFF000);

        for (uint32_t j = 0; j < 1024; j++) {
            if (pt[j] & VMM_PRESENT)
                pmm_free_page(pt[j] & 0xFFFFF000);
        }
        pmm_free_page((uint32_t)pt);
    }

    pmm_free_page((uint32_t)as->pd);
    kfree(as);
}

void aspace_switch(aspace_t *as)
{
    ensure();

    if (!as)
        as = &kernel_as;

    uint32_t f = irq_save();

    if (as != cur) {
        cur = as;
        __asm__ volatile ("mov %0, %%cr3" : : "r"((uint32_t)as->pd) : "memory");
    }
    irq_restore(f);
}

int aspace_map(aspace_t *as, uint32_t virt, uint32_t phys, uint32_t flags)
{
    return vmm_map_in(as->pd, virt, phys, flags);
}

int aspace_map_new(aspace_t *as, uint32_t virt, uint32_t flags)
{
    uint32_t phys = pmm_alloc_page();

    if (!phys)
        return -1;

    memset((void *)phys, 0, PAGE_SIZE);

    if (vmm_map_in(as->pd, virt, phys, flags) != 0) {
        pmm_free_page(phys);
        return -1;
    }
    return 0;
}

uint32_t aspace_phys(aspace_t *as, uint32_t virt)
{
    return vmm_get_phys_in(as->pd, virt);
}

int aspace_user_ok(aspace_t *as, uint32_t ptr, uint32_t len, int write)
{
    return vmm_user_ok(as->pd, ptr, len, write);
}