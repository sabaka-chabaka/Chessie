#include <stdint.h>

#include <gdt.h>
#include <string.h>
#include <tss.h>

struct tss_entry {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1, ss1, esp2, ss2;
    uint32_t cr3, eip, eflags;
    uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed));

_Static_assert(sizeof(struct tss_entry) == 104, "TSS must be 104 bytes");

static struct tss_entry tss;

void tss_init(void) {
    memset(&tss, 0, sizeof(tss));

    tss.ss0 = GDT_KERNEL_DATA;
    tss.esp0 = 0;
    tss.iomap_base = sizeof(tss);

    gdt_set_entry(GDT_TSS_INDEX, (uint32_t)&tss, sizeof(tss) - 1, GDT_ACCESS_TSS, 0);

    __asm__ volatile ("ltr %%ax" : : "a"((uint16_t)GDT_TSS));
}

void tss_set_kernel_stack(uint32_t esp0)
{
    tss.esp0 = esp0;
}

uint32_t tss_get_kernel_stack(void)
{
    return tss.esp0;
}