#include "ulib.h"

static int recurse(int n) __attribute__((noinline));
static int recurse(int n)
{
    volatile char pad[512];

    pad[0] = (char)n;
    return recurse(n + 1) + pad[0];
}

static int check_pointers(void)
{
    int bad = 0;

    if (sys_write(1, "ok\n", 3) != 3)                   bad++;
    if (sys_write(1, (void *)0x00100000, 8) != -1)      bad++;
    if (sys_write(1, (void *)0xC0000000, 8) != -1)      bad++;
    if (sys_write(1, (void *)0x50000000, 8) != -1)      bad++;
    if (sys_write(1, (void *)0, 4) != -1)               bad++;
    if (sys_write(1, (void *)0xFFFFFFF0u, 64) != -1)    bad++;
    if (sys_write(1, (void *)0xBFFFFFF0u, 64) != -1)    bad++;
    if (sys_spawn((const char *)0x00100000, 0, 0) != E_FAULT) bad++;
    return bad;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        u_puts("usage: crash null|kernel|cli|div|ud2|wcode|outb|stack|syscall\n");
        return 2;
    }

    const char *m = argv[1];

    if (u_streq(m, "null"))    *(volatile int *)0 = 1;
    if (u_streq(m, "kernel"))  (void)*(volatile int *)0x00100000;
    if (u_streq(m, "cli"))     __asm__ volatile ("cli");
    if (u_streq(m, "div"))     { volatile int z = 0; return 1 / z; }
    if (u_streq(m, "ud2"))     __asm__ volatile ("ud2");
    if (u_streq(m, "wcode"))   *(volatile uint8_t *)(uintptr_t)main = 0;
    if (u_streq(m, "outb"))    __asm__ volatile ("outb %%al, %%dx" : : "a"(0x58), "d"(0x3F8));
    if (u_streq(m, "stack"))   return recurse(0);
    if (u_streq(m, "syscall")) return check_pointers();

    u_puts("crash: unknown mode\n");
    return 3;
}