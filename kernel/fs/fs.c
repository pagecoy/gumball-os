#include "fs.h"
#include "../drivers/term.h"
#include <stddef.h>

/* pointer to the start of our RAM disk */
static uint8_t* ram_disk_start = 0;
static uint32_t ram_disk_size = 0;

/* pointer to the filesystem header in RAM */
static struct gfs_header* fs_header = 0;

static int str_equal(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return *a == *b; 
}

void fs_init(uint32_t magic, void* mbi_ptr) {
    /* 1. Verify GRUB actually passed us valid info */
    if (magic != 0x2BADB002) {
        term_set_color(0x0C); /* red */
        term_write("fs: Invalid Multiboot magic number\n");
        return;
    }

    struct multiboot_info* mbi = (struct multiboot_info*) mbi_ptr;

    /* 2. Check if GRUB loaded a module (our disk.img) */
    if (mbi->mods_count == 0) {
        term_set_color(0x0C);
        term_write("fs: No disk image loaded by GRUB\n");
        return;
    }

    /* 3. Get the memory address of the loaded module */
    struct multiboot_module* modules = (struct multiboot_module*) mbi->mods_addr;
    ram_disk_start = (uint8_t*) modules[0].mod_start;
    ram_disk_size = modules[0].mod_end - modules[0].mod_start;

    term_set_color(0x0A); /* green */
    term_write("fs: Found RAM disk at 0x");
    term_write_uint((uint32_t)ram_disk_start);
    term_write(" (Size: ");
    term_write_uint(ram_disk_size / 1024);
    term_write(" KB)\n");
    term_set_color(0x0F);

    /* 4. Point our header to the very beginning of the RAM disk */
    fs_header = (struct gfs_header*) ram_disk_start;

    /* 5. Check if the disk is formatted. If not, format it */
    if (fs_header->magic[0] != 'G' || fs_header->magic[1] != 'F' || 
        fs_header->magic[2] != 'S' || fs_header->magic[3] != '1') {
        
        term_write("fs: Disk unformatted. Formatting...\n");
        
        /* Wipe the header */
        for (int i = 0; i < sizeof(struct gfs_header); i++) {
            ram_disk_start[i] = 0;
        }
        
        /* Write our magic number and set file count to 0 */
        fs_header->magic[0] = 'G';
        fs_header->magic[1] = 'F';
        fs_header->magic[2] = 'S';
        fs_header->magic[3] = '1';
        fs_header->num_files = 0;
    } else {
        term_write("fs: Loaded existing disk with ");
        term_write_uint(fs_header->num_files);
        term_write(" files.\n");
    }
}

/* Safely copy a filename into a GFS_NAME_LEN buffer, always null-terminated,
   silently truncating names that are too long */
