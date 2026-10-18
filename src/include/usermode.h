#pragma once

#include <stdint.h>

void enter_usermode(uint32_t entry, uint32_t user_esp) __attribute__((noreturn));