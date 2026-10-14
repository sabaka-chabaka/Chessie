#include <stdint.h>

#include <idt.h>
#include <kprintf.h>
#include <sched.h>
#include <thread.h>

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

static void page_fault_info(const registers_t *regs)
{
    uint32_t cr2;

    __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));   /* адрес, на котором упали */

    kprintf("page fault at %p: %s, %s, %s mode\n",
            (void *)cr2,
            (regs->err_code & 1) ? "protection violation" : "page not present",
            (regs->err_code & 2) ? "write" : "read",
            (regs->err_code & 4) ? "user" : "kernel");
}

void isr_handler(registers_t *regs)
{
    const char *name = regs->int_no < 32 ? names[regs->int_no] : "?";

    if ((regs->cs & 3) == 3) {
        thread_t *t = sched_current();

        kprintf("\nEXCEPTION %u (%s) in user mode, thread %u '%s'\n",
                regs->int_no, name, t->id, t->name);
        if (regs->int_no == 14)
            page_fault_info(regs);
        kprintf("err=%x eip=%x esp=%x cs=%x\n",
                regs->err_code, regs->eip, regs->useresp, regs->cs);
        kprintf("thread killed\n");
        thread_exit();
    }

    kprintf("\nEXCEPTION %u: %s\n", regs->int_no, name);

    if (regs->int_no == 14)
        page_fault_info(regs);

    kprintf("err=%x eip=%x cs=%x eflags=%x\n",
            regs->err_code, regs->eip, regs->cs, regs->eflags);
    kprintf("eax=%x ebx=%x ecx=%x edx=%x\n",
            regs->eax, regs->ebx, regs->ecx, regs->edx);

    for (;;)
        __asm__ volatile ("cli; hlt");
}