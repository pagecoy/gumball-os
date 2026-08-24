#include "fs.h"
#include "../drivers/term.h"
#include <stddef.h>

static uint8_t* ram_disk_start = 0;
static uint32_t ram_disk_size = 0;
static struct gfs_header* fs_header = 0;
static int current_dir = GFS_ROOT_INDEX;

static int str_equal(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return *a == *b; 
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

/* Bump allocator: hand out the next free chunk of the data region.
   Old space from deleted/regrown entries is never reclaimed here */
static uint32_t fs_alloc_data(uint32_t size) {
    uint32_t offset = fs_header->data_write_ptr;
    fs_header->data_write_ptr += size;
    return offset;
}

/* Scans the "mailboxes"/header to find used = 0 
   which means empty or tombstoned and hands back its index */
static int fs_find_free_slot(void) {
    for (int i = 0; i < GFS_MAX_FILES; i++) {
        if (!fs_header->files[i].used) return i;
    }
    return -1;
}

/* Looks up files/dirs in current dir_index's child index list from the 
   data region. This is so that touch, cat, rm, etc. only sees files in the current dir.
   This replaces the old loop that checks the entire files[] table before dirs existed  */
static int fs_find_child(int dir_index, const char* name) {
    struct gfs_entry* dir = &fs_header->files[dir_index];
    uint8_t* list = ram_disk_start + dir->offset;
    for (uint32_t i = 0; i < dir->size; i++) {
        int idx = list[i];
        if (fs_header->files[idx].used && str_equal(fs_header->files[idx].name, name)) {
            return idx;
        }
    }
    return -1;
}

/* If a new file/dir is created, its parent directory's child list needs to be updated by
   one byte. Since data region is append-only, we copy the current child list into a buffer ->
   add the new index, and write the whole new list using fs_alloc_data, then repoints 
   offset/size to the new location */
static void fs_append_child(int dir_index, int child_index) {
    struct gfs_entry* dir = &fs_header->files[dir_index];
    uint8_t temp[GFS_MAX_FILES];
    uint32_t old_size = dir->size;
    uint8_t* old_list = ram_disk_start + dir->offset;

    for (uint32_t i = 0; i < old_size; i++) temp[i] = old_list[i];
    temp[old_size] = (uint8_t) child_index;
    uint32_t new_size = old_size + 1;

    uint32_t new_offset = fs_alloc_data(new_size);
    uint8_t* dest = ram_disk_start + new_offset;
    for (uint32_t i = 0; i < new_size; i++) dest[i] = temp[i];

    dir->offset = new_offset;
    dir->size = new_size;
}

/* Rebuild a directory's child list without one entry (used by rm) */
static void fs_remove_child(int dir_index, int child_index) {
    struct gfs_entry* dir = &fs_header->files[dir_index];
    uint8_t temp[GFS_MAX_FILES];
    uint32_t old_size = dir->size;
    uint8_t* old_list = ram_disk_start + dir->offset;

    uint32_t new_size = 0;
    for (uint32_t i = 0; i < old_size; i++) {
        if (old_list[i] != (uint8_t) child_index) {
            temp[new_size++] = old_list[i];
        }
    }

    uint32_t new_offset = fs_alloc_data(new_size);
    uint8_t* dest = ram_disk_start + new_offset;
    for (uint32_t i = 0; i < new_size; i++) dest[i] = temp[i];

    dir->offset = new_offset;
    dir->size = new_size;
}

/* Walks parent links from current_dir up to root, collecting entry
   indices on a small stack, then writes them out in root-to-current
   order into buf, joined by '/'. */
void fs_current_path(char* buf, int buf_size) {
    int stack[GFS_MAX_FILES];
    int depth = 0;
    int idx = current_dir;

    while (idx != GFS_ROOT_INDEX) {
        stack[depth++] = idx;
        idx = fs_header->files[idx].parent;
    }

    int pos = 0;
    buf[pos++] = '/';
    for (int i = depth - 1; i >= 0; i--) {
        const char* name = fs_header->files[stack[i]].name;
        int j = 0;
        while (name[j] != '\0' && pos < buf_size - 2) {
            buf[pos++] = name[j++];
        }
        if (i > 0) buf[pos++] = '/';
    }
    buf[pos] = '\0';
}

/* Handles exactly one path segment against the CURRENT current_dir */
static int fs_cd_step(const char* name) {
    if (name[0] == '\0' || str_equal(name, ".")) return 1;

    if (str_equal(name, "..") || str_equal(name, "../")) {
        int parent = fs_header->files[current_dir].parent;
        if (parent != -1) current_dir = parent;
        return 1;
    }

    int idx = fs_find_child(current_dir, name);
    if (idx == -1 || fs_header->files[idx].type != GFS_TYPE_DIR) {
        term_set_color(0x0C);
        term_write("fs: cd: '");
        term_write(name);
        term_write("' is not a directory\n");
        term_set_color(0x0F);
        return 0;
    }

    current_dir = idx;
    return 1;
}

/* Splits "path" into a resolved directory index + final name component.
   "home/user.txt" -> dir = the "home" entry, name = "user.txt"
   "user.txt"       -> dir = current_dir,      name = "user.txt"
   Leaves current_dir unchanged either way. Returns 0 (fs_cd already
   printed the error) if any directory component doesn't exist. */
static int fs_resolve_path(const char* path, int* out_dir, char* out_name) {
    int saved = current_dir;

    int last_slash = -1;
    for (int i = 0; path[i] != '\0'; i++) {
        if (path[i] == '/' && path[i + 1] != '\0') last_slash = i;
    }

    if (last_slash == -1) {
        *out_dir = current_dir;
        fs_copy_name(out_name, path);
        return 1;
    }

    char dir_part[128];
    if (last_slash == 0) {
        /* path was like "/file.txt" - directory part is just root */
        dir_part[0] = '/';
        dir_part[1] = '\0';
    } else {
        int i = 0;
        for (; i < last_slash && i < 127; i++) dir_part[i] = path[i];
        dir_part[i] = '\0';
    }

    if (!fs_cd(dir_part)) {
        current_dir = saved;
        return 0;
    }

    *out_dir = current_dir;
    fs_copy_name(out_name, path + last_slash + 1);
    current_dir = saved;
    return 1;
}

/* Deletes entry at idx by index directly (already resolved). If it's a
   non-empty directory and recursive is set, deletes all its children
   first, depth-first, before removing itself. */
static void fs_remove_entry(int idx, int recursive) {
    struct gfs_entry* e = &fs_header->files[idx];

    if (e->type == GFS_TYPE_DIR && e->size > 0 && recursive) {
        uint8_t children[GFS_MAX_FILES];
        uint32_t count = e->size;
        uint8_t* list = ram_disk_start + e->offset;
        for (uint32_t i = 0; i < count; i++) children[i] = list[i];

        for (uint32_t i = 0; i < count; i++) {
            fs_remove_entry(children[i], recursive);
        }
    }

    fs_remove_child(e->parent, idx);
    for (int i = 0; i < GFS_NAME_LEN; i++) e->name[i] = 0;
    e->offset = 0;
    e->size = 0;
    e->used = 0;
    fs_header->num_files--;
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
        fs_header->magic[2] != 'S' || fs_header->magic[3] != '2') {
        
        term_write("fs: Disk unformatted. Formatting...\n");
        
        /* Wipe the header */
        for (uint32_t i = 0; i < sizeof(struct gfs_header); i++) {
            ram_disk_start[i] = 0;
        }
        
        /* Write our magic number and set file count to 0 */
        fs_header->magic[0] = 'G';
        fs_header->magic[1] = 'F';
        fs_header->magic[2] = 'S';
        fs_header->magic[3] = '2';
        fs_header->num_files = 0;
        fs_header->data_write_ptr = sizeof(struct gfs_header);

        /* Create the root directory as a real entry at index 0 */
        struct gfs_entry* root = &fs_header->files[GFS_ROOT_INDEX];
        fs_copy_name(root->name, "/");
        root->type = GFS_TYPE_DIR;
        root->used = 1;
        root->offset = fs_header->data_write_ptr;
        root->size = 0;
        root->parent = -1;
        fs_header->num_files = 1;
    } else {
        term_write("fs: Loaded existing disk with ");
        term_write_uint(fs_header->num_files);
        term_write(" files.\n");
    }

    current_dir = GFS_ROOT_INDEX;
}


