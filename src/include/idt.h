#pragma once

#include <stdint.h>

#define IDT_ENTRIES 256

#define IDT_GATE_KERNEL 0x8E

struct idt_entry {
    uint16_t base_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  flags;
    uint16_t base_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

typedef struct registers {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags;
} registers_t;

void idt_init(void);
void idt_set_gate(uint8_t vector, uint32_t handler, uint16_t selector, uint8_t flags);

void isr_handler(registers_t *regs);

extern uint32_t isr_stub_table[32];