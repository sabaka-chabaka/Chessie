#include <vga.h>
#include <kprintf.h>
#include <serial.h>

void kernel_main(void)
{
    vga_init();
    serial_init();

    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("Hello from Chessie!\n");

    kprintf("%d %u %x %s %c %%\n", -42, 42, 0xDEAD, "ok", 'A');
    kprintf("%d %d\n", 0, -2147483647 - 1);
    kprintf("%u %x\n", 4294967295u, 0xFFFFFFFFu);
    kprintf("%s|%p\n", (char *)0, (void *)0x1000);
}