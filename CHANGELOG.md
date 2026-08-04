# Changelog

Changes happening to Gumball will be available here.
**Format: MM-DD-YY**  

## Version 1.2.0 - 08-04-26
### Added
* `mv` can now move files between directories, not just rename in place.
* `rm` now accepts `-r` (delete non-empty directories) and `-f` (don't error on a missing target) flags.
* `ls` can list a directory without `cd`-ing into it first (e.g. `ls home`).
* `touch`, `mkdir`, `echo [TEXT] > [FILE]`, `cat`, and `mv` can all now operate on nested paths (e.g. `touch home/notes.txt`) without needing to be inside that directory.

### Files Changed
* `/kernel/fs/fs.c`: `fs_resolve_path`, `fs_remove_entry`, updated `fs_touch_file`, `fs_mkdir`, `fs_write_file`, `fs_cat_file`, `fs_rename_file`, `fs_list_files`, `fs_remove_path` (was `fs_remove_file`), `fs_init`
* `/kernel/fs/fs.h`: updated `fs_remove_file` signature to `fs_remove_path`, added new parameter to `ls`.
* `/kernel/shell/shell.c`: new commands added.


## Version 1.1.0 - 08-03-26
### Added
* `mv` command to rename files, doesn't move files yet.
### Files Changed
* `/kernel/fs/fs.c`: `void fs_rename_file(...) {`
* `/kernel/fs/fs.h`: `void fs_rename_file(...);`
* `/kernel/shell/shell.c`: `term_write("...")`...`else if (str_equal(cmd, "mv")) {`

## Initial Commit version 1.0.0 - 08-03-26
No changes. Minimal features (view initial commit of README)