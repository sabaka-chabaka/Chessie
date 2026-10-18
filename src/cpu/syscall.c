#include <stdint.h>

#include <aspace.h>
#include <heap.h>
#include <kprintf.h>
#include <pit.h>
#include <process.h>
#include <sched.h>
#include <syscall.h>
#include <thread.h>
#include <vfs.h>

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

static int32_t user_cstr(const registers_t *r, uint32_t uptr, char *dst, uint32_t max)
{
    for (uint32_t i = 0; i < max; i++) {
        uint32_t a = uptr + i;

        if (a < uptr)
            return -1;
        if ((i == 0 || (a & 0xFFF) == 0) && !buf_ok(r, a, 1, 0))
            return -1;

        dst[i] = *(const char *)a;
        if (!dst[i])
            return (int32_t)i;
    }
    return -2;
}

static int user_u32(const registers_t *r, uint32_t uptr, uint32_t *out)
{
    if (!buf_ok(r, uptr, 4, 0))
        return -1;
    *out = *(const uint32_t *)uptr;
    return 0;
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
    thread_t *t = sched_current();

    if (!t->proc)
        kprintf("[thread %u exited with %d]\n", t->id, (int)r->ebx);
    proc_exit((int32_t)r->ebx);
}

static int32_t sys_ticks(const registers_t *r)
{
    (void)r;
    return (int32_t)timer_ticks();
}

static int32_t sys_getpid(const registers_t *r)
{
    (void)r;

    process_t *p = proc_current();

    return p ? (int32_t)p->pid : 0;
}

typedef struct {
    char        path[VFS_PATH_MAX];
    char        strs[PROC_ARGS_MAX_BYTES];
    const char *argv[PROC_MAX_ARGS + 1];
} spawn_buf_t;

static int32_t sys_spawn(const registers_t *r)
{
    if (r->edx & ~(uint32_t)PROC_SPAWN_DETACHED)
        return VFS_EINVAL;

    spawn_buf_t *b = kmalloc(sizeof(*b));

    if (!b)
        return PROC_ENOMEM;

    int32_t ret = PROC_EFAULT;
    int32_t len = user_cstr(r, r->ebx, b->path, sizeof(b->path));

    if (len < 0) {
        ret = (len == -2) ? VFS_EINVAL : PROC_EFAULT;
        goto out;
    }

    uint32_t used = 0;
    uint32_t n = 0;

    if (r->ecx) {
        for (;;) {
            uint32_t uptr;

            if (n == PROC_MAX_ARGS) {
                ret = PROC_E2BIG;
                goto out;
            }
            if (r->ecx + 4 * n < r->ecx || user_u32(r, r->ecx + 4 * n, &uptr) != 0) {
                ret = PROC_EFAULT;
                goto out;
            }
            if (!uptr)
                break;

            len = user_cstr(r, uptr, b->strs + used, sizeof(b->strs) - used);
            if (len < 0) {
                ret = (len == -2) ? PROC_E2BIG : PROC_EFAULT;
                goto out;
            }
            b->argv[n++] = b->strs + used;
            used += (uint32_t)len + 1;
        }
    }
    b->argv[n] = NULL;

    ret = proc_spawn(b->path, b->argv, r->edx);

out:
    kfree(b);
    return ret;
}

static int32_t sys_wait(const registers_t *r)
{
    uint32_t status_ptr = r->ecx;
    int32_t st = 0;

    if (status_ptr && !buf_ok(r, status_ptr, 4, 1))
        return PROC_EFAULT;

    int ret = proc_wait((int)r->ebx, &st);

    if (ret >= 0 && status_ptr)
        *(int32_t *)status_ptr = st;
    return ret;
}

static const syscall_fn table[] = {
    [SYS_write]  = sys_write,
    [SYS_exit]   = sys_exit,
    [SYS_ticks]  = sys_ticks,
    [SYS_getpid] = sys_getpid,
    [SYS_spawn]  = sys_spawn,
    [SYS_wait]   = sys_wait,
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