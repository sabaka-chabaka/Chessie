#include "ulib.h"

int main(void)
{
    int bad = 0;
    int32_t st;

    char *a1[] = { "/bin/exitcode", "10", "3", 0 };
    char *a2[] = { "/bin/exitcode", "20", "3", 0 };
    char *a3[] = { "/bin/exitcode", "30", "3", 0 };

    int p1 = sys_spawn("/bin/exitcode", a1, 0);
    int p2 = sys_spawn("/bin/exitcode", a2, 0);
    int p3 = sys_spawn("/bin/exitcode", a3, 0);

    if (p1 <= 0 || p2 <= 0 || p3 <= 0 || p1 == p2 || p2 == p3 || p1 == p3)
        return 100;

    if (sys_wait(p3, &st) != p3 || st != 30)    bad++;
    if (sys_wait(p1, &st) != p1 || st != 10)    bad++;
    if (sys_wait(p2, &st) != p2 || st != 20)    bad++;

    if (sys_wait(p1, &st) != -1)                bad++;
    if (sys_wait(99999, &st) != -1)             bad++;
    if (sys_wait(sys_getpid(), &st) != -1)      bad++;

    int pc = sys_spawn("/bin/exitcode", a1, 0);

    if (pc <= 0)                                           bad++;
    if (sys_wait(pc, (int32_t *)0x00100000) != E_FAULT)    bad++;
    if (sys_wait(pc, &st) != pc || st != 10)               bad++;

    if (sys_spawn("/nope", 0, 0) != E_NOENT)               bad++;
    if (sys_spawn("/bin/hello", 0, 0x80) != E_INVAL)       bad++;

    char *a4[] = { "/bin/exitcode", "5", 0 };
    char *a5[] = { "/bin/exitcode", "9", "40", 0 };

    if (sys_spawn("/bin/exitcode", a4, 0) <= 0)            bad++;
    if (sys_spawn("/bin/exitcode", a5, 0) <= 0)            bad++;

    return bad;
}