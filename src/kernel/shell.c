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
#include <gdt.h>
#include <syscall.h>
#include <tss.h>
#include <aspace.h>
#include <usertest.h>
#include <initrd.h>
#include <vfs.h>

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
static int cmd_tss(int argc, char **argv);
static int cmd_syscall(int argc, char **argv);
static int cmd_usertest(int argc, char **argv);
static int cmd_aspacetest(int argc, char **argv);
static int cmd_ls(int argc, char **argv);
static int cmd_cat(int argc, char **argv);
static int cmd_vfstest(int argc, char **argv);

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
    { "tss",     "check TR, GDT and tss.esp0 per thread", cmd_tss     },
    { "syscall", "test int 0x80 from kernel mode",        cmd_syscall },
{ "usertest",   "usertest [hello|cli|kread|wcode|outb]: ring 3 demo", cmd_usertest   },
{ "aspacetest", "address space isolation and cleanup tests",          cmd_aspacetest },
{ "ls",      "ls [path]: list a directory",    cmd_ls      },
{ "cat",     "cat <path>: print a file",       cmd_cat     },
{ "vfstest", "VFS, tarfs and initrd checks",   cmd_vfstest },
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

static void tss_probe(void *arg)
{
    (void)arg;

    thread_t *t  = sched_current();
    uint32_t top = (uint32_t)t->kstack + THREAD_STACK_SIZE;
    uint32_t esp0 = tss_get_kernel_stack();

    kprintf("thread %u: tss.esp0 %p, stack top %p: %s\n",
            t->id, (void *)esp0, (void *)top, esp0 == top ? "ok" : "FAIL");
}

static int cmd_tss(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    uint16_t tr;
    struct { uint16_t limit; uint32_t base; } __attribute__((packed)) gdtr;

    __asm__ volatile ("str %0" : "=r"(tr));
    __asm__ volatile ("sgdt %0" : "=m"(gdtr));

    kprintf("TR=%x (expected %x)\n", (unsigned)tr, (unsigned)GDT_TSS);
    kprintf("GDT limit=%u (expected %u)\n", (unsigned)gdtr.limit, GDT_ENTRIES * 8 - 1);

    thread_create("tssprobe1", tss_probe, NULL);
    thread_create("tssprobe2", tss_probe, NULL);
    return 0;
}

static int regs_clobbered;
static int total_checks;
static int bad_checks;

static void check(const char *name, int cond)
{
    total_checks++;
    if (!cond)
        bad_checks++;
    kprintf("%s %s\n", cond ? "ok  " : "FAIL", name);
}

static int32_t do_sys(uint32_t n, uint32_t a, uint32_t b, uint32_t c)
{
    int32_t ret;
    uint32_t a2 = a, b2 = b, c2 = c;

    __asm__ volatile ("int $0x80"
                      : "=a"(ret), "+b"(a2), "+c"(b2), "+d"(c2)
                      : "0"(n)
                      : "memory");

    if (a2 != a || b2 != b || c2 != c)
        regs_clobbered = 1;
    return ret;
}

static void exit_probe(void *arg)
{
    (void)arg;

    kprintf("exit_probe: calling SYS_exit(42)\n");
    do_sys(SYS_exit, 42, 0, 0);
    kprintf("exit_probe: UNREACHABLE, SYS_exit returned!\n");
}

