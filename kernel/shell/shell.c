#include "shell.h"
#include "../drivers/term.h"
#include "../drivers/keyboard.h"
#include <stdint.h>
#include <stddef.h>
#include "../fs/fs.h"

#define MAX_ARGS 16

#define HIST_SIZE 10

static void str_copy(char* dest, const char* src) {
    while (*src) { *dest++ = *src++; }
    *dest = '\0';
}
static int str_len(const char* s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

static char history[HIST_SIZE][256];
static int hist_count = 0;
static int hist_index = 0;

static int str_equal(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; 
        b++;
    }
    return *a == *b; 
}

static int parse_args(char* input, char** argv) {
    int argc = 0;

    while (*input) {
        while (*input == ' ') {
            input++;
        }
        if (*input == '\0') {
            break; 
        }
        if (argc < MAX_ARGS) {
            argv[argc] = input;
            argc++;
        }
        while (*input && *input != ' ') {
            input++;
        }
        if (*input == ' ') {
            *input = '\0';
            input++;
        }
    }
    
    return argc;
}

/* --- Eval: Figure out what the user typed --- */
static void shell_execute(int argc, char** argv) {
    if (argc == 0) return;
    const char* cmd = argv[0];

    int redirect_index = -1;
    for (int i = 0; i < argc; i++) {
        if (str_equal(argv[i], ">")) {
            redirect_index = i;
            break;
        }
    }

    if (redirect_index != -1) {
        if (redirect_index + 1 >= argc) {
            term_write("Usage: command > filename\n");
            return;
        }
        
        char* target_file = argv[redirect_index + 1];
        
        char content_buffer[256];
        int pos = 0;
        
        for (int i = 1; i < redirect_index; i++) {
            int len = str_len(argv[i]);
            for (int j = 0; j < len; j++) {
                content_buffer[pos++] = argv[i][j];
            }
            if (i < redirect_index - 1) {
                content_buffer[pos++] = ' ';
            }
        }
        
        fs_write_file(target_file, content_buffer, pos);
        return; 
    }

    if (str_equal(cmd, "help")) {
        term_write("Available commands:\n");
        term_write("  help                 - Show this help message\n");
        term_write("  clear                - Clear the screen\n");
        term_write("  version              - Show Gumball version\n");
        term_write("  echo                 - Print text arguments to screen\n");
        term_write("  echo [TEXT] > [FILE] - Write text to a file\n");
        term_write("  touch [FILE]         - Create a new empty file\n");
        term_write("  ls                   - List files\n");
        term_write("  rm [FILE]            - Remove a file\n");
        term_write("  cat [FILE]           - Display file contents\n");
    } 
    else if (str_equal(cmd, "clear")) {
        term_clear();
    } 
    else if (str_equal(cmd, "version")) {
        term_write("Gumball v1.0.0\n");
    } 
    else if (str_equal(cmd, "echo")) {
        for (int i = 1; i < argc; i++) {
            term_write(argv[i]);
            if (i < argc - 1) {
                term_write(" ");
            }
        }
        term_write("\n");
    }
    else if (str_equal(cmd, "ls")) {
        fs_list_files();
    }
    else if (str_equal(cmd, "touch")) {
        if (argc < 2) {
            term_write("Usage: touch <filename>\n");
        } else {
            for (int i = 1; i < argc; i++) {
                fs_touch_file(argv[i]);
            }
        }
    }
    else if (str_equal(cmd, "rm")) {
        if (argc < 2) {
            term_write("usage: rm <filename> [filename2]...\n");
        } else {
            for (int i = 1; i < argc; i++) {
                fs_remove_file(argv[i]);
            }
        }
    }
    else if (str_equal(cmd, "cat")) {
        if (argc < 2) {
            term_write("usage: cat <filename>\n");
        } else {
            fs_cat_file(argv[1]);
        }
    }
    else if (cmd[0] != '\0') {
        term_write("gumball: unknown command '");
        term_write(cmd);
        term_write("'\n");
    }
}

/* --- The Main Shell Loop (REPL) --- */
void shell_run(void) {
    char line_buffer[256];
    char* argv[MAX_ARGS];
    int pos = 0;

    while (1) {
        term_set_color(0x0E); /* yellow */
        term_write("gumball:/$ ");
        term_set_color(0x0F); /* white */

        pos = 0;
        line_buffer[0] = '\0';

        while (1) {
            char c = keyboard_read_char();
            if (c == 0) continue;

            if (c == KEY_UP) {
                if (hist_index > 0) {
                    hist_index--;

                    term_putchar('\r');
                    for (int i = 0; i < 79; i++) term_putchar(' ');
                    
                    term_putchar('\r');
                    term_set_color(0x0E);
                    term_write("gumball:/$ ");
                    term_set_color(0x0F);
                    str_copy(line_buffer, history[hist_index]);
                    pos = str_len(line_buffer);
                    term_write(line_buffer);
                }
                continue; 
            } 
            else if (c == KEY_DOWN) {
                if (hist_index < hist_count) {
                    hist_index++;

                    term_putchar('\r');
                    for (int i = 0; i < 79; i++) term_putchar(' ');
                    
                    term_putchar('\r');
                    term_set_color(0x0E);
                    term_write("gumball:/$ ");
                    term_set_color(0x0F);
                    if (hist_index == hist_count) {
                        line_buffer[0] = '\0';
                        pos = 0;
                    } else {
                        str_copy(line_buffer, history[hist_index]);
                        pos = str_len(line_buffer);
                    }
                    term_write(line_buffer);
                }
                continue; 
            }

            if (c == '\n') {
                term_putchar('\n');
                line_buffer[pos] = '\0';

                if (pos > 0) {
                    if (hist_count < HIST_SIZE) {
                        str_copy(history[hist_count], line_buffer);
                        hist_count++;
                    } else {
                        for (int i = 0; i < HIST_SIZE - 1; i++) {
                            str_copy(history[i], history[i+1]);
                        }
                        str_copy(history[HIST_SIZE - 1], line_buffer);
                    }
                }
                hist_index = hist_count;
                break;
            } 
            else if (c == '\b') {
                if (pos > 0) {
                    pos--;
                    term_putchar('\b');
                }
            } 
            else {
                if (pos < 255) {
                    line_buffer[pos++] = c;
                    term_putchar(c);
                }
            }
        }
        int argc = parse_args(line_buffer, argv);
        shell_execute(argc, argv);
    }
}