#include <stddef.h>
#include <stdint.h>

#include <aspace.h>
#include <elf.h>
#include <heap.h>
#include <kprintf.h>
#include <pmm.h>
#include <process.h>
#include <sched.h>
#include <string.h>
#include <sync.h>
#include <thread.h>
#include <usermode.h>
#include <vfs.h>
#include <vmm.h>

static process_t *head;
static uint32_t   next_pid = 1;

static process_t *find(uint32_t pid)
{
    for (process_t *p = head; p; p = p->next) {
        if (p->pid == pid)
            return p;
    }
    return NULL;
}

static void unlink_free(process_t *p)
{
    for (process_t **pp = &head; *pp; pp = &(*pp)->next) {
        if (*pp == p) {
            *pp = p->next;
            break;
        }
    }
    kfree(p);
}

process_t *proc_current(void)
{
    thread_t *t = sched_current();

    return t ? t->proc : NULL;
}

static int copy_to_aspace(aspace_t *as, uint32_t va, const void *src, uint32_t len)
{
    const uint8_t *s = src;

    while (len) {
        uint32_t phys = aspace_phys(as, va);

        if (!phys)
            return -1;

        uint32_t chunk = PAGE_SIZE - (va & 0xFFF);

        if (chunk > len)
            chunk = len;
        memcpy((void *)phys, s, chunk);
        va += chunk;
        s += chunk;
        len -= chunk;
    }
    return 0;
}

int proc_build_stack(aspace_t *as, const char *const *argv, uint32_t *esp_out)
{
    uint32_t argc = 0, strbytes = 0;

    while (argv && argv[argc]) {
        if (argc == PROC_MAX_ARGS)
            return PROC_E2BIG;
        strbytes += (uint32_t)strlen(argv[argc]) + 1;
        if (strbytes > PROC_ARGS_MAX_BYTES)
            return PROC_E2BIG;
        argc++;
    }

    uint32_t top   = PROC_STACK_TOP;
    uint32_t strs  = top - strbytes;
    uint32_t words = argc + 3;
    uint32_t esp   = (strs - 4 * words) & ~15u;

    if (esp < top - (PROC_STACK_PAGES * PAGE_SIZE) / 2)
        return PROC_E2BIG;

    uint32_t table[PROC_MAX_ARGS + 3];
    uint32_t at = strs;

    table[0] = argc;
    for (uint32_t i = 0; i < argc; i++) {
        uint32_t n = (uint32_t)strlen(argv[i]) + 1;

        if (copy_to_aspace(as, at, argv[i], n) != 0)
            return PROC_ENOMEM;
        table[1 + i] = at;
        at += n;
    }
    table[1 + argc] = 0;
    table[2 + argc] = 0;

    if (copy_to_aspace(as, esp, table, 4 * words) != 0)
        return PROC_ENOMEM;

    *esp_out = esp;
    return 0;
}

static void proc_entry(void *arg)
{
    process_t *p = arg;

    enter_usermode(p->entry, p->user_esp);
}

static const char *base_name(const char *path)
{
    const char *b = path;

    for (; *path; path++) {
        if (*path == '/')
            b = path + 1;
    }
    return b;
}

int proc_spawn(const char *path, const char *const *argv, uint32_t flags)
{
    const char *fallback[2] = { path, NULL };
    process_t *p = NULL;
    thread_t *t = NULL;
    uint32_t entry = 0, esp = 0;
    int r;

    if (!argv || !argv[0])
        argv = fallback;

    aspace_t *as = aspace_create();

    if (!as)
        return PROC_ENOMEM;

    r = elf_load(path, as, &entry);
    if (r < 0)
        goto fail;

    for (uint32_t i = 0; i < PROC_STACK_PAGES; i++) {
        if (aspace_map_new(as, PROC_STACK_TOP - (i + 1) * PAGE_SIZE, VMM_USER | VMM_WRITE) != 0) {
            r = PROC_ENOMEM;
            goto fail;
        }
    }

    r = proc_build_stack(as, argv, &esp);
    if (r < 0)
        goto fail;

    p = kcalloc(1, sizeof(*p));
    if (!p) {
        r = PROC_ENOMEM;
        goto fail;
    }

    const char *name = base_name(path);
    size_t n = strlen(name);

    if (n >= PROC_NAME_MAX)
        n = PROC_NAME_MAX - 1;
    memcpy(p->name, name, n);

    t = thread_alloc(p->name, proc_entry, p);
    if (!t) {
        kfree(p);
        r = PROC_ENOMEM;
        goto fail;
    }

    t->aspace   = as;
    t->proc     = p;
    p->thread   = t;
    p->entry    = entry;
    p->user_esp = esp;
    p->detached = (flags & PROC_SPAWN_DETACHED) ? 1 : 0;
    p->state    = PROC_ALIVE;

    uint32_t f = irq_save();
    int pid;

    p->pid    = next_pid++;
    p->parent = proc_current();
    p->next   = head;
    head      = p;
    pid       = (int)p->pid;
    sched_add(t);
    irq_restore(f);

    return pid;

fail:
    aspace_destroy(as);
    return r;
}

int proc_wait(int pid, int32_t *status)
{
    thread_t *self = sched_current();
    process_t *me = self ? self->proc : NULL;

    if (pid <= 0 || !self)
        return -1;

    uint32_t f = irq_save();
    process_t *p = find((uint32_t)pid);

    if (!p || p->parent != me || p->detached || p->waiter) {
        irq_restore(f);
        return -1;
    }

    while (p->state != PROC_ZOMBIE) {
        p->waiter = self;
        self->state = THREAD_BLOCKED;
        schedule();
    }

    if (status)
        *status = p->exit_code;
    unlink_free(p);
    irq_restore(f);
    return pid;
}

void proc_exit(int32_t code)
{
    thread_t *t = sched_current();
    process_t *p = t ? t->proc : NULL;

    if (!p)
        thread_exit();

    (void)irq_save();

    p->exit_code = code;
    p->state = PROC_ZOMBIE;

    process_t **pp = &head;

    while (*pp) {
        process_t *c = *pp;

        if (c->parent == p) {
            if (c->state == PROC_ZOMBIE) {
                *pp = c->next;
                kfree(c);
                continue;
            }
            c->parent = NULL;
            c->detached = 1;
        }
        pp = &c->next;
    }

    if (p->detached) {
        unlink_free(p);
        t->proc = NULL;
    } else if (p->waiter) {
        thread_wake(p->waiter);
    }

    thread_exit();
}

void proc_list(void)
{
    uint32_t f = irq_save();

    if (!head)
        kprintf("  (no user processes)\n");

    for (process_t *p = head; p; p = p->next) {
        uint32_t ppid = p->parent ? p->parent->pid : 0;

        if (p->state == PROC_ZOMBIE)
            kprintf("  %u %s zombie code=%d parent=%u\n", p->pid, p->name, (int)p->exit_code, ppid);
        else
            kprintf("  %u %s alive parent=%u%s\n", p->pid, p->name, ppid, p->detached ? " detached" : "");
    }
    irq_restore(f);
}

uint32_t proc_count(void)
{
    uint32_t n = 0;
    uint32_t f = irq_save();

    for (process_t *p = head; p; p = p->next)
        n++;
    irq_restore(f);
    return n;
}

const char *proc_strerror(int err)
{
    switch (err) {
    case PROC_ENOMEM: return "out of memory";
    case PROC_E2BIG:  return "too many or too long arguments";
    case PROC_EFAULT: return "bad pointer";
    }
    if (err <= ELF_EFORMAT && err >= ELF_EIO)
        return elf_strerror(err);
    return vfs_strerror(err);
}