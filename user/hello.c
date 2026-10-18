#include "ulib.h"

int main(void)
{
    u_puts("hello from /bin/hello, pid ");
    u_putu((uint32_t)sys_getpid());
    u_puts("\n");
    return 0;
}