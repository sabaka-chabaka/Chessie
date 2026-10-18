#pragma once

#include <stdint.h>

#include "aspace.h"
#include "thread.h"
#include "vmm.h"

#define PROC_NAME_MAX       32
#define PROC_MAX_ARGS       16
#define PROC_ARGS_MAX_BYTES 2048
#define PROC_STACK_PAGES    4
#define PROC_STACK_TOP      VMM_USER_END

#define PROC_SPAWN_DETACHED 1

#define PROC_FAULT_BASE 128

#define PROC_ENOMEM (-30)
#define PROC_E2BIG  (-31)
#define PROC_EFAULT (-32)

typedef enum { PROC_ALIVE, PROC_ZOMBIE } proc_state_t;

typedef struct process {
    uint32_t         pid;
    char             name[PROC_NAME_MAX];
    proc_state_t     state;
    int32_t          exit_code;
    struct process  *parent;
    struct process  *next;
    thread_t        *thread;
    thread_t        *waiter;
    uint32_t         entry;
    uint32_t         user_esp;
    uint8_t          detached;
} process_t;

int  proc_spawn(const char *path, const char *const *argv, uint32_t flags);

int  proc_wait(int pid, int32_t *status);

void proc_exit(int32_t code) __attribute__((noreturn));

process_t *proc_current(void);
void       proc_list(void);
uint32_t   proc_count(void);

int proc_build_stack(aspace_t *as, const char *const *argv, uint32_t *esp);

const char *proc_strerror(int err);