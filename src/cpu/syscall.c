#include <stdint.h>

#include <aspace.h>
#include <kprintf.h>
#include <pit.h>
#include <sched.h>
#include <syscall.h>
#include <thread.h>

#define WRITE_MAX 4096

typedef int32_t (*syscall_fn)(const registers_t *r);

static int buf_ok(const registers_t *r, uint32_t ptr, uint32_t len, int write)
{
    if (len > WRITE_MAX)
        return 0;
    if (ptr == 0 || ptr + len < ptr)
        return 0;

    if ((r->cs & 3) == 3)
        return aspace_user_ok(aspace_current(), ptr, len, write);
    return 1;
}

static int32_t sys_write(const registers_t *r)
{
    uint32_t fd  = r->ebx;
    uint32_t buf = r->ecx;
    uint32_t len = r->edx;

    if (fd != 1 && fd != 2)
        return -1;
    if (len == 0)
        return 0;
    if (!buf_ok(r, buf, len, 0))
        return -1;

    const char *s = (const char *)buf;

    for (uint32_t i = 0; i < len; i++)
        kprintf("%c", s[i]);

    return (int32_t)len;
}

static int32_t sys_exit(const registers_t *r)
{
    kprintf("[thread %u exited with %d]\n", sched_current()->id, (int)r->ebx);
    thread_exit();
}

static int32_t sys_ticks(const registers_t *r)
{
    (void)r;
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

    regs->eax = (uint32_t)table[n](regs);
}