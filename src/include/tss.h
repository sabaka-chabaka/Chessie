#pragma once

#include <stdint.h>

void tss_init(void);

void     tss_set_kernel_stack(uint32_t esp0);
uint32_t tss_get_kernel_stack(void);