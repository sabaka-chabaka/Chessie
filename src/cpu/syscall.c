#include <stdint.h>

#include <kprintf.h>
#include <pit.h>
#include <sched.h>
#include <syscall.h>
#include <thread.h>

#define WRITE_MAX 4096

typedef int32_t (*syscall_fn)(uint32_t a, uint32_t b, uint32_t c);

static int range_ok(uint32_t ptr, uint32_t len)
{
    return ptr != 0 && len <= WRITE_MAX && ptr + len >= ptr;
}

static int32_t sys_write(uint32_t fd, uint32_t buf, uint32_t len)
{
    if (fd != 1 && fd != 2)
        return -1;
    if (len == 0)
        return 0;
    if (!range_ok(buf, len))
        return -1;

    const char *s = (const char *)buf;

    for (uint32_t i = 0; i < len; i++)
        kprintf("%c", s[i]);

    return (int32_t)len;
}

static int32_t sys_exit(uint32_t code, uint32_t b, uint32_t c)
{
    (void)b;
    (void)c;

    kprintf("[thread %u exited with %d]\n", sched_current()->id, (int)code);
    thread_exit();
}

static int32_t sys_ticks(uint32_t a, uint32_t b, uint32_t c)
{
    (void)a;
    (void)b;
    (void)c;

    return (int32_t)timer_ticks();
}

static const syscall_fn table[] = {
    [SYS_write] = sys_write,
    [SYS_exit]  = sys_exit,
    [SYS_ticks] = sys_ticks,
};

#define NSYSCALLS (sizeof(table) / sizeof(table[0]))

void syscall_handler(registers_t *regs)
{
    uint32_t n = regs->eax;

    if (n >= NSYSCALLS || !table[n]) {
        regs->eax = SYSCALL_ERR;
        return;
    }

    regs->eax = (uint32_t)table[n](regs->ebx, regs->ecx, regs->edx);
}