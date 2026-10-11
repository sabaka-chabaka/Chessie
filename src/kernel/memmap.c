#include <stdint.h>

#include <kprintf.h>
#include <memmap.h>

extern char kernel_start[];
extern char kernel_end[];

static const char *type_name(uint32_t type)
{
    static const char *names[] = {
        "?", "available", "reserved", "ACPI reclaimable", "ACPI NVS", "bad",
    };

    if (type < sizeof(names) / sizeof(names[0]))
        return names[type];
    return "?";
}

void memmap_print(const multiboot_info_t *mbi)
{
    kprintf("kernel: %p - %p (%u KiB)\n",
            (void *)kernel_start, (void *)kernel_end,
            ((uint32_t)kernel_end - (uint32_t)kernel_start) >> 10);

    if (!(mbi->flags & MULTIBOOT_INFO_MMAP)) {
        kprintf("no memory map from bootloader\n");
        return;
    }

    uint64_t free_total = 0;
    uint32_t cur = mbi->mmap_addr;
    uint32_t end = mbi->mmap_addr + mbi->mmap_length;

    kprintf("memory map:\n");

    while (cur < end) {
        const multiboot_mmap_entry_t *e = (const multiboot_mmap_entry_t *)cur;

        if (e->addr >> 32) {
            kprintf("  (above 4 GiB) %s\n", type_name(e->type));
        } else {
            uint64_t last = e->addr + e->len - 1;
            if (last > 0xFFFFFFFFull)
                last = 0xFFFFFFFFull;

            kprintf("  %p - %p  %s\n",
                    (void *)(uint32_t)e->addr, (void *)(uint32_t)last,
                    type_name(e->type));
        }

        if (e->type == MULTIBOOT_MEMORY_AVAILABLE)
            free_total += e->len;

        cur += e->size + sizeof(e->size);
    }

    kprintf("available: %u KiB (%u MiB)\n",
            (uint32_t)(free_total >> 10), (uint32_t)(free_total >> 20));
}