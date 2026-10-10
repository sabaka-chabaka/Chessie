#pragma once

#include <stdint.h>

#include <idt.h>

typedef void (*irq_handler_t)(registers_t *regs);

void irq_register_handler(uint8_t irq, irq_handler_t handler);
void irq_handler(registers_t *regs);

extern uint32_t irq_stub_table[16];