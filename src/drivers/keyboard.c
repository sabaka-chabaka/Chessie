#include <stdint.h>

#include <io.h>
#include <irq.h>
#include <keyboard.h>
#include <pic.h>

#define BUF_MASK (KBD_BUFFER_SIZE - 1)

#define SC_LSHIFT 0x2A
#define SC_RSHIFT 0x36
#define SC_CAPS   0x3A

static const char map_normal[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\b','\t',
    'q','w','e','r','t','y','u','i','o','p','[',']','\n', 0, 'a','s',
    'd','f','g','h','j','k','l',';','\'','`', 0,'\\','z','x','c','v',
    'b','n','m',',','.','/', 0,'*', 0,' ', 0, 0, 0, 0, 0, 0,
};

static const char map_shift[128] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+','\b','\t',
    'Q','W','E','R','T','Y','U','I','O','P','{','}','\n', 0, 'A','S',
    'D','F','G','H','J','K','L',':','"','~', 0,'|','Z','X','C','V',
    'B','N','M','<','>','?', 0,'*', 0,' ', 0, 0, 0, 0, 0, 0,
};

static volatile uint8_t head;
static volatile uint8_t tail;
static char buffer[KBD_BUFFER_SIZE];

static uint8_t shift;
static uint8_t caps;
static uint8_t extended;

static void buffer_put(char c) {
    uint8_t next = (head + 1) & BUF_MASK;
    if (next == tail) return;
    buffer[head] = c;
    head = next;
}

static void kbd_callback(registers_t *regs) {
    (void)regs;

    uint8_t sc = inb(KBD_DATA);

    if (sc == 0xE0) {
        extended = 1;
        return;
    }

    if (extended) {
        extended = 0;
        return;
    }

    uint8_t released = sc & 0x80;
    sc &= 0x7F;

    if (sc == SC_LSHIFT || sc == SC_RSHIFT) {
        shift = !released;
        return;
    }
    if (sc == SC_CAPS) {
        if (!released)
            caps ^= 1;
        return;
    }
    if (released)
        return;

    char c = map_normal[sc];
    if (!c)
        return;

    int is_letter = (c >= 'a' && c <= 'z');
    int upper = is_letter ? (shift ^ caps) : shift;

    if (upper)
        c = map_shift[sc];

    buffer_put(c);
}

void keyboard_init(void) {
    head = 0;
    tail = 0;
    irq_register_handler(IRQ_KEYBOARD, kbd_callback);
    pic_clear_mask(IRQ_KEYBOARD);
}

int keyboard_trygetchar(void) {
    if (head == tail)
        return -1;

    char c = buffer[tail];
    tail = (tail + 1) & BUF_MASK;
    return (unsigned char)c;
}

char keyboard_getchar(void) {
    int c;

    while ((c = keyboard_trygetchar()) < 0) {
        __asm__ volatile ("hlt");
    }

    return (char)c;
}