#include <stdint.h>
#include "keyboard.h"
#include "../arch/io.h"

#define KBD_DATA_PORT 0x60

static int extended = 0; 

#define SC_LSHIFT       0x2A
#define SC_RSHIFT       0x36
#define SC_LSHIFT_REL   0xAA
#define SC_RSHIFT_REL   0xB6
#define SC_CAPSLOCK     0x3A

static const char scancode_to_ascii[] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0, /* left ctrl */
    'a','s','d','f','g','h','j','k','l',';','\'','`',
    0, /* left shift */
    '\\','z','x','c','v','b','n','m',',','.','/',
    0, /* right shift */
    '*',
    0, /* alt */
    ' ', /* space */
};

static const char scancode_to_ascii_shifted[] = {
    0,  27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,
    'A','S','D','F','G','H','J','K','L',':','"','~',
    0,
    '|','Z','X','C','V','B','N','M','<','>','?',
    0,
    '*',
    0,
    ' ',
};

#define BUFFER_SIZE 256
static char buffer[BUFFER_SIZE];
static int buf_head = 0;
static int buf_tail = 0;

static int shift_held = 0;
static int caps_lock_on = 0;

static inline int is_letter(char c) {
    return (c >= 'a' && c <= 'z');
}

void keyboard_init(void) {
    buf_head = 0;
    buf_tail = 0;
    shift_held = 0;
    caps_lock_on = 0;
}

void keyboard_handler(void) {
    uint8_t scancode = inb(KBD_DATA_PORT);

    if (scancode == 0xE0) {
        extended = 1;
        return;
    }

    if (extended) {
        extended = 0;
        
        if (scancode & 0x80) return; 

        char special = 0;
        if (scancode == 0x48) special = KEY_UP;
        else if (scancode == 0x50) special = KEY_DOWN;

        if (special) {
            int next_head = (buf_head + 1) % BUFFER_SIZE;
            if (next_head != buf_tail) {
                buffer[buf_head] = special;
                buf_head = next_head;
            }
        }
        return;
    }

    if (scancode == SC_LSHIFT || scancode == SC_RSHIFT) {
        shift_held = 1;
        return;
    }
    if (scancode == SC_LSHIFT_REL || scancode == SC_RSHIFT_REL) {
        shift_held = 0;
        return;
    }
    if (scancode == SC_CAPSLOCK) {
        caps_lock_on = !caps_lock_on;
        return;
    }

    if (scancode & 0x80) {
        return;
    }
    if (scancode >= sizeof(scancode_to_ascii)) {
        return;
    }

    char base = scancode_to_ascii[scancode];
    if (base == 0) {
        return;
    }

    int use_shifted = shift_held;
    if (caps_lock_on && is_letter(base)) {
        use_shifted = !use_shifted;
    }

    char c = use_shifted ? scancode_to_ascii_shifted[scancode] : base;

    int next_head = (buf_head + 1) % BUFFER_SIZE;
    if (next_head != buf_tail) {
        buffer[buf_head] = c;
        buf_head = next_head;
    }
}

char keyboard_read_char(void) {
    if (buf_tail == buf_head) {
        return 0;
    }
    char c = buffer[buf_tail];
    buf_tail = (buf_tail + 1) % BUFFER_SIZE;
    return c;
}