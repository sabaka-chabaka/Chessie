#pragma once

#include <stdint.h>

#include <multiboot.h>

#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

void pmm_init(const multiboot_info_t *mbi);

uint32_t pmm_alloc_page(void);

void pmm_free_page(uint32_t addr);

uint32_t pmm_total_pages(void);
uint32_t pmm_free_pages(void);