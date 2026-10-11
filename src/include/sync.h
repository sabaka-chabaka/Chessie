#pragma once

#include <stdint.h>

static inline uint32_t irq_save(void)
{
    uint32_t flags;

    __asm__ volatile ("pushf\n\tpop %0\n\tcli" : "=r"(flags) : : "memory");
    return flags;
}

static inline void irq_restore(uint32_t flags)
{
    __asm__ volatile ("push %0\n\tpopf" : : "r"(flags) : "memory", "cc");
}

struct thread;

typedef struct mutex {
    struct thread *owner;
    struct thread *wait_head;
    struct thread *wait_tail;
} mutex_t;

void mutex_init(mutex_t *m);
void mutex_lock(mutex_t *m);
void mutex_unlock(mutex_t *m);