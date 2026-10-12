#pragma once

#include <stdint.h>

#define VMM_PRESENT 0x1
#define VMM_WRITE   0x2
#define VMM_USER    0x4

#define VMM_USER_BASE 0x40000000u
#define VMM_USER_END  0xC0000000u

#define VMM_IDENTITY_LIMIT VMM_USER_BASE

void vmm_init(void);
uint32_t *vmm_kernel_pd(void);

int vmm_map_page(uint32_t virt, uint32_t phys, uint32_t flags);

void vmm_unmap_page(uint32_t virt);

uint32_t vmm_get_phys(uint32_t virt);

int      vmm_map_in(uint32_t *pd, uint32_t virt, uint32_t phys, uint32_t flags);
uint32_t vmm_unmap_in(uint32_t *pd, uint32_t virt);
uint32_t vmm_get_phys_in(uint32_t *pd, uint32_t virt);

int vmm_user_ok(uint32_t *pd, uint32_t ptr, uint32_t len, int write);