#include <stddef.h>
#include <stdint.h>

#include <io.h>
#include <keyboard.h>
#include <kprintf.h>
#include <pit.h>
#include <shell.h>
#include <string.h>
#include <vga.h>
#include <pmm.h>
#include <heap.h>
#include <vmm.h>
#include <sched.h>
#include <sync.h>
#include <thread.h>

extern char kernel_start[];
extern char kernel_end[];

#define LINE_MAX 128
#define ARGV_MAX 16

typedef int (*cmd_fn)(int argc, char **argv);

struct command {
    const char *name;
    const char *help;
    cmd_fn fn;
};

static int cmd_help(int argc, char **argv);
static int cmd_clear(int argc, char **argv);
static int cmd_uptime(int argc, char **argv);
static int cmd_echo(int argc, char **argv);
static int cmd_reboot(int argc, char **argv);
static int cmd_mem(int argc, char **argv);
static int cmd_memtest(int argc, char **argv);
static int cmd_vmtest(int argc, char **argv);
static int cmd_nullptr(int argc, char **argv);
static int cmd_heapinfo(int argc, char **argv);
static int cmd_heaptest(int argc, char **argv);
static int cmd_ps(int argc, char **argv);
static int cmd_threads(int argc, char **argv);
static int cmd_sleeptest(int argc, char **argv);
static int cmd_racetest(int argc, char **argv);

static const struct command commands[] = {
    { "help",   "list commands",        cmd_help   },
    { "clear",  "clear the screen",     cmd_clear  },
    { "uptime", "time since boot",      cmd_uptime },
    { "echo",   "print arguments",      cmd_echo   },
    { "reboot", "restart the machine",  cmd_reboot },
    { "mem",     "physical memory usage",     cmd_mem     },
    { "memtest", "stress the page allocator", cmd_memtest },
    { "vmtest",   "test page mapping",           cmd_vmtest   },
    { "nullptr",  "read address 0 (page fault)", cmd_nullptr  },
    { "heapinfo", "kernel heap statistics",      cmd_heapinfo },
    { "heaptest", "stress the kernel heap",      cmd_heaptest },
    { "ps",        "list threads",                         cmd_ps        },
    { "threads",   "3 threads, preemption demo",           cmd_threads   },
    { "sleeptest", "3 threads with different sleeps",      cmd_sleeptest },
    { "racetest",  "racetest [irq|mutex]: data race demo", cmd_racetest  },
};

#define NCMDS (sizeof(commands) / sizeof(commands[0]))

static int cmd_help(int argc, char **argv) {
    (void)argc;
    (void)argv;

    for (size_t i = 0; i < NCMDS; i++) {
        kprintf("  %s - %s\n", commands[i].name, commands[i].help);
    }
    return 0;
}

static int cmd_clear(int argc, char **argv) {
    (void)argc;
    (void)argv;
    vga_clear();
    return 0;
}

static int cmd_uptime(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    uint32_t t = timer_ticks();
    kprintf("%u\n", t / TIMER_HZ);
    return 0;
}

static int cmd_echo(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (i > 1)
            kprintf(" ");
        kprintf("%s", argv[i]);
    }
    kprintf("\n");
    return 0;
}

static int cmd_reboot(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    kprintf("rebooting...\n");
    __asm__ volatile ("cli");

    while (inb(0x64) & 0x02)
        ;
    outb(0x64, 0xFE);

    for (;;)
        __asm__ volatile ("hlt");
}

static int cmd_mem(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    uint32_t total = pmm_total_pages();
    uint32_t free  = pmm_free_pages();

    kprintf("total: %u pages (%u KiB)\n", total, total * 4);
    kprintf("free:  %u pages (%u KiB)\n", free,  free * 4);
    kprintf("used:  %u pages (%u KiB)\n", total - free, (total - free) * 4);
    return 0;
}

static int cmd_memtest(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    enum { N = 1000 };
    static uint32_t pages[N];
    uint32_t before = pmm_free_pages();
    int got = 0;
    int bad = 0;

    for (; got < N; got++) {
        pages[got] = pmm_alloc_page();
        if (!pages[got])
            break;
    }

    for (int i = 0; i < got; i++) {
        if (pages[i] & (PAGE_SIZE - 1))
            bad++;
        if (pages[i] < 0x100000)
            bad++;
        if (pages[i] >= (uint32_t)kernel_start && pages[i] < (uint32_t)kernel_end)
            bad++;
        for (int j = 0; j < i; j++)
            if (pages[i] == pages[j])
                bad++;
    }

    kprintf("allocated %d, first %p, last %p, errors %d\n",
            got, (void *)pages[0], (void *)pages[got - 1], bad);

    for (int i = 0; i < got; i++)
        pmm_free_page(pages[i]);

    kprintf("free before %u, after %u: %s\n", before, pmm_free_pages(),
            before == pmm_free_pages() ? "ok" : "LEAK");
    return 0;
}