static void fs_copy_name(char* dest, const char* src) {
    int i = 0;
    for (; i < GFS_NAME_LEN - 1 && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
}

/* the ls command to list files in disk */
void fs_list_files(void) {
    if (fs_header->num_files == 0) {
        term_write("fs: ls: disk is empty\n");
        return;
    }
    
    for (uint32_t i = 0; i < fs_header->num_files; i++) {
        term_write("  - ");
        term_write(fs_header->files[i].name);
        term_write("\n");
    }
}

/* Find an empty slot in files[] -> copy the filename into it
   -> set offset to point to freespace after existing files 
   -> set size to 0 -> increment file count (num_files)*/
void fs_touch_file(const char* filename) {
    /* Check if we have room for more files */
    if (fs_header->num_files >= GFS_MAX_FILES) {
        term_set_color(0x0C); /* red */
        term_write("fs: disk is full (max ");
        term_write_uint(GFS_MAX_FILES);
        term_write(" files)\n");
        term_set_color(0x0F);
        return;
    }
    
    /* Check if file already exists */
    for (uint32_t i = 0; i < fs_header->num_files; i++) {
        if (str_equal(fs_header->files[i].name, filename)) {
            term_set_color(0x0C);
            term_write("fs: file '");
            term_write(filename);
            term_write("' already exists\n");
            term_set_color(0x0F);
            return;
        }
    }
    
    /* Calculate where this file's data should start */
    uint32_t data_offset = sizeof(struct gfs_header);
    for (uint32_t i = 0; i < fs_header->num_files; i++) {
        data_offset += fs_header->files[i].size;
    }
    
    /* Create the new file entry */
    struct gfs_entry* new_file = &fs_header->files[fs_header->num_files];
    
    /* Copy filename into the entry */
    fs_copy_name(new_file->name, filename);
    
    new_file->offset = data_offset;
    new_file->size = 0; /* Empty file */
    
    fs_header->num_files++;
    
    /* Writing message to terminal about file creation, commented out cuz linux does this
    term_write("Created file '");
    term_write(filename);
    term_write("'\n"); */
}

/* Find the filename in the header, shift down the remaining files from that index
   ex: if you remove index 1, bring down index 2 to 1, then 1 to 2, then so on
   like a line in a coffee shop, if a person leaves in the middle the people behind them
   shift forward.
   
   This doesnt work like FAT32 or Linux filesystem where you rename files empty
   to mark them as deleted and let its "mailbox" point nowhere*/
void fs_remove_file(const char* filename) {
    /* 1. Find the file in our list */
    int found_index = -1;
    for (uint32_t i = 0; i < fs_header->num_files; i++) {
        if (str_equal(fs_header->files[i].name, filename)) {
            found_index = i;
            break;
        }
    }

    /* 2. If we didn't find it, print an error */
    if (found_index == -1) {
        term_set_color(0x0C); /* Red */
        term_write("fs: rm: file '");
        term_write(filename);
        term_write("' cannot be found\n");
        term_set_color(0x0F);
        return;
    }

    /* 3. Shift all files after the deleted one down by one slot */
    for (uint32_t i = found_index; i < fs_header->num_files - 1; i++) {
        fs_header->files[i] = fs_header->files[i + 1];
    }

    /* 4. Decrease the file count */
    fs_header->num_files--;

    /* 5. Clean up the last slot (so no ghost data remains) */
    struct gfs_entry* last = &fs_header->files[fs_header->num_files];
    for (int i = 0; i < GFS_NAME_LEN; i++) last->name[i] = 0;
    last->offset = 0;
    last->size = 0;

    /*
    term_write("Removed file '");
    term_write(filename);
    term_write("'\n"); */
}

/* To write to a file, normally it would find the location/offset of the filename
   and write the content after it but since each file is next to each other the content
   will overwrite the file next to it.
   
   Like: if fileA is 5 bytes long fileB is right next to it. if we change fileA to 10 bytes
   it will overwrite fileB
   
   so what we do is remove the file we're writing to -> put it in the end of the list
   -> write content there */
void fs_write_file(const char* filename, const char* content, uint32_t size) {
    /* 1. If the file already exists, remove it to make space at the end */
    int found_index = -1;
    for (uint32_t i = 0; i < fs_header->num_files; i++) {
        if (str_equal(fs_header->files[i].name, filename)) {
            found_index = i;
            break;
        }
    }

    if (found_index != -1) {
        /* Shift files down to delete it */
        for (uint32_t i = found_index; i < fs_header->num_files - 1; i++) {
            fs_header->files[i] = fs_header->files[i + 1];
        }
        fs_header->num_files--;
    }

    /* 2. Check if we have room */
    if (fs_header->num_files >= GFS_MAX_FILES) {
        term_set_color(0x0C);
        term_write("fs: disk is full\n");
        term_set_color(0x0F);
        return;
    }

    /* 3. Calculate offset for the new file (at the very end of existing data) */
    uint32_t data_offset = sizeof(struct gfs_header);
    for (uint32_t i = 0; i < fs_header->num_files; i++) {
        data_offset += fs_header->files[i].size;
    }

    /* 4. Create the entry */
    struct gfs_entry* new_file = &fs_header->files[fs_header->num_files];
    fs_copy_name(new_file->name, filename);
    
    new_file->offset = data_offset;
    new_file->size = size;

    /* 5. Copy the actual text content into the RAM disk */
    uint8_t* dest = ram_disk_start + data_offset;
    for (uint32_t i = 0; i < size; i++) {
        dest[i] = content[i];
    }

    fs_header->num_files++;
    /*
    term_write("Wrote ");
    term_write_uint(size);
    term_write(" bytes to '");
    term_write(filename);
    term_write("'\n");*/
}

/* Find filename in header and read the content until the end without touching the next file */
void fs_cat_file(const char* filename) {
    /* 1. Find the file */
    int found_index = -1;
    for (uint32_t i = 0; i < fs_header->num_files; i++) {
        if (str_equal(fs_header->files[i].name, filename)) {
            found_index = i;
            break;
        }
    }

    if (found_index == -1) {
        term_set_color(0x0C);
        term_write("fs: cat: file '");
        term_write(filename);
        term_write("' not found\n");
        term_set_color(0x0F);
        return;
    }

    /* 2. Read and print the file contents */
    struct gfs_entry* file = &fs_header->files[found_index];
    uint8_t* data = ram_disk_start + file->offset;
    
    for (uint32_t i = 0; i < file->size; i++) {
        term_putchar(data[i]);
    }
    term_write("\n");
}