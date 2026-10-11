#include <stddef.h>
#include <stdint.h>

#include <io.h>
#include <keyboard.h>
#include <kprintf.h>
#include <pit.h>
#include <shell.h>
#include <string.h>
#include <vga.h>

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

static const struct command commands[] = {
    { "help",   "list commands",        cmd_help   },
    { "clear",  "clear the screen",     cmd_clear  },
    { "uptime", "time since boot",      cmd_uptime },
    { "echo",   "print arguments",      cmd_echo   },
    { "reboot", "restart the machine",  cmd_reboot },
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