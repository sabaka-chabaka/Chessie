#include <stdint.h>

#include <gdt.h>

static struct gdt_entry gdt[GDT_ENTRIES];
static struct gdt_ptr   gp;

static void gdt_set_entry(int idx, uint32_t base, uint32_t limit,
                          uint8_t access, uint8_t flags)
{
    gdt[idx].base_low  = base & 0xFFFF;
    gdt[idx].base_mid  = (base >> 16) & 0xFF;
    gdt[idx].base_high = (base >> 24) & 0xFF;

    gdt[idx].limit_low        = limit & 0xFFFF;
    gdt[idx].limit_high_flags = ((limit >> 16) & 0x0F) | ((flags & 0x0F) << 4);

    gdt[idx].access = access;
}

void gdt_init(void)
{
    gp.limit = sizeof(gdt) - 1;
    gp.base  = (uint32_t)&gdt;

    gdt_set_entry(0, 0, 0,       0,                      0);
    gdt_set_entry(1, 0, 0xFFFFF, GDT_ACCESS_KERNEL_CODE, GDT_FLAGS_32BIT_PAGE);
    gdt_set_entry(2, 0, 0xFFFFF, GDT_ACCESS_KERNEL_DATA, GDT_FLAGS_32BIT_PAGE);

    gdt_flush((uint32_t)&gp);
}