/* Lists files in the given path if one is provided (resolved via fs_cd,
   then current_dir restored so ls doesn't actually move you), otherwise
   falls back to listing the current directory. */
void fs_list_files(const char* path) {
    int target_dir = current_dir;

    if (path != NULL && path[0] != '\0') {
        int saved = current_dir;
        if (!fs_cd(path)) return;
        target_dir = current_dir;
        current_dir = saved;
    }

    struct gfs_entry* dir = &fs_header->files[target_dir];
    if (dir->size == 0) {
        term_write("fs: ls: directory is empty\n");
        return;
    }

    uint8_t* list = ram_disk_start + dir->offset;
    for (uint32_t i = 0; i < dir->size; i++) {
        uint8_t child_idx = list[i];
        
        if (child_idx >= GFS_MAX_FILES || !fs_header->files[child_idx].used) {
            continue; 
        }
        
        struct gfs_entry* e = &fs_header->files[child_idx];
        term_write("  - ");
        if (e->type == GFS_TYPE_DIR) {
            term_set_color(0x09);
            term_write(e->name);
            term_write("/");
            term_set_color(0x0F);
        } else {
            term_write(e->name);
        }
        term_write("\n");
    }
}

void fs_pwd(void) {
    char path_buf[128];
    fs_current_path(path_buf, sizeof(path_buf));
    term_write(path_buf);
    term_write("\n");
}

