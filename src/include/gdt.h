#pragma once

#include <stdint.h>

#define GDT_ENTRIES 6

#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_CODE   0x18
#define GDT_USER_DATA   0x20
#define GDT_TSS         0x28

#define GDT_ACCESS_KERNEL_CODE 0x9A
#define GDT_ACCESS_KERNEL_DATA 0x92
#define GDT_ACCESS_USER_CODE   0xFA
#define GDT_ACCESS_USER_DATA   0xF2
#define GDT_ACCESS_TSS         0x89

#define GDT_FLAGS_32BIT_PAGE   0xC

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  limit_high_flags;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

void gdt_init(void);

extern void gdt_flush(uint32_t gdt_ptr_addr);