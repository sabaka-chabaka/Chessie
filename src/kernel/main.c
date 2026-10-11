#include <vga.h>
#include <kprintf.h>
#include <serial.h>
#include <gdt.h>
#include <idt.h>
#include <irq.h>
#include <pic.h>
#include <pit.h>
#include <keyboard.h>

void kernel_main(void)
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

    __asm__ volatile ("sti");

    for (;;)
        kprintf("%c", keyboard_getchar());
}