#include <stddef.h>
#include <stdint.h>

#include <kprintf.h>
#include <sched.h>
#include <sync.h>
#include <thread.h>

void mutex_init(mutex_t *m)
{
    m->owner = NULL;
    m->wait_head = NULL;
    m->wait_tail = NULL;
}

void mutex_lock(mutex_t *m)
{
    thread_t *self = sched_current();

    if (!self)
        return;

    uint32_t flags = irq_save();

    if (m->owner == self) {
        kprintf("mutex: recursive lock by thread %u\n", self->id);
        for (;;)
            __asm__ volatile ("cli; hlt");
    }

    while (m->owner) {
        self->wait_next = NULL;
        if (m->wait_tail)
            m->wait_tail->wait_next = self;
        else
            m->wait_head = self;
        m->wait_tail = self;

        self->state = THREAD_BLOCKED;
        schedule();
    }

    m->owner = self;
    irq_restore(flags);
}

void mutex_unlock(mutex_t *m)
{
    thread_t *self = sched_current();

    if (!self)
        return;

    uint32_t flags = irq_save();

    if (m->owner != self) {
        kprintf("mutex: unlock by thread that does not own it\n");
        irq_restore(flags);
        return;
    }

    m->owner = NULL;

    thread_t *w = m->wait_head;

    if (w) {
        m->wait_head = w->wait_next;
        if (!m->wait_head)
            m->wait_tail = NULL;
        w->wait_next = NULL;
        thread_wake(w);
    }

    irq_restore(flags);
}