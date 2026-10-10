#include <stdint.h>

#include <io.h>
#include <pic.h>

void pic_init(void)
{
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4); io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4); io_wait();

    outb(PIC1_DATA, IRQ_BASE);  io_wait();
    outb(PIC2_DATA, IRQ_BASE2); io_wait();

    outb(PIC1_DATA, 0x04); io_wait();
    outb(PIC2_DATA, 0x02); io_wait();

    outb(PIC1_DATA, ICW4_8086); io_wait();
    outb(PIC2_DATA, ICW4_8086); io_wait();

    outb(PIC1_DATA, 0xFB);
    outb(PIC2_DATA, 0xFF);
}

void pic_send_eoi(uint8_t irq)
{
    if (irq >= 8)
        outb(PIC2_COMMAND, PIC_EOI);
    outb(PIC1_COMMAND, PIC_EOI);
}

void pic_set_mask(uint8_t irq)
{
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    if (irq >= 8)
        irq -= 8;
    outb(port, inb(port) | (1 << irq));
}

void pic_clear_mask(uint8_t irq)
{
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    if (irq >= 8)
        irq -= 8;
    outb(port, inb(port) & ~(1 << irq));
}