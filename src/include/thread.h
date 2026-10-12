#pragma once

#include <stdint.h>
#include <stddef.h>

#define THREAD_STACK_SIZE 8192
#define THREAD_STACK_CANARY 0xDEADC0DEu
#define THREAD_NAME_MAX 16

struct aspace;

typedef enum {
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_SLEEPING,
    THREAD_BLOCKED,
    THREAD_DEAD,
} thread_state_t;

typedef void (*thread_fn_t)(void *arg);

typedef struct thread {
    uint32_t       esp;
    uint32_t       id;
    thread_state_t state;
    char           name[THREAD_NAME_MAX];
    uint8_t       *kstack;
    thread_fn_t    entry;
    void          *arg;
    uint32_t       wake_tick;
    struct aspace *aspace;
    struct thread *next;
    struct thread *wait_next;
} thread_t;

thread_t *thread_create(const char *name, thread_fn_t entry, void *arg);

void thread_exit(void) __attribute__((noreturn));

thread_t *thread_alloc(const char *name, thread_fn_t entry, void *arg);
void      thread_free(thread_t *t);