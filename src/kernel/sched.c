#include <stddef.h>
#include <stdint.h>

#include <kprintf.h>
#include <pit.h>
#include <sched.h>
#include <string.h>
#include <sync.h>
#include <thread.h>
#include <tss.h>

extern void switch_context(uint32_t *old_esp, uint32_t new_esp);

static thread_t  main_thread;
static thread_t *idle_thread;
static thread_t *current;
static thread_t *list_head;
static uint32_t  next_id = 1;
static uint32_t  slice_left = SCHED_QUANTUM_TICKS;

static void sched_panic(const char *msg, const thread_t *t)
{
    kprintf("\nSCHED PANIC: %s (thread %u '%s')\n", msg, t->id, t->name);
    for (;;)
        __asm__ volatile ("cli; hlt");
}

static void list_add(thread_t *t)
{
    if (!list_head) {
        t->next = t;
        list_head = t;
    } else {
        t->next = list_head->next;
        list_head->next = t;
    }
}

static void list_remove(thread_t *t)
{
    thread_t *p = t;

    while (p->next != t)
        p = p->next;

    if (p == t) {
        list_head = NULL;
        return;
    }

    p->next = t->next;
    if (list_head == t)
        list_head = p;
    t->next = NULL;
}

static void reap_dead(const thread_t *keep)
{
    for (;;) {
        thread_t *victim = NULL;
        thread_t *t = list_head;

        if (!t)
            return;

        do {
            if (t->state == THREAD_DEAD && t != keep) {
                victim = t;
                break;
            }
            t = t->next;
        } while (t != list_head);

        if (!victim)
            return;

        list_remove(victim);
        thread_free(victim);
    }
}

static thread_t *pick_next(thread_t *prev)
{
    thread_t *start = prev->next ? prev->next : list_head;
    thread_t *t = start;

    do {
        if (t->state == THREAD_READY)
            return t;
        t = t->next;
    } while (t != start);

    return prev->state == THREAD_RUNNING ? prev : idle_thread;
}

void schedule(void)
{
    uint32_t flags = irq_save();
    thread_t *prev = current;

    if (!prev) {
        irq_restore(flags);
        return;
    }

    if (prev->kstack && *(uint32_t *)prev->kstack != THREAD_STACK_CANARY)
        sched_panic("kernel stack overflow", prev);

    reap_dead(prev);

    thread_t *next = pick_next(prev);

    if (next != prev) {
        if (prev->state == THREAD_RUNNING)
            prev->state = THREAD_READY;
        next->state = THREAD_RUNNING;
        current = next;
        slice_left = SCHED_QUANTUM_TICKS;

        if (next->kstack)
            tss_set_kernel_stack((uint32_t)next->kstack + THREAD_STACK_SIZE);

        switch_context(&prev->esp, next->esp);
    }

    irq_restore(flags);
}

static void sched_tick(void)
{
    if (!current)
        return;

    uint32_t now = timer_ticks();
    thread_t *t = list_head;

    do {
        if (t->state == THREAD_SLEEPING && (int32_t)(now - t->wake_tick) >= 0)
            t->state = THREAD_READY;
        t = t->next;
    } while (t != list_head);

    if (current == idle_thread || --slice_left == 0) {
        slice_left = SCHED_QUANTUM_TICKS;
        schedule();
    }
}

void thread_yield(void)
{
    schedule();
}

void thread_sleep_ms(uint32_t ms)
{
    if (!current) {
        sleep_ms(ms);
        return;
    }
    if (ms == 0) {
        thread_yield();
        return;
    }

    uint32_t ticks = (ms * TIMER_HZ + 999) / 1000;
    uint32_t flags = irq_save();

    current->wake_tick = timer_ticks() + ticks;
    current->state = THREAD_SLEEPING;
    schedule();
    irq_restore(flags);
}

void thread_wake(thread_t *t)
{
    if (t->state == THREAD_BLOCKED || t->state == THREAD_SLEEPING)
        t->state = THREAD_READY;
}

thread_t *sched_current(void)
{
    return current;
}

void sched_add(thread_t *t)
{
    uint32_t flags = irq_save();

    t->id = next_id++;
    list_add(t);
    irq_restore(flags);
}

void sched_exit(void)
{
    uint32_t flags = irq_save();

    (void)flags;
    current->state = THREAD_DEAD;
    schedule();

    for (;;)
        __asm__ volatile ("hlt");
}

static void idle_main(void *arg)
{
    (void)arg;

    for (;;)
        __asm__ volatile ("hlt");
}

void sched_init(void)
{
    memcpy(main_thread.name, "main", 5);
    main_thread.id = 0;
    main_thread.state = THREAD_RUNNING;
    list_add(&main_thread);
    current = &main_thread;

    idle_thread = thread_alloc("idle", idle_main, NULL);
    if (!idle_thread) {
        kprintf("sched: cannot allocate idle thread\n");
        for (;;)
            __asm__ volatile ("cli; hlt");
    }
    idle_thread->id = next_id++;

    pit_set_hook(sched_tick);
}

void sched_list(void)
{
    static const char *const names[] = { "ready", "running", "sleeping", "blocked", "dead" };
    uint32_t flags = irq_save();
    thread_t *t = list_head;

    do {
        kprintf("  %u %s %s\n", t->id, t->name, names[t->state]);
        t = t->next;
    } while (t != list_head);

    kprintf("  %u %s %s\n", idle_thread->id, idle_thread->name, names[idle_thread->state]);
    irq_restore(flags);
}