static int cmd_vmtest(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    uint32_t virt = 0xE0000000u;
    uint32_t phys = pmm_alloc_page();

    if (!phys) {
        kprintf("out of memory\n");
        return 1;
    }
    if (vmm_map_page(virt, phys, VMM_WRITE) != 0) {
        kprintf("map failed\n");
        pmm_free_page(phys);
        return 1;
    }

    volatile uint32_t *v = (volatile uint32_t *)virt;
    volatile uint32_t *p = (volatile uint32_t *)phys;   /* та же страница через identity */

    kprintf("virt %p -> phys %p (lookup %p)\n",
            (void *)virt, (void *)phys, (void *)vmm_get_phys(virt));

    *v = 0xDEADBEEF;
    kprintf("write via virt, read via phys: %x %s\n", *p, *p == 0xDEADBEEF ? "ok" : "FAIL");

    *p = 0xCAFEBABE;
    kprintf("write via phys, read via virt: %x %s\n", *v, *v == 0xCAFEBABE ? "ok" : "FAIL");

    vmm_unmap_page(virt);
    kprintf("after unmap, lookup: %p %s\n",
            (void *)vmm_get_phys(virt), vmm_get_phys(virt) == 0 ? "ok" : "FAIL");

    pmm_free_page(phys);
    return 0;
}

static int cmd_nullptr(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    volatile uint32_t *p = (volatile uint32_t *)0;

    kprintf("%x\n", *p);
    return 0;
}

static int cmd_heapinfo(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    heap_stats_t s;

    heap_stats(&s);
    kprintf("mapped: %u KiB, used: %u B, free: %u B\n", s.mapped >> 10, s.used, s.free);
    kprintf("blocks: %u, headers: %u B, errors: %u\n",
            s.blocks, s.mapped - s.used - s.free, s.errors);
    return 0;
}

static int cmd_ps(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    sched_list();
    return 0;
}

static void spinner(void *arg)
{
    char c = (char)(uintptr_t)arg;

    for (int i = 0; i < 60; i++) {
        kprintf("%c", c);

        uint32_t t0 = timer_ticks();
        while (timer_ticks() - t0 < 2)
            ;
    }
}

static int cmd_threads(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    for (char c = 'A'; c <= 'C'; c++)
        thread_create("spinner", spinner, (void *)(uintptr_t)c);
    return 0;
}

struct sleeper {
    const char *name;
    uint32_t    period;
};

static const struct sleeper sleepers[] = {
    { "fast", 300 }, { "mid", 500 }, { "slow", 700 },
};

static void sleeper_main(void *arg)
{
    const struct sleeper *s = arg;

    for (int i = 1; i <= 5; i++) {
        thread_sleep_ms(s->period);
        kprintf("[%s] wake %d at tick %u\n", s->name, i, timer_ticks());
    }
}

static int cmd_sleeptest(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    for (size_t i = 0; i < sizeof(sleepers) / sizeof(sleepers[0]); i++)
        thread_create(sleepers[i].name, sleeper_main, (void *)&sleepers[i]);
    return 0;
}

#define RACE_ITER 5000000u

static volatile uint32_t shared;
static volatile uint32_t finished;
static int race_mode;
static mutex_t race_lock;

static void race_worker(void *arg)
{
    (void)arg;

    for (uint32_t i = 0; i < RACE_ITER; i++) {
        uint32_t f = 0;

        if (race_mode == 1)
            f = irq_save();
        else if (race_mode == 2)
            mutex_lock(&race_lock);

        uint32_t tmp = shared;
        tmp++;
        shared = tmp;

        if (race_mode == 1)
            irq_restore(f);
        else if (race_mode == 2)
            mutex_unlock(&race_lock);
    }

    uint32_t f = irq_save();

    finished++;
    irq_restore(f);
}

static int cmd_racetest(int argc, char **argv)
{
    static const char *const mode_names[] = { "no protection", "irq_save", "mutex" };

    race_mode = 0;
    if (argc > 1) {
        if (strcmp(argv[1], "irq") == 0)
            race_mode = 1;
        else if (strcmp(argv[1], "mutex") == 0)
            race_mode = 2;
    }

    shared = 0;
    finished = 0;
    mutex_init(&race_lock);

    uint32_t t0 = timer_ticks();

    thread_create("race1", race_worker, NULL);
    thread_create("race2", race_worker, NULL);

    while (finished < 2)
        thread_sleep_ms(10);

    uint32_t expected = 2 * RACE_ITER;

    kprintf("%s: counter %u, expected %u, lost %u, %u ms\n",
            mode_names[race_mode], shared, expected, expected - shared,
            (timer_ticks() - t0) * 1000 / TIMER_HZ);
    return 0;
}

static uint32_t seed = 12345;

static uint32_t rnd(uint32_t n)
{
    seed = seed * 1103515245u + 12345u;
    return (seed >> 16) % n;
}

static int free_checked(void *p, uint32_t len, uint8_t pat)
{
    uint8_t *b = p;
    int bad = 0;

    for (uint32_t k = 0; k < len; k++) {
        if (b[k] != pat) {
            bad = 1;
            break;
        }
    }
    kfree(p);
    return bad;
}

static int cmd_heaptest(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    enum { N = 2000 };
    static void    *ptr[N];
    static uint32_t len[N];
    static uint8_t  pat[N];
    static int      order[N];
    int bad = 0;

    for (int i = 0; i < N; i++) {
        len[i] = 1 + rnd(1000);
        pat[i] = (uint8_t)(i * 7 + 1);
        ptr[i] = kmalloc(len[i]);
        if (!ptr[i] || ((uint32_t)ptr[i] & 15)) {
            bad++;
            ptr[i] = 0;
            continue;
        }
        memset(ptr[i], pat[i], len[i]);
    }

    for (int i = 0; i < N; i += 2) {
        if (ptr[i])
            bad += free_checked(ptr[i], len[i], pat[i]);

        len[i] = 1 + rnd(1000);
        pat[i] = (uint8_t)(i * 13 + 5);
        ptr[i] = kmalloc(len[i]);
        if (!ptr[i]) {
            bad++;
            continue;
        }
        memset(ptr[i], pat[i], len[i]);
    }

    for (int i = 0; i < N; i++)
        order[i] = i;
    for (int i = N - 1; i > 0; i--) {
        int j = (int)rnd((uint32_t)i + 1);
        int t = order[i];

        order[i] = order[j];
        order[j] = t;
    }
    for (int k = 0; k < N; k++) {
        int i = order[k];

        if (ptr[i])
            bad += free_checked(ptr[i], len[i], pat[i]);
    }

    heap_stats_t s;

    heap_stats(&s);
    kprintf("heap grew to %u KiB, corrupted blocks: %d\n", s.mapped >> 10, bad);
    kprintf("after free: used %u, blocks %u, errors %u -> %s\n",
            s.used, s.blocks, s.errors,
            (!bad && s.used == 0 && s.blocks == 1 && !s.errors) ? "ok" : "FAIL");
    return 0;
}

static void readline(char *buf, size_t max)
{
    size_t len = 0;

    for (;;) {
        char c = keyboard_getchar();

        if (c == '\n') {
            kprintf("\n");
            buf[len] = '\0';
            return;
        }

        if (c == '\b') {
            if (len > 0) {
                len--;
                kprintf("\b \b");
            }
            continue;
        }

        if (c >= 32 && c < 127 && len < max - 1) {
            buf[len++] = c;
            kprintf("%c", c);
        }
    }
}

static int tokenize(char *line, char **argv)
{
    int argc = 0;

    while (*line) {
        while (*line == ' ')
            *line++ = '\0';
        if (!*line)
            break;
        if (argc == ARGV_MAX - 1)
            break;

        argv[argc++] = line;
        while (*line && *line != ' ')
            line++;
    }

    argv[argc] = 0;
    return argc;
}

void shell_run(void) {
    char line[LINE_MAX];
    char *argv[ARGV_MAX];

    kprintf("Type 'help' for commands.\n");

    for (;;) {
        kprintf("chessie> ");
        readline(line, sizeof(line));

        int argc = tokenize(line, argv);
        if (argc == 0)
            continue;

        int found = 0;

        for (size_t i = 0; i < NCMDS; i++) {
            if (strcmp(argv[0], commands[i].name) == 0) {
                commands[i].fn(argc, argv);
                found = 1;
                break;
            }
        }

        if (!found)
            kprintf("unknown command %s\n", argv[0]);
    }
}