/* Identical to mkdir: find a free slot, stamp name and type, set parent to what dir_idx is, 
   -> then call fs_append_child to append the new entry's index to dir_idx's children list.
   The difference is type and mkdir starts with an empty child list of its own (size=0) */
void fs_touch_file(const char* path) {
    int dir_idx;
    char name[GFS_NAME_LEN];
    if (!fs_resolve_path(path, &dir_idx, name)) return;

    if (fs_header->num_files >= GFS_MAX_FILES) {
        term_set_color(0x0C);
        term_write("fs: disk is full (max ");
        term_write_uint(GFS_MAX_FILES);
        term_write(" files)\n");
        term_set_color(0x0F);
        return;
    }

    if (fs_find_child(dir_idx, name) != -1) {
        term_set_color(0x0C);
        term_write("fs: file '");
        term_write(path);
        term_write("' already exists\n");
        term_set_color(0x0F);
        return;
    }

    int idx = fs_find_free_slot();
    if (idx == -1) {
        term_set_color(0x0C);
        term_write("fs: disk full\n");
        term_set_color(0x0F);
        return;
    }

    struct gfs_entry* new_file = &fs_header->files[idx];
    fs_copy_name(new_file->name, name);
    new_file->type = GFS_TYPE_FILE;
    new_file->used = 1;
    new_file->offset = fs_header->data_write_ptr;
    new_file->size = 0;
    new_file->parent = (int8_t) dir_idx;

    fs_header->num_files++;
    fs_append_child(dir_idx, idx);
}

