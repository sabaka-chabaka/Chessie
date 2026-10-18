#include "ulib.h"

int main(int argc, char **argv)
{
    u_puts("argc=");
    u_puti(argc);
    u_puts("\n");

    for (int i = 0; i < argc; i++) {
        u_puts("argv[");
        u_puti(i);
        u_puts("]=");
        u_puts(argv[i]);
        u_puts("\n");
    }
    return argc;
}