static int cmd_syscall(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    static const char msg[] = "hello via int 0x80\n";
    uint32_t len = sizeof(msg) - 1;

    total_checks = 0;
    bad_checks = 0;
    regs_clobbered = 0;

    check("write returns length",          do_sys(SYS_write, 1, (uint32_t)msg, len) == (int32_t)len);
    check("write of zero bytes returns 0", do_sys(SYS_write, 1, (uint32_t)msg, 0) == 0);
    check("bad fd -> -1",                  do_sys(SYS_write, 7, (uint32_t)msg, len) == -1);
    check("NULL buffer -> -1",             do_sys(SYS_write, 1, 0, 8) == -1);
    check("ptr + len overflow -> -1",      do_sys(SYS_write, 1, 0xFFFFFFF0u, 64) == -1);
    check("huge length -> -1",             do_sys(SYS_write, 1, (uint32_t)msg, 5000) == -1);
    check("syscall 0 -> -1",               do_sys(0, 0, 0, 0) == -1);
    check("unknown syscall -> -1",         do_sys(999, 0, 0, 0) == -1);

    uint32_t t0 = timer_ticks();
    uint32_t r  = (uint32_t)do_sys(SYS_ticks, 0, 0, 0);
    uint32_t t1 = timer_ticks();

    check("ticks between two reads", r >= t0 && r <= t1);

    uint32_t fl;

    __asm__ volatile ("pushf; pop %0" : "=r"(fl));
    check("IF restored by iret", (fl & 0x200) != 0);
    check("ebx/ecx/edx preserved", !regs_clobbered);

    if (bad_checks)
        kprintf("%d of %d checks FAILED\n", bad_checks, total_checks);
    else
        kprintf("all %d checks passed\n", total_checks);

    thread_create("exitprobe", exit_probe, NULL);
    thread_sleep_ms(200);
    kprintf("exitprobe must be gone from ps now\n");
    return 0;
}

static int cmd_usertest(int argc, char **argv)
{
    static const char *const modes[] = { "hello", "cli", "kread", "wcode", "outb" };
    const uint32_t nmodes = sizeof(modes) / sizeof(modes[0]);
    uint32_t mode = 0;

    if (argc > 1) {
        while (mode < nmodes && strcmp(argv[1], modes[mode]) != 0)
            mode++;
        if (mode == nmodes) {
            kprintf("usage: usertest [hello|cli|kread|wcode|outb]\n");
            return 1;
        }
    }

    heap_stats_t s0;
    heap_stats(&s0);
    uint32_t free0 = pmm_free_pages();

    usertest_start(mode);
    thread_sleep_ms(500);

    heap_stats_t s1;
    heap_stats(&s1);
    uint32_t free1 = pmm_free_pages();

    int32_t heap_pages = (int32_t)((s1.mapped - s0.mapped) / PAGE_SIZE);
    int32_t leaked = (int32_t)(free0 - free1) - heap_pages;

    kprintf("leaked pages: %d %s\n", leaked, leaked == 0 ? "ok" : "FAIL");
    return 0;
}

