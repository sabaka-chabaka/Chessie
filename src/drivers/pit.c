#include <stdint.h>

#include <io.h>
#include <irq.h>
#include <pic.h>
#include <pit.h>

static volatile uint32_t ticks;
static uint32_t freq_hz = TIMER_HZ;

static void pit_callback(registers_t *regs)
{
    (void)regs;
    ticks++;
}

void pit_init(uint32_t hz)
{
    uint32_t divisor = PIT_BASE_FREQ / hz;

    if (divisor < 1)
        divisor = 1;
    if (divisor > 0xFFFF)
        divisor = 0xFFFF;

    freq_hz = PIT_BASE_FREQ / divisor;

    outb(PIT_COMMAND, PIT_CMD_CH0_LOHI_MODE3);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));

    irq_register_handler(IRQ_TIMER, pit_callback);
    pic_clear_mask(IRQ_TIMER);
}

uint32_t timer_ticks(void)
{
    return ticks;
}

void sleep_ms(uint32_t ms)
{
    uint32_t need  = (ms * freq_hz + 999) / 1000;
    uint32_t start = ticks;

    while ((uint32_t)(ticks - start) < need)
        __asm__ volatile ("hlt");
}