void fs_mkdir(const char* path) {
    int dir_idx;
    char dirname[GFS_NAME_LEN];
    if (!fs_resolve_path(path, &dir_idx, dirname)) return;

    if (fs_header->num_files >= GFS_MAX_FILES) {
        term_set_color(0x0C);
        term_write("fs: disk is full (max ");
        term_write_uint(GFS_MAX_FILES);
        term_write(" files)\n");
        term_set_color(0x0F);
        return;
    }

    if (fs_find_child(dir_idx, dirname) != -1) {
        term_set_color(0x0C);
        term_write("fs: mkdir: '");
        term_write(path);
        term_write("' already exists\n");
        term_set_color(0x0F);
        return;
    }

    int slot = fs_find_free_slot();
    if (slot == -1) {
        term_set_color(0x0C);
        term_write("fs: disk is full\n");
        term_set_color(0x0F);
        return;
    }
    struct gfs_entry* new_dir = &fs_header->files[slot];
    fs_copy_name(new_dir->name, dirname);
    new_dir->type = GFS_TYPE_DIR;
    new_dir->used = 1;
    new_dir->offset = fs_header->data_write_ptr;
    new_dir->size = 0;
    new_dir->parent = (int8_t) dir_idx;

    fs_header->num_files++;
    fs_append_child(dir_idx, slot);
}

/* Splits a path like "../../docs/nested" on '/' and walks it segment
   by segment. Fails atomically if any segment along the way is
   invalid, current_dir is restored to where it started, same as a
   real shell's cd not leaving you half-moved on a bad path. */
int fs_cd(const char* path) {
    int start_dir = current_dir;
    const char* p = path;

    if (path[0] == '/') {
        current_dir = GFS_ROOT_INDEX;
        p++;
    }

    char segment[GFS_NAME_LEN];
    int seg_len = 0;

    while (1) {
        if (*p == '/' || *p == '\0') {
            segment[seg_len] = '\0';
            if (seg_len > 0 && !fs_cd_step(segment)) {
                current_dir = start_dir;
                return 0;
            }
            seg_len = 0;
            if (*p == '\0') break;
            p++;
        } else {
            if (seg_len < GFS_NAME_LEN - 1) segment[seg_len++] = *p;
            p++;
        }
    }

    return 1;
}

/* New check: if you are removing a directory that still has children in it (size > 0), it refuses
   to delete. Otherwise, delete and remove its entry (set used=0) and detatch itself from 
   its parent's child list */
void fs_remove_path(const char* path, int recursive, int force) {
    int dir_idx;
    char name[GFS_NAME_LEN];
    if (!fs_resolve_path(path, &dir_idx, name)) return;

    int idx = fs_find_child(dir_idx, name);
    if (idx == -1) {
        if (!force) {
            term_set_color(0x0C);
            term_write("fs: rm: file or directory '");
            term_write(path);
            term_write("' cannot be found\n");
            term_set_color(0x0F);
        }
        return;
    }

    if (fs_header->files[idx].type == GFS_TYPE_DIR &&
        fs_header->files[idx].size > 0 && !recursive) {
        term_set_color(0x0C);
        term_write("fs: rm: '");
        term_write(path);
        term_write("' is not empty (use -r)\n");
        term_set_color(0x0F);
        return;
    }

    fs_remove_entry(idx, recursive);
}

/* Find or create an entry then always allocate a fresh data block via fs_alloc_data and 
   repoint that same entry's offset/size at it. No table shuffling like in v1.0.0 */
void fs_write_file(const char* path, const char* content, uint32_t size) {
    int dir_idx;
    char name[GFS_NAME_LEN];
    if (!fs_resolve_path(path, &dir_idx, name)) return;

    int idx = fs_find_child(dir_idx, name);

    if (idx != -1 && fs_header->files[idx].type == GFS_TYPE_DIR) {
        term_set_color(0x0C);
        term_write("fs: '");
        term_write(path);
        term_write("' is a directory\n");
        term_set_color(0x0F);
        return;
    }

    if (idx == -1) {
        if (fs_header->num_files >= GFS_MAX_FILES) {
            term_set_color(0x0C);
            term_write("fs: disk is full\n");
            term_set_color(0x0F);
            return;
        }
        idx = fs_find_free_slot();
        if (idx == -1) {
            term_set_color(0x0C);
            term_write("fs: no free slot found (bookkeeping desync)\n");
            term_set_color(0x0F);
            return;
        }
        fs_copy_name(fs_header->files[idx].name, name);
        fs_header->files[idx].type = GFS_TYPE_FILE;
        fs_header->files[idx].used = 1;
        fs_header->files[idx].parent = (int8_t) dir_idx;
        fs_header->num_files++;
        fs_append_child(dir_idx, idx);
    }

    struct gfs_entry* file = &fs_header->files[idx];
    uint32_t data_offset = fs_alloc_data(size);
    uint8_t* dest = ram_disk_start + data_offset;
    for (uint32_t i = 0; i < size; i++) dest[i] = content[i];

    file->offset = data_offset;
    file->size = size;
}

