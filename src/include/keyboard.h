#pragma once

#include <stdint.h>

#define KBD_DATA 0x60
#define KBD_STATUS 0x64

#define KBD_BUFFER_SIZE 256

void keyboard_init(void);
int keyboard_trygetchar(void);
char keyboard_getchar(void);