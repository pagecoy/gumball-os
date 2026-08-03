#ifndef FS_H
#define FS_H

#include <stdint.h>

/* --- Multiboot Structures (Minimal) --- */
struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count; /* how many extra files grub loaded */
    uint32_t mods_addr; /* memory addrss of where the list of the files starts */
};

struct multiboot_module {
    uint32_t mod_start;    /* Start address of the module in RAM */
    uint32_t mod_end;      /* End address */
    uint32_t string;
    uint32_t reserved;
};

/* --- Our Custom Filesystem (FFS) Structures --- */
#define GFS_MAX_FILES 16
#define GFS_NAME_LEN  32

/* A single "Mailbox" */
struct gfs_entry {
    char name[GFS_NAME_LEN];
    uint32_t offset;       /* Where the file data starts in RAM */
    uint32_t size;         /* How many bytes long the file is */
};

/* The "Header" - The Table of Contents */
struct gfs_header {
    char magic[4];         /* Should be "GFS1" */
    uint32_t num_files;    /* How many files are currently saved */
    struct gfs_entry files[GFS_MAX_FILES]; /* The "Mailboxes" */
};

/* Functions */
void fs_init(uint32_t magic, void* mbi_ptr);
void fs_list_files(void);
void fs_touch_file(const char* filename);
void fs_remove_file(const char* filename);
void fs_write_file(const char* filename, const char* content, uint32_t size);
void fs_cat_file(const char* filename);
void fs_rename_file(const char* old_name, const char* new_name);

#endif