static int cmd_aspacetest(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    const uint32_t VA = VMM_USER_BASE;
    const uint32_t BIG = 6u << 20;

    total_checks = 0;
    bad_checks = 0;

    heap_stats_t h0;
    heap_stats(&h0);
    uint32_t free0 = pmm_free_pages();

    aspace_t *a = aspace_create();
    aspace_t *b = aspace_create();

    check("two address spaces created", a && b && a != b && a->pd != b->pd);
    if (!a || !b) {
        kprintf("out of memory\n");
        return 1;
    }

    check("map in a", aspace_map_new(a, VA, VMM_USER | VMM_WRITE) == 0);
    check("map in b", aspace_map_new(b, VA, VMM_USER | VMM_WRITE) == 0);

    uint32_t pa = aspace_phys(a, VA);
    uint32_t pb = aspace_phys(b, VA);

    check("different physical pages", pa && pb && pa != pb);
    check("kernel space does not see VA", vmm_get_phys(VA) == 0);

    *(volatile uint32_t *)pa = 0xAAAA1111;
    *(volatile uint32_t *)pb = 0xBBBB2222;

    volatile uint32_t *v = (volatile uint32_t *)VA;
    int a_own, b_own, wrote_a, b_intact;

    uint32_t f = irq_save();

    aspace_switch(a);
    a_own = (*v == 0xAAAA1111);
    aspace_switch(b);
    b_own = (*v == 0xBBBB2222);
    aspace_switch(a);
    *v = 0x12345678;
    wrote_a  = (*(volatile uint32_t *)pa == 0x12345678);
    b_intact = (*(volatile uint32_t *)pb == 0xBBBB2222);
    aspace_switch(aspace_kernel());
    irq_restore(f);

    check("a reads its own value at VA", a_own);
    check("b reads its own value at VA", b_own);
    check("write through VA lands in a's page", wrote_a);
    check("b's page untouched", b_intact);

    void *big = kmalloc(BIG);

    check("kmalloc 6 MiB", big != NULL);

    if (big) {
        uint32_t first = (uint32_t)big;
        uint32_t last  = first + BIG - 4;
        uint32_t kf = vmm_get_phys(first), kl = vmm_get_phys(last);

        int visible = kf && kl &&
                      aspace_phys(a, first) == kf && aspace_phys(a, last) == kl &&
                      aspace_phys(b, first) == kf && aspace_phys(b, last) == kl;

        check("grown heap is mapped in both spaces", visible);

        if (visible) {
            int shared;

            f = irq_save();
            aspace_switch(a);
            *(volatile uint32_t *)first = 0x600DF00D;
            *(volatile uint32_t *)last  = 0xFEEDFACE;
            aspace_switch(b);
            shared = *(volatile uint32_t *)first == 0x600DF00D &&
                     *(volatile uint32_t *)last  == 0xFEEDFACE;
            aspace_switch(aspace_kernel());
            irq_restore(f);

            check("heap memory is shared between spaces", shared);
        }
    }

    check("user_ok: whole rw page",             aspace_user_ok(a, VA, PAGE_SIZE, 1));
    check("user_ok: last word of page",         aspace_user_ok(a, VA + PAGE_SIZE - 4, 4, 1));
    check("user_ok: crosses into unmapped",     !aspace_user_ok(a, VA + PAGE_SIZE - 4, 8, 0));
    check("user_ok: unmapped address",          !aspace_user_ok(a, VA + 0x100000, 4, 0));
    check("user_ok: kernel address",            !aspace_user_ok(a, 0x00100000, 4, 0));
    check("user_ok: kernel heap address",       !aspace_user_ok(a, KHEAP_START, 4, 0));
    check("user_ok: address wraps around",      !aspace_user_ok(a, 0xFFFFFFF0u, 64, 0));

    check("map read-only page", aspace_map_new(a, VA + 0x2000, VMM_USER) == 0);
    check("user_ok: read-only is readable",     aspace_user_ok(a, VA + 0x2000, 4, 0));
    check("user_ok: read-only is not writable", !aspace_user_ok(a, VA + 0x2000, 4, 1));

    check("user flag outside user region is refused",
          vmm_map_page(0xE0000000u, pa, VMM_USER) == -1);

    kfree(big);
    aspace_destroy(a);
    aspace_destroy(b);

    heap_stats_t h1;
    heap_stats(&h1);

    int32_t growth = (int32_t)((h1.mapped - h0.mapped) / PAGE_SIZE);
    int32_t leaked = (int32_t)(free0 - pmm_free_pages()) - growth;

    kprintf("leaked pages after destroy: %d\n", leaked);
    check("no pages leaked", leaked == 0);

    if (bad_checks)
        kprintf("%d of %d checks FAILED\n", bad_checks, total_checks);
    else
        kprintf("all %d checks passed\n", total_checks);
    return 0;
}

static int cmd_ls(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "/";
    vfs_stat_t st;
    int r = vfs_stat(path, &st);

    if (r < 0) {
        kprintf("ls: %s: %s\n", path, vfs_strerror(r));
        return 1;
    }

    if (st.type == VFS_FILE) {
        kprintf("%u  %s\n", st.size, path);
        return 0;
    }

    vfs_dirent_t d;
    uint32_t i = 0;

    while ((r = vfs_readdir(path, i++, &d)) == 0) {
        if (d.type == VFS_DIR)
            kprintf("  %s/\n", d.name);
        else
            kprintf("  %s  %u\n", d.name, d.size);
    }
    if (r < 0)
        kprintf("ls: %s: %s\n", path, vfs_strerror(r));
    return 0;
}

