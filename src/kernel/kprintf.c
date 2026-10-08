#include <stdarg.h>
#include <stdint.h>

#include <kprintf.h>
#include <vga.h>

static const char digits[] = "0123456789abcdef";

static void kputc(char c) {
    vga_putc(c);
}

static void kputs(const char *s) {
    while (*s) kputc(*s++);
}

static void print_uint(uint32_t v, unsigned base) {
    char buf[32];
    int i = sizeof(buf);

    do {
        buf[--i] = digits[v % base];
        v /= base;
    } while (v);

    while (i < (int)sizeof(buf))
        kputc(buf[i++]);
}

static void print_int(int v) {
    uint32_t u = (uint32_t)v;

    if (v < 0) {
        kputc('-');
        u = 0u - u;
    }
    print_uint(u, 10);
}

static void print_ptr(uint32_t v) {
    kputs("0x");
    for (int shift = 28; shift >= 0; shift -= 4)
        kputc(digits[(v >> shift) & 0xF]);
}

void kvprintf(const char *fmt, va_list ap) {
    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            kputc(*fmt);
            continue;
        }

        fmt++;
        switch (*fmt) {
            case 'c':
                kputc((char)va_arg(ap, int));
                break;
            case 's': {
                const char *s = va_arg(ap, const char *);
                kputs(s ? s : "(null)");
                break;
            }
            case 'd':
                print_int(va_arg(ap, int));
                break;
            case 'u':
                print_uint(va_arg(ap, unsigned int), 10);
                break;
            case 'x':
                print_uint(va_arg(ap, unsigned int), 16);
                break;
            case 'p':
                print_ptr((uint32_t)(uintptr_t)va_arg(ap, void *));
                break;
            case '%':
                kputc('%');
                break;
            case '\0':
                return;
            default:
                kputc('%');
                kputc(*fmt);
                break;
        }
    }
}

void kprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
}