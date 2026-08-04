# Gumball File System - Current Version: GFS2
GFS is the file system used by the Gumball kernel. Information about GFS is in this file.

## Previously
The older version, GFS1, worked differently. Everything was a flat table. When you write to files, that entry has to be deleted, then moved to the back of the table, and be written there. When you deleted something, it's place would be a gap that would be filled with the next entry after it, then that entry's previous place would be a gap so it would be filled by the one following it, then so on. There was no stability of where everything was. That was something Gumball can't have if I were to add directories. So, taking inspiration from existing file systems (e.g. FAT, ext), I made GFS2.

## GFS2
With Gumball just running on a virtual disk as of writing, total number of files and directories you can make is only 16, one of that being root, so it's only **15 free slots**. This is hardcoded for now because I haven't figured out how to calculate slots based on disk space available. File and directory names can be up to 31 characters (32-byte buffer, 1 byte reserved for the null terminator).

### How I Visualize GFS2
Every file and directory is imagined as a mailbox. Every mailbox has a `name`, `offset`, `size`, `type`, `used`, and `parent`. The header, where the pointers to the `offset`/`size` is, is referred to as mailboxes.

**For File Mailboxes**:
* **`offset`:** where its data starts in the data region.
* **`size`:** its content length in bytes
* **`type`:** tell us wether it's a file or directory, `GFS_TYPE_FILE` for files.

**For Directory Mailboxes**
* **`offset`:** where its child index list starts in the data region. What normally is where a file's content is stored, a directory stores a child index list that contains a list of its childrens indexes in the header/mailboxes.
* **`size`:** number of children (1 index byte each)
* **`type`:** tells us wether it's a file or directory, `GFS_TYPE_DIR` for directories.

**Unified Variables Between Files and Dirs**
* **`used`:** is either `0` which is empty or was tombstoned (previously deleted) or `1` which means it's occupied.
* **`parent`:** the index of it's parent directory. `-1` for root since there's nothing above it. Index is it's place in the header/mailboxes, not in the data region.

### Internal Helpers
These aren't commands you type, but the private functions everything else is built on top of.

* **`fs_copy_name`**: safely copies a name into a mailbox's 32-byte `name` field. Always leaves room for a null terminator (max 31 real characters), so a name that's too long gets silently truncated instead of corrupting whatever's stored right after it in memory.

* **`fs_alloc_data`**: the bump allocator. The data region is one long strip of memory that only ever grows forward — this function just hands out the next free chunk and moves the "next free byte" marker (`data_write_ptr`) past it. It never reuses space that was freed by a delete or an overwrite, which is the source of GFS2's fragmentation (a known tradeoff, cleanup is on the way soon).

* **`fs_find_free_slot`**: scans all 16 mailboxes for one marked `used = 0` (either never used, or tombstoned by a delete) and hands back its index. This is what lets deleted slots get reused by whatever's created next.

* **`fs_find_child`**: looks up a name, but only among one directory's *direct children* — not the whole mailbox table. This is what scopes `touch`, `cat`, `rm`, etc. to "the directory you told it to look in" instead of searching the entire disk.

* **`fs_append_child`**: grows a directory's child-index list by one. Since the data region only grows forward, this copies the existing list out, adds the new index, and writes the whole thing fresh via `fs_alloc_data` — the old copy becomes wasted space, same tradeoff as everything else in the data region.

* **`fs_remove_child`**: the mirror of `fs_append_child` — rebuilds a directory's child list with one specific entry removed, used by `rm`.

* **`fs_resolve_path`**: splits something like `"home/notes.txt"` into a resolved directory (the mailbox index for `home`) and a final name (`"notes.txt"`). This is what lets commands reach into other directories without `cd`-ing into them first.

* **`fs_cd_step`** / **`fs_cd`**: `fs_cd_step` handles one segment of a path (a name, `..`, or `.`) against wherever you currently are. `fs_cd` splits a full path like `../../docs` on `/` and walks it one segment at a time using `fs_cd_step`, restoring your original location if any segment along the way turns out invalid.

* **`fs_remove_entry`**: the actual deletion logic behind `rm`. If the target is a non-empty directory and `-r` was used, it deletes every child first (recursively, depth-first) before removing itself.

* **`fs_current_path`**: walks `parent` links backward from wherever you currently are, all the way up to root, then writes the names back out in root-to-current order, joined by `/`. This is what powers both the prompt and `pwd`.

This file is unfinished. I would want to write how each filesystem-connected command but I need to push this today. `/kernel/fs/fs.c` has comments on each command (`ls`, `touch`, `cd`, etc.). But I probably forgot to add some other comments. Bare with me, I'm the only one managing this project.