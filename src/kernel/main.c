#include <vga.h>
#include <kprintf.h>
#include <serial.h>
#include <gdt.h>
#include <idt.h>
#include <irq.h>
#include <pic.h>
#include <pit.h>
#include <keyboard.h>
#include <multiboot.h>
#include <memmap.h>
#include <pmm.h>
#include <shell.h>

void kernel_main(uint32_t magic, multiboot_info_t *mbi)
{
    vga_init();
    serial_init();
    gdt_init();
    idt_init();
    pic_init();
    pit_init(TIMER_HZ);
    keyboard_init();

    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("Hello from Chessie!\n");

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        kprintf("bad multiboot magic: %x\n", magic);
        for (;;)
            __asm__ volatile ("cli; hlt");
    }

    memmap_print(mbi);

    pmm_init(mbi);
    kprintf("pmm: %u of %u pages free (%u MiB)\n",
            pmm_free_pages(), pmm_total_pages(), pmm_free_pages() >> 8);

    __asm__ volatile ("sti");

    shell_run();
}