static int cmd_cat(int argc, char **argv)
{
    static char buf[256];

    if (argc < 2) {
        kprintf("usage: cat <path>\n");
        return 1;
    }

    int fd = vfs_open(argv[1]);

    if (fd < 0) {
        kprintf("cat: %s: %s\n", argv[1], vfs_strerror(fd));
        return 1;
    }

    int32_t n;

    while ((n = vfs_read(fd, buf, sizeof(buf))) > 0) {
        for (int32_t i = 0; i < n; i++) {
            char c = buf[i];

            kprintf("%c", (c == '\n' || c == '\t' || (c >= 32 && c < 127)) ? c : '.');
        }
    }

    vfs_close(fd);
    return 0;
}

#define HELLO_TXT    "Hello from initrd!\n"
#define NOTES_TXT    "second file\n"
#define LEAF_TXT     "leaf\n"
#define NUMBERS_SIZE 8893u              /* "1\n" ... "2000\n" */

/* Печатает только провалы: так на экране не тонет итог */
static void vcheck(const char *name, int cond)
{
    total_checks++;
    if (!cond) {
        bad_checks++;
        kprintf("FAIL %s\n", name);
    }
}

static int read_all(const char *path, char *dst, uint32_t cap, uint32_t *got)
{
    int fd = vfs_open(path);
    uint32_t total = 0;
    int32_t n = 0;

    if (fd < 0)
        return fd;

    while (total < cap && (n = vfs_read(fd, dst + total, cap - total)) > 0)
        total += (uint32_t)n;

    vfs_close(fd);
    *got = total;
    return 0;
}

static int file_is(const char *path, const char *expect)
{
    static char b[128];
    uint32_t got, len = (uint32_t)strlen(expect);

    return read_all(path, b, sizeof(b), &got) == 0 && got == len && memcmp(b, expect, len) == 0;
}

/* Читает numbers.txt кусками по chunk байт: 2000 строк, i-я строка равна i, граница куска где угодно */
static int verify_numbers(uint32_t chunk, uint32_t *bytes)
{
    static char b[512];
    int fd = vfs_open("/numbers.txt");

    if (fd < 0)
        return 0;

    uint32_t expect = 1, val = 0, digits = 0, total = 0;
    int ok = 1;
    int32_t n;

    while ((n = vfs_read(fd, b, chunk)) > 0) {
        total += (uint32_t)n;
        for (int32_t i = 0; i < n; i++) {
            char c = b[i];

            if (c >= '0' && c <= '9') {
                val = val * 10 + (uint32_t)(c - '0');
                digits++;
            } else if (c == '\n') {
                if (!digits || val != expect)
                    ok = 0;
                expect++;
                val = 0;
                digits = 0;
            } else {
                ok = 0;
            }
        }
    }

    vfs_close(fd);
    *bytes = total;
    return ok && n == 0 && expect == 2001 && digits == 0;
}

static int dir_find(const char *dir, const char *name, vfs_dirent_t *out, int *times)
{
    vfs_dirent_t d;
    uint32_t i = 0;
    int found = 0;

    while (vfs_readdir(dir, i++, &d) == 0) {
        if (strcmp(d.name, name) == 0) {
            if (!found)
                *out = d;
            found++;
        }
    }
    *times = found;
    return found > 0;
}

