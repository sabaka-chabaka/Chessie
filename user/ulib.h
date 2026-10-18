#pragma once

#include <stddef.h>
#include <stdint.h>

#define SYS_write  1
#define SYS_exit   2
#define SYS_ticks  3
#define SYS_getpid 4
#define SYS_spawn  5
#define SYS_wait   6

#define SPAWN_DETACHED 1

#define E_NOENT (-1)
#define E_INVAL (-6)
#define E_NOMEM (-30)
#define E_2BIG  (-31)
#define E_FAULT (-32)

static inline int32_t syscall3(uint32_t n, uint32_t a, uint32_t b, uint32_t c)
{
    int32_t ret;

    __asm__ volatile ("int $0x80" : "=a"(ret) : "0"(n), "b"(a), "c"(b), "d"(c) : "memory");
    return ret;
}

static inline int32_t sys_write(int fd, const void *buf, uint32_t len)
{
    return syscall3(SYS_write, (uint32_t)fd, (uint32_t)(uintptr_t)buf, len);
}

static inline void sys_exit(int code) __attribute__((noreturn));
static inline void sys_exit(int code)
{
    syscall3(SYS_exit, (uint32_t)code, 0, 0);
    for (;;)
        ;
}

static inline uint32_t sys_ticks(void)
{
    return (uint32_t)syscall3(SYS_ticks, 0, 0, 0);
}

static inline int sys_getpid(void)
{
    return syscall3(SYS_getpid, 0, 0, 0);
}

static inline int sys_spawn(const char *path, char *const *argv, uint32_t flags)
{
    return syscall3(SYS_spawn, (uint32_t)(uintptr_t)path, (uint32_t)(uintptr_t)argv, flags);
}

static inline int sys_wait(int pid, int32_t *status)
{
    return syscall3(SYS_wait, (uint32_t)pid, (uint32_t)(uintptr_t)status, 0);
}

static inline uint32_t u_strlen(const char *s)
{
    uint32_t n = 0;

    while (s[n])
        n++;
    return n;
}

static inline void u_puts(const char *s)
{
    sys_write(1, s, u_strlen(s));
}

static inline void u_putu(uint32_t v)
{
    char buf[12];
    int i = sizeof(buf);

    do {
        buf[--i] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    sys_write(1, buf + i, (uint32_t)(sizeof(buf) - i));
}

static inline void u_puti(int v)
{
    if (v < 0) {
        u_puts("-");
        u_putu(0u - (uint32_t)v);
    } else {
        u_putu((uint32_t)v);
    }
}

static inline int u_streq(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static inline int u_endswith(const char *s, const char *suffix)
{
    uint32_t n = u_strlen(s), m = u_strlen(suffix);

    return n >= m && u_streq(s + n - m, suffix);
}

static inline int u_atoi(const char *s)
{
    int neg = 0, v = 0;

    if (*s == '-') {
        neg = 1;
        s++;
    }
    while (*s >= '0' && *s <= '9')
        v = v * 10 + (*s++ - '0');
    return neg ? -v : v;
}