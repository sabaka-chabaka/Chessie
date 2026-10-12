#pragma once

#include <stdint.h>

#include <idt.h>

#define SYS_write 1
#define SYS_exit  2
#define SYS_ticks 3

#define SYSCALL_ERR ((uint32_t)-1)

void syscall_handler(registers_t *regs);

extern void syscall_entry(void);