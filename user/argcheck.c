#include "ulib.h"

int main(int argc, char **argv)
{
    char **envp = argv + argc + 1;

    if (argc != 4)                               return 1;
    if (argv[argc] != 0)                         return 2;
    if (envp[0] != 0)                            return 3;
    if (!u_streq(argv[1], "one"))                return 4;
    if (!u_streq(argv[2], "two"))                return 5;
    if (!u_streq(argv[3], "three"))              return 6;
    if (!u_endswith(argv[0], "argcheck"))        return 7;
    if (((uintptr_t)argv - 4) & 15)              return 8;

    for (int i = 0; i < argc; i++) {
        uintptr_t a = (uintptr_t)argv[i];

        if (a < 0xBFFFC000u || a >= 0xC0000000u) return 9;
    }
    return 0;
}