void fs_cat_file(const char* path) {
    int dir_idx;
    char name[GFS_NAME_LEN];
    if (!fs_resolve_path(path, &dir_idx, name)) return;

    int idx = fs_find_child(dir_idx, name);
    if (idx == -1) {
        term_set_color(0x0C);
        term_write("fs: cat: file '");
        term_write(path);
        term_write("' not found\n");
        term_set_color(0x0F);
        return;
    }

    struct gfs_entry* file = &fs_header->files[idx];
    if (file->type == GFS_TYPE_DIR) {
        term_set_color(0x0C);
        term_write("fs: cat: '");
        term_write(path);
        term_write("' is a directory\n");
        term_set_color(0x0F);
        return;
    }

    uint8_t* data = ram_disk_start + file->offset;
    for (uint32_t i = 0; i < file->size; i++) term_putchar(data[i]);
    term_write("\n");
}

void fs_rename_file(const char* old_path, const char* new_path) {
    int old_dir, new_dir;
    char old_name[GFS_NAME_LEN], new_name[GFS_NAME_LEN];
    if (!fs_resolve_path(old_path, &old_dir, old_name)) return;
    if (!fs_resolve_path(new_path, &new_dir, new_name)) return;

    int idx = fs_find_child(old_dir, old_name);

    if (idx == -1) {
        term_set_color(0x0C);
        term_write("fs: mv: file '");
        term_write(old_path);
        term_write("' cannot be found\n");
        term_set_color(0x0F);
        return;
    }

    if (fs_find_child(new_dir, new_name) != -1) {
        term_set_color(0x0C);
        term_write("fs: mv: file '");
        term_write(new_path);
        term_write("' already exists\n");
        term_set_color(0x0F);
        return;
    }

    fs_copy_name(fs_header->files[idx].name, new_name);

    if (old_dir != new_dir) {
        fs_remove_child(old_dir, idx);
        fs_append_child(new_dir, idx);
        fs_header->files[idx].parent = (int8_t) new_dir;
    }
}

void fs_debug_dump(void) {
    term_write("---- GFS2 raw state ----\n");
    term_write("magic: ");
    for (int i = 0; i < 4; i++) term_putchar(fs_header->magic[i]);
    term_write("\n");
    term_write("num_files: ");
    term_write_uint(fs_header->num_files);
    term_write("\n");
    term_write("data_write_ptr: ");
    term_write_uint(fs_header->data_write_ptr);
    term_write("\n");
    term_write("sizeof(header): ");
    term_write_uint((uint32_t) sizeof(struct gfs_header));
    term_write("\n");

    for (int i = 0; i < GFS_MAX_FILES; i++) {
        struct gfs_entry* e = &fs_header->files[i];
        if (!e->used) continue;
        term_write("[");
        term_write_uint(i);
        term_write("] name='");
        term_write(e->name);
        term_write("' type=");
        term_write_uint(e->type);
        term_write(" offset=");
        term_write_uint(e->offset);
        term_write(" size=");
        term_write_uint(e->size);
        term_write(" parent=");
        term_write_uint((uint8_t) e->parent);
        term_write("\n");
    }
}