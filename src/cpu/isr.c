#include <../include/idt.h>

#include "../include/kprintf.h"

static const char *names[32] = {
    "Divide by zero", "Debug", "NMI", "Breakpoint", "Overflow",
    "Bound range exceeded", "Invalid opcode", "Device not available",
    "Double fault", "Coprocessor segment overrun", "Invalid TSS",
    "Segment not present", "Stack-segment fault", "General protection fault",
    "Page fault", "Reserved", "x87 FPU error", "Alignment check",
    "Machine check", "SIMD FP exception", "Virtualization", "Control protection",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Hypervisor injection", "VMM communication", "Security", "Reserved"
};

void isr_handler(registers_t *regs)
{
    kprintf("\nEXCEPTION %u: %s\n", regs->int_no, names[regs->int_no]);
    kprintf("err=%x eip=%x cs=%x eflags=%x\n",
            regs->err_code, regs->eip, regs->cs, regs->eflags);
    kprintf("eax=%x ebx=%x ecx=%x edx=%x\n",
            regs->eax, regs->ebx, regs->ecx, regs->edx);

    for (;;)
        __asm__ volatile ("cli; hlt");
}