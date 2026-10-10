#pragma once

#include <stdint.h>

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43

#define PIT_CMD_CH0_LOHI_MODE3 0x36

#define PIT_BASE_FREQ 1193182

#define TIMER_HZ 100

void pit_init(uint32_t hz);
uint32_t timer_ticks(void);
void sleep_ms(uint32_t ms);