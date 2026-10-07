#include <vga.h>
#include <string.h>

void kernel_main(void)
{
    vga_init();

    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_write("Hello from Chessie!\n");

    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_write("VGA driver works.\n");

    const char *s1 = "Hello World";
    const char *s2 = "Hello";
    int res = strcmp(s1, s2);

    char resStr[16];
    int_to_str(res, resStr);

    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_write(resStr);

}