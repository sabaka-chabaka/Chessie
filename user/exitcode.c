#include "ulib.h"

int main(int argc, char **argv)
{
    int code = argc > 1 ? u_atoi(argv[1]) : 0;
    uint32_t delay = argc > 2 ? (uint32_t)u_atoi(argv[2]) : 0;
    uint32_t t0 = sys_ticks();

    while ((uint32_t)(sys_ticks() - t0) < delay)
        ;
    return code;
}