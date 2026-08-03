#include "drivers/term.h"
#include "arch/gdt.h"
#include "arch/idt.h"
#include "drivers/keyboard.h"
#include "shell/shell.h"
#include "fs/fs.h"

void kernel_main(uint32_t magic, void* mbi_ptr) {
    term_clear();
    term_set_color(0x09); /* blue */
    term_write("Gumball v1.1.0 booting...\n\n");
    term_set_color(0x0F); /* white */
    term_write("Welcome! This is Gumball OS by pagecoy.\n");
    term_write("Get started by typing 'help' to see available commands\n\n");

    gdt_init();
    idt_init();
    keyboard_init();
    fs_init(magic, mbi_ptr);
    term_write("\n");

    shell_run();
}