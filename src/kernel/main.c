#include <vga.h>
#include <kprintf.h>
#include <serial.h>
#include <gdt.h>
#include <tss.h>
#include <idt.h>
#include <irq.h>
#include <pic.h>
#include <pit.h>
#include <keyboard.h>
#include <multiboot.h>
#include <memmap.h>
#include <pmm.h>
#include <vmm.h>
#include <heap.h>
#include <initrd.h>
#include <sched.h>
#include <shell.h>

void kernel_main(uint32_t magic, multiboot_info_t *mbi)
{
    vga_init();
    serial_init();
    gdt_init();
    tss_init();
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
    vmm_init();
    heap_init();
    kprintf("mem: %u pages free, heap at %p\n", pmm_free_pages(), (void *)KHEAP_START);

    initrd_init(mbi);

    sched_init();
    kprintf("sched: ready\n");

    __asm__ volatile ("sti");

    shell_run();
}