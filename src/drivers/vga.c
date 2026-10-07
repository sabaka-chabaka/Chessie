#include <stddef.h>
#include <stdint.h>

#include <io.h>
#include <vga.h>

#define VGA_MEMORY      ((volatile uint16_t *)0xB8000)

#define CRTC_INDEX      0x3D4
#define CRTC_DATA       0x3D5
#define CRTC_CURSOR_START   0x0A
#define CRTC_CURSOR_END     0x0B
#define CRTC_CURSOR_HIGH    0x0E
#define CRTC_CURSOR_LOW     0x0F

#define TAB_WIDTH       4

static size_t  cursor_x;
static size_t  cursor_y;
static uint8_t current_color;

static inline uint8_t make_color(vga_color_t fg, vga_color_t bg)
{
    return (uint8_t)fg | ((uint8_t)bg << 4);
}

static inline uint16_t make_entry(char c, uint8_t color)
{
    return (uint16_t)(uint8_t)c | ((uint16_t)color << 8);
}

static void hw_update_cursor(void)
{
    uint16_t pos = (uint16_t)(cursor_y * VGA_WIDTH + cursor_x);

    outb(CRTC_INDEX, CRTC_CURSOR_LOW);
    outb(CRTC_DATA, (uint8_t)(pos & 0xFF));
    outb(CRTC_INDEX, CRTC_CURSOR_HIGH);
    outb(CRTC_DATA, (uint8_t)((pos >> 8) & 0xFF));
}

static void scroll_up(void)
{
    for (size_t i = 0; i < VGA_WIDTH * (VGA_HEIGHT - 1); i++)
        VGA_MEMORY[i] = VGA_MEMORY[i + VGA_WIDTH];

    for (size_t i = VGA_WIDTH * (VGA_HEIGHT - 1); i < VGA_WIDTH * VGA_HEIGHT; i++)
        VGA_MEMORY[i] = make_entry(' ', current_color);

    cursor_y = VGA_HEIGHT - 1;
}

static void new_line(void)
{
    cursor_x = 0;
    cursor_y++;

    if (cursor_y >= VGA_HEIGHT)
        scroll_up();
}

void vga_set_color(vga_color_t fg, vga_color_t bg)
{
    current_color = make_color(fg, bg);
}

void vga_clear(void)
{
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        VGA_MEMORY[i] = make_entry(' ', current_color);

    cursor_x = 0;
    cursor_y = 0;
    hw_update_cursor();
}

void vga_set_cursor(uint8_t x, uint8_t y)
{
    if (x >= VGA_WIDTH || y >= VGA_HEIGHT)
        return;

    cursor_x = x;
    cursor_y = y;
    hw_update_cursor();
}

void vga_enable_cursor(void)
{
    outb(CRTC_INDEX, CRTC_CURSOR_START);
    outb(CRTC_DATA, (inb(CRTC_DATA) & 0xC0) | 14);
    outb(CRTC_INDEX, CRTC_CURSOR_END);
    outb(CRTC_DATA, (inb(CRTC_DATA) & 0xE0) | 15);
}

void vga_disable_cursor(void)
{
    outb(CRTC_INDEX, CRTC_CURSOR_START);
    outb(CRTC_DATA, 0x20);
}

void vga_init(void)
{
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_clear();
    vga_enable_cursor();
}

void vga_putc(char c)
{
    switch (c) {
    case '\n':
        new_line();
        break;

    case '\r':
        cursor_x = 0;
        break;

    case '\t':
        cursor_x = (cursor_x + TAB_WIDTH) & ~(size_t)(TAB_WIDTH - 1);
        if (cursor_x >= VGA_WIDTH)
            new_line();
        break;

    case '\b':
        if (cursor_x > 0) {
            cursor_x--;
        } else if (cursor_y > 0) {
            cursor_y--;
            cursor_x = VGA_WIDTH - 1;
        }
        VGA_MEMORY[cursor_y * VGA_WIDTH + cursor_x] = make_entry(' ', current_color);
        break;

    default:
        VGA_MEMORY[cursor_y * VGA_WIDTH + cursor_x] = make_entry(c, current_color);
        cursor_x++;
        if (cursor_x >= VGA_WIDTH)
            new_line();
        break;
    }

    hw_update_cursor();
}

void vga_write(const char *s)
{
    while (*s)
        vga_putc(*s++);
}