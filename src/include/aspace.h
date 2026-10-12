#pragma once

#include <stdint.h>

typedef struct aspace {
    uint32_t *pd;
} aspace_t;

aspace_t *aspace_kernel(void);
aspace_t *aspace_current(void);

aspace_t *aspace_create(void);

void aspace_destroy(aspace_t *as);

void aspace_switch(aspace_t *as);

int aspace_map(aspace_t *as, uint32_t virt, uint32_t phys, uint32_t flags);

int aspace_map_new(aspace_t *as, uint32_t virt, uint32_t flags);

uint32_t aspace_phys(aspace_t *as, uint32_t virt);
int aspace_user_ok(aspace_t *as, uint32_t ptr, uint32_t len, int write);