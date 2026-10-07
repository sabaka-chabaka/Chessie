#include <vga.h>

void kernel_main(void)
{
    vga_init();

    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_write("Hello from Chessie!\n");

    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_write("VGA driver works.\n");
}