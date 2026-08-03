#ifndef KEYBOARD_H
#define KEYBOARD_H

#define KEY_UP   1
#define KEY_DOWN 2

void keyboard_init(void);
void keyboard_handler(void);
char keyboard_read_char(void);

#endif