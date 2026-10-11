#pragma once

#include <stddef.h>
#include <stdint.h>

#define KHEAP_START 0xC0000000u
#define KHEAP_MAX   0xD0000000u

typedef struct heap_stats {
    uint32_t mapped;
    uint32_t used;
    uint32_t free;
    uint32_t blocks;
    uint32_t errors;
} heap_stats_t;

void  heap_init(void);
void *kmalloc(size_t size);
void *kcalloc(size_t n, size_t size);
void  kfree(void *ptr);
void  heap_stats(heap_stats_t *s);