#pragma once

#include <stdint.h>

#include <thread.h>

#define SCHED_QUANTUM_TICKS 5

void sched_init(void);
void sched_add(thread_t *t);
void schedule(void);
void thread_yield(void);
void thread_sleep_ms(uint32_t ms);
void thread_wake(thread_t *t);
thread_t *sched_current(void);
void sched_exit(void) __attribute__((noreturn));
void sched_list(void);