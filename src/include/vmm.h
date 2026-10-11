#pragma once

#include <stdint.h>

#define VMM_PRESENT 0x1
#define VMM_WRITE   0x2
#define VMM_USER    0x4

#define VMM_IDENTITY_LIMIT 0xC0000000u

void vmm_init(void);

int vmm_map_page(uint32_t virt, uint32_t phys, uint32_t flags);

void vmm_unmap_page(uint32_t virt);

uint32_t vmm_get_phys(uint32_t virt);