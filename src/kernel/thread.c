#include <stddef.h>
#include <stdint.h>

#include <heap.h>
#include <sched.h>
#include <string.h>
#include <thread.h>
#include <aspace.h>

_Static_assert(offsetof(thread_t, esp) == 0, "esp must be the first field: switch.S relies on it");

static void thread_start(void)
{
    __asm__ volatile ("sti");

    thread_t *t = sched_current();

    t->entry(t->arg);
    thread_exit();
}

thread_t *thread_alloc(const char *name, thread_fn_t entry, void *arg)
{
    thread_t *t = kcalloc(1, sizeof(*t));

    if (!t)
        return NULL;

    t->kstack = kmalloc(THREAD_STACK_SIZE);
    if (!t->kstack) {
        kfree(t);
        return NULL;
    }

    *(uint32_t *)t->kstack = THREAD_STACK_CANARY;

    size_t n = strlen(name);

    if (n >= THREAD_NAME_MAX)
        n = THREAD_NAME_MAX - 1;
    memcpy(t->name, name, n);

    t->entry = entry;
    t->arg   = arg;
    t->state = THREAD_READY;

    uint32_t *sp = (uint32_t *)(t->kstack + THREAD_STACK_SIZE);

    *--sp = 0;
    *--sp = (uint32_t)thread_start;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    t->esp = (uint32_t)sp;

    return t;
}

void thread_free(thread_t *t)
{
    if (t->aspace)
        aspace_destroy(t->aspace);
    kfree(t->kstack);
    kfree(t);
}

thread_t *thread_create(const char *name, thread_fn_t entry, void *arg)
{
    thread_t *t = thread_alloc(name, entry, arg);

    if (t)
        sched_add(t);
    return t;
}

void thread_exit(void)
{
    sched_exit();
}