#include <stdint.h>

#include <gdt.h>
#include <idt.h>
#include <irq.h>
#include <pic.h>
#include <syscall.h>

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr   idtp;

void idt_set_gate(uint8_t vector, uint32_t handler, uint16_t selector, uint8_t flags)
{
    idt[vector].base_low  = handler & 0xFFFF;
    idt[vector].base_high = (handler >> 16) & 0xFFFF;
    idt[vector].selector  = selector;
    idt[vector].zero      = 0;
    idt[vector].flags     = flags;
}

void idt_init(void)
{
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint32_t)&idt;

    for (int i = 0; i < 32; i++)
        idt_set_gate(i, isr_stub_table[i], GDT_KERNEL_CODE, IDT_GATE_KERNEL);

    for (int i = 0; i < 16; i++)
        idt_set_gate(IRQ_BASE + i, irq_stub_table[i], GDT_KERNEL_CODE, IDT_GATE_KERNEL);

    idt_set_gate(0x80, (uint32_t)syscall_entry, GDT_KERNEL_CODE, IDT_GATE_USER);

    __asm__ volatile ("lidt %0" : : "m"(idtp));
}