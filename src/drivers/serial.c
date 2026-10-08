#include <serial.h>

#include "io.h"

void serial_init(void) {
    const uint16_t base = COM1;

    outb(base + 1, 0x00);
    outb(base + 3, 0x80);

    outb(base + 0, 0x01);
    outb(base + 1, 0x00);

    outb(base + 3, 0x03);
    outb(base + 2, 0xC7);
    outb(base + 4, 0x0B);
}

static void serial_send(char c) {
    while (!(inb(COM1 + 5) & 0x20))
        ;
    outb(COM1, (uint8_t)c);
}

void serial_putc(char c) {
    if (c == '\n') serial_send('\r');
    serial_send(c);
}

void serial_write(const char *s) {
    while (*s) serial_putc(*s++);
}