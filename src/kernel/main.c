#include <vga.h>
#include <kprintf.h>
#include <serial.h>
#include <gdt.h>
#include <idt.h>
#include <irq.h>
#include <pic.h>
#include <pit.h>

void kernel_main(void)
{
    vga_init();
    serial_init();
    gdt_init();
    idt_init();
    pic_init();
    pit_init(TIMER_HZ);

    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("Hello from Chessie!\n");

    __asm__ volatile ("sti");

    for (unsigned s = 1; ; s++) {
        sleep_ms(1000);
        kprintf("uptime: %u s\n", s);
    }
}