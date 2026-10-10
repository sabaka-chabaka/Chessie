#include <stdint.h>

#include <io.h>
#include <irq.h>
#include <pic.h>

#define PIC_READ_ISR 0x0B

static irq_handler_t handlers[16];

void irq_register_handler(uint8_t irq, irq_handler_t handler)
{
    if (irq < 16)
        handlers[irq] = handler;
}

static int is_spurious(uint8_t irq)
{
    uint16_t cmd = (irq == 7) ? PIC1_COMMAND : PIC2_COMMAND;

    outb(cmd, PIC_READ_ISR);
    return !(inb(cmd) & 0x80);
}

void irq_handler(registers_t *regs)
{
    uint8_t irq = (uint8_t)(regs->int_no - IRQ_BASE);

    if ((irq == 7 || irq == 15) && is_spurious(irq)) {
        if (irq == 15)
            outb(PIC1_COMMAND, PIC_EOI);
        return;
    }

    if (handlers[irq])
        handlers[irq](regs);

    pic_send_eoi(irq);
}