#include <vga.h>
#include <kprintf.h>
#include <serial.h>
#include <gdt.h>

void kernel_main(void)
{
    vga_init();
    serial_init();
    gdt_init();

    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("Hello from Chessie!\n");
}