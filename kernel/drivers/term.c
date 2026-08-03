#include <stdint.h>
#include <stddef.h>
#include "../arch/io.h"

static uint16_t* const VGA_MEMORY = (uint16_t*) 0xB8000;
static const int VGA_WIDTH = 80;
static const int VGA_HEIGHT = 25;

static int term_row = 0;
static int term_col = 0;
static uint8_t term_color = 0x0A; 

static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t) c | (uint16_t) color << 8;
}

static void term_update_cursor(void) {
    uint16_t pos = term_row * VGA_WIDTH + term_col;
    outb(0x3D4, 14);
    outb(0x3D5, (pos >> 8) & 0xFF);
    outb(0x3D4, 15);
    outb(0x3D5, pos & 0xFF);
}

static void term_scroll(void) {
    for (int y = 1; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            VGA_MEMORY[(y - 1) * VGA_WIDTH + x] = VGA_MEMORY[y * VGA_WIDTH + x];
        }
    }
    
    for (int x = 0; x < VGA_WIDTH; x++) {
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', term_color);
    }
    
    term_row = VGA_HEIGHT - 1;
}

static void term_check_scroll(void) {
    if (term_row >= VGA_HEIGHT) {
        term_scroll();
    }
}

void term_set_color(uint8_t color) {
    term_color = color;
}

void term_clear(void) {
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            VGA_MEMORY[y * VGA_WIDTH + x] = vga_entry(' ', term_color);
        }
    }
    term_row = 0;
    term_col = 0;
    term_update_cursor();
}

void term_putchar(char c) {
    if (c == '\n') {
        term_col = 0;
        term_row++;
        term_check_scroll();
        term_update_cursor();
        return;
    }
    if (c == '\r') {
        term_col = 0;
        term_update_cursor();
        return;
    }
    if (c == '\b') {
        if (term_col > 0) {
            term_col--;
        } else if (term_row > 0) {
            term_row--;
            term_col = VGA_WIDTH - 1;
        }
        VGA_MEMORY[term_row * VGA_WIDTH + term_col] = vga_entry(' ', term_color);
        term_update_cursor();
        return;
    }
    
    VGA_MEMORY[term_row * VGA_WIDTH + term_col] = vga_entry(c, term_color);
    if (++term_col == VGA_WIDTH) {
        term_col = 0;
        term_row++;
    }
    
    term_check_scroll();
    term_update_cursor();
}

void term_write(const char* str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        term_putchar(str[i]);
    }
}

void term_write_uint(uint32_t num) {
    char buf[12];
    int i = 0;
    
    if (num == 0) {
        term_putchar('0');
        return;
    }
    
    while (num > 0) {
        buf[i++] = (num % 10) + '0';
        num /= 10;
    }
    
    while (i > 0) {
        term_putchar(buf[--i]);
    }
}