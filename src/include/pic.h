#pragma once

#include <stdint.h>

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

#define PIC_EOI      0x20

#define ICW1_ICW4    0x01
#define ICW1_INIT    0x10
#define ICW4_8086    0x01

#define IRQ_BASE     32
#define IRQ_BASE2    40

#define IRQ_TIMER    0
#define IRQ_KEYBOARD 1
#define IRQ_CASCADE  2
#define IRQ_RTC      8

void pic_init(void);
void pic_send_eoi(uint8_t irq);
void pic_set_mask(uint8_t irq);
void pic_clear_mask(uint8_t irq);