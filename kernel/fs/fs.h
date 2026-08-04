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

/* --- Our Custom Filesystem (GFS) Structures --- */
#define GFS_MAX_FILES 16
#define GFS_NAME_LEN  32
#define GFS_ROOT_INDEX 0

#define GFS_TYPE_FILE 0
#define GFS_TYPE_DIR  1

/* A single "Mailbox" shared by files and directories*/
struct gfs_entry {
    char name[GFS_NAME_LEN];
    uint32_t offset;       /* file: where data starts in data region
                              dir: where its child index list starts */
    uint32_t size;         /* file: content length in bytes
                              dir: number of children (1 index byte each) */
    uint8_t type;          /* GFS_TYPE_FILE or GFS_TYPE_DIR */
    uint8_t used;          /* 0 = empty/tombstone slot, 1 = occupied
                              before directories were implemented I used to 
                              shift data on delete and writing. its removed now
                              because indices must stay stable since dirs refer
                              children by index */
    int8_t parent;         /* entry index of the containing directory.
                              -1 for root (nothing above it) */
};

/* The "Header" - The Table of Contents */
struct gfs_header {
    char magic[4];         /* Should be current GFS version */
    uint32_t num_files;    /* How many files are currently saved */
    uint32_t data_write_ptr; /* bump allocator: next free byte in data region  */
    struct gfs_entry files[GFS_MAX_FILES]; /* The "Mailboxes" */
};

/* Functions */
void fs_init(uint32_t magic, void* mbi_ptr);
void fs_list_files(const char* path); /* pass "" or NULL to list the current directory */
void fs_touch_file(const char* filename);
void fs_remove_path(const char* path, int recursive, int force);
void fs_write_file(const char* filename, const char* content, uint32_t size);
void fs_cat_file(const char* filename);
void fs_rename_file(const char* old_name, const char* new_name);
void fs_mkdir(const char* dirname);
int fs_cd(const char* dirname);
void fs_current_path(char* buf, int buf_size);
void fs_debug_dump(void);
void fs_pwd(void);

#endif