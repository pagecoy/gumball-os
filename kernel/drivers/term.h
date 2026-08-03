#include <stdint.h>

void term_clear(void);
void term_write(const char* str);
void term_set_color(uint8_t color);
void term_putchar(char c);
void term_write_uint(uint32_t num);