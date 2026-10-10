#include <vga.h>
#include <kprintf.h>
#include <serial.h>
#include <gdt.h>
#include <idt.h>
#include <irq.h>
#include <pic.h>

static volatile unsigned ticks;

static void timer_callback(registers_t *regs)
{
    (void)regs;
    ticks++;
    kprintf(".");
}

void kernel_main(void)
{
    vga_init();
    serial_init();
    gdt_init();
    idt_init();
    pic_init();

    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("Hello from Chessie!\n");

    irq_register_handler(IRQ_TIMER, timer_callback);
    pic_clear_mask(IRQ_TIMER);

    __asm__ volatile ("sti");

    for (;;)
        __asm__ volatile ("hlt");
}