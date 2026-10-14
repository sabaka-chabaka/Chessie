#pragma once

#include <stdint.h>

#include <multiboot.h>

int initrd_init(const multiboot_info_t *mbi);

int initrd_range(uint32_t *start, uint32_t *end);