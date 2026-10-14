#include <stddef.h>
#include <stdint.h>

#include <aspace.h>
#include <kprintf.h>
#include <pmm.h>
#include <sched.h>
#include <string.h>
#include <thread.h>
#include <usertest.h>
#include <vmm.h>

#define USER_CODE_VA   VMM_USER_BASE
#define USER_STACK_TOP VMM_USER_END

extern char user_blob_start[];
extern char user_blob_end[];
extern void enter_usermode(uint32_t entry, uint32_t user_esp) __attribute__((noreturn));

static void user_thread(void *arg)
{
    uint32_t mode = (uint32_t)(uintptr_t)arg;
    uint32_t size = (uint32_t)(user_blob_end - user_blob_start);
    thread_t *self = sched_current();

    if (size > PAGE_SIZE) {
        kprintf("usertest: blob does not fit one page\n");
        return;
    }

    aspace_t *as = aspace_create();

    if (!as) {
        kprintf("usertest: out of memory\n");
        return;
    }

    if (aspace_map_new(as, USER_CODE_VA, VMM_USER) != 0 ||
        aspace_map_new(as, USER_STACK_TOP - PAGE_SIZE, VMM_USER | VMM_WRITE) != 0) {
        kprintf("usertest: out of memory\n");
        aspace_destroy(as);
        return;
        }

    memcpy((void *)aspace_phys(as, USER_CODE_VA), user_blob_start, size);
    *(uint32_t *)(aspace_phys(as, USER_STACK_TOP - PAGE_SIZE) + PAGE_SIZE - 4) = mode;

    self->aspace = as;
    aspace_switch(as);

    kprintf("[user thread %u] entering ring 3, mode %u, entry %p\n",
            self->id, mode, (void *)USER_CODE_VA);

    enter_usermode(USER_CODE_VA, USER_STACK_TOP - 4);
}

thread_t *usertest_start(uint32_t mode)
{
    return thread_create("user", user_thread, (void *)(uintptr_t)mode);
}