static int cmd_vfstest(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    static char buf[512];
    static char whole[9000];
    static uint32_t pages[300];
    vfs_stat_t st;
    vfs_dirent_t d;
    uint32_t got, bytes;
    int times;

    total_checks = 0;
    bad_checks = 0;

    uint32_t open0 = vfs_open_files();

    /* --- нормализация путей --- */
    static const struct { const char *in, *out; } norm[] = {
        { "/", "" }, { "", "" }, { "hello.txt", "hello.txt" }, { "/hello.txt", "hello.txt" },
        { "//docs///notes.txt/", "docs/notes.txt" },
        { "/docs/./deep/../notes.txt", "docs/notes.txt" },
        { "/../../hello.txt", "hello.txt" }, { "/docs/..", "" }, { "./a/./b", "a/b" },
    };
    char out[VFS_PATH_MAX];

    for (size_t i = 0; i < sizeof(norm) / sizeof(norm[0]); i++) {
        int ok = vfs_normalize(norm[i].in, out, sizeof(out)) == 0 && strcmp(out, norm[i].out) == 0;

        vcheck(norm[i].in, ok);
    }

    memset(buf, 'a', 300);
    buf[300] = '\0';
    vcheck("normalize refuses a 300-char path", vfs_normalize(buf, out, sizeof(out)) != 0);
    vcheck("open of a too long path -> EINVAL", vfs_open(buf) == VFS_EINVAL);

    /* --- stat --- */
    vcheck("stat /",              vfs_stat("/", &st) == 0 && st.type == VFS_DIR);
    vcheck("stat hello.txt",      vfs_stat("/hello.txt", &st) == 0 && st.type == VFS_FILE &&
                                  st.size == strlen(HELLO_TXT));
    vcheck("stat empty.txt",      vfs_stat("/empty.txt", &st) == 0 && st.type == VFS_FILE && st.size == 0);
    vcheck("stat numbers.txt",    vfs_stat("/numbers.txt", &st) == 0 && st.size == NUMBERS_SIZE);
    vcheck("docs is an implied directory",   vfs_stat("/docs", &st) == 0 && st.type == VFS_DIR);
    vcheck("docs/deep is an implied directory", vfs_stat("/docs/deep", &st) == 0 && st.type == VFS_DIR);
    vcheck("missing file -> ENOENT",         vfs_stat("/nope", &st) == VFS_ENOENT);
    vcheck("file used as directory -> ENOENT", vfs_stat("/hello.txt/x", &st) == VFS_ENOENT);
    vcheck("'doc' is not a prefix match for docs", vfs_stat("/doc", &st) == VFS_ENOENT);

    /* --- содержимое --- */
    vcheck("hello.txt content",   file_is("/hello.txt", HELLO_TXT));
    vcheck("docs/notes.txt content", file_is("/docs/notes.txt", NOTES_TXT));
    vcheck("docs/deep/leaf.txt content", file_is("/docs/deep/leaf.txt", LEAF_TXT));
    vcheck("path with ./ and ../ resolves", file_is("//docs/./deep/../notes.txt", NOTES_TXT));

    int fd = vfs_open("/hello.txt");

    vcheck("open hello.txt", fd >= VFS_FIRST_FD);
    vcheck("read returns the whole file", vfs_read(fd, buf, sizeof(buf)) == (int32_t)strlen(HELLO_TXT));
    vcheck("read at EOF returns 0", vfs_read(fd, buf, sizeof(buf)) == 0);
    vcheck("read of 0 bytes returns 0", vfs_read(fd, buf, 0) == 0);
    vcheck("read into NULL -> EINVAL", vfs_read(fd, NULL, 4) == VFS_EINVAL);
    vcheck("close", vfs_close(fd) == 0);

    /* по одному байту */
    fd = vfs_open("/hello.txt");
    {
        uint32_t len = (uint32_t)strlen(HELLO_TXT), n = 0;

        while (n < sizeof(buf) && vfs_read(fd, buf + n, 1) == 1)
            n++;
        vcheck("byte-by-byte read", n == len && memcmp(buf, HELLO_TXT, len) == 0);
    }
    vfs_close(fd);

    /* пустой файл */
    fd = vfs_open("/empty.txt");
    vcheck("empty file reads 0", fd >= VFS_FIRST_FD && vfs_read(fd, buf, 16) == 0);
    vcheck("seek END in empty file", vfs_seek(fd, 0, VFS_SEEK_END) == 0);
    vfs_close(fd);

    /* --- seek --- */
    fd = vfs_open("/hello.txt");
    {
        int32_t len = (int32_t)strlen(HELLO_TXT);

        vcheck("seek SET 6",  vfs_seek(fd, 6, VFS_SEEK_SET) == 6);
        vcheck("read after seek", vfs_read(fd, buf, 4) == 4 && memcmp(buf, "from", 4) == 0);
        vcheck("seek CUR 0 reports position", vfs_seek(fd, 0, VFS_SEEK_CUR) == 10);
        vcheck("seek END -1", vfs_seek(fd, -1, VFS_SEEK_END) == len - 1);
        vcheck("last byte is newline", vfs_read(fd, buf, 1) == 1 && buf[0] == '\n');
        vcheck("seek to exact end is allowed", vfs_seek(fd, len, VFS_SEEK_SET) == len);
        vcheck("read at exact end returns 0", vfs_read(fd, buf, 4) == 0);
        vcheck("seek before start -> EINVAL", vfs_seek(fd, -1, VFS_SEEK_SET) == VFS_EINVAL);
        vcheck("seek past end -> EINVAL", vfs_seek(fd, len + 1, VFS_SEEK_SET) == VFS_EINVAL);
        vcheck("bad whence -> EINVAL", vfs_seek(fd, 0, 7) == VFS_EINVAL);
        vcheck("failed seek keeps position", vfs_seek(fd, 0, VFS_SEEK_CUR) == len);
    }
    vfs_close(fd);

    /* --- большой файл, чтение кусками разного размера --- */
    static const uint32_t chunks[] = { 1, 7, 509, 512 };

    for (size_t i = 0; i < sizeof(chunks) / sizeof(chunks[0]); i++) {
        bytes = 0;
        vcheck("numbers.txt parsed, all 2000 lines intact",
               verify_numbers(chunks[i], &bytes) && bytes == NUMBERS_SIZE);
    }

    vcheck("numbers.txt read in full",
           read_all("/numbers.txt", whole, sizeof(whole), &got) == 0 && got == NUMBERS_SIZE);
    vcheck("numbers.txt ends with 2000", memcmp(whole + NUMBERS_SIZE - 5, "2000\n", 5) == 0);

    /* seek в большом файле, включая границы страниц */
    static const uint32_t offs[] = { 0, 1, 511, 512, 4093, 4096, 8887 };

    fd = vfs_open("/numbers.txt");
    for (size_t i = 0; i < sizeof(offs) / sizeof(offs[0]); i++) {
        int ok = vfs_seek(fd, (int32_t)offs[i], VFS_SEEK_SET) == (int32_t)offs[i] &&
                 vfs_read(fd, buf, 6) == 6 && memcmp(buf, whole + offs[i], 6) == 0;

        vcheck("seek and read inside numbers.txt", ok);
    }
    vcheck("read is clamped at the end",
           vfs_seek(fd, 8890, VFS_SEEK_SET) == 8890 && vfs_read(fd, buf, 100) == 3);
    vfs_close(fd);

    /* --- каталоги --- */
    vcheck("open of a directory -> EISDIR", vfs_open("/docs") == VFS_EISDIR);
    vcheck("open of / -> EISDIR", vfs_open("/") == VFS_EISDIR);
    vcheck("readdir of a file -> ENOTDIR", vfs_readdir("/hello.txt", 0, &d) == VFS_ENOTDIR);
    vcheck("readdir of a missing path -> ENOENT", vfs_readdir("/nope", 0, &d) == VFS_ENOENT);

    vcheck("root lists docs once, as a directory",
           dir_find("/", "docs", &d, &times) && times == 1 && d.type == VFS_DIR);
    vcheck("root lists hello.txt",
           dir_find("/", "hello.txt", &d, &times) && times == 1 && d.type == VFS_FILE &&
           d.size == strlen(HELLO_TXT));
    vcheck("root lists numbers.txt",
           dir_find("/", "numbers.txt", &d, &times) && times == 1 && d.size == NUMBERS_SIZE);
    vcheck("root lists empty.txt",
           dir_find("/", "empty.txt", &d, &times) && times == 1 && d.size == 0);
    vcheck("root does not list nested names",
           !dir_find("/", "notes.txt", &d, &times) && !dir_find("/", "leaf.txt", &d, &times));

    vcheck("docs lists notes.txt",
           dir_find("/docs", "notes.txt", &d, &times) && times == 1 && d.type == VFS_FILE);
    vcheck("docs lists deep as a directory",
           dir_find("/docs", "deep", &d, &times) && times == 1 && d.type == VFS_DIR);
    vcheck("docs/deep lists leaf.txt",
           dir_find("/docs/deep", "leaf.txt", &d, &times) && times == 1);

    {
        vfs_dirent_t a, b;

        vcheck("readdir is stateless: same index, same answer",
               vfs_readdir("/", 0, &a) == 0 && vfs_readdir("/", 0, &b) == 0 &&
               strcmp(a.name, b.name) == 0);
        vcheck("readdir past the end returns 1", vfs_readdir("/", 1000, &d) == 1);
    }

    /* --- таблица дескрипторов --- */
    static int fds[VFS_MAX_FDS + 8];
    int nfds = 0, last = 0;

    for (; nfds < VFS_MAX_FDS + 8; nfds++) {
        last = vfs_open("/hello.txt");
        if (last < 0)
            break;
        fds[nfds] = last;
    }

    vcheck("table fills up at exactly MAX - FIRST descriptors", nfds == VFS_MAX_FDS - VFS_FIRST_FD);
    vcheck("full table -> ENFILE", last == VFS_ENFILE);

    int distinct = 1;

    for (int i = 0; i < nfds; i++) {
        if (fds[i] < VFS_FIRST_FD || fds[i] >= VFS_MAX_FDS)
            distinct = 0;
        for (int j = 0; j < i; j++) {
            if (fds[i] == fds[j])
                distinct = 0;
        }
    }
    vcheck("all descriptors are distinct and in range", distinct);

    for (int i = 0; i < nfds; i++)
        vfs_close(fds[i]);

    vcheck("no descriptors left open", vfs_open_files() == open0);

    int a = vfs_open("/hello.txt");
    int b = vfs_open("/hello.txt");

    vcheck("two opens give two descriptors", a >= VFS_FIRST_FD && b >= VFS_FIRST_FD && a != b);
    vcheck("first descriptor advances", vfs_read(a, buf, 5) == 5 && memcmp(buf, "Hello", 5) == 0);
    vcheck("second descriptor has its own position", vfs_read(b, buf, 1) == 1 && buf[0] == 'H');
    vfs_close(a);
    vfs_close(b);

    vcheck("double close -> EBADF", vfs_close(a) == VFS_EBADF);
    vcheck("read after close -> EBADF", vfs_read(a, buf, 4) == VFS_EBADF);
    vcheck("seek after close -> EBADF", vfs_seek(a, 0, VFS_SEEK_SET) == VFS_EBADF);
    vcheck("close(-1) -> EBADF", vfs_close(-1) == VFS_EBADF);
    vcheck("close(1000) -> EBADF", vfs_close(1000) == VFS_EBADF);
    vcheck("stdin/stdout/stderr are reserved", vfs_close(0) == VFS_EBADF && vfs_close(1) == VFS_EBADF &&
                                               vfs_close(2) == VFS_EBADF);

    /* --- страницы модуля не должны попасть в аллокатор --- */
    uint32_t mstart, mend;

    if (initrd_range(&mstart, &mend) == 0) {
        uint32_t n = 0;
        int overlap = 0;

        while (n < 300) {
            pages[n] = pmm_alloc_page();
            if (!pages[n])
                break;
            n++;
        }

        for (uint32_t i = 0; i < n; i++) {
            if (pages[i] < mend && pages[i] + PAGE_SIZE > mstart)
                overlap = 1;
            memset((void *)pages[i], 0xEE, PAGE_SIZE);      /* затираем всё, что выдал аллокатор */
        }

        vcheck("pmm never hands out module pages", !overlap);
        vcheck("module intact after filling every allocated page",
               verify_numbers(512, &bytes) && bytes == NUMBERS_SIZE);

        for (uint32_t i = 0; i < n; i++)
            pmm_free_page(pages[i]);
    } else {
        vcheck("initrd module is present", 0);
    }

    vcheck("no descriptors leaked by the whole test", vfs_open_files() == open0);

    if (bad_checks)
        kprintf("%d of %d checks FAILED\n", bad_checks, total_checks);
    else
        kprintf("all %d checks passed\